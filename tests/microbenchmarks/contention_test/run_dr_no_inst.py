#!/usr/bin/python

import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, commands
from math import sqrt

def usage():
    print("Usage: run_dr <microbenchmark.cpp>")


configs = []

entry = { "NAME" : "RUN_ALL_BENCHMARKS",
          "NUM_RUNS" : 1,
          "CLEAN_LINE" : " make clean ",
          "BUILD_LINE" : " make ",
          "BUILD_ARCH" : "x86_64",
          "RUN_ARCH" : "x86_64",
          "RUN_LINE" : "./",
          "ARGS" : "",
}

configs.append(entry)

ref_cwd = os.getcwd()
arch = platform.machine()
full_hostname = platform.node()
hostname=full_hostname
pass_compile_flags =  "-fopenmp -O2 -std=c++11 -I./ -I$LLVM_HOME/include -I$OMPP_ROOT/src/omprace/ -I$OMPP_ROOT/src/include -g -c"
omprace_compile_flags =  "-O2 -std=c++11 -I./ -I$LLVM_HOME/include -I$OMPP_ROOT/src/omprace/ -I$OMPP_ROOT/src/include -g $OMPP_ROOT/src/omprace/libomprace.a"

ll_suffix = ".ll"
bc_suffix = ".bc"
obj_suffix = ".o"
bin_suffix = ".out"
util_objs = ""

POLYFLAG="./utilities/polybench.c -I ./ -I ./utilities -DPOLYBENCH_NO_FLUSH_CACHE -DPOLYBENCH_TIME -D_POSIX_C_SOURCE=200112L"
poly_flags = ""
if len(sys.argv) != 2:
    usage()
    sys.exit(1)

microbenchmark = sys.argv[1] 
index = microbenchmark.rfind('.')
microbenchmark_name=microbenchmark[0:index]
print('microbench is ' + microbenchmark_name)

util.run_command("clang++ " + pass_compile_flags + " " + microbenchmark + " " + poly_flags, verbose=True)
util.run_command("clang++ -fopenmp " + util_objs + " " + microbenchmark_name + obj_suffix + " " + poly_flags + " " + omprace_compile_flags + " -o " + microbenchmark_name + bin_suffix, verbose=True)

sys.exit(0)
