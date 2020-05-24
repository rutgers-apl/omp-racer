#!/usr/bin/python

import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, commands
from math import sqrt
import csv

def usage():
    print("Usage: clean_all")


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

        util.chdir(ref_cwd + "/" + t_dict['name'])
        bench_root = os.getcwd()
        util.chdir(bench_root+ "/" + t_dict['data_dir'])
        util.run_command(clean_command, verbose=True)
        util.chdir(bench_root + "/" + t_dict['bench_dir'])
        util.run_command(clean_command, verbose=True)
        #run_benchmark(benchmarks[i])
        util.chdir(ref_cwd)
    