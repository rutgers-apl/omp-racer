
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/CaptureTracking.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Type.h"
#include "llvm/ProfileData/InstrProf.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/EscapeEnumerator.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"

#include <map>
#include <string>
#include <set>

#define DEBUG_TYPE "DataAnnotation"
using namespace llvm;

STATISTIC(NumFunctions, "Number of functions");
STATISTIC(NumInstFunctions, "Number of instrumented functions");
STATISTIC(NumInstrumentedReads, "Number of instrumented reads");
STATISTIC(NumInstrumentedWrites, "Number of instrumented writes");
STATISTIC(NumOmittedReadsBeforeWrite,"Number of reads ignored due to following writes");
STATISTIC(NumAccessesWithBadSize, "Number of accesses with bad size");
STATISTIC(NumInstrumentedVtableWrites, "Number of vtable ptr writes");
STATISTIC(NumInstrumentedVtableReads, "Number of vtable ptr reads");
STATISTIC(NumOmittedReadsFromConstantGlobals, "Number of reads from constant globals");
STATISTIC(NumOmittedReadsFromVtable, "Number of vtable reads");
STATISTIC(NumOmittedNonCaptured, "Number of accesses ignored due to capturing");

namespace{
    struct OmpRaceInstrument : public FunctionPass {
        std::map<std::string, int> DataAnnotation;
        std::set<Value*> DontInstrument;
        static char ID;
        OmpRaceInstrument() : FunctionPass(ID) {}
        
        void getAnalysisUsage(AnalysisUsage &AU) const override;
        bool runOnFunction(Function &F) override;
        bool doInitialization(Module &M) override;   
        bool doFinalization(Module &M) override;
    private:
        long LNumFunctions;
        long LNumInstFunctions;
        long LNumInstrumentedReads;
        long LNumInstrumentedWrites;
        long LNumOmittedReadsBeforeWrite;
        long LNumAccessesWithBadSize;
        long LNumInstrumentedVtableWrites;
        long LNumInstrumentedVtableReads;
        long LNumOmittedReadsFromConstantGlobals;
        long LNumOmittedReadsFromVtable;
        long LNumOmittedNonCaptured;
        Function *OMPInstrumentFunction;
        void initializeCallbacks(Module &M);//
        bool instrumentLoadOrStore(Module *M, Instruction *I, const DataLayout &DL);
        void chooseInstructionsToInstrument(SmallVectorImpl<Instruction *> &Local,
                                            SmallVectorImpl<Instruction *> &All,
                                            const DataLayout &DL);
        bool addrPointsToConstantData(Value *Addr);
        int getMemoryAccessFuncIndex(Value *Addr, const DataLayout &DL);
    };
}

static bool isAtomic(Instruction *I) {
  // TODO: Ask TTI whether synchronization scope is between threads.
  if (LoadInst *LI = dyn_cast<LoadInst>(I))
    return LI->isAtomic() && LI->getSyncScopeID() != SyncScope::SingleThread;
  if (StoreInst *SI = dyn_cast<StoreInst>(I))
    return SI->isAtomic() && SI->getSyncScopeID() != SyncScope::SingleThread;
  if (isa<AtomicRMWInst>(I))
    return true;
  if (isa<AtomicCmpXchgInst>(I))
    return true;
  if (isa<FenceInst>(I))
    return true;
  return false;
}

static bool isVtableAccess(Instruction *I) {
  if (MDNode *Tag = I->getMetadata(LLVMContext::MD_tbaa))
    return Tag->isTBAAVtableAccess();
  return false;
}

// Do not instrument known races/"benign races" that come from compiler
// instrumentatin. The user has no way of suppressing them.
static bool shouldInstrumentReadWriteFromAddress(const Module *M, Value *Addr) {
  // Peel off GEPs and BitCasts.
  Addr = Addr->stripInBoundsOffsets();

  if (GlobalVariable *GV = dyn_cast<GlobalVariable>(Addr)) {
    if (GV->hasSection()) {
      StringRef SectionName = GV->getSection();
      // Check if the global is in the PGO counters section.
      auto OF = Triple(M->getTargetTriple()).getObjectFormat();
      if (SectionName.endswith(
              getInstrProfSectionName(IPSK_cnts, OF, /*AddSegmentInfo=*/false)))
        return false;
    }

    // Check if the global is private gcov data.
    if (GV->getName().startswith("__llvm_gcov") ||
        GV->getName().startswith("__llvm_gcda"))
      return false;
  }

  // Do not instrument acesses from different address spaces; we cannot deal
  // with them.
  if (Addr) {
    Type *PtrTy = cast<PointerType>(Addr->getType()->getScalarType());
    if (PtrTy->getPointerAddressSpace() != 0)
      return false;
  }

  return true;
}

void OmpRaceInstrument::getAnalysisUsage(AnalysisUsage &AU) const {
    //why?
    AU.addRequired<TargetLibraryInfoWrapperPass>();
    AU.addRequired<AAResultsWrapperPass>();
}

bool OmpRaceInstrument::runOnFunction(Function &F){
    errs() << "Function " << F.getName() << '\n';
    Module *M = F.getParent();
    NumFunctions++;
    LNumFunctions++;
    //count the number of instructions in the code
    for (Function::iterator bb = F.begin(), e = F.end(); bb != e; ++bb) {
        for (BasicBlock::iterator i = bb->begin(), e = bb->end(); i != e; ++i) {
            if(DataAnnotation.find(i->getOpcodeName()) == DataAnnotation.end()) {
                DataAnnotation[i->getOpcodeName()] = 1;
            } else {
                DataAnnotation[i->getOpcodeName()] += 1;
            }
        }
    }
    std::map <std::string, int>::iterator i = DataAnnotation.begin();
    std::map <std::string, int>::iterator e = DataAnnotation.end();
    while (i != e) {
        errs() << i->first << ": " << i->second << "\n";
        i++;
    }
    errs() << "\n";
    DataAnnotation.clear();
    
    StringRef functionName = F.getName();
    //is this even needed since we have the library and its not instrumented    
    if( functionName.endswith("_dtor") ||
        functionName.endswith("__omprace__") ||
        functionName.endswith("__clang_call_terminate") ||
        functionName.startswith(".omp.reduction.reduction_func") ||
        (F.getLinkage() == llvm::GlobalValue::AvailableExternallyLinkage)) {
            return false;
    }

    IRBuilder<> IRB(M->getContext());
    initializeCallbacks(*M);
    //SmallVector<Instruction*, 8> RetVec;
    SmallVector<Instruction*, 8> AllLoadsAndStores;
    SmallVector<Instruction*, 8> LocalLoadsAndStores;
    SmallVector<Instruction*, 8> AtomicAccesses;
    //not doing anything with them at them moment
    SmallVector<Instruction*, 8> MemIntrinCalls;
    Function *IF;
    IF = &F;
    bool res = false;
    bool hasCalls = false;
    const DataLayout &DL = IF->getParent()->getDataLayout();
    
    // Traverse all instructions, collect loads/stores/returns, check for calls.
    for (auto &BB : *IF) {
        for (auto &Inst : BB) {
            if (isAtomic(&Inst))
                AtomicAccesses.push_back(&Inst);
            else if (isa<LoadInst>(Inst) || isa<StoreInst>(Inst))
                LocalLoadsAndStores.push_back(&Inst);
            else if (isa<CallInst>(Inst) || isa<InvokeInst>(Inst)) {
                if (isa<MemIntrinsic>(Inst))
                    MemIntrinCalls.push_back(&Inst);
                hasCalls = true;
                chooseInstructionsToInstrument(LocalLoadsAndStores, AllLoadsAndStores,
                                       DL);
            }
        }
        chooseInstructionsToInstrument(LocalLoadsAndStores, AllLoadsAndStores, DL);
    }

    // Instrument memory accesses only if we want to report bugs in the function.
    for (auto Inst : AllLoadsAndStores) {
        res |= instrumentLoadOrStore(M,Inst, DL);
    }

    // Instrument atomic memory accesses in any case (they can be used to
    // implement synchronization).
    //for (auto Inst : AtomicAccesses) {
    //    res |= instrumentAtomic(Inst, DL);
    //}

    return res;
}

bool OmpRaceInstrument::doInitialization(Module &M){
    LNumFunctions = 0;
    LNumInstFunctions = 0;
    LNumInstrumentedReads = 0;
    LNumInstrumentedWrites = 0;
    
    LNumOmittedReadsBeforeWrite = 0;
    LNumAccessesWithBadSize = 0;
    LNumInstrumentedVtableWrites = 0;
    LNumInstrumentedVtableReads = 0;
    LNumOmittedReadsFromConstantGlobals = 0;
    LNumOmittedReadsFromVtable = 0;
    LNumOmittedNonCaptured = 0;
STATISTIC(NumAccessesWithBadSize, "Number of accesses with bad size");
STATISTIC(NumInstrumentedVtableWrites, "Number of vtable ptr writes");
STATISTIC(NumInstrumentedVtableReads, "Number of vtable ptr reads");
STATISTIC(NumOmittedReadsFromConstantGlobals,
          "Number of reads from constant globals");
STATISTIC(NumOmittedReadsFromVtable, "Number of vtable reads");
    return false;
}

//printing out some stat information since -stats doesn't work with release builds
bool OmpRaceInstrument::doFinalization(Module &M){
    errs() << "[STAT] Num Functions: " << LNumFunctions << "\n";
    errs() << "[STAT] Num InstrumentedReads: " << LNumInstrumentedReads << "\n";
    errs() << "[STAT] Num InstrumentedWrites: " << LNumInstrumentedWrites << "\n";
    errs() << "[STAT] Num OmittedReadsBeforeWrite: " << LNumOmittedReadsBeforeWrite << "\n";
    errs() << "[STAT] Num OmittedReadsFromConstantGlobals: " << LNumOmittedReadsFromConstantGlobals << "\n";
    errs() << "[STAT] Num OmittedReadsFromVtable: " << LNumOmittedReadsFromVtable << "\n";
    errs() << "[STAT] Num OmittedNonCaptured: " << LNumOmittedNonCaptured << "\n";
    //    long LNumInstFunctions;
    //    long LNumInstrumentedReads;
    //    long LNumInstrumentedWrites;
    //    long LNumOmittedReadsBeforeWrite;
    //    long LNumOmittedReadsFromConstantGlobals;
    //    long LNumOmittedReadsFromVtable;
    //    long LNumOmittedNonCaptured;

    //    long LNumAccessesWithBadSize; //not incr
    //    long LNumInstrumentedVtableWrites;//not incr
    //    long LNumInstrumentedVtableReads;// not incr
    
    
    
    return false;
}

void OmpRaceInstrument::initializeCallbacks(Module &M){
    IRBuilder<> IRB(M.getContext());
    AttributeList Attr;
    Attr = Attr.addAttribute(M.getContext(), AttributeList::FunctionIndex,
                             Attribute::NoUnwind);

    Type* VoidTy = Type::getVoidTy(M.getContext());
    Type* void_ptr_ty = PointerType::getUnqual(Type::getInt8Ty(M.getContext()));
    Type* int_ty = Type::getInt32Ty(M.getContext());

    //llvm 9 
    OMPInstrumentFunction = cast<Function>(M.getOrInsertFunction(
        "__omprace_instrument_access__", VoidTy, void_ptr_ty, int_ty, void_ptr_ty).getCallee());


}

bool OmpRaceInstrument::instrumentLoadOrStore(Module *M, Instruction *I,
                                            const DataLayout &DL) {
    IRBuilder<> IRB(I);
    bool IsWrite = isa<StoreInst>(*I);
    Value *Addr = IsWrite
        ? cast<StoreInst>(I)->getPointerOperand()
        : cast<LoadInst>(I)->getPointerOperand();

    // swifterror memory addresses are mem2reg promoted by instruction selection.
    // As such they cannot have regular uses like an instrumentation function and
    // it makes no sense to track them as memory.
    if (Addr->isSwiftError())
        return false;

    // don't instrument the added location strings
    if (DontInstrument.count(Addr) == 1){
        return false;
    }

    //int Idx = getMemoryAccessFuncIndex(Addr, DL);
    //if (Idx < 0)
    //    return false;

    
    SmallVector<Value*, 8> ArgsV;
    BitCastInst* bitcast = new BitCastInst(Addr,
        PointerType::getUnqual(IRB.getInt8Ty()),
                                "", I); 
    //push addr as void*
    ArgsV.push_back(bitcast);
    //push type of access as int W:1 R:0
    if(IsWrite){
        Constant* write = IRB.getInt32(1);
        //Constant* write = ConstantInt::get(Type::getInt32Ty(M.getContext()), 1);
        ArgsV.push_back(write);
    }
    else{
        Constant* read = IRB.getInt32(0);
        //Constant* read = ConstantInt::get(Type::getInt32Ty(M.getContext()), 0);
        ArgsV.push_back(read);
    }

    if (IsWrite){
        LNumInstrumentedWrites++;
        NumInstrumentedWrites++;
    } 
    else{
        LNumInstrumentedReads++;
        NumInstrumentedReads++;
    }  
    unsigned Line = 0;
    StringRef File = "none";
    StringRef Dir = "none";
    
    if (DILocation *Loc = I->getDebugLoc()){
        Line = Loc->getLine();
        File = Loc->getFilename();
        Dir = Loc->getDirectory();
        //errs() << "debug info exists File= " << File << ", Line= " << Line << "\n";
    }

    //errs() << "after" << "\n";
    std::string lineStr = std::to_string(Line);
    //std::string cat = (Dir + ":" + File + ":" +lineStr).str();
    std::string cat = (File + ":" +lineStr).str();
    //errs() << "after2" << "\n";
    //StringRef locStr = (Dir + ":" + File + ":" + lineStr).getSingleStringRef();
    //errs() << "locStr= " << locStr << "\n";
    Constant* fName = ConstantDataArray::getString(M->getContext(), cat, true);
    Value* InstLoc = new GlobalVariable(*M, fName->getType(), true, GlobalValue::InternalLinkage, fName);   
    BitCastInst* BCToAddr = new BitCastInst(InstLoc, PointerType::getUnqual(Type::getInt8Ty(M->getContext())),"", I);
    //Value* InstLoc =IRB.CreateGlobalStringPtr(locStr);
    //errs() << "instLoc instruction = " << *InstLoc << "\n";
    //errs() << "bitcast instruction = " << *BCToAddr << "\n";
    //InstLoc->dump();
    ArgsV.push_back(BCToAddr);
    DontInstrument.insert(InstLoc);
    
    /*unsigned Line = 0;
    if (DILocation *Loc = I->getDebugLoc()){
        Line = Loc->getLine();
    }

    Constant* LineConst = IRB.getInt32(Line);
    ArgsV.push_back(LineConst);*/
    IRB.CreateCall(OMPInstrumentFunction, ArgsV);


  return true;
}

// Instrumenting some of the accesses may be proven redundant.
// Currently handled:
//  - read-before-write (within same BB, no calls between)
//  - not captured variables
//
// We do not handle some of the patterns that should not survive
// after the classic compiler optimizations.
// E.g. two reads from the same temp should be eliminated by CSE,
// two writes should be eliminated by DSE, etc.
//
// 'Local' is a vector of insns within the same BB (no calls between).
// 'All' is a vector of insns that will be instrumented.
void OmpRaceInstrument::chooseInstructionsToInstrument(
    SmallVectorImpl<Instruction *> &Local, SmallVectorImpl<Instruction *> &All,
    const DataLayout &DL) {
  std::vector<Instruction*> LoadsAndStoresTotal;
  std::vector<Instruction*> LoadsAndStoresToRemove;
  SmallSet<Value*, 8> WriteTargets;
  // Iterate from the end.
  for (Instruction *I : reverse(Local)) {
    //  continue;
    if (StoreInst *Store = dyn_cast<StoreInst>(I)) {
      Value *Addr = Store->getPointerOperand();
      if (!shouldInstrumentReadWriteFromAddress(I->getModule(), Addr))
        continue;
      //if (shouldNotInstrumentFromMetadata(I)){
      //    continue;
      //}  

      WriteTargets.insert(Addr);
    } else {
      LoadInst *Load = cast<LoadInst>(I);
      Value *Addr = Load->getPointerOperand();
      if (!shouldInstrumentReadWriteFromAddress(I->getModule(), Addr))
        continue;
      if (WriteTargets.count(Addr)) {
        // We will write to this temp, so no reason to analyze the read.
        NumOmittedReadsBeforeWrite++;
        LNumOmittedReadsBeforeWrite++;
        continue;
      }
      if (addrPointsToConstantData(Addr)) {
        // Addr points to some constant data -- it can not race with any writes.
        continue;
      }
      //if (shouldNotInstrumentFromMetadata(I)){
      //    continue;
      //}
    }
    Value *Addr = isa<StoreInst>(*I)
        ? cast<StoreInst>(I)->getPointerOperand()
        : cast<LoadInst>(I)->getPointerOperand();
    if (isa<AllocaInst>(GetUnderlyingObject(Addr, DL)) &&
        !PointerMayBeCaptured(Addr, true, true)) {

        // The variable is addressable but not captured, so it cannot be
        // referenced from a different thread and participate in a data race
        // (see llvm/Analysis/CaptureTracking.h for details).
        LNumOmittedNonCaptured++;
        NumOmittedNonCaptured++;
        continue;
    }
    All.push_back(I);
  }

  for(SmallVectorImpl<Instruction *>::iterator it=All.begin() ; it < All.end(); it++) {
    for(unsigned i = 0; i < (*it)->getNumOperands(); i++) {
      if(((*it)->getOperand(i)->getName().compare(".omp.iv") == 0) ||
         ((*it)->getOperand(i)->getName().compare(".omp.lb") == 0) ||
         ((*it)->getOperand(i)->getName().compare(".omp.ub") == 0) ||
         ((*it)->getOperand(i)->getName().compare(".omp.stride") == 0) ||
         ((*it)->getOperand(i)->getName().compare(".omp.is_last") == 0) ||
         ((*it)->getOperand(i)->getName().compare(".global_tid") == 0)) {
        All.erase(it);
        break;
      }
    }
  }

  Local.clear();
}

bool OmpRaceInstrument::addrPointsToConstantData(Value *Addr) {
  // If this is a GEP, just analyze its pointer operand.
  if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(Addr))
    Addr = GEP->getPointerOperand();

  if (GlobalVariable *GV = dyn_cast<GlobalVariable>(Addr)) {
    if (GV->isConstant()) {
      // Reads from constant globals can not race with any writes.
      NumOmittedReadsFromConstantGlobals++;
      LNumOmittedReadsFromConstantGlobals++;
      return true;
    }
  } else if (LoadInst *L = dyn_cast<LoadInst>(Addr)) {
    if (isVtableAccess(L)) {
      // Reads from a vtable pointer can not race with any writes.
      NumOmittedReadsFromVtable++;
      LNumOmittedReadsFromVtable++;
      return true;
    }
  }
  return false;
}

int OmpRaceInstrument::getMemoryAccessFuncIndex(Value *Addr,
                                              const DataLayout &DL) {
  Type *OrigPtrTy = Addr->getType();
  Type *OrigTy = cast<PointerType>(OrigPtrTy)->getElementType();
  assert(OrigTy->isSized());
  uint32_t TypeSize = DL.getTypeStoreSizeInBits(OrigTy);
  if (TypeSize != 8  && TypeSize != 16 &&
      TypeSize != 32 && TypeSize != 64 && TypeSize != 128) {
    NumAccessesWithBadSize++;
    // Ignore all unusual sizes.
    return -1;
  }
  size_t Idx = countTrailingZeros(TypeSize / 8);
  //assert(Idx < kNumberOfAccessSizes);
  return Idx;
}

/*
bool OmpRaceInstrument::shouldNotInstrumentFromMetadata(Instruction *I){
    if (MDNode* N = I->getMetaData("llvm.mem.parallel_loop_access")){
        return true;
    }
    return false;
}
*/

char OmpRaceInstrument::ID = 0;
static RegisterPass<OmpRaceInstrument> X("DataAnnotation", "Annotates data to use omprace");