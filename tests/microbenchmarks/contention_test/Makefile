CXX=clang++

INCLUDE = -I./ -I$(LLVM_INCLUDE) -I$(OMPP_ROOT)/src/omprace/ -I$(OMPP_ROOT)/src/include  -g
LIBS = $(OMPP_ROOT)/src/omprace/libomprace.a $(LLVM_LIB)/libomp.so -Wl,-rpath=$(LLVM_LIB) $(LLVM_LIB)/libomp.so

CXXFLAGS := -fopenmp -std=c++11 -O2
LDFLAGS = -fopenmp -O2

all: test_1 test_1_fa test_1_cas

test_1: test_1.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_1.out 

test_1_fa: test_1_fa.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_1_fa.out 

test_1_cas: test_1_cas.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_1_cas.out 

%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<

clean:
	rm -f *.o $(TARGET) *~ *.out
