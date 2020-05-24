#!/usr/bin/python

import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, commands, csv
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

analyzer = '$PGEN/genprof'

configs = []

entry = { "NAME" : "RUN_ALL_BENCHMARKS",
          "NUM_RUNS" : 1,
          "CLEAN_LINE" : " make clean ",
          "BUILD_LINE" : " make ",
          "BUILD_ARCH" : "x86_64",
          "RUN_ARCH" : "x86_64",
          #"RUN_LINE" : "./",
          "RUN_LINE" : '/usr/bin/time -f "%E" ./',
          "ARGS" : "",
}

configs.append(entry)

ref_cwd = os.getcwd()
arch = platform.machine()
full_hostname = platform.node()
hostname=full_hostname

benchmarks=[
    "amgmk-v1.0_orig",
    "amgmk-v1.0_orig_prof",
    "amgmk-v1.0_orig_prof"
]

executable=[
    "AMGMk",
    "AMGMk",
    "AMGMk"
]

build_line=[
    "make -f Makefile.static",
    "make -f Makefile_static.no_inst",
    "make -f Makefile.static"
]

if __name__ == "__main__":
    with open('omprace_static.csv', 'wb') as csvfile:
        res_writer = csv.writer(csvfile, delimiter=',')
        res_writer.writerow(['test name', 'baseline openmp(s)', 'omprace no_inst(s)', 'omprace(s)', 'overhead ospg', 'overhead datarace', 'num violations'])
        for config in configs:
            util.log_heading(config["NAME"], character="-")
            row = []
            row.append(benchmarks[0])
            num_violations = -1
            for b_index in range(len(benchmarks)):
                util.chdir(ref_cwd)
                for i in range(0, config["NUM_RUNS"]):
                    try:
                        
                        util.chdir(ref_cwd + "/" + benchmarks[b_index] )
                        util.log_heading(benchmarks[b_index], character="=")
                        try:
                            clean_string = config["CLEAN_LINE"]
                            util.run_command(clean_string, verbose=True)
                        except:
                            print "Clean failed"
                        
                        build_bench_string = build_line[b_index]
                        util.run_command(build_bench_string, verbose=True) 
                        util.log_heading("running: " + benchmarks[b_index], character="=")
                        run_string = config["RUN_LINE"] + executable[b_index]
                        #running applications
                        output_string = util.run_command(run_string, verbose=True) 
                        output_lines = output_string.split('\n')
                        if b_index == len(benchmarks)-1:                        
                            for output_line in output_lines:
                                if output_line.startswith("Number of violations ="):
                                    num_violations=int(output_line[output_line.index('=')+1:])
                        time_line = output_lines[-2] #format is hh:mm:sec
                        #print('--------')
                        #print(output_lines)
                        #print('--------')
                        time_line = time_line.split(':')
                        tot_secs = 0.0
                        #print(time_line)
                        for t in time_line:
                            tot_secs = (tot_secs*60) + float(t)
                        row.append(tot_secs)
                    except util.ExperimentError, e:
                        print "Error: %s" % e
                        print "-----------"
                        print "%s" % e.output
                        continue
            #finalize row
            row.append("{0:.2f}".format(row[2]/row[1]))#ospg ov
            row.append("{0:.2f}".format(row[3]/row[1]))#omprace ov
            row.append(num_violations)
            res_writer.writerow(row)  
    util.chdir(ref_cwd)        
    print("done")
