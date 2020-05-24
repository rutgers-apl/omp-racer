CXX=clang++

INCLUDE = -I./ -I$(LLVM_INCLUDE) -I$(OMPP_ROOT)/src/omprace/ -I$(OMPP_ROOT)/src/include  -g
LIBS = $(OMPP_ROOT)/src/omprace/libomprace.a $(LLVM_LIB)/libomp.so -Wl,-rpath=$(LLVM_LIB) $(LLVM_LIB)/libomp.so

CXXFLAGS := -fopenmp -std=c++11 -mcx16 -DOPENMP -DMCX16
LDFLAGS = -fopenmp

all: pbbs_io 

pbbs_io: main.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o main.out 

%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<

clean:
	rm -f *.o $(TARGET) *~ *.out
