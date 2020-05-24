ifdef LONG
INTT = -DLONG
endif
ifdef INT
INTT = -DINT
endif

PCC = clang++

include parallelDefs.ser
include parallelDefs.par
include parallelDefs.ompp
include parallelDefs.omprn

# required files
TOPOLOGY = geometry.h topology.h topologyFromTri.C
HASH = deterministicHash.h
GLOBAL_REQUIRE = parallel.h sequence.h delaunayQs.h $(HASH) $(TOPOLOGY)
LOCAL_REQUIRE = 
OBJS = refine.o topologyFromTri.o
INTRMEDIATES = refine.bc topologyFromTri.bc
OPT_FLAGS = -load $(LLVM_HOME)/lib/DataAnnotation.so -DataAnnotation
OPT      = opt

include ../common/timeRequiredFiles
include MakeBench
