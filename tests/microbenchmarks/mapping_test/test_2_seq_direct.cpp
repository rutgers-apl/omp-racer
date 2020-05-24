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
#define ROUNDS 200

static const size_t kShadowBeg     = 0x100000000000ull;
//static const size_t kShadowEnd     = 0x200000000000ull;
static const size_t kShadowEnd     = 0x240000000000ull;
static const size_t appBase = 0x7f0000000000ull;
static const size_t appMask = 0x00fffffffffcull;

int main(){

    TraceReader t_reader("trace", "tr", 1);
    t_reader.ReadTraceFiles();
    std::vector<size_t> *mem_accessed = t_reader.GetAccessVecotrs();
    
    auto start = std::chrono::high_resolution_clock::now();
    #if PERF_COUNTER
    long long counter = 0;
    start_count(0);
    #endif

    for (int i=0; i<ROUNDS; i++){
        for (size_t addr: mem_accessed[0]){
            size_t saddr = (size_t) addr;
            //size_t primary_bucket = (saddr >> 26) & 0x3fffff;
            //or update appmask
            size_t target = (((saddr & appMask) << 4) + kShadowBeg);
            auto metadata_entry = (Metadata_128*) target;
            metadata_entry->setRead1(addr);    
            
        }
    }
    #if PERF_COUNTER
    counter= stop_n_get_count(0);
    printf("inst count = %lld\n", counter);
    #endif
    auto stop = std::chrono::high_resolution_clock::now();

    auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop-start);
    std::cout << "duration= " << int_ms.count() << " ms" << std::endl;

    return 0;
}