CXX=clang++

INCLUDE = -I./ -I$(LLVM_INCLUDE) -I$(OMPP_ROOT)/src/omprace/ -I$(OMPP_ROOT)/src/include  -g
LIBS = $(OMPP_ROOT)/src/omprace/libomprace.a $(LLVM_LIB)/libomp.so -Wl,-rpath=$(LLVM_LIB) $(LLVM_LIB)/libomp.so

CXXFLAGS := -fopenmp -std=c++11
LDFLAGS = -fopenmp

all: test_for_1 test_for_2 test_for_3 test_for_4 test_for_5 test_for_6 test_taskwait_1

test_for_1: test_for_1.o util.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_for_1.out 

test_for_2: test_for_2.o util.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_for_2.out 

test_for_3: test_for_3.o util.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_for_3.out 

test_for_4: test_for_4.o util.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_for_4.out 

test_for_5: test_for_5.o util.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_for_5.out 

test_for_6: test_for_6.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_for_6.out 

test_taskwait_1: test_taskwait_1.o 
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_taskwait_1.out 

%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<

clean:
	rm -f *.o $(TARGET) *~ *.out
