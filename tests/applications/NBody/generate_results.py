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

analyzer = '$PGEN/genprof'

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

benchmarks=[
    "nBody",
    "nBody",
    "nBody",
    "nBody_opt",
    "nBody_opt",
    "nBody_whatif"
]

analyzer_benchmarks=[
    "nBody",
    "nBody_whatif",
    "nBody_opt"
]

inner_folder=[
    "parallelCK",
    "parallelCK",
    "parallelCK",
    "parallelCK",
    "parallelCK",
    "parallelCK"
]
#no need to be array
inner_data_folder=[
    "geometryData/data",
    "geometryData/data",
    "geometryData/data",
    "geometryData/data",
    "geometryData/data",
    "geometryData/data"
]
#no need to be array
input_file=[
    "3DonSphere_1000000",
    "3DonSphere_1000000",
    "3DonSphere_1000000",
    "3DonSphere_1000000",
    "3DonSphere_1000000",
    "3DonSphere_1000000"
]

executable=[
    "nbody.ser",
    "nbody.openmp",
    "nbody.ompp",
    "nbody.openmp",
    "nbody.ompp",
    "nbody.ompp"
]

inputs=[
    "-r 1 -o /tmp/ofile974877_207802 ../geometryData/data/3DonSphere_1000000",
    "-r 1 -o /tmp/ofile974877_207802 ../geometryData/data/3DonSphere_1000000",
    "-r 1 -o /tmp/ofile974877_207802 ../geometryData/data/3DonSphere_1000000",
    "-r 1 -o /tmp/ofile974877_207802 ../../nBody/geometryData/data/3DonSphere_1000000",
    "-r 1 -o /tmp/ofile974877_207802 ../../nBody/geometryData/data/3DonSphere_1000000",
    "-r 1 -o /tmp/ofile974877_207802 ../../nBody/geometryData/data/3DonSphere_1000000"
]

for config in configs:
    util.log_heading(config["NAME"], character="-")
    times = []
    report_file = open("report.txt","w")

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
                output_string = util.run_command(run_string, verbose=True) 
                pbbs_time = []
                for line in output_string.split('\n'):
                    if line.startswith("PBBS-time:"):
                        pbbs_time.append(line)
                print("output is ")
                output_tokens = pbbs_time[-1].split(' ')
                print (output_tokens[1])
                times.append(float(output_tokens[1]))  
            except util.ExperimentError, e:
                print "Error: %s" % e
                print "-----------"
                print "%s" % e.output
                continue
    print("running analysis")
    report_file.write("Analysis result for each benchmark available in their respecitive log folder. Aggregated results in report.txt\n")
    for a_index in range(len(analyzer_benchmarks)):
        util.chdir(ref_cwd)
        util.chdir(ref_cwd + "/" + analyzer_benchmarks[a_index] + "/" + inner_folder[a_index])
        try:
            util.run_command(analyzer + " -f") 
            analyzer_output_file = open('log/profiler_output.csv')
            profiler_output_lines = analyzer_output_file.readlines()
            report_file.write("="*50 + '\n')
            report_file.write('analysis report for ' + analyzer_benchmarks[a_index] + '\n')
            report_file.write("="*50 + '\n')
            report_file.writelines(profiler_output_lines)
            if analyzer_benchmarks[a_index].endswith("whatif"):
                report_file.write("*"*50 + '\n')
                report_file.write('what-if analysis report for' + analyzer_benchmarks[a_index] + '\n')
                report_file.write("*"*50 + '\n')
                whatif_output_file = open('log/all_region_prof.csv')
                whatif_output_lines = whatif_output_file.readlines()
                report_file.writelines(whatif_output_lines)
          
            analyzer_output_file.close()
        except util.ExperimentError, e:
            print "Error: %s" % e
            print "-----------"
            print "%s" % e.output
            continue
    report_file.write("speedup values\n")
    orig_speedup = times[0]/times[1]
    opt_speedup = times[0]/times[3]
    report_file.write("="*50 + '\n')
    report_file.write("original_speedup: " + str(orig_speedup) + '\n')
    report_file.write("opt_speedup: " + str(opt_speedup) + '\n')
    report_file.write("="*50 + '\n')

report_file.close()
util.chdir(ref_cwd)        
print("script done")
