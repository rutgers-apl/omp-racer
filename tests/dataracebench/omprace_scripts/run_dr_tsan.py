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
pass_compile_flags =  "-fopenmp -O0 -std=c++11 -I./ -I$LLVM_HOME/include -I$OMPP_ROOT/src/omprace/ -I$OMPP_ROOT/src/include -g -S -emit-llvm"
omprace_compile_flags =  "-O0 -std=c++11 -I./ -I$LLVM_HOME/include -I$OMPP_ROOT/src/omprace/ -I$OMPP_ROOT/src/include -g $OMPP_ROOT/src/omprace/libomprace.a"
opt_flags = "-load $LLVM_HOME/lib/DataAnnotation.so -DataAnnotation"
ll_suffix = ".ll"
bc_suffix = ".bc"
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
#clean_string = entry["CLEAN_LINE"]
#util.run_command(clean_string, verbose=True)
grep_res = util.run_command('grep PolyBench '  + microbenchmark, verbose=True)
if grep_res.find('PolyBench') != -1:
    print('requires polybench')
    poly_flags = POLYFLAG
    

util.run_command("clang++ " + pass_compile_flags + " " + microbenchmark + " " + poly_flags, verbose=True)
util.run_command("opt " + opt_flags + " < " + microbenchmark_name + ll_suffix + " > " + microbenchmark_name + bc_suffix, verbose=True)

util.run_command("clang++ -fopenmp " + util_objs + " " + microbenchmark_name + bc_suffix + " " + poly_flags + " " + omprace_compile_flags + " -o " + microbenchmark_name + bin_suffix, verbose=True)

sys.exit(0)
