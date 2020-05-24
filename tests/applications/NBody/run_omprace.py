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
          #"RUN_LINE" : "./",
          "RUN_LINE" : '/usr/bin/time -f "%E" ./',
          "ARGS" : "",
}

configs.append(entry)

ref_cwd = os.getcwd()
arch = platform.machine()
full_hostname = platform.node()
hostname=full_hostname

bench_name = "nBody"
benchmarks=[
    "nBody",
    "nBody",
    "nBody"
]

inner_folder=[
    "parallelCK",
    "parallelCK",
    "parallelCK"
]
#no need to be array
inner_data_folder=[
    "geometryData/data",
    "geometryData/data",
    "geometryData/data",
]
#no need to be array
input_file=[
    "3DonSphere_100000",
    "3DonSphere_100000",
    "3DonSphere_100000"
]

executable=[
    "nbody.openmp",
    "nbody.omprn",
    "nbody.ompp"
]

inputs=[
    "-r 1 -o /tmp/ofile974877_207802 ../geometryData/data/3DonSphere_100000",
    "-r 1 -o /tmp/ofile974877_207802 ../geometryData/data/3DonSphere_100000",
    "-r 1 -o /tmp/ofile974877_207802 ../geometryData/data/3DonSphere_100000"
]

if __name__ == "__main__":
    with open('omprace.csv', 'wb') as csvfile:
        res_writer = csv.writer(csvfile, delimiter=',')
        res_writer.writerow(['test name', 'baseline openmp(s)', 'omprace no_inst(s)', 'omprace(s)', 'overhead ospg', 'overhead datarace', 'num violations'])
        for config in configs:
            util.log_heading(config["NAME"], character="-")
            row = []
            row.append(bench_name)
            num_violations = -1

            print('input file folder: ' + inner_data_folder[0])
            data_input = benchmarks[0] + '/' + inner_data_folder[0]+'/'+input_file[0]
            print('checking if input data exists at:' + data_input)
            if not os.path.exists(data_input):
                print("input data doesn't exist. building input data")
                util.chdir(ref_cwd + "/" + benchmarks[0] + "/" + inner_data_folder[0])
                build_data = config["BUILD_LINE"] + " " + input_file[0]
                util.run_command(build_data, verbose=True)
                util.chdir(ref_cwd)
            else:
                print("input data exists")

            for b_index in range(len(benchmarks)):
                util.chdir(ref_cwd)
                for i in range(0, config["NUM_RUNS"]):
                    try:
                        
                        util.chdir(ref_cwd + "/" + benchmarks[b_index] + "/" + inner_folder[b_index])
                        util.log_heading(benchmarks[b_index], character="=")
                        try:
                            clean_string = config["CLEAN_LINE"]
                            util.run_command(clean_string, verbose=True)
                        except:
                            print "Clean failed"
                        
                        build_bench_string = config["BUILD_LINE"]
                        util.run_command(build_bench_string, verbose=True) 
                        util.log_heading("running: " + benchmarks[b_index], character="=")
                        run_string = config["RUN_LINE"] + executable[b_index] + " " + inputs[b_index]
                        #running applications
                        if b_index == 0:#warm up openmp run
                            util.run_command(run_string, verbose=True) 
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
