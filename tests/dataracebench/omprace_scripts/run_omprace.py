#!/usr/bin/python

import sys, string, os, popen2, shutil, platform, subprocess, pprint, time
import util, commands
from math import sqrt
import csv

folder = []        
cmd= [
    "python run_all.py",

if __name__ == "__main__":
    #files = os.listdir("./")
    
    util.run_command(cmd ,verbose=True)
    