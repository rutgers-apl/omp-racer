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

bench_name = "sForest"
benchmarks=[
    "ndSTArcher"
]

inner_data_folder=[
    "graphData/data"
]

input_file=[
    "randLocalGraph_E_5_4000000"
]

executable=[
    "ST.openmp.dynamic",
]

inputs=[
    "-r 1 -o /tmp/ofile780084_677212 ../graphData/data/randLocalGraph_E_5_4000000"
]


if __name__ == "__main__":
    with open('archer.csv', 'wb') as csvfile:
        res_writer = csv.writer(csvfile, delimiter=',')
        res_writer.writerow(['test name', 'archer time(s)'])
        for config in configs:
            util.log_heading(config["NAME"], character="-")
            row = []
            row.append(bench_name)
            num_violations = -1

            print('input file folder: ' + inner_data_folder[0])
            data_input = inner_data_folder[0]+'/'+input_file[0]
            print('checking if input data exists at:' + data_input)
            if not os.path.exists(data_input):
                print("input data doesn't exist. building input data")
                util.chdir(ref_cwd + "/" + inner_data_folder[0])
                build_data = config["BUILD_LINE"] + " " + input_file[0]
                util.run_command(build_data, verbose=True)
                util.chdir(ref_cwd)
            else:
                print("input data exists")

            for b_index in range(len(executable)):
                util.chdir(ref_cwd)
                for i in range(0, config["NUM_RUNS"]):
                    try:
                        
                        util.chdir(ref_cwd + "/" + benchmarks[0] )
                        util.log_heading(benchmarks[0], character="=")
                        try:
                            clean_string = config["CLEAN_LINE"]
                            util.run_command(clean_string, verbose=True)
                        except:
                            print "Clean failed"
                        
                        build_bench_string = config["BUILD_LINE"] + " openmp-dynamic"
                        util.run_command(build_bench_string, verbose=True) 
                        util.log_heading("running: " + benchmarks[0], character="=")
                        run_string = config["RUN_LINE"] + executable[b_index] + " " + inputs[0]
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
