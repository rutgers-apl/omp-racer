#include <stdio.h>      
#include <iostream>
#include <stdlib.h>     
#include <time.h>       
#include <random>

#include "../performance_tests/metadata_128.h"
#include "../performance_tests/tracereader.h"
//use for perfromance counter reading
#include "../performance_tests/perf_counter.h"
#include "triemap.h"
#include <chrono>

#define PERF_COUNTER 1
#define ROUNDS 100000000


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

    TrieMap<Metadata_128> trie;
    Metadata_128** m_primary_trie = trie.get_primary_trie();
    printf("map ptr is %p\n", trie.get_primary_trie());
    
    auto start = std::chrono::high_resolution_clock::now();
    #if PERF_COUNTER
    long long counter = 0;
    start_count(0);
    #endif
    //ignore traces from other threads since this is sequential
    for (int j=0; j<ROUNDS; j++){
        for (int i=0; i<N; i++){

            size_t saddr = (size_t) arr[i];
            saddr = saddr >> 2;
            auto primary_bucket = (saddr >> 24) & 0x3fffff;
            Metadata_128* secondary_bucket = m_primary_trie[primary_bucket];
            
            if (secondary_bucket == NULL){
                size_t sec_length = (SS_SEC_TABLE_ENTRIES) * sizeof(Metadata_128);
        
                auto new_page = (Metadata_128 *)mmap(0, sec_length, PROT_READ| PROT_WRITE,
                            MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
                m_primary_trie[primary_bucket] = new_page;
            }

            auto page_offset = (saddr) & 0xffffff;
            Metadata_128* metadata_entry = &m_primary_trie[primary_bucket][page_offset];
            //update meta so that the compiler doesn't optimize previous addr calculations
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
        saddr = saddr >> 2;
        auto primary_bucket = (saddr >> 24) & 0x3fffff;
        Metadata_128* secondary_bucket = m_primary_trie[primary_bucket];
        auto page_offset = (saddr) & 0xffffff;
        Metadata_128* metadata_entry = &m_primary_trie[primary_bucket][page_offset];
        std::cout << "metadata read1 = " << metadata_entry->getRead1() 
        << " expected val = " << vals[i] << std::endl;
        assert(metadata_entry->getRead1() == vals[i]);

    }
    auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop-start);
    std::cout << "duration= " << int_ms.count() << " ms" << std::endl;

    return 0;
}