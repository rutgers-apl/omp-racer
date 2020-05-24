#ifndef __TRIE_MAP_H_
#define __TRIE_MAP_H_


#include "../performance_tests/metadata_128.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>

#define __OMP_RACER_MMAP_FLAGS (MAP_PRIVATE| MAP_ANONYMOUS| MAP_NORESERVE)

// Primary trie has 2^ 22 entries
#define SS_PRIMARY_TABLE_ENTRIES  ((size_t)4 * (size_t) 1024* (size_t) 1024)

// Each secondary table has 2^24 entries
#define SS_SEC_TABLE_ENTRIES ((size_t) 16*(size_t) 1024 * (size_t) 1024)

template <class META>
class TrieMap{
    public:
        TrieMap(){
            size_t primary_length = (SS_PRIMARY_TABLE_ENTRIES) * sizeof(META*);
            m_primary_trie = (META**)mmap(0, primary_length, PROT_READ| PROT_WRITE,
						__OMP_RACER_MMAP_FLAGS, -1, 0);
            assert(m_primary_trie != (void *)-1);
        }

        inline META** get_primary_trie(){
            return m_primary_trie;
        }
    private:
        META** m_primary_trie;

};

#endif