#include <stdio.h>      
#include <iostream>
#include <stdlib.h>     
#include <time.h>       
#include <random>

#include "../performance_tests/metadata_128.h"
//use for perfromance counter reading
#include "../performance_tests/perf_counter.h"
#include "triemap.h"
#include <chrono>

#define PERF_COUNTER 1
#define N_REQ 1000000   //num requests
/*
* in theory max can be 2^22 ((size_t)4 * (size_t) 1024* (size_t) 1024) 
* however that would imply that we may use up to 2^50 Bytes
* because 22+24+4 (add 4 because meta data is 16 bytes)
* so in main bucket should be at most ((size_t)1024 * (size_t)512) to
* fit in 47 bit application vitual addr space
* to reach parity with the 40 bit = 1TB application memory restriction
* we impose on direct mapping, max of main bucket should be
* 16 bits = 1024 * 64 = 65536 
*/
#define M_BUCKET_1 ((size_t)1024 * (size_t)4) //number of buckets on first level
//max should be 2^24 ((size_t)16 *(size_t) 1024 * (size_t) 1024)
#define M_BUCKET_2 ((size_t)16 *(size_t) 1024 * (size_t) 1024) //number of buckets on second level

int main(){
    std::random_device rd;
    std::mt19937 mt(rd());
    //TODO need to separate random number generation from accessing memory cause it might be taking too much time to get a new int
    //Make an access pattern writer and reader or make omprace dump accesses
    std::uniform_int_distribution<int> uni(0,M_BUCKET_1-1); // guaranteed unbiased
    std::uniform_int_distribution<int> uni2(0,M_BUCKET_2-1); // guaranteed unbiased    

    TrieMap<Metadata_128> trie;
    Metadata_128** m_primary_trie = trie.get_primary_trie();
    printf("map ptr is %p\n", trie.get_primary_trie());
    

    auto start = std::chrono::high_resolution_clock::now();
    #if PERF_COUNTER
    long long counter = 0;
    start_count(0);
    #endif
    for (int i=0; i < N_REQ; ++i){
        
        //std::cout<< "rand is: " << uni(mt) << std::endl;    
        auto primary_bucket = uni(mt);
        Metadata_128* secondary_bucket = m_primary_trie[primary_bucket];
        if (secondary_bucket == NULL){
            size_t sec_length = (SS_SEC_TABLE_ENTRIES) * sizeof(Metadata_128);
    
            auto new_page = (Metadata_128 *)mmap(0, sec_length, PROT_READ| PROT_WRITE,
						MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
            //assert(new_page != (void *)-1);
            m_primary_trie[primary_bucket] = new_page;

        }

        auto page_offset = uni2(mt);
        Metadata_128* metadata_entry = &m_primary_trie[primary_bucket][page_offset];
        //update meta randomly without calling random again
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

    return 0;
}