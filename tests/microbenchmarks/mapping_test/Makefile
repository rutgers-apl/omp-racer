CXX=clang++
#INCLUDE = -I./ -I$(LLVM_INCLUDE) -I$(OMPP_ROOT)/src/omprace/ -I$(OMPP_ROOT)/src/include  -g
#LIBS = $(OMPP_ROOT)/src/omprace/libomprace.a $(LLVM_LIB)/libomp.so -Wl,-rpath=$(LLVM_LIB) $(LLVM_LIB)/libomp.so

INCLUDE = 
LIBS =
CXXFLAGS = -std=c++11 -fPIE -g -O3
LDFLAGS = -pie

all: test_1_seq test_1_seq_direct test_2_seq test_2_seq_direct test_3_seq test_3_seq_direct

test_1_seq: test_1_seq.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o trie_1.out 

test_1_seq_direct: test_1_seq_direct.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o direct_1.out 

test_2_seq: test_2_seq.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o trie_2.out 

test_2_seq_direct: test_2_seq_direct.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o direct_2.out 

test_3_seq: test_3_seq.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o trie_3.out 

test_3_seq_direct: test_3_seq_direct.o
	$(CXX) $(LDFLAGS) $^ $(INCLUDE) $(LIBS) -o direct_3.out 

%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $<

clean:
	rm -f *.o *~ *.out *.bc *.ll
