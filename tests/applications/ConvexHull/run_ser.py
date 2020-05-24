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
          "NUM_RUNS" : 3,
          "CLEAN_LINE" : " make clean ",
          "BUILD_LINE" : " make ",
          "BUILD_ARCH" : "x86_64",
          "RUN_ARCH" : "x86_64",
          "RUN_LINE" : '/usr/bin/time -f "%E" ./',
          "ARGS" : "",
}

configs.append(entry)

ref_cwd = os.getcwd()
arch = platform.machine()
full_hostname = platform.node()
hostname=full_hostname

bench_name="cHull"
benchmarks=[
    "convexHull/quickHull"
]

inner_data_folder=[
    "convexHull/geometryData/data"
]

input_file=[
    "2DinSphere_10000000"
]

executable=[
    "hull.ser"
]

inputs=[
    "-r 1 -o /tmp/ofile971367_438110 ../geometryData/data/2DinSphere_10000000"
]


if __name__ == "__main__":
    with open('ser.csv', 'wb') as csvfile:
        res_writer = csv.writer(csvfile, delimiter=',')
        res_writer.writerow(['test name', 'time (s)'])
        for config in configs:
            util.log_heading(config["NAME"], character="-")
            

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

            total_sum = 0.0
            for b_index in range(len(executable)):
                util.chdir(ref_cwd)
                for i in range(0, config["NUM_RUNS"]):
                    row = []
                    row.append(bench_name)
                    try:
                        
                        util.chdir(ref_cwd + "/" + benchmarks[0] )
                        util.log_heading(benchmarks[0], character="=")
                        if i==0:
                            try:
                                clean_string = config["CLEAN_LINE"]
                                util.run_command(clean_string, verbose=True)
                            except:
                                print "Clean failed"
                        
                            build_bench_string = config["BUILD_LINE"]
                            util.run_command(build_bench_string, verbose=True) 
                        util.log_heading("running: " + benchmarks[0], character="=")
                        run_string = config["RUN_LINE"] + executable[b_index] + " " + inputs[0]
                        #running applications
                        if i == 0:#warm up serial run
                            util.run_command(run_string, verbose=True) 
                        output_string = util.run_command(run_string, verbose=True) 
                        output_lines = output_string.split('\n')
                        
                        time_line = output_lines[-2] #format is hh:mm:sec
                        time_line = time_line.split(':')
                        tot_secs = 0.0
                        for t in time_line:
                            tot_secs = (tot_secs*60) + float(t)
                        row.append(tot_secs)
                        total_sum+=tot_secs
                        print ('total secs= ' + str(tot_secs))
                        res_writer.writerow(row)            
                        


                        

                    except util.ExperimentError, e:
                        print "Error: %s" % e
                        print "-----------"
                        print "%s" % e.output
                        continue
            

        row = ['mean', "{0:.2f}".format(total_sum/config["NUM_RUNS"])]
        res_writer.writerow(row)            
        util.chdir(ref_cwd)        
        print("done")
