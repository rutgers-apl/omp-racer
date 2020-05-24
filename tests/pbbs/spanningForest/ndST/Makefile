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
GLOBAL_REQUIRE = utils.h parallel.h sequence.h graph.h speculative_for.h unionFind.h
LOCAL_REQUIRE = 
OBJS = ST.o
INTRMEDIATES = ST.bc
OPT_FLAGS = -load $(LLVM_HOME)/lib/DataAnnotation.so -DataAnnotation
OPT      = opt

include ../common/timeRequiredFiles
include MakeBench

