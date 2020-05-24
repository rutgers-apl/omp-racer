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
SORT = sampleSort.h quickSort.h transpose.h
GLOBAL_REQUIRE = parallel.h sequence.h geometry.h dataGen.h $(SORT)
BENCH_REQUIRE = ray.h
LOCAL_REQUIRE = kdTree.C kdTree.h rayTriangleIntersect.h
OBJS = kdTree.o
INTRMEDIATES = kdTree.bc
OPT_FLAGS = -load $(LLVM_HOME)/lib/DataAnnotation.so -DataAnnotation
OPT      = opt

include ../common/timeRequiredFiles
include MakeBench
