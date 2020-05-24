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
NEAREST_NEIGHBORS = geometry.h nearestNeighbors.h blockRadixSort.h transpose.h quickSort.h
GLOBAL_REQUIRE = parallel.h sequence.h $(NEAREST_NEIGHBORS)
LOCAL_REQUIRE = octTree.h
INCLUDE = nearestNeighbors.h
OPT_FLAGS = -load $(LLVM_HOME)/lib/DataAnnotation.so -DataAnnotation
OPT      = opt

include ../common/timeRequiredFiles
include MakeBenchI
