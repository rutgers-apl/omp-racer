## omp-racer
A dynamic apparent race detector for OpenMP programs using llvm 10 using the Enhanced OpenMP Series Parallel Graph (EOSPG)

## Prerequisites

To build omp-racer, we rely on the following software dependencies:

1) As tested, Linux Ubuntu 16.04 LTS distribution with perf event modules installed. To check if the machine supports hardware performance counters, use the command,

2) LLVM+Clang compiled with the provided OpenMP runtime with OMPT support. For more information, refer to the installation section.

3) Common packages such as git, cmake, python2.7.

## Installation

Checkout release 10 of llvm-project
	
	$ git clone https://github.com/llvm/llvm-project.git --branch release/10x
	
Copy the provided modified openmp runtime to replace openmp directory in the cloned repository <llvm download path>/openmp
build llvm+clang+openmp. We've used a prior version of clang (release-9) for the build but gcc should also work:

	$ cd <llvm download path>
	$ mkdir build && cd build
	$ cmake -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=$HOME/llvm-10 -DLLVM_ENABLE_PROJECTS="clang;libcxx;libcxxabi;openmp" ../llvm
	$ make 
	$ make install	


After this, in case there are any changes to the openmp source code and it needs rebuilding, use:
$ make omp
$ make install

Now to build the llvm instrumentation pass by copying the files in the ins_pass directory to <llvm download path>/llvm/lib/Transforms/DataAnnotation

There are two variations of the instrumentation pass, one instruments memory locations with source line information that allows omp-racer to report the source line information for a data race. this pass is located at the directory pass_with_location_info. Depending which instrumentation pass is chosen, omp-racer needs to be recompiled to work with either pass.
This is done by setting the LOC_INFO macro in src/openmp_dpst.h. When using the pass with location info, set this macro to 1. otherwise set it to 0.

Add the following line to <llvm download>/llvm/lib/Transforms/CMakeLists.txt
	
	add_subdirectory(DataAnnotation)

Run the following commands to build the instrumentation pass:
	
	$ cd <llvm download path>/build
	$ make

To build omp-racer in precise mode, run:

	source clang_10.sh
	source setup-omprace.sh

The second script assumes that the following 3 environment variables are updated to point to the built llvm which are set via the first script. These are:
	
	$ export LLVM_HOME="$HOME/llvm-10"
	$ export LD_LIBRARY_PATH="$LLVM_HOME/lib:$LD_LIBRARY_PATH"
	$ export PATH="$LLVM_HOME/bin:$PATH"

This mode can be slow and tends to consume a high memory overhead when tested with 
task-based OpenMP applications. To alleviate this issue we provide a fast mode that
trades accuracy for speed. to use the fast mode, run:

	source clang_10.sh
	source setup-omprace-fast.sh

Moreover, we provide an OMPT tool that specifically checks if using fast mode is safe
(it detects the same races as precise mode). This tool can be built by going to 
src/omprace_precise and running the following makefile:

	make clean
	make -f Makefile.check_nested

Note that this makefile replaces the omp-racer precise mode built earlier. Hence, we'd
need to rebuild the tool for use in precise mode.

omp-racer can be reconfigured by changing the value of the macros declared at src/omprace/openmp_dpst.h:35-62 to utilize different implementation for shadow memory management. 

## How to run data race bench applications

The dataracebench applications are stored in tests/dataracebench/omprace_scripts:

We provide two scripts to test the applications within this folder. the first script,
run_dr.py, is used to build each test case individually and takes the the c/cpp file 
associated with the test file to build the application for example:

	$ python run_dr.py DRB001-antidep1-orig-yes.c 
	$ ./DRB001-antidep1-orig-yes.out

The second script, run_all.py, tests all the datarace bench applications in the 
directory. it takes an optional argument, -b. When -b is given, it builds all the
applications. Without -b, the script runs all applications and stores the results
in drbench_res.csv. For example:

	$ python run_all.py -b
	# builds all datarace bench binaries
	$ python run_all.py
	$ cat drbench_res.csv

## How to run the pass (example)

applications in the tests folder include a python script called run_omprace.py that runs omp-racer with and without memory instrumentation and records the performance overhead in a omprace.csv file

For microbenchmarks there is a run_dr.py script that takes the source of the microbenchmarks as an input and apply the following commands to build the omprace version of the microbenchmark

	$clang++ -fopenmp -std=c++11 -I./ -I$LLVM_HOME/include -I$OMPP_ROOT/src/omprace/ -I$OMPP_ROOT/src/include  -g -S -emit-llvm test_parallel_1_race.cpp
 
	$opt -load $LLVM_HOME/lib/DataAnnotation.so -DataAnnotation < test_parallel_1_race.ll > ./test_parallel_1_inst.bc

	$clang++ -fopenmp test_parallel_1_inst.bc util.o -I./ -I$LLVM_HOME/include -I$OMPP_ROOT/src/omprace/ -I$OMPP_ROOT/src/include  -g $OMPP_ROOT/src/omprace/libomprace.a $LLVM_HOME/lib/libomp.so -Wl,-rpath=$LLVM_HOME/lib $LLVM_HOME/lib/libomp.so -o test_parallel_1_race.out


