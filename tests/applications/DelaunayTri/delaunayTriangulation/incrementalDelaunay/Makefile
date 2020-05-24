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
NEAREST_NEIGHBORS = geometry.h nearestNeighbors.h blockRadixSort.h quickSort.h transpose.h
DELAUNAY = topology.h delaunayQs.h delaunayDefs.h delaunay.C 
GLOBAL_REQUIRE = parallel.h sequence.h $(NEAREST_NEIGHBORS) $(DELAUNAY)
LOCAL_REQUIRE = octTree.h
OBJS = delaunay.o
INTRMEDIATES = delaunay.bc
OPT_FLAGS = -load $(LLVM_HOME)/lib/DataAnnotation.so -DataAnnotation
OPT      = opt

include ../common/timeRequiredFiles
include MakeBench
