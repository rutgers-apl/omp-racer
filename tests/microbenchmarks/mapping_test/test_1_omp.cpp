#include <stdio.h>      
#include <iostream>
#include <stdlib.h>     
#include <time.h>       
#include <random>

#include "../performance_tests/metadata_128.h"
#include "../performance_tests/tracereader.h"
#include "triemap.h"
#include <chrono>

#define NUM_THREADS 16
#define ROUNDS 200



void simulateAccess(Metadata_128** trie, std::vector<size_t> memory){
    Metadata_128** m_primary_trie = trie;
    for (int i=0; i<ROUNDS; i++){
        for (size_t addr: memory){
            size_t saddr = (size_t) addr;
            saddr = saddr >> 2;
            auto primary_bucket = (saddr >> 24) & 0x3fffff;
            Metadata_128* secondary_bucket = m_primary_trie[primary_bucket];
            
            if (secondary_bucket == NULL){
                size_t sec_length = (SS_SEC_TABLE_ENTRIES) * sizeof(Metadata_128);
        
                Metadata_128* new_page = (Metadata_128 *)mmap(0, sec_length, PROT_READ| PROT_WRITE,
                            MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);
                
                if(cas_gcc<Metadata_128*>(&secondary_bucket, 0, new_page)){//first thread will eval to true
                    m_primary_trie[primary_bucket] = secondary_bucket;  
                }
                else{//others should unmap otherwise we'll have memory leak
                    munmap(new_page, sec_length);
                }
                m_primary_trie[primary_bucket] = new_page;
            }

            auto page_offset = (saddr) & 0xffffff;
            Metadata_128* metadata_entry = &m_primary_trie[primary_bucket][page_offset];
            //update meta so that the compiler doesn't optimize previous addr calculations
            metadata_entry->setRead1(addr);    
            
        }    
    }

}
int main(){
    
    TraceReader t_reader("trace", "tr", NUM_THREADS);
    t_reader.ReadTraceFiles();
    std::vector<size_t> *mem_accessed = t_reader.GetAccessVecotrs();

    TrieMap<Metadata_128> trie;
    Metadata_128** m_primary_trie = trie.get_primary_trie();
    printf("map ptr is %p\n", trie.get_primary_trie());
    

    auto start = std::chrono::high_resolution_clock::now();
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        #pragma omp for schedule(dynamic, 1) 
            for (int i=0; i<NUM_THREADS; i++){
                simulateAccess(m_primary_trie, mem_accessed[i]);
            }
    }
    auto stop = std::chrono::high_resolution_clock::now();

    auto int_ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop-start);
    std::cout << "duration= " << int_ms.count() << " ms" << std::endl;

    return 0;
}