#include <stdio.h>      
#include <iostream>
#include <stdlib.h>     
#include <time.h>       
#include <random>
#include <fstream>

#include "../performance_tests/metadata_128.h"
//use for perfromance counter reading
#include "../performance_tests/perf_counter.h"

#include <chrono>

#define PERF_COUNTER 1
#define N_REQ 1000000   //num requests
//max of the two bucket should not go over 36bits since total address space is
//limited to 40bits from appBase to appEnd
#define M_BUCKET_1 ((size_t)1024 * (size_t)4) //number of buckets on first level
//max should be 2^24 ((size_t)16 *(size_t) 1024 * (size_t) 1024)
#define M_BUCKET_2 ((size_t)16 *(size_t) 1024 * (size_t) 1024) //number of buckets on second level
// Each secondary table has 2^24 entries
#define SS_SEC_TABLE_ENTRIES ((size_t) 16*(size_t) 1024 * (size_t) 1024)

#define DUMP_TRACE 0

static const size_t kShadowBeg     = 0x100000000000ull;
//static const size_t kShadowEnd     = 0x200000000000ull;
static const size_t kShadowEnd     = 0x240000000000ull;
static const size_t appBase = 0x7f0000000000ull;
static const size_t appEnd =  0x7fffffffffffull;
static const size_t appMask = 0x00fffffffffcull;

int main(){
    std::random_device rd;
    std::mt19937 mt(rd());
    //TODO need to separate random number generation from accessing memory cause it might be taking too much time to get a new int
    //Make an access pattern writer and reader or make omprace dump accesses
    std::uniform_int_distribution<int> uni(0,M_BUCKET_1-1); // guaranteed unbiased
    std::uniform_int_distribution<int> uni2(0,M_BUCKET_2-1); // guaranteed unbiased    

    #if DUMP_TRACE
    std::ofstream report("tr_0.csv");
    #endif
    
    auto page_size = (SS_SEC_TABLE_ENTRIES) * sizeof(Metadata_128); 

    auto start = std::chrono::high_resolution_clock::now();
    #if PERF_COUNTER
    long long counter = 0;
    start_count(0);
    #endif

    for (int i=0; i < N_REQ; ++i){

        //std::cout<< "rand is: " << uni(mt) << std::endl;    
        auto primary_bucket = uni(mt);
        auto addr_offset = primary_bucket * page_size;
        auto addr = addr_offset + uni2(mt) + appBase;
        #if DUMP_TRACE
        assert(addr <= appEnd);
        report << (void*)addr << "," << 0 << std::endl;
        #endif
        size_t target = (((addr & appMask) << 4) + kShadowBeg);
        auto metadata_entry = (Metadata_128*) target;
        
        if (primary_bucket % 3 == 0){
            metadata_entry->setRead1(primary_bucket);    
        }
        else if (primary_bucket % 3 == 1){
            metadata_entry->setRead2(primary_bucket);    
        }
        else{
            metadata_entry->setWrite1(primary_bucket);    
        }
        
    }

    #if PERF_COUNTER
    counter= stop_n_get_count(0);
    printf("inst count = %lld\n", counter);
    #endif
    auto stop = std::chrono::high_resolution_clock::now();

    auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop-start);
    std::cout << "duration= " << int_ms.count() << " ms" << std::endl;

    #if DUMP_TRACE
    report.close();
    #endif
    return 0;
}