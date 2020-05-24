#!/usr/bin/python

import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, commands
from math import sqrt
import csv

def usage():
    print("Usage: clean_all")


benchmarks=[
    {'name':'alignment-for', 'bench_dir': 'bots-alignment-for' },
    {'name':'alignment-single', 'bench_dir': 'bots-alignment-single' },
    {'name':'concom', 'bench_dir': 'bots-concom' },
    {'name':'fft', 'bench_dir': 'bots-fft' },
    {'name':'fib', 'bench_dir': 'bots-fib' },
    {'name':'floorp', 'bench_dir': 'bots-floorplan' },
    {'name':'health', 'bench_dir': 'bots-health' },
    {'name':'knapsack', 'bench_dir': 'bots-knapsack' },
    {'name':'nqueens', 'bench_dir': 'bots-nqueens' },
    {'name':'sort', 'bench_dir': 'bots-sort' },
    {'name':'sparselu-for', 'bench_dir': 'bots-sparselu-for' },
    {'name':'sparselu-single', 'bench_dir': 'bots-sparselu-single' },
    {'name':'strassen', 'bench_dir': 'bots-strassen' },
    {'name':'uts', 'bench_dir': 'bots-uts' }
]



clean_command = "make clean"
run_command = "python run_omprace.py"
    
def run_benchmark(f_name):
    #time_command = '/usr/bin/time -f "%E %M" '
    #output = util.run_command(time_command + "./" + f_name, verbose=True)
    output = util.run_command(run_command, verbose=True)
    return output


if __name__ == "__main__":
    ref_cwd = os.getcwd()
    for i in range(len(benchmarks)):
        t_dict = benchmarks[i]
        
        util.chdir(ref_cwd + "/" + t_dict['bench_dir'])
        util.run_command(clean_command, verbose=True)

        util.chdir(ref_cwd)
    
