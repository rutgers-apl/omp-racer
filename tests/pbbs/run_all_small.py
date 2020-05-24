#!/usr/bin/python

import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, commands
from math import sqrt
import csv

def usage():
    print("Usage: run_all")


benchmarks=[
    {'name':'breadthFirstSearch', 'data_dir': 'graphData/data', 'bench_dir': 'ndBFS' },
    {'name':'comparisonSort', 'data_dir': 'sequenceData/data', 'bench_dir': 'sampleSort' },
    {'name':'delaunayTriangulation', 'data_dir': 'geometryData/data', 'bench_dir': 'incrementalDelaunay' },
    {'name':'delaunayRefine', 'data_dir': 'geometryData/data', 'bench_dir': 'incrementalRefine' },
    {'name':'dictionary', 'data_dir': 'sequenceData/data', 'bench_dir': 'deterministicHash' },
    {'name':'integerSort', 'data_dir': 'sequenceData/data', 'bench_dir': 'blockRadixSort' },
    {'name':'maximalIndependentSet', 'data_dir': 'graphData/data', 'bench_dir': 'ndMIS' },
    {'name':'maximalMatching', 'data_dir': 'graphData/data', 'bench_dir': 'ndMatching' },
    {'name':'nearestNeighbors', 'data_dir': 'geometryData/data', 'bench_dir': 'octTree2Neighbors' },
    {'name':'rayCast', 'data_dir': 'geometryData/data', 'bench_dir': 'kdTree' },
    {'name':'removeDuplicates', 'data_dir': 'sequenceData/data', 'bench_dir': 'deterministicHash' },
    {'name':'spanningForest', 'data_dir': 'graphData/data', 'bench_dir': 'ndST' },
    {'name':'suffixArray', 'data_dir': 'sequenceData/data', 'bench_dir': 'parallelKS' }
]

small_input = ['dictionary', 'integerSort', 'maximalIndependentSet', 'maximalMatching']
run_command = "python run_omprace.py"
run_command_small = "python run_omprace_small.py"

def run_benchmark(command):
    output = util.run_command(command, verbose=True)
    return output

if __name__ == "__main__":
    ref_cwd = os.getcwd()
    st = 0
    if len(sys.argv) == 2:
        st = int(sys.argv[1])
    for i in range(st,len(benchmarks)):
        t_dict = benchmarks[i]
        util.chdir(ref_cwd + "/" + t_dict['name'])
        if t_dict['name'] in small_input:
		    run_benchmark(run_command_small)
        else:
		    run_benchmark(run_command)
        util.chdir(ref_cwd)
    
