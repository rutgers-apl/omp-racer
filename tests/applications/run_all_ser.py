#!/usr/bin/python

import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, commands
from math import sqrt
import csv

def usage():
    print("Usage: run_dr <microbenchmark.cpp>")


#amgmk  ConvexHull  DelaunayTri  MST  NBody
benchmarks=[
    "amgmk",
    "ConvexHull",
    "DelaunayTri",
    "MST",
    "NBody",
    "QuickSilver"
]

run_command = "python run_ser.py"
    
def run_benchmark(f_name):
    #time_command = '/usr/bin/time -f "%E %M" '
    #output = util.run_command(time_command + "./" + f_name, verbose=True)
    output = util.run_command(run_command, verbose=True)
    return output


if __name__ == "__main__":
    ref_cwd = os.getcwd()
    for i in range(len(benchmarks)):
        util.chdir(ref_cwd + "/" + benchmarks[i])
        run_benchmark(benchmarks[i])
        util.chdir(ref_cwd)
    
