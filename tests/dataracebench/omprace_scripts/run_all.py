#!/usr/bin/python

import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, commands
from math import sqrt
import csv

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
    ref_cwd = os.getcwd()
    arch = platform.machine()
    full_hostname = platform.node()
    hostname=full_hostname
    pass_compile_flags =  "-O1 -fopenmp -std=c++11 -I./ -I$LLVM_HOME/include -I$OMPP_ROOT/src/omprace/ -I$OMPP_ROOT/src/include -g -S -emit-llvm"
    omprace_compile_flags =  "-std=c++11 -I./ -I$LLVM_HOME/include -I$OMPP_ROOT/src/omprace/ -I$OMPP_ROOT/src/include -g $OMPP_ROOT/src/omprace/libomprace.a"
    opt_flags = "-load $LLVM_HOME/lib/DataAnnotation.so -DataAnnotation"
    ll_suffix = ".ll"
    bc_suffix = ".bc"
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
        

    util.run_command("clang++ " + pass_compile_flags + " " + microbenchmark + " " + poly_flags, verbose=True)
    util.run_command("opt " + opt_flags + " < " + microbenchmark_name + ll_suffix + " > " + microbenchmark_name + bc_suffix, verbose=True)

    util.run_command("clang++ -fopenmp -O1" + util_objs + " " + microbenchmark_name + bc_suffix + " " + poly_flags + " " + omprace_compile_flags + " -o " + microbenchmark_name + bin_suffix, verbose=True)

    
def run_benchmark(f_name):
    #time_command = '/usr/bin/time -f "%E %M" '
    #output = util.run_command(time_command + "./" + f_name, verbose=True)
    output = util.run_command("./" + f_name, verbose=True)
    return output

def write_res(f_name, res, writer):
    test_num = int(f_name[3:6])
    t_res = False
    if "yes" in f_name:
        t_res = True
    v = -1
    
    res = res.split('\n')
    for l in res:
        if l.startswith("Number of violations ="):
            v=int(l[l.index('=')+1:])
            print('v is: ', str(v))
    assert(v!=-1)
    v_b = True if v>0 else False
    res_row = [0] * 5
    res_row[0] = test_num
    if v_b and t_res:
        res_row[1] = 1#TP
    elif not v_b and not t_res:
        res_row[3] = 1#TN
    elif v_b and not t_res:
        res_row[2] = 1#FP
    else:
        res_row[4] = 1#FN
    writer.writerow(res_row)

if __name__ == "__main__":
    files = os.listdir("./")
    filtered = ["DRB094-doall2-ordered-orig-no.c", 
                "DRB095-doall2-taskloop-orig-yes.c", 
                "DRB096-doall2-taskloop-collapse-orig-no.c", 
                "DRB097-target-teams-distribute-orig-no.c", 
                #"DRB100-task-reference-orig-no.cpp", 
                "DRB112-linear-orig-no.c"]
                #"DRB116-target-teams-orig-yes.c"]
    benchmark_files = [f for f in files if (f.endswith(".c") or f.endswith(".cpp")) and f.startswith("DRB") and f not in filtered]
    #benchmark_files.sort()
    #print(benchmark_files)
    print(len(benchmark_files))
    #-b builds the benchmarks
    if len(sys.argv) > 1 and sys.argv[1] == "-b":
        for f in benchmark_files:
            build_benchmark(f)
        exit()
    
    binary_files = [f for f in files if f.endswith(".out") and f.startswith("DRB") and f not in filtered]
    binary_files.sort()
    print(len(binary_files))
    with open('drbench_res.csv', 'wb') as csvfile:
        res_writer = csv.writer(csvfile, delimiter=',')
        res_writer.writerow(['test num', 'TP', 'FP', 'TN', 'FN'])
        #output = run_benchmark(binary_files[0])
        for f in binary_files:
            output = run_benchmark(f)
            write_res(f, output, res_writer)
