CXX=clang++

INCLUDE = -I./ -I$(LLVM_INCLUDE)
LIBS = $(LLVM_LIB)/libomp.so -Wl,-rpath=$(LLVM_LIB) 

CXXFLAGS := -fopenmp -std=c++11
LDFLAGS = -fopenmp

all: test_taskgroup_2

#test_taskgroup_0: test_taskgroup_0.o util.o
#	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_taskgroup_0.out 
#
#test_taskgroup_1: test_taskgroup_1.o util.o
#	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_taskgroup_1.out 

test_taskgroup_2: test_taskgroup_2.o util.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_taskgroup_2.out 

#test_taskgroup_3: test_taskgroup_2.o util.o
#	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_taskgroup_3.out 
#
#test_taskgroup_4: test_taskgroup_2.o util.o
#	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o test_taskgroup_4.out 

%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<

clean:
	rm -f *.o *~ *.out *.bc *.ll
