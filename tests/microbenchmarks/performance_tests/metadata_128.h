#ifndef __METADATA_128_H_
#define __METADATA_128_H_


#include <stdint.h>
#include <unistd.h>
#include "atomic_ops.h"

typedef uint32_t GraphType;

//get upper 32bits of 64bit value
#define GETU32BIT_M 0xffffffff00000000  
//get lower 32bits of 64bit value
#define GETL32BIT_M 0x00000000ffffffff  

typedef struct __metadata_128{
  types::uint128_t m;
  //pthread_mutex_t metadata_lock;
  inline GraphType getWrite1(){
    return (uint32_t)m.hi;
  }
  inline GraphType getRead1(){
    return (uint32_t)m.lo;
  }
  inline GraphType getRead2(){
    return (uint32_t)(m.lo >> 32);
  }

  inline void setWrite1(GraphType new_write){
    m.hi = new_write;
  }

  inline void setRead1(GraphType new_read1){
    m.lo = (m.lo & GETU32BIT_M) | new_read1;
  }

  inline void setRead2(GraphType new_read2){
    uint64_t new_val = new_read2;
    m.lo = (m.lo & GETL32BIT_M) | (new_val << 32);
  }

  inline void setRead12(GraphType new_read1, GraphType new_read2){
    uint64_t new_val = new_read2;
    m.lo = new_read1 | (new_val << 32);
  }

  inline void setRead12Write(GraphType new_read1, GraphType new_read2,GraphType new_write){
    uint64_t new_val = new_read2;
    m.lo = new_read1 | (new_val << 32);
    m.hi = new_write;
  }

}Metadata_128;

#endif