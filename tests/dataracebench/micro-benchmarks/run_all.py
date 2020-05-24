#!/usr/bin/python

import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, commands
from math import sqrt

def usage():
    print("Usage: run_all -b")


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

def build_benchmark(f_name):
    pass_compile_flags =  "-fopenmp -std=c++11 -I./ -g -c"
    obj_suffix = ".o"
    bin_suffix = ".out"
    util_objs = ""

    POLYFLAG="./utilities/polybench.c -I ./ -I ./utilities -DPOLYBENCH_NO_FLUSH_CACHE -DPOLYBENCH_TIME -D_POSIX_C_SOURCE=200112L"
    poly_flags = ""

    microbenchmark = f_name
    index = microbenchmark.rfind('.')
    microbenchmark_name=microbenchmark[0:index]
    print('microbench is ' + microbenchmark_name)
    #clean_string = entry["CLEAN_LINE"]
    #util.run_command(clean_string, verbose=True)
    grep_res = util.run_command('grep PolyBench '  + microbenchmark, verbose=True)
    if grep_res.find('PolyBench') != -1:
        print('requires polybench')
        poly_flags = POLYFLAG
        

    util.run_command("clang++ -O1 " + pass_compile_flags + " " + microbenchmark + " " + poly_flags, verbose=True)
    #util.run_command("opt " + opt_flags + " < " + microbenchmark_name + ll_suffix + " > " + microbenchmark_name + bc_suffix, verbose=True)

    util.run_command("clang++ -fopenmp -O1 " + util_objs + " " + microbenchmark_name + obj_suffix + " " + poly_flags + " -o " + microbenchmark_name + bin_suffix, verbose=True)

    
def run_benchmark(f_name):
    util.run_command("./" + f_name, verbose=True)

if __name__ == "__main__":
    files = os.listdir("./")
    filtered = ["DRB094-doall2-ordered-orig-no.c", 
                "DRB095-doall2-taskloop-orig-yes.c", 
                "DRB096-doall2-taskloop-collapse-orig-no.c", 
                "DRB097-target-teams-distribute-orig-no.c", 
                "DRB100-task-reference-orig-no.cpp", 
                "DRB112-linear-orig-no.c", 
                "DRB116-target-teams-orig-yes.c"]
    benchmark_files = [f for f in files if f.endswith(".c") and f.startswith("DRB") and f not in filtered]
    print(len(benchmark_files))
    
    #-b builds the benchmarks
    if len(sys.argv) > 1 and sys.argv[1] == "-b":
        for f in benchmark_files:
            build_benchmark(f)
        exit()
    
    binary_files = [f for f in files if f.endswith(".out") and f.startswith("DRB")]
    binary_files.sort()
    print(len(binary_files))
    for f in binary_files:
        run_benchmark(f)