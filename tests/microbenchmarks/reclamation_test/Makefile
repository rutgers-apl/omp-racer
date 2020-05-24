CXX=clang++

INCLUDE = -I./ -I$(LLVM_INCLUDE) -I$(OMPP_ROOT)/src/omprace/ -I$(OMPP_ROOT)/src/include  -g
LIBS = $(OMPP_ROOT)/src/omprace/libomprace.a $(LLVM_LIB)/libomp.so #-Wl,-rpath=$(LLVM_LIB) $(LLVM_LIB)/libomp.so

CXXFLAGS := -fopenmp -std=c++11
LDFLAGS = -fopenmp

all: test_1 test_2 #test_parallel_3 test_parallel_4 test_parallel_5 test_parallel_6

test_1: test_1.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_1.out 

test_2: test_2.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_2.out 

#test_parallel_3: test_parallel_3.o util.o
#	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_parallel_3.out 
#
#test_parallel_4: test_parallel_4.o util.o
#	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_parallel_4.out 
#
#test_parallel_5: test_parallel_5.o util.o
#	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_parallel_5.out 
#
#test_parallel_6: test_parallel_6.o util.o
#	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_parallel_6.out 

%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<

clean:
	rm -f *.o *~ *.out *.bc *.ll
