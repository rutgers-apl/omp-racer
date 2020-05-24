CXX=clang++

INCLUDE = -I./ -I$(LLVM_INCLUDE) -I$(OMPP_ROOT)/src/omprace/ -I$(OMPP_ROOT)/src/include  -g
LIBS = $(OMPP_ROOT)/src/omprace/libomprace.a $(LLVM_LIB)/libomp.so -Wl,-rpath=$(LLVM_LIB) $(LLVM_LIB)/libomp.so

CXXFLAGS := -fopenmp -std=c++11
LDFLAGS = -fopenmp

all: test_task_1 test_task_2 test_task_3 example example_nr

test_task_1: test_task_1.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_task_1.out 

test_task_2: test_task_2.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_task_2.out 

test_task_3: test_task_3.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_task_3.out 

example: example.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o example.out 

example_nr: example_nr.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o example_nr.out 
	
%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<

clean:
	rm -f *.o *~ *.out *.bc *.ll
