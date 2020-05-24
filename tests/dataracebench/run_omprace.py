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
          "RUN_LINE" : '/usr/bin/time -f "%E" ',
          #"RUN_LINE" : 'time ./',
          "ARGS" : "",
}

configs.append(entry)

ref_cwd = os.getcwd()
arch = platform.machine()
full_hostname = platform.node()
hostname=full_hostname

benchmarks=[
    "dr_bench",
    "dr_bench",
    "dr_bench",
]


inner_folder=[
    "micro-benchmarks",
    "omprace_scripts",
    "omprace_scripts",
]


script = [
    "python run_all.py",
    "python run_all_no_inst.py",
    "python run_all.py"
]

build_script = [
    "python run_all.py -b",
    "python run_all_no_inst.py -b",
    "python run_all.py -b"
]
if __name__ == "__main__":
    with open('omprace.csv', 'wb') as csvfile:
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
                        util.chdir(ref_cwd + "/" + inner_folder[b_index] )
                        util.log_heading(benchmarks[b_index], character="=")
                        
                        build_bench_string = build_script[b_index]
                        util.run_command(build_bench_string, verbose=True) 
                        util.log_heading("running: " + benchmarks[b_index], character="=")
                        run_string = config["RUN_LINE"] + script[b_index]
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
                        #min_sec = time_line[-1]
                        #min_index = min_sec.find('m')
                        #tot_secs = float(min_sec[0:min_index]) * 60
                        #tot_secs+= float(min_sec[min_index,-2])
                        row.append(tot_secs)
                        print ('total secs= ' + str(tot_secs))
                        
                        

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
