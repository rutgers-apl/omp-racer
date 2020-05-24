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


configs = []

entry = { "NAME" : "RUN_ALL_BENCHMARKS",
          "NUM_RUNS" : 1,
          "CLEAN_LINE" : " make clean ",
          "BUILD_LINE" : " make ",
          "BUILD_ARCH" : "x86_64",
          "RUN_ARCH" : "x86_64",
          "RUN_LINE" : '/usr/bin/time -f "%E" ./',
          #"RUN_LINE" : 'time ./',
          "ARGS" : "",
}

configs.append(entry)

ref_cwd = os.getcwd()
arch = platform.machine()
full_hostname = platform.node()
hostname=full_hostname

benchmark= "strassen"

make_commands = [
    "-f Makefile.archer",
]

executable= "bots-strassen.out"
inputs= "-n 2048"


if __name__ == "__main__":
    with open('archer.csv', 'wb') as csvfile:
        res_writer = csv.writer(csvfile, delimiter=',')
        res_writer.writerow(['test name', 'archer time(s)'])
        for config in configs:
            util.log_heading(config["NAME"], character="-")
            row = []
            row.append(benchmark)
            num_violations = -1

            for b_index in range(len(make_commands)):
                util.chdir(ref_cwd)
                for i in range(0, config["NUM_RUNS"]):
                    try:
                        
                        util.log_heading(benchmark, character="=")
                        try:
                            clean_string = config["CLEAN_LINE"]
                            util.run_command(clean_string, verbose=True)
                        except:
                            print "Clean failed"
                        
                        build_bench_string = config["BUILD_LINE"] + make_commands[b_index] 
                        util.run_command(build_bench_string, verbose=True) 
                        util.log_heading("running: " + benchmark + " , " +  make_commands[b_index], character="=")
                        run_string = config["RUN_LINE"] + executable + " " + inputs
                        #running applications
                        output_string = util.run_command(run_string, verbose=True) 
                        output_lines = output_string.split('\n')
                        time_line = output_lines[-2] #format is hh:mm:sec
                        time_line = time_line.split(':')
                        tot_secs = 0.0
                        for t in time_line:
                            tot_secs = (tot_secs*60) + float(t)
                        row.append(tot_secs)
                        print ('total secs= ' + str(tot_secs))
                        


                        

                    except util.ExperimentError, e:
                        print "Error: %s" % e
                        print "-----------"
                        print "%s" % e.output
                        continue
            
            #finalize row
            res_writer.writerow(row)            

        util.chdir(ref_cwd)        
        print("done")
