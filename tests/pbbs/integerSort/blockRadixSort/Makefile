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
SORT = blockRadixSort.h transpose.h
GLOBAL_REQUIRE = parallel.h sequence.h $(SORT)
LOCAL_REQUIRE = 
INCLUDE = blockRadixSort.h
OPT_FLAGS = -load $(LLVM_HOME)/lib/DataAnnotation.so -DataAnnotation
OPT      = opt

include ../common/timeRequiredFiles
include MakeBenchI
