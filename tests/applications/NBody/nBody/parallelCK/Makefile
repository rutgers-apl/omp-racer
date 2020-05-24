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
OCTTREE = geometry.h octTree.h blockRadixSort.h transpose.h
GLOBAL_REQUIRE = parallel.h sequence.h $(OCTTREE)
BENCH_REQUIRE = spherical.h
OBJS = CK.o
INTRMEDIATES = CK.bc
OPT_FLAGS = -load $(LLVM_HOME)/lib/DataAnnotation.so -DataAnnotation
OPT      = opt

include ../common/timeRequiredFiles
include MakeBench
