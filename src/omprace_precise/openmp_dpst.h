#ifndef __OPENMP_DPST_H_
#define __OPENMP_DPST_H_

#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <assert.h>
#include <map>
#include <unordered_map>
#include <vector>
#include <omp.h>
#include <pthread.h>
#include <ompt.h>
//for perf_event_open
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/mman.h>
#include <utility>

#include <atomic>
#include "util.h"
#include "TaskDependency.h"
#include "LockSet.h"

#define NUM_THREADS 16
#define READ 0
#define WRITE 1

//set to 1 to only check if taskwaits are perfectly nested
//#define __OMP_RACER_NESTED 1

//set to 1 to get stat collections
#define STAT 0
//set to 1 to reclaim metadata after each parallel region
#define MEM_RECLAIM 0
//set to 1 to have a no opt instrument mem calls
#define BASELINE 0
//set to 1 to skip lca checking
#define NOLCA 0

//choose either between direct mapping and trie based shadow memory mapping. 
#define TRIESHADOW 1
#define DIRECTSHADOW 0

//set to 1 to dump out instrument memory calls
#define DUMP_INST_MEM 0

//choose one of the following
#define ORIG_META 0
#define CAS_META 0
#define LOCK_META 0
#define PRECISE_META 1
#define LOCKSET_META 0
typedef unsigned int THREADID;

#define SYS_CAS 1
//define as 1 so that instrument mem provides src location info
#define LOC_INFO 0

//used hashmap to support faster dependency checking
#define FAST_DEP 1

//Add these since these types are not defined in latest version of ompt
typedef uint64_t ompt_thread_id_t;
typedef uint64_t ompt_task_id_t;
typedef uint64_t ompt_parallel_id_t;
typedef uint64_t ompt_wait_id_t;

#define __OMP_RACER_MMAP_FLAGS (MAP_PRIVATE| MAP_ANONYMOUS| MAP_NORESERVE)
#define __OMP_RACER_NUM_GRAPH_NODES 134217728

// Primary trie has 2^ 22 entries
#define SS_PRIMARY_TABLE_ENTRIES  ((size_t)4 * (size_t) 1024* (size_t) 1024)

// Each secondary table has 2^24 entries
#define SS_SEC_TABLE_ENTRIES ((size_t) 16*(size_t) 1024 * (size_t) 1024)
#define GraphType uint32_t

//maximum number of data race violations kept
#define __OMP_RACER_MAX_VIOLATIONS 10

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


//TODO does this need replacing?
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

//template< class T > inline bool cas( volatile T * src, T cmp, T with );
//inline bool cas( volatile types::uint128_t * src, types::uint128_t cmp, types::uint128_t with );

#if SYS_CAS
inline bool cas( types::uint128_t * ptr, types::uint128_t oldv, types::uint128_t newv )
{
  return __sync_bool_compare_and_swap((__int128*)ptr, *((__int128*)&oldv), *((__int128*)&newv));
}
#else
inline bool cas( volatile types::uint128_t * src, types::uint128_t cmp, types::uint128_t with )
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
#endif
// Each index can be an integer. Max number of nodes is 2^27. 
#if ORIG_META
struct Metadata{
  GraphType write1;
  GraphType read1;
  GraphType read2;
  pthread_mutex_t metadata_lock;
  //pthread_spinlock_t metadata_lock;
};
#endif

//get upper 32bits of 64bit value
#define GETU32BIT_M 0xffffffff00000000  
//get lower 32bits of 64bit value
#define GETL32BIT_M 0x00000000ffffffff  


#if CAS_META
struct Metadata{
  types::uint128_t m;
  //pthread_mutex_t metadata_lock;
  
  //currently unused
  inline GraphType getFlags(){
    return (uint32_t)(m.hi >> 32);
  }

  inline GraphType getWrite1(){

    return (uint32_t)m.hi;
    
    //types::uint128_t exp_val = {0x0,0x0};
    //do{
    //  exp_val = m;
    //} while(!cas(&m, exp_val, exp_val));
    //return (uint32_t)exp_val.hi;

  }
  inline GraphType getRead1(){
    
    return (uint32_t)m.lo;

    //using atomic
    //types::uint128_t exp_val = {0x0,0x0};
    //do{
    //    exp_val = m;
    //  } while(!cas(&m, exp_val, exp_val));
    //return (uint32_t)exp_val.lo;

  }
  inline GraphType getRead2(){
    
    return (uint32_t)(m.lo >> 32);
    
    //types::uint128_t exp_val = {0x0,0x0};
    //do{
    //  exp_val = m;
    // } while(!cas(&m, exp_val, exp_val));
    //return (uint32_t)(exp_val.lo >> 32);

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

};
#endif

#if LOCK_META
struct Metadata{
  types::uint128_t m;
  pthread_mutex_t metadata_lock;
  
  inline GraphType getWrite1(){
    //lock based
    pthread_mutex_lock(&metadata_lock);
    types::uint128_t exp_val = m;
    pthread_mutex_unlock(&metadata_lock);
    return (uint32_t)exp_val.hi;
  }
  inline GraphType getRead1(){
    pthread_mutex_lock(&metadata_lock);
    types::uint128_t exp_val = m;
    pthread_mutex_unlock(&metadata_lock);
    
    return (uint32_t)exp_val.lo;
  }
  inline GraphType getRead2(){
    pthread_mutex_lock(&metadata_lock);
    types::uint128_t exp_val = m;
    pthread_mutex_unlock(&metadata_lock);
    return (uint32_t)(exp_val.lo >> 32);
  
  }

  inline void setWrite1(GraphType new_write){
    
    //lock based
    pthread_mutex_lock(&metadata_lock);
    m.hi = new_write;
    pthread_mutex_unlock(&metadata_lock);
  }

  inline void setRead1(GraphType new_read1){
    pthread_mutex_lock(&metadata_lock);
    m.lo = (m.lo & GETU32BIT_M) | new_read1;
    pthread_mutex_unlock(&metadata_lock);
    
  }

  inline void setRead2(GraphType new_read2){
    uint64_t new_lo = new_read2;
    new_lo = new_lo << 32;
    pthread_mutex_lock(&metadata_lock);
    m.lo = new_lo | (m.lo & GETL32BIT_M);
    pthread_mutex_unlock(&metadata_lock);
    
  }

  inline void setRead12(GraphType new_read1, GraphType new_read2){
    uint64_t new_lo = new_read2;
    new_lo = new_lo << 32;
    new_lo = new_lo | new_read1;
    pthread_mutex_lock(&metadata_lock);
    m.lo = new_lo;
    pthread_mutex_unlock(&metadata_lock);
  }

  inline void setRead12Write(GraphType new_read1, GraphType new_read2,GraphType new_write){
    uint64_t new_lo = new_read2;
    new_lo = new_lo << 32;
    new_lo = new_lo | new_read1;
    pthread_mutex_lock(&metadata_lock);
    m.lo = new_lo;
    m.hi = new_write;
    pthread_mutex_unlock(&metadata_lock);
  }

};
#endif


#if PRECISE_META
#define NUM_META_ENTRIES 8
#define ROOT_INDEX 1
struct MetadataEntry{
  GraphType id;
  GraphType write1;
  GraphType read1;
  GraphType read2;
};
struct Metadata{
  pthread_mutex_t metadata_lock;
  int entry_count{0};
  struct MetadataEntry entries[NUM_META_ENTRIES];
  std::unordered_map<GraphType, struct MetadataEntry>* entry_map{nullptr};
};
#endif

#if LOCKSET_META
#define NUM_META_ENTRIES 4

struct MetadataEntry{
  GraphType write1;
  GraphType write2;
  GraphType read1;
  GraphType read2;
  size_t lock_set;
  //pthread_spinlock_t metadata_lock;
};
struct Metadata{
  //types::uint128_t m;
  int entry_count{0};
  struct MetadataEntry entries[NUM_META_ENTRIES];
  std::vector<struct MetadataEntry>* entry_list{nullptr};
  //move lock
  pthread_mutex_t metadata_lock;
  
};
#endif

// Used for collecting per thread statistics
typedef struct PerThreadStats{
  long m_enter_instrument_mem{0};
  long m_exit_instrument_mem{0};
  long m_lca_calls{0};
  long m_lca_height{0};
  long m_shadow_mem_buckets{0};
  long m_lca_cache_hit{0};
  long m_lca_cache_miss{0};
  long padding{0};

} PerThreadStats;


//A stat structure used to keep count of different operations, aggregates per thread stats
struct Statistics{
  PerThreadStats pt_stat[NUM_THREADS];
  long m_ospg_nodes_created{0};
  
  
  void display(){
    std::ofstream stat_file;
    long m_enter_instrument_mem_sum = 0;
    long m_exit_instrument_mem_sum = 0;
    long m_lca_calls_sum = 0;
    long m_lca_height_sum = 0;
    long m_shadow_mem_buckets_sum = 0;
    long m_lca_cache_hit_sum = 0;
    long m_lca_cache_miss_sum = 0;

    for (int i=0; i < NUM_THREADS; i++){
      m_enter_instrument_mem_sum += pt_stat[i].m_enter_instrument_mem;
      m_exit_instrument_mem_sum += pt_stat[i].m_exit_instrument_mem;
      m_lca_calls_sum += pt_stat[i].m_lca_calls;
      m_lca_height_sum += pt_stat[i].m_lca_height;
      m_shadow_mem_buckets_sum += pt_stat[i].m_shadow_mem_buckets;
      
      m_lca_cache_hit_sum += pt_stat[i].m_lca_cache_hit;
      m_lca_cache_miss_sum += pt_stat[i].m_lca_cache_miss;
    }
    
    std::string file_name = "stat.txt";
		stat_file.open(file_name);
    stat_file << "m_enter_instrument_mem count = " << m_enter_instrument_mem_sum << std::endl;
    stat_file << "m_exit_instrument_mem  count = " << m_exit_instrument_mem_sum << std::endl;
    stat_file << "m_mem_accesses skipped count = " << m_enter_instrument_mem_sum - m_exit_instrument_mem_sum 
              << " % of instrument mems skipped (higher is better) = " 
              << (1.0 - (double)m_exit_instrument_mem_sum/m_enter_instrument_mem_sum) * 100 << "%" << std::endl;
    stat_file << "m_lca_calls count            = " << m_lca_calls_sum << std::endl;
    stat_file << "m_lca_height total count     = " << m_lca_height_sum << std::endl;
    if (m_lca_calls_sum > 0){
      stat_file << "avg lca_height per call      = " << m_lca_height_sum / m_lca_calls_sum << std::endl;
    }
    else{
      stat_file << "avg lca_height per call      = 0" << std::endl;
    }
    stat_file << "ospg nodes created           = " << m_ospg_nodes_created << std::endl;
    stat_file << "buckets count max(2^22)      = " << m_shadow_mem_buckets_sum << std::endl;
    auto cache_hit_rate = ((double)m_lca_cache_hit_sum/(m_lca_cache_hit_sum+m_lca_cache_miss_sum) * 100);
    stat_file << "lca_cache_hit_rate      = " << cache_hit_rate << std::endl;
    stat_file.close();
  }
};    



typedef std::pair<size_t,size_t> work_pair;

struct comp
{
	template<typename T>
	/*bool operator()(const T &l, const T &r) const
	{
		if (l.first == r.first)
			return l.second > r.second;

		return l.first < r.first;
	}*/
	std::size_t operator()(const T &p) const
	{		
		size_t seed = 0;
		hash_combine<size_t>(seed, p.first);
		hash_combine<size_t>(seed, p.second); 
		return seed;
	}
	//this is boost's hash combine since STL has none 
	template <class T>
	static inline void hash_combine(std::size_t& seed, const T& v)
	{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
	}
};

class CLNode {
	public:
	work_pair key;
	int value;
	CLNode *prev, *next;
	CLNode(work_pair k, int v): key(k), value(v), prev(NULL), next(NULL) {}
};

class DoublyLinkedList {
	CLNode *head, *tail;
	
	bool isEmpty() {
		return head->next == tail;
	}

	public:
	DoublyLinkedList(){
		head = new CLNode(std::make_pair(0,0),0);
		tail = new CLNode(std::make_pair(0,0),0);
		head->next = tail;
		tail->prev = head;

	}
	
	inline CLNode* 
	add_node_to_head(work_pair key, int value=0) {
		CLNode *node = new CLNode(key, value);
		//move node to head
		head->next->prev = node;
		node->next = head->next;
		node->prev = head;
		head->next = node;
		
		return node;
	}

	inline void 
	move_node_to_head(CLNode *node) {
		if (head->next == node)
			return;
		node->prev->next = node->next;
		node->next->prev = node->prev;

		//move node to head
		head->next->prev = node;
		node->next = head->next;
		node->prev = head;
		head->next = node;
	}

	inline void 
	remove_lru_node() {
		if(isEmpty()) {
			return;
		}
		CLNode* evict_node = tail->prev;
		tail->prev = evict_node->prev;
		evict_node->prev->next = tail;
		delete evict_node;
	}
	//pre-cond the list is not empty
	inline CLNode* 
	get_lru_node() {
		return tail->prev;
	}
	
};

//class implementing LRU cache
//Can be used to cache LCA queries
class LRUCache{
	int capacity, size;
	DoublyLinkedList *lcaList;
	std::unordered_map<work_pair, CLNode*, comp> lcaMap;

	public:
    LRUCache() {
    	this->capacity = 2 << 20;
    	size = 0;
      lcaList = new DoublyLinkedList();
      lcaMap = std::unordered_map<work_pair, CLNode*, comp>();
    }
    LRUCache(int capacity) {
    	this->capacity = capacity;
    	size = 0;
        lcaList = new DoublyLinkedList();
        lcaMap = std::unordered_map<work_pair, CLNode*, comp>();
    }
    
    inline int 
	get(work_pair key) {
        if(lcaMap.find(key)==lcaMap.end()) {
        	return -1;
        }
        int val = lcaMap[key]->value;

        // move the page to front
        lcaList->move_node_to_head(lcaMap[key]);
        return val;
    }
    
    inline void 
	put(work_pair key, int value) {
    	if(lcaMap.find(key)!=lcaMap.end()) {
    		// if key already present, update value and move node to head
    		lcaMap[key]->value = value;
    		lcaList->move_node_to_head(lcaMap[key]);
    		return;
    	}

        if(size == capacity) {
        	// remove rear node
        	work_pair k = lcaList->get_lru_node()->key;
        	lcaMap.erase(k);
        	lcaList->remove_lru_node();
        	size--;
        }

        // add new page to head of the list
        CLNode *page = lcaList->add_node_to_head(key, value);
        size++;
        lcaMap[key] = page;
    }

	void 
	clearCache(){
		for (auto it: lcaMap){
			delete it.second;
		}
    	delete lcaList;
	}

	//this can be removed since at this point program will be over
    ~LRUCache() {
		for (auto it: lcaMap){
			delete it.second;
		}
    	delete lcaList;
    }
};


enum NodeType {
  NO_TYPE = 0,
  PARALLEL = 1,
  SERIES = 2,
  WORK = 3
};

//TODO use one int to pack the boolean flags used in a node. Also make sure is the node is cache aligned in size
struct Node {
  NodeType m_type;
  //id of parent
  GraphType m_parent;
  GraphType m_depth;
  //Sequence number uniquely identifies the child of a particular parent, used with LCA queries
  size_t m_seqnum;
  GraphType m_numchildren;
  NodeType m_young_ns_child;
  //used for task dependencies. non zero value indicates the node has dependencies
  ompt_task_id_t m_task_id{0};
  int st_val{0};
  //flag to distinguish an ST-node from an S-node
  bool m_finish_flag;  
  #if FAST_DEP
  std::unordered_map<size_t,char> *dependencies{nullptr};
  #endif 
};

struct violation{
  GraphType m_task1;
  GraphType m_task2;
  void* m_addr;
  bool m_access1;
  bool m_access2;
  bool valid{false};
  std::string loc{"N/A"};
};


class OpenmpRaceDetector{

 private:
  //TODO consider making this a regular bool
  std::atomic<bool>omprace_in_parallel{false};
  //used for debugging or dumping instrument memory traces
  std::ofstream report[NUM_THREADS];
  
  
  /* array storing dpst nodes */
  struct Node* m_tgraph_nodes;
  /* len of m_tgraph_nodes */
  //TODO does this need to be atomic? make this atomic
  GraphType m_last_allocated_node;

  struct Metadata** m_primary_trie;

  /* array of locks to use to access 1st level of trie*/
  pthread_mutex_t* m_primary_trie_lock;
  // keep track of active shadow memory directories for faster cleanup
  std::unordered_map<size_t,bool> active_directories;
  void cleanup_shadow_memory();
  struct violation m_violations[__OMP_RACER_MAX_VIOLATIONS];
  std::atomic<int> m_violations_count{0};
  
  //alignas(64) LRUCache lcaCache[NUM_THREADS];
  alignas(64) std::vector<GraphType> m_last_nodes[NUM_THREADS];
  #if LOCKSET_META
  alignas(64) std::stack<struct LockSet*> m_lockset[NUM_THREADS];
  #endif
  std::vector<GraphType> m_par_finish_index;

  std::map<ompt_task_id_t, GraphType> m_task_map;
  std::unordered_map<ompt_task_id_t, std::vector<TaskDependence>> m_task_dependencies;

  //index to the finish node for current parallel region
  //unsigned int m_par_node_index; this is now a local var
  
  /* 
     Keeps tracks of the number of threads in a team
     gets set at m_parallel_begin
  */
  unsigned int m_num_threads;

  //used to synchornize between parallel end and implicit_task_end
  int m_impl_end_count;

  //new sync solution
  alignas(64) size_t m_barrier_count[NUM_THREADS];

  std::vector<GraphType> m_pending_finish;

  
  //1 denotes it's the first dispatch, 0 otherwise
  alignas(64) int m_first_dispatch[NUM_THREADS];

  #if __OMP_RACER_NESTED
  std::atomic<bool> taskwaits_nested{true};
  #endif

  /*
    Clean up the finish node created for a task_alloc
  */	
  void m_cleanup_finish_node(ompt_thread_id_t);
  
  /* 
    Sets the remaining Node members: m_parent, m_depth, m_seq
    Called right after an m_create_node call
  */
  void m_set_nodeparent_info(GraphType, ompt_thread_id_t);

  // report all the races to the output file
  std::ofstream m_report;
  
public:

  OpenmpRaceDetector();
  ~OpenmpRaceDetector();

  GraphType m_create_node(NodeType,ompt_thread_id_t);
  
  void m_init_racer(ompt_thread_id_t,
		    ompt_parallel_id_t,
		    ompt_task_id_t);

  void m_capture_thread_begin(ompt_thread_id_t);
  void m_capture_thread_end(ompt_thread_id_t);
  
  void m_capture_parallel_begin(ompt_thread_id_t,
				ompt_task_id_t,
				ompt_parallel_id_t,
				uint32_t,
				const char*);
				
  void m_capture_parallel_end(ompt_thread_id_t,
			      ompt_parallel_id_t,
			      ompt_task_id_t);
  
  void m_capture_master_begin(ompt_thread_id_t,
			      ompt_parallel_id_t,
			      ompt_task_id_t,
			      const char*);

  // m_add_step_node should correspond to points where work of the
  // step node is measured with the performance profiler. Hence,
  // m_add_step_node should be roughly same as start_count in
  // ompprof. Also, m_pop_step_node should be roughly equivalent to
  // stop_count 
  void m_add_step_node(ompt_thread_id_t);

  //almost always a pop should be matched by an m_add_step_node before the callback returns
  void m_pop_step_node(ompt_thread_id_t);

  void m_capture_master_end(ompt_thread_id_t,
			    ompt_parallel_id_t,
			    ompt_task_id_t);
  
  void m_capture_single_begin(ompt_thread_id_t,
			      ompt_parallel_id_t,
			      ompt_task_id_t,
			      const char*);
  
  void m_capture_single_end(ompt_thread_id_t,
			    ompt_parallel_id_t,
			    ompt_task_id_t);

  void m_capture_task_begin(ompt_thread_id_t,
			    ompt_parallel_id_t,
			    ompt_task_id_t,
			    ompt_task_id_t);

  void m_capture_task_end(ompt_thread_id_t,
			  ompt_task_id_t);

  void m_capture_implicit_task_begin(ompt_thread_id_t,
				     ompt_parallel_id_t,
				     ompt_task_id_t,
				     const char*);

  void m_capture_implicit_task_end(ompt_thread_id_t,
				   ompt_parallel_id_t,
				   ompt_task_id_t);

  void m_capture_barrier_begin(ompt_thread_id_t,
			       ompt_parallel_id_t,
			       ompt_task_id_t);

  void m_capture_barrier_end(ompt_thread_id_t,
			     ompt_parallel_id_t,
			     ompt_task_id_t);

  void m_capture_taskwait_begin(ompt_thread_id_t,
				ompt_parallel_id_t,
				ompt_task_id_t);

  void m_capture_taskwait_end(ompt_thread_id_t,
			      ompt_parallel_id_t,
			      ompt_task_id_t);

  void m_capture_task_alloc(ompt_thread_id_t,
			    ompt_parallel_id_t,
			    ompt_task_id_t,
			    ompt_task_id_t,
			    void *,
          int,
			    const char*);

  /* handlers for measuring inherent parallelism with work sharing */
  
  void m_capture_loop_begin(ompt_thread_id_t,
			    ompt_parallel_id_t,
			    ompt_task_id_t,
			    const char*);

  void m_capture_loop_end(ompt_thread_id_t,
			  ompt_parallel_id_t,
			  ompt_task_id_t);

  void m_capture_loop_chunk(ompt_thread_id_t);
  
  void m_capture_task_dependences(ompt_thread_id_t, 
        ompt_task_id_t, 
        const ompt_dependence_t*, 
        int);

  int m_get_first_dispatch(ompt_thread_id_t);

  Node* m_get_current_parent(ompt_thread_id_t);

  void m_capture_taskgroup_begin(ompt_thread_id_t,
			       ompt_parallel_id_t,
			       ompt_task_id_t);

  void m_capture_taskgroup_end(ompt_thread_id_t,
			      ompt_parallel_id_t,
			      ompt_task_id_t);
  
  void m_capture_section_begin(ompt_thread_id_t);
  void m_capture_section_end(ompt_thread_id_t);
  void m_fini_race_detector();
  void m_print_dpst();
  void m_print_violations();
  void m_add_violations(void*, GraphType,
			bool, GraphType, bool,const std::string& loc="");

  #if DUMP_INST_MEM
  //void m_instrument_access(uint64_t tid, uint64_t par_id, void* addr, bool type);
  void m_instrument_access(uint64_t tid, uint64_t par_id, void* addr, bool type,const std::string& loc);
  #elif LOC_INFO
  void m_instrument_access(uint64_t tid,void* addr, bool type,const std::string& loc);
  #else
  void m_instrument_access(uint64_t tid, void* addr, bool type);
  #endif
  
  GraphType m_get_curr_step();

  bool m_isparallel(GraphType, GraphType);
  bool m_are_dependent(GraphType, GraphType);

  GraphType m_lca(GraphType, GraphType);

  void m_report_race(struct violation*);
  void clear_metadata(void* start_addr, void* end_addr);
};

#endif /* OPENMP_DPST_H */
