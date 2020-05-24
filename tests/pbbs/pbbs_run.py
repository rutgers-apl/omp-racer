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

analyzer = '../analyzer/analyzerDriver.out'

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
    "breadthFirstSearch",
    "comparisonSort",
    "convexHull",
    "delaunayTriangulation",
    "delaunayRefine",
    "dictionary",
    "integerSort",
    "maximalIndependentSet",
    "minSpanningForest",
    "nBody",
    "maximalMatching",
    "nearestNeighbors",
    "rayCast",
    "removeDuplicates",
    "spanningForest",
    "suffixArray"
]

inner_folder=[
    "ndBFS",
    "sampleSort",
    "quickHull",
    "incrementalDelaunay",
    "incrementalRefine",
    "deterministicHash",
    "blockRadixSort",
    "ndMIS",
    "parallelKruskal",
    "parallelCK",
    "ndMatching",
    "octTree2Neighbors",
    "kdTree",
    "deterministicHash",
    "ndST",
    "parallelKS"
]

executable=[
    "BFS",
    "sort",
    "hull",
    "delaunay",
    "refine",
    "dict",
    "isort",
    "MIS",
    "MST",
    "nbody",
    "matching",
    "neighbors",
    "ray",
    "remDups",
    "ST",
    "SA"

]

inputs=[
    "-r 1 -o /tmp/ofile971367_438110 ../graphData/data/randLocalGraph_J_5_10000000",
    "-r 1 -o /tmp/ofile971367_438111 ../sequenceData/data/randomSeq_100M_double", 
    "-r 1 -o /tmp/ofile971367_438112 ../geometryData/data/2DinSphere_10000000",
    "-r 1 -o /tmp/ofile971367_438113 ../geometryData/data/2DinCube_10M",
    "-r 1 -o /tmp/ofile699250_954868 ../geometryData/data/2DinCubeDelaunay_2000000",
    "-r 1 -o /tmp/ofile971367_438121 ../sequenceData/data/randomSeq_100M_int",
    "-r 1 -o /tmp/ofile971367_438122 ../sequenceData/data/randomSeq_100M_int",
    "-r 1 -o /tmp/ofile971367_438110 ../graphData/data/randLocalGraph_J_5_10000000",
    "-r 1 -o /tmp/ofile897171_477798 ../graphData/data/rMatGraph_WE_5_10000000",
    "-r 1 -o /tmp/ofile974877_207802 ../geometryData/data/3DonSphere_1000000",
    "-r 1 -o /tmp/ofile685095_551810 ../graphData/data/randLocalGraph_E_5_10000000",
    "-d 2 -k 1 -r 1 -o /tmp/ofile677729_89710 ../geometryData/data/2DinCube_10M",
    "-r 1 -o /tmp/ofile136986_843068 ../geometryData/data/happyTriangles ../geometryData/data/happyRays",
    "-r 1 -o /tmp/ofile530965_884365 ../sequenceData/data/randomSeq_100M_int",
    "-r 1 -o /tmp/ofile780084_677212 ../graphData/data/randLocalGraph_E_5_10000000",
    "-r 1 -o /tmp/ofile802103_686914 ../sequenceData/data/trigramString_100000000"
]

for config in configs:
    util.log_heading(config["NAME"], character="-")
        
    for benchmark in benchmarks:
        util.chdir(ref_cwd)
        for i in range(0, config["NUM_RUNS"]):
            try:
                util.chdir(ref_cwd + "/" + benchmark + "/" + inner_folder[benchmarks.index(benchmark)])
                util.log_heading(benchmark, character="=")
                if do_run:
                    try:
                        clean_string = config["CLEAN_LINE"]
                        util.run_command(clean_string, verbose=True)
                    except:
                        print "Clean failed"
                    build_string = config["BUILD_LINE"]
                    util.run_command(build_string, verbose=True)

                    run_string = config["RUN_LINE"] + executable[benchmarks.index(benchmark)] + " " + inputs[benchmarks.index(benchmark)]
                    util.run_command(run_string, verbose=True)

                    
                    util.run_command(analyzer, verbose=True)

                    
            except util.ExperimentError, e:
                print "Error: %s" % e
                print "-----------"
                print "%s" % e.output
                continue

util.chdir(ref_cwd)