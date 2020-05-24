#ifndef __ATOMIC_OPS_H_
#define __ATOMIC_OPS_H_

#include <stdint.h>
#include <iostream>
#include <cassert>
#include <cstdint>
#include <cstdio>

#include <thread>
#include <atomic>

namespace types
{
    // alternative: union with  unsigned __int128
    struct uint128_t
    {
        uint64_t lo;
        uint64_t hi;
    }
    __attribute__ (( __aligned__( 16 ) ));
}

inline bool cas_128( volatile types::uint128_t * src, types::uint128_t cmp, types::uint128_t with )
{
    // cmp can be by reference so the caller's value is updated on failure.

    // suggestion: use __sync_bool_compare_and_swap and compile with -mcx16 instead of inline asm
    bool result;
    __asm__ __volatile__
    (
        "lock cmpxchg16b %1\n\t"
        "setz %0"       // on gcc6 and later, use a flag output constraint instead
        : "=q" ( result )
        , "+m" ( *src )
        , "+d" ( cmp.hi )
        , "+a" ( cmp.lo )
        : "c" ( with.hi )
        , "b" ( with.lo )
        : "cc", "memory" // compile-time memory barrier.  Omit if you want memory_order_relaxed compile-time ordering.
    );
    return result;
}

template <class ET>
inline bool cas_gcc(ET *ptr, ET oldv, ET newv) {
  if (sizeof(ET) == 4) {
    return __sync_bool_compare_and_swap((int*)ptr, *((int*)&oldv), *((int*)&newv));
  } else if (sizeof(ET) == 8) {
    return __sync_bool_compare_and_swap((long*)ptr, *((long*)&oldv), *((long*)&newv));
  } 
  //else if (sizeof(ET) == 16) {
  //  return __sync_bool_compare_and_swap((__int128*)ptr, *((__int128*)&oldv), *((__int128*)&newv));
  //} 
  else {
    std::cout << "CAS bad length" << std::endl;
    abort();
  }
}

#endif