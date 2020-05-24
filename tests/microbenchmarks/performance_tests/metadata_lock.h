#ifndef __METADATA_H_
#define __METADATA_H_


#include <stdint.h>
#include <unistd.h>
#include "atomic_ops.h"

typedef uint32_t GraphType;

typedef struct __metadata{
  types::uint128_t m;
  //pthread_mutex_t metadata_lock;
  inline GraphType getWrite1(){
    return (uint32_t)m.hi;
    //types::uint128_t exp_val = {0x0,0x0};
    //do{
    //  exp_val = m;
    //} while(!cas(&m, exp_val, exp_val));

    //lock based
    //pthread_mutex_lock(&metadata_lock);
    //  types::uint128_t exp_val = m;
    //pthread_mutex_unlock(&metadata_lock);
    //return (uint32_t)exp_val.hi;
  }
  inline GraphType getRead1(){
    //pthread_mutex_lock(&metadata_lock);
    //  types::uint128_t exp_val = m;
    //pthread_mutex_unlock(&metadata_lock);
    
    //return (uint32_t)m.lo;
    return (uint32_t)m.lo;
  }
  inline GraphType getRead2(){
    //pthread_mutex_lock(&metadata_lock);
    //  types::uint128_t exp_val = m;
    //pthread_mutex_unlock(&metadata_lock);
    //return (uint32_t)(m.lo >> 32);
    
    //return (uint32_t)(exp_val.lo >> 32);
    return (uint32_t)(m.lo >> 32);
  }

  inline void setWrite1(GraphType new_write){
    types::uint128_t exp_val = {0x0,0x0}; 
    types::uint128_t new_val = {0x0,0x0}; 
    do{
      exp_val.hi = m.hi;
      exp_val.lo = m.lo;
      new_val.hi = new_write;
      new_val.lo = m.lo;
      //is exp_val needed. I can probably use m as the second arg
    } while (!cas(&m, exp_val, new_val));

    //lock based
    //pthread_mutex_lock(&metadata_lock);
    //  m.hi = new_write;
    //pthread_mutex_unlock(&metadata_lock);
  }

  inline void setRead1(GraphType new_read1){
    types::uint128_t exp_val = {0x0,0x0}; 
    types::uint128_t new_val = {0x0,0x0}; 
    do{
      exp_val.hi = m.hi;
      exp_val.lo = m.lo;
      new_val.hi = m.hi;
      new_val.lo = (m.lo & GETU32BIT_M) | new_read1;
    } while (!cas(&m, exp_val, new_val));
  }

  inline void setRead2(GraphType new_read2){
    types::uint128_t exp_val = {0x0,0x0}; 
    types::uint128_t new_val = {0x0,0x0}; 
    do{
      exp_val.hi = m.hi;
      exp_val.lo = m.lo;
      new_val.hi = m.hi;
      new_val.lo = new_read2;
      new_val.lo = (new_val.lo << 32) | (exp_val.lo & GETL32BIT_M);
    } while (!cas(&m, exp_val, new_val));
  }

  inline void setRead12(GraphType new_read1, GraphType new_read2){
    types::uint128_t exp_val = {0x0,0x0}; 
    types::uint128_t new_val = {0x0,0x0}; 
    do{
      exp_val.hi = m.hi;
      exp_val.lo = m.lo;
      new_val.hi = m.hi;
      new_val.lo = new_read2;
      new_val.lo = (new_val.lo << 32) | new_read1;
    } while (!cas(&m, exp_val, new_val));
  }

  inline void setRead12Write(GraphType new_read1, GraphType new_read2,GraphType new_write){
    types::uint128_t exp_val = {0x0,0x0}; 
    types::uint128_t new_val = {0x0,0x0}; 
    do{
      exp_val.hi = m.hi;
      exp_val.lo = m.lo;
      new_val.hi = new_write;
      new_val.lo = new_read2;
      new_val.lo = (new_val.lo << 32) | new_read1;
    } while (!cas(&m, exp_val, new_val));
  }

}Metadata;

#endif