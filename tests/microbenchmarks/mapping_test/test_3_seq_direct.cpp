#include <stdio.h>      
#include <iostream>
#include <stdlib.h>     
#include <time.h>       
#include <random>

#include "../performance_tests/metadata_128.h"
#include "../performance_tests/tracereader.h"
//use for perfromance counter reading
#include "../performance_tests/perf_counter.h"

#include <chrono>

#define PERF_COUNTER 1
#define ROUNDS 100000000

static const size_t kShadowBeg     = 0x100000000000ull;
//static const size_t kShadowEnd     = 0x200000000000ull;
static const size_t kShadowEnd     = 0x240000000000ull;
static const size_t appBase = 0x7f0000000000ull;
static const size_t appMask = 0x00fffffffffcull;

#define N 10
#define START 7

int main(){

    void* arr[N];
    int vals[N];
    size_t sz= START;
    for (int i=0; i<N; i++){
        vals[i] = (int)sz;
        arr[i] = malloc(sz);
        sz <<= 2;
    }

    auto start = std::chrono::high_resolution_clock::now();
    #if PERF_COUNTER
    long long counter = 0;
    start_count(0);
    #endif

    for (int j=0; j<ROUNDS; j++){
        for (int i=0; i<N; i++){
            size_t saddr = (size_t) arr[i];
            size_t target = (((saddr & appMask) << 4) + kShadowBeg);
            //size_t target = (((saddr & appMask) << 4) ^ kShadowBeg);
            auto metadata_entry = (Metadata_128*) target;
            metadata_entry->setRead1(vals[i]);    
            
        }
    }
    #if PERF_COUNTER
    counter= stop_n_get_count(0);
    printf("inst count = %lld\n", counter);
    #endif
    auto stop = std::chrono::high_resolution_clock::now();

    for (int i=0; i<N; i++){
        size_t saddr = (size_t) arr[i];
        size_t target = (((saddr & appMask) << 4) + kShadowBeg);
        auto metadata_entry = (Metadata_128*) target;
        std::cout << "metadata read1 = " << metadata_entry->getRead1() 
        << " expected val = " << vals[i] << std::endl;
        assert(metadata_entry->getRead1() == vals[i]);

    }

    auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop-start);
    std::cout << "duration= " << int_ms.count() << " ms" << std::endl;

    return 0;
}