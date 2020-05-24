CXX=clang++
OPT=opt

TARGET=bots-health.out
PROG=bots-health.out
OBJS=bots_main.o bots_common.o health.o 
INTERMEDIATES=bots_main.bc bots_common.bc health.bc
CXXFLAGS := -fopenmp -DMANUAL_CUTOFF -g -O3
LDFLAGS = -fopenmp -g -O3

PASS_COMPILE_FLAGS =  -fopenmp -DOMPR -I./ -I$(LLVM_HOME)/include -I$(OMPP_ROOT)/src/omprace/ -I$(OMPP_ROOT)/src/include -g -S -emit-llvm
OPT_FLAGS = -load $(LLVM_HOME)/lib/DataAnnotation.so -DataAnnotation
omprace_compile_flags = -I./ -I$(LLVM_HOME)/include -I$(OMPP_ROOT)/src/omprace/ -I$(OMPP_ROOT)/src/include -g $(OMPP_ROOT)/src/omprace/libomprace.a

all : $(PROG)

$(PROG) : $(INTERMEDIATES)
	$(CXX) -lm $(LDFLAGS) $(INTERMEDIATES) $(omprace_compile_flags) -o $(PROG) 

%.bc : %.ll
	$(OPT) $(OPT_FLAGS) < $< > $@

 
%.ll : %.c 
	$(CXX) $(CXXFLAGS) $(PASS_COMPILE_FLAGS) $<

clean:
	rm -f *.o $(TARGET) *~ *.out *.txt *.ll *.bc

