CXX=clang++

INCLUDE = -I./ -I$(LLVM_INCLUDE) -I$(OMPP_ROOT)/src/omprace/ -I$(OMPP_ROOT)/src/include  -g
LIBS = $(OMPP_ROOT)/src/omprace/libomprace.a $(LLVM_LIB)/libomp.so -Wl,-rpath=$(LLVM_LIB) $(LLVM_LIB)/libomp.so

CXXFLAGS := -fopenmp -std=c++11
LDFLAGS = -fopenmp

all: taskdep1-orig-no taskdep2-orig-no taskdep3-orig-no

taskdep1-orig-no: taskdep1-orig-no.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o taskdep1-orig-no.out 

taskdep2-orig-no: taskdep2-orig-no.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o taskdep2-orig-no.out 

taskdep3-orig-no: taskdep3-orig-no.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o taskdep3-orig-no.out 

	
%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<

clean:
	rm -f *.o *~ *.out *.bc *.ll
