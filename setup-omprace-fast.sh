#!/bin/bash

OMPP_ROOT=`readlink -f .`
export OMPP_ROOT 

    export PATH=$LLVM_HOME/bin:$PATH
    export LD_LIBRARY_PATH=$LLVM_HOME/lib:$LD_LIBRARY_PATH
    echo $LD_LIBRARY_PATH
    LLVM_INCLUDE=$LLVM_HOME/include
    echo $LLVM_INCLUDE
    export LLVM_INCLUDE
    LLVM_LIB=$LLVM_HOME/lib
    echo $LLVM_LIB
    export LLVM_LIB

    #build race detector
    cd $OMPP_ROOT
    cd src/omprace_fast
    make clean
    make
    cd $OMPP_ROOT/src
    rm -f omprace
    ln -s omprace_fast omprace
    #
    cd $OMPP_ROOT
    echo "OMP-Race built successfully"
