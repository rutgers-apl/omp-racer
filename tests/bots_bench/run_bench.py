#!/usr/bin/python

import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, commands
from math import sqrt

#clean up the src 
do_clean = True

#build the src
do_build = True

#clean, build, and run the benchmarks
do_run = True

#collect data to plot
#do_collect_data = True

if do_clean and not do_build:
    print "Clean - true and build - false not allowed"
    exit(0)

analyzer = '../analyzer/analyzerDriver.out'

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

#print arch
#print full_hostname

benchmarks=[
    "bots-fib",
    "bots-sort",
    "bots-nqueens",
    "bots-floorplan",
    "bots-alignment-single",
    "bots-concom",
    "bots-fft",
    "bots-health",
    "bots-knapsack",
    "bots-sparselu-single",
    "bots-strassen",
    "bots-uts"
]

executable=[
    "bots-fib.out",
    "bots-sort.out",
    "bots-nqueens.out",
    "bots-floorplan.out",
    "bots-alignment.out",
    "bots-concom.out",
    "bots-fft.out",
    "bots-health.out",
    "bots-knapsack.out",
    "bots-sparselu.out",
    "bots-strassen.out",
    "bots-uts.out"
]

inputs=[
    "-n 45 -x 5",
    "-n 33554432",
    "",
    "-f input/input.5",
    "-f input/prot.20.aa",
    "",
    "-n 8388608",
    "-f inputs/small.input",
    "-f inputs/knapsack-032.input",
    "",
    "-n 2048",
    "-f inputs/test.input"
]

for config in configs:
    util.log_heading(config["NAME"], character="-")
    if do_clean:
        util.run_command("make clean", verbose=True)
        
    if do_build:
        util.run_command("make", verbose=True)
        
    for benchmark in benchmarks:
        util.chdir(ref_cwd)
        for i in range(0, config["NUM_RUNS"]):
            try:
                util.chdir(ref_cwd + "/" + benchmark)
                util.log_heading(benchmark, character="=")
                if do_run:
                    try:
                        clean_string = config["CLEAN_LINE"]
                        util.run_command(clean_string, verbose=True)
                    except:
                        print "Clean failed"
                    build_string = config["BUILD_LINE"]
                    util.run_command(build_string, verbose=True)

                    run_string = config["RUN_LINE"] + executable[benchmarks.index(benchmark)] + " " + inputs[benchmarks.index(benchmark)]
                    util.run_command(run_string, verbose=True)

                    
                    util.run_command(analyzer, verbose=True)

                    
            except util.ExperimentError, e:
                print "Error: %s" % e
                print "-----------"
                print "%s" % e.output
                continue

util.chdir(ref_cwd)