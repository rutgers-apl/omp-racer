CXX=clang++

INCLUDE = -I./ -I$(LLVM_INCLUDE) -I$(OMPP_ROOT)/src/omprace/ -I$(OMPP_ROOT)/src/include -g
LIBS = $(OMPP_ROOT)/src/omprace/libomprace.a $(LLVM_LIB)/libomp.so -Wl,-rpath=$(LLVM_LIB) $(LLVM_LIB)/libomp.so

CXXFLAGS := -fopenmp -std=c++11
LDFLAGS = -fopenmp

all: DRB109-orderedmissing-orig-yes DRB109-orderedmissing-orig-yes-omp DRB110-ordered-orig-no DRB110-ordered-orig-no-omp doaccross-1

DRB109-orderedmissing-orig-yes: DRB109-orderedmissing-orig-yes.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o DRB109-orderedmissing-orig-yes.out 

DRB109-orderedmissing-orig-yes-omp: DRB109-orderedmissing-orig-yes-omp.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o DRB109-orderedmissing-orig-yes-omp.out 

DRB110-ordered-orig-no: DRB110-ordered-orig-no.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o DRB110-ordered-orig-no.out 

DRB110-ordered-orig-no-omp: DRB110-ordered-orig-no-omp.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o DRB110-ordered-orig-no-omp.out 

doaccross-1: doaccross-1.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o doaccross-1.out
%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<

%.o : %.c
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<

clean:
	rm -f *.o $(TARGET) *~ *.out
