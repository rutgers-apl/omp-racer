#include "openmp_dpst.h"

//Setting these preprocessors is for test purposes
//set to 1 to get log report
#define DEBUG 0

#if MEM_RECLAIM
  //used to keep track of OSPG node id when reclaiming memory. needed if we need to dump ospg for debugging
  GraphType base_id = 0;
#endif
#if STAT
struct Statistics stat_count;
#endif

pthread_mutex_t bar_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t report_map_mutex = PTHREAD_MUTEX_INITIALIZER;
//used to synchronize access to m_tgraph_nodes, the array u
//pthread_mutex_t alloc_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t metadata_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t violation_mutex = PTHREAD_MUTEX_INITIALIZER;

//For direct shadow mapping
static const size_t kShadowBeg     = 0x100000000000ull;
static const size_t kShadowEnd     = 0x240000000000ull;
static const size_t appBase = 0x7f0000000000ull;

static const size_t appMask = 0x00fffffffffcull;

/** @brief create an ospg node on the m_tgraph_nodes mmaped array
 *  @return: returns the index to the created node on the array 
 */
GraphType OpenmpRaceDetector::m_create_node(
  NodeType node_type, 
  ompt_thread_id_t tid)
{
  //pthread_mutex_lock(&alloc_mutex);
  //GraphType index = ++m_last_allocated_node;
  GraphType index = __sync_fetch_and_add(&m_last_allocated_node,1);
  assert(m_last_allocated_node < __OMP_RACER_NUM_GRAPH_NODES);
  m_tgraph_nodes[index].m_type = node_type;
  m_tgraph_nodes[index].m_young_ns_child = NO_TYPE;
  m_tgraph_nodes[index].m_finish_flag = false;
  m_tgraph_nodes[index].m_numchildren = 0;
  if (node_type == NodeType::PARALLEL) {
    m_tgraph_nodes[index].st_val = 1;
  } 
  else {
    m_tgraph_nodes[index].st_val = 0;
  }
  #if DEBUG
  report[tid] << "[m_create_node] node_id=" << index << ", type=" << node_type << std::endl;
  #endif
  //pthread_mutex_unlock(&alloc_mutex);
  
  return index;
}

inline void 
OpenmpRaceDetector::cleanup_shadow_memory()
{
  return;
  //with active directory map optimiziation
  /*for (auto& key_val : active_directories){
    size_t primary_index =  key_val.first;
    struct Metadata* primary_ptr = m_primary_trie[primary_index];
    assert(primary_ptr != NULL);
    size_t sec_length = (SS_SEC_TABLE_ENTRIES) * sizeof(struct Metadata);
    munmap(primary_ptr, sec_length);
    m_primary_trie[primary_index] = NULL;
  }
  active_directories.clear();*/
  size_t sec_length = (SS_SEC_TABLE_ENTRIES) * sizeof(struct Metadata);
  for (size_t i = 0;i<SS_PRIMARY_TABLE_ENTRIES;i++){
    if (m_primary_trie[i] != NULL){
        munmap(m_primary_trie[i], sec_length);
    }
  }

  //TODO make this a precomputed constant
  size_t primary_length = (SS_PRIMARY_TABLE_ENTRIES) * sizeof(struct Metadata*);
  munmap(m_primary_trie, primary_length);
  m_primary_trie = (struct Metadata**)mmap(0, primary_length, PROT_READ| PROT_WRITE,
						__OMP_RACER_MMAP_FLAGS, -1, 0);
  
}

OpenmpRaceDetector::OpenmpRaceDetector(){
  //temporarily commented out as omp calls are not allowed in ompt callbacks
  //which leads to deadlock in llvm 6.0.1
  //int num_procs = omp_get_num_procs();
  //TEMP fix
	int num_procs = NUM_THREADS;
  
  if(NUM_THREADS < num_procs){
    std::cout << "This system supports more cores than the tool has been configured to handle. please update NUM_THREADS in openmp_dpst.h" << std::endl;
    std::cout << "exiting" << std::endl;
    exit(1); 
  }

  m_num_threads = num_procs;
  //creat trace directory for dumping instrument mem calls. cannot be used in debug mode
  #if DUMP_INST_MEM
  const char *path = "./trace";
	const int exist = dirExists(path);
  if (exist ==1){
		std::cout << "trace directory exists" << std::endl;
	}
	else{
		std::cout << "no trace directory found. creating it" << std::endl;
		const int res = createDir(path);
		if (res == 0){
			std::cout << "trace directory created" << std::endl;
		}
		else{
			std::cout << "failed to create trace directory. exiting" << std::endl;
			exit(1);
		}
	}
  #endif
  //create log directory for debug logs
  #if DEBUG
  const char *path = "./log";
	const int exist = dirExists(path);
	if (exist ==1){
		std::cout << "log directory exists" << std::endl;
	}
	else{
		std::cout << "no log directory found. creating it" << std::endl;
		const int res = createDir(path);
		if (res == 0){
			std::cout << "log directory created successfully" << std::endl;
		}
		else{
			std::cout << "failed to create directory exiting" << std::endl;
			exit(1);
		}
	}
  #endif 

  for(unsigned int i = 0; i < NUM_THREADS; i++){
    #if DUMP_INST_MEM
    std::string file_name = "trace/tr_" + std::to_string(i) + ".csv";
		report[i].open(file_name);
    #endif
    #if DEBUG
		std::string file_name = "log/log_" + std::to_string(i) + ".csv";
		report[i].open(file_name);
		#endif
    m_first_dispatch[i] = 0;
    m_barrier_count[i] = 0;
    m_last_nodes[i].reserve(1024);
  }

  
  m_impl_end_count = m_num_threads;

  /* Initializing array based DPST data structures */
  m_last_allocated_node = 1;
  
  //pre allocate ospg nodes using mmap
  size_t length = __OMP_RACER_NUM_GRAPH_NODES * sizeof(struct Node);
  m_tgraph_nodes = (struct Node*)mmap(0, length, PROT_READ|PROT_WRITE, __OMP_RACER_MMAP_FLAGS, -1, 0);
  assert(m_tgraph_nodes != (void*) -1);

  // Allocate space for metadata with each memory address 
  // this mmaps only the first level the shadow memory table
  
  #if TRIESHADOW
  size_t primary_length = (SS_PRIMARY_TABLE_ENTRIES) * sizeof(struct Metadata*);
  m_primary_trie = (struct Metadata**)mmap(0, primary_length, PROT_READ| PROT_WRITE,
						__OMP_RACER_MMAP_FLAGS, -1, 0);
  assert(m_primary_trie != (void *)-1);
  #endif

  // Allocate fine-grained lock for acquiring a page directory in the shadow memory
  // Memory leak here since we don't need to deallocate lock at the end
  //m_primary_trie_lock = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t)*SS_PRIMARY_TABLE_ENTRIES);
  //for (int i=0; i<SS_PRIMARY_TABLE_ENTRIES;i++){
  //  pthread_mutex_init(&m_primary_trie_lock[i], NULL);
  //}

  //pthread_mutex_lock(&violation_mutex);
  m_violations_count = 0;
  //pthread_mutex_unlock(&violation_mutex);
  
  #if DEBUG
  std::cout << "omprace initialized." << std::endl;
  #endif

  #if STAT
  /*for(unsigned int i = 0; i < NUM_THREADS; i++){
    stat_count.m_enter_instrument_mem[i] = 0;
    stat_count.m_exit_instrument_mem[i] = 0;
    stat_count.m_lca_calls[i] = 0;
    stat_count.m_lca_height[i] = 0;
    stat_count.m_shadow_mem_buckets[i] = 0;
  }*/
  //to prevent fp error when not instrumenting memory
  //stat_count.m_lca_calls[0] = 1;
  #endif
}

/** @brief Destructor call. Ensure that it doesn't get called before the program returns
 */
OpenmpRaceDetector::~OpenmpRaceDetector(){
	std::cout << "[OpenmpRaceDetector] dtor called" << std::endl;
}

//TODO fix the lockless implementation
inline void 
OpenmpRaceDetector::m_add_violations(
  void* addr,
  GraphType node1,
  bool type1,
  GraphType node2,
  bool type2,
  const std::string& loc)
{
  
  //this is problematic if m_violations_count overflows. 
  //in practice we shouldn't have that many data races
  int cur_violation = m_violations_count++;
  //do not add anything after the __OMP_RACER_MAX_VIOLATIONS races
  if (cur_violation >= __OMP_RACER_MAX_VIOLATIONS){
    return;
  }
  m_violations[cur_violation].m_addr = addr;
  m_violations[cur_violation].m_task1 = node1;
  m_violations[cur_violation].m_task2 = node2;
  m_violations[cur_violation].m_access1 = type1;
  m_violations[cur_violation].m_access2 = type2;
  m_violations[cur_violation].valid = true;
  if (loc!=""){
    m_violations[cur_violation].loc = loc;
  }


  //uncomement here so that the program reports the first 

  //race detected. useful in finding false positives
  //m_report.open("violations.out");
  //m_report_race(&m_violations[0]);
  //std::cout << "Number of violations = " << m_violations_count << std::endl;
  //m_report.close();
  //for(unsigned int i = 0; i < NUM_THREADS; i++){
  //  report[i].flush();
	//	report[i].close();
  //}
  //exit(1);

  //pthread_mutex_unlock(&violation_mutex);
  return;

  //TODO try implementing a lock free version
  //previous lockfree solution had a datarace
  /*
  pthread_mutex_lock(&violation_mutex);
  if (m_violations_count == 10){
    pthread_mutex_unlock(&violation_mutex);
    return;
  }
    m_violations[m_violations_count].m_addr = addr;
    m_violations[m_violations_count].m_task1 = node1;
    m_violations[m_violations_count].m_task2 = node2;
    m_violations[m_violations_count].m_access1 = type1;
    m_violations[m_violations_count].m_access2 = type2;
    if (loc!=""){
      m_violations[m_violations_count].loc = loc;
    }
    m_violations_count++;
  pthread_mutex_unlock(&violation_mutex);
  */
  //int index = __sync_fetch_and_add(&m_violations_count,1);
  //if (index == 10){
  //  return;
 // }
//  m_violations[index].m_addr = addr;
//  m_violations[index].m_task1 = node1;
//  m_violations[index].m_task2 = node2;
//  m_violations[index].m_access1 = type1;
//  m_violations[index].m_access2 = type2;
//  if (loc!=""){
//    m_violations[m_violations_count].loc = loc;
//  }
  
  //++m_violations_count;
  
  
  // Report only first 10 races;
  //assert(m_violations_count < 10);
}

/* The stack of each thread maintains the list of nodes executing. It
   should only have one step node in its stack and it should be the
   last one. All other nodes on the stack should be PARALLEL or SERIES */

void
OpenmpRaceDetector::m_pop_step_node(ompt_thread_id_t tid){

  GraphType step_index = m_last_nodes[tid].back();
  #if DEBUG
  assert(m_tgraph_nodes[step_index].m_type == WORK);
  report[tid] << "[m_pop_step_node] node_id=" << m_last_nodes[tid].back() << std::endl;
  #endif
  m_last_nodes[tid].pop_back();

}

inline void
OpenmpRaceDetector::m_add_step_node(ompt_thread_id_t tid){

  GraphType step_index = m_create_node(WORK, tid);
  m_set_nodeparent_info(step_index, tid);
  m_last_nodes[tid].push_back(step_index);
}

/** @brief Called to cleanup the finish node associated with a first task spawn
 *  i.e. ST-nodes  on the encountering thread's stack. if any
 */
inline void
OpenmpRaceDetector::m_cleanup_finish_node(ompt_thread_id_t tid){
  #if DEBUG
  assert(!m_last_nodes[tid].empty());
  #endif 
  GraphType leaf_node_index = m_last_nodes[tid].back();
  if(m_tgraph_nodes[leaf_node_index].m_finish_flag == true){
    #if DEBUG
    report[tid] << "[m_cleanup_finish_node] removing st-node id=" 
    << leaf_node_index << ", parent_id = " 
    << m_tgraph_nodes[leaf_node_index].m_parent << std::endl; 
    #endif 
    #if __OMP_RACER_NESTED
    if (m_tgraph_nodes[leaf_node_index].st_val != -1){
      taskwaits_nested = false;
    }
    #endif
    m_last_nodes[tid].pop_back();
  }
}

/** @brief initialize omprace by creating the program's root S-node (finish)
 */ 
void
OpenmpRaceDetector::m_init_racer(ompt_thread_id_t serial_thread_id,
				 ompt_parallel_id_t serial_parallel_id,
				 ompt_task_id_t serial_task_id){


  GraphType index = m_create_node(SERIES, serial_thread_id);
  #if DEBUG
  std::cout << "root index = " << index << std::endl;
  #endif
  m_tgraph_nodes[index].m_seqnum = 0;
  m_tgraph_nodes[index].m_parent = 0;
  m_tgraph_nodes[index].m_depth = 0;
  m_last_nodes[serial_thread_id].push_back(index);
  m_add_step_node(serial_thread_id);

  
}


void OpenmpRaceDetector::m_capture_thread_begin(ompt_thread_id_t tid){
  #if LOCKSET_META
  m_lockset[tid].push(new LockSet());
  #endif
  return;
}

void OpenmpRaceDetector::m_capture_thread_end(ompt_thread_id_t tid){
  return;
}

/** @brief called whenever a new node is created. it's used to
 *  set the new node's parent and update the parent's numchildren
 *  
 *  TODO check if we need to make numchildren atomic
 */
void
OpenmpRaceDetector::m_set_nodeparent_info(GraphType node_index,
					  ompt_thread_id_t tid){

    GraphType parent = m_last_nodes[tid].back();
    m_tgraph_nodes[node_index].m_parent = parent;
    m_tgraph_nodes[node_index].m_seqnum = m_tgraph_nodes[parent].m_numchildren;
    m_tgraph_nodes[node_index].m_depth = m_tgraph_nodes[parent].m_depth + 1;
    
    m_tgraph_nodes[parent].m_numchildren++;
    #if DEBUG
    report[tid] << "[m_set_nodeparent_info] node_id=" << node_index << ", parent_id=" << parent << std::endl;
    #endif

}

/** @brief called when the master thread encouters a new parallel region
 *  master thread pops the current step nodes
 *  master creates two finish nodes and pushes them into its stack
 *  it also sets m_par_finish_index to be used by other threads in implicit_task_begin
 *  
 */
void
OpenmpRaceDetector::m_capture_parallel_begin(
  ompt_thread_id_t tid,
  ompt_task_id_t parent_task_id,
  ompt_parallel_id_t parallel_id,
  uint32_t requested_team_size,
  const char* loc)
{
  
  #if DEBUG
  assert(requested_team_size <= NUM_THREADS);
	report[tid] << "[captureParallelBegin] parallel_id = " << parallel_id << ", parent_task_id = "
	<< parent_task_id << ", requested team size: " << requested_team_size 
	<< ",loc =" << loc <<  std::endl;
  report[tid] << "[captureParallelBegin] start. m_last_nodes[0]: " << m_last_nodes[tid] << std::endl;
	#endif

  m_pop_step_node(tid);
  /* Remove prior ospg nodes by resetting the last_allocated_node index to 2 */
  //std::cout << "m_last_allocate_node = " << m_last_allocated_node << std::endl;
  //std::cout << "root node ="  << m_last_nodes[tid][0] << std::endl;
  #if MEM_RECLAIM && STAT
  //stat_count.m_ospg_nodes_created += m_last_allocated_node;
  #endif 
  #if MEM_RECLAIM
  //stat_count.m_ospg_nodes_created += m_last_allocated_node;
  //pre allocate ospg nodes using mmap
  size_t length = __OMP_RACER_NUM_GRAPH_NODES * sizeof(struct Node);
  munmap(m_tgraph_nodes, length);
  m_tgraph_nodes = (struct Node*)mmap(0, length, PROT_READ|PROT_WRITE, __OMP_RACER_MMAP_FLAGS, -1, 0);
  assert(m_tgraph_nodes != (void*) -1);
  m_last_allocated_node = 0;
  GraphType index = m_create_node(SERIES, tid);
  m_tgraph_nodes[index].m_seqnum = 0;
  m_tgraph_nodes[index].m_parent = 0;
  m_tgraph_nodes[index].m_depth = 0;
  m_last_nodes[tid].clear();
  m_last_nodes[tid].push_back(index);
  cleanup_shadow_memory();
  #endif 
  if (requested_team_size > 0){
    m_num_threads = requested_team_size;
  } else{
    //TODO is this problematic?
    m_num_threads = omp_get_num_procs();
  }

  GraphType finish_index = m_create_node(SERIES, tid);
  /* set the seqnum of the newly created finish node */

  //only root node should be on master's stack
  #if DEBUG
	assert(m_last_nodes[tid].size() == 1);
  #endif
  m_set_nodeparent_info(finish_index, tid);
  m_last_nodes[tid].push_back(finish_index);

  /* Create another finish node to account for multiple barriers
     within a parallel block */

  GraphType new_finish = m_create_node(SERIES, tid);
  m_set_nodeparent_info(new_finish, tid);
  m_last_nodes[tid].push_back(new_finish);

  m_par_finish_index.clear();
  m_par_finish_index.push_back(finish_index);  
  m_par_finish_index.push_back(new_finish);
  //int m_par_node_index = m_last_nodes[tid].size() - 1;

  #if DEBUG
	report[tid] << "[captureParallelBegin] done. m_last_nodes[0]: " << m_last_nodes[tid] << std::endl;
	#endif
  //
  omprace_in_parallel=true;
}

/** @brief called when the master thread exits the parallel region
 *  Master thread: pops the finish node associated with parallel region 
 *  master pushes a step node into its stack
 *  
 */
void
OpenmpRaceDetector:: m_capture_parallel_end(ompt_thread_id_t tid,
					    ompt_parallel_id_t parallel_id,
					    ompt_task_id_t task_id){

  /* The order of operations for parallel directive is the following:
     parallel begin, implicit begin, barrier begin/end, implicit end,
     parallel_end */

  //not true anymore
  /* A busy waiting loop to make sure parallel end happens after all
     the implicit task ends have occurred. This is necessary so that
     the upcoming parallel begin doesn't try to update last_nodes of
     other threads while they are still working in the previous
     parallel block (especially with back to back parallel directives) */
  
  
  #if DEBUG
	report[tid] << "[captureParallelEnd] start : popping parallel finish. node_id= " << m_last_nodes[tid].back() << std::endl;
	#endif 
  
  m_last_nodes[tid].pop_back();
  
  //add the new step node
  m_add_step_node(tid);
  
  #if DEBUG
	report[tid] << "[captureParallelEnd] done : parallel_id = " << parallel_id 
	<< ", task_id = "<< task_id << std::endl; 
	#endif
  omprace_in_parallel=false;
}

/** @brief Master thread pops the current step node and pushes new one
 *  
 */
void
OpenmpRaceDetector:: m_capture_master_begin(ompt_thread_id_t tid,
					    ompt_parallel_id_t parallel_id,
					    ompt_task_id_t task_id,
					    const char * loc){
  
  
  #if DEBUG
	report[tid] << "[captureMasterBegin] start : parallel_id = " << parallel_id 
	<< ", task_id = " << task_id << ", loc =" << loc << std::endl;
	#endif

  m_pop_step_node(tid);
  //GraphType finish_index = m_create_node(SERIES,tid);
  //m_set_nodeparent_info(finish_index, tid);
  //m_last_nodes[tid].push_back(finish_index);
  
  m_add_step_node(tid);
  #if DEBUG
	report[tid]<< "[captureMasterBegin] done : m_last_nodes = "  << m_last_nodes[tid]  
	<< std::endl;
	#endif
}

/** @brief Master thread pops the current step node and pushes new one
 *  
 */
void
OpenmpRaceDetector::m_capture_master_end(ompt_thread_id_t tid,
					 ompt_parallel_id_t parallel_id,
					 ompt_task_id_t task_id){

  
  #if DEBUG
	report[tid] << "[captureMasterEnd] start : parallel_id = " << parallel_id 
	<< ", task_id = " << task_id << std::endl;
	#endif 

  m_pop_step_node(tid);
  #if DEBUG
  assert(!m_last_nodes[tid].empty());
  #endif
  //m_cleanup_finish_node(tid);
  //m_last_nodes[tid].pop_back();

  m_add_step_node(tid);

  #if DEBUG
	report[tid]<< "[captureMasterEnd] done : m_last_nodes = "  << m_last_nodes[tid]  
	<< std::endl;
	#endif
}

/** @brief single thread pops the current step node and pushes new one
 *  
 */
void
OpenmpRaceDetector::m_capture_single_begin(ompt_thread_id_t tid,
					   ompt_parallel_id_t parallel_id,
					   ompt_task_id_t task_id,
					   const char* loc){

  #if DEBUG
	report[tid] << "[captureSingleBegin] start : parallel_id = " << parallel_id 
	<< ", task_id = " << task_id << ", loc =" << loc << std::endl;
	#endif

  //m_pop_step_node(tid);
  //GraphType finish_index = m_create_node(SERIES,tid);
  //m_set_nodeparent_info(finish_index, tid);
  //m_last_nodes[tid].push_back(finish_index);
  
  //m_add_step_node(tid);
  #if DEBUG
	report[tid]<< "[captureSingleBegin] done : m_last_nodes = "  << m_last_nodes[tid]  
	<< std::endl;
	#endif
}

/** @brief Single thread pops the current step node and pushes new one
 *  
 */
void OpenmpRaceDetector:: m_capture_single_end(ompt_thread_id_t tid,
					       ompt_parallel_id_t parallel_id,
					       ompt_task_id_t task_id){

  #if DEBUG
	report[tid] << "[captureSingleEnd] start : parallel_id = " << parallel_id 
	<< ", task_id = " << task_id << std::endl;
	#endif 

  //m_pop_step_node(tid);
  //#if DEBUG
  //assert(!m_last_nodes[tid].empty());
  //#endif
  //
  //m_cleanup_finish_node(tid);
  //m_last_nodes[tid].pop_back();

  //m_add_step_node(tid);

  #if DEBUG
	report[tid]<< "[captureSingleEnd] done : m_last_nodes = "  << m_last_nodes[tid]  
	<< std::endl;
	#endif
}


//Every thread gets a call back for loop begin. There is an implicit
//async node associated with this thread when omp parallel is
//executed. Hence, iterations executed by different threads will be in
//parallel. 

// To handle dynamic scheduling, each chunk adds an async node and it
// is pushed to to the current stack. When we get a new chunk, we need
// to pop the async node that was pushed by the previous chunk except for the first chunk. 

void
OpenmpRaceDetector::m_capture_loop_begin(ompt_thread_id_t tid,
					 ompt_parallel_id_t parallel_id,
					 ompt_task_id_t task_id,
					 const char* loc){

	#if DEBUG
	report[tid] << "[captureLoopBegin] start : parallel_id = " << parallel_id << ", task_id = " 
	<< task_id << "loc = " << loc  << std::endl;
	#endif
	
  //m_pop_step_node(tid);

  m_first_dispatch[tid] = 1;

  /* FIXME: We need to push an async node if the omp for has a no wait
     clause */
  //TODO remove the finish node I don't think its needed
  //GraphType finish_index = m_create_node(SERIES, tid);
  //m_set_nodeparent_info(finish_index, tid);
  //m_last_nodes[tid].push_back(finish_index);

  //m_add_step_node(tid);
  #if DEBUG
	report[tid] << "[captureLoopBegin] done " << std::endl;
	#endif
}


void
OpenmpRaceDetector::m_capture_loop_end(ompt_thread_id_t tid,
				       ompt_parallel_id_t parallel_id,
				       ompt_task_id_t task_id){
  
  #if DEBUG
	report[tid] << "[captureLoopEnd] start : parallel_id = " << parallel_id << ", task_id = " 
	<< task_id << std::endl;
	#endif

  if (m_first_dispatch[tid] == 1){//indicates we had a static loop
    #if DEBUG
	  report[tid] << "[captureLoopEnd] static done" << std::endl;
	  #endif
    return;
  }
  //we have a dynamic loop
  //pop the step and async node assoricated with the last chunk
  m_pop_step_node(tid);
  m_last_nodes[tid].pop_back();
  m_add_step_node(tid);
  
  #if DEBUG
	report[tid] << "[captureLoopEnd] done" << std::endl;
	#endif

}
/*  In llvm 8.0.1 we don't receive the callback with the last flag set to 0 
*   to indicate so the logic to handle the last chunk is moved to m_capture_loop_end 
*/
void
OpenmpRaceDetector::m_capture_loop_chunk(ompt_thread_id_t tid){
    
  //std::cout<< "[captureLoopChunk] start: tid = " << tid << ", last = " << last << std::endl; 
  #if DEBUG
	report[tid] << "[captureLoopChunk] start: tid = " << tid << std::endl; 
	#endif
  if(m_first_dispatch[tid] == 1){
    m_first_dispatch[tid] = 0;
    m_pop_step_node(tid);
  }
  else{
    m_pop_step_node(tid);
    m_last_nodes[tid].pop_back();
  }
  //if(last != 0){
    
    GraphType async_index = m_create_node(PARALLEL, tid);
    m_set_nodeparent_info(async_index, tid);
    m_last_nodes[tid].push_back(async_index);
    m_add_step_node(tid);
  //}
  //else{
  //  m_add_step_node(tid);
  //}
  #if DEBUG
	report[tid] << "[captureLoopChunk] end: tid = " << tid << std::endl; 
	#endif  
}

void 
OpenmpRaceDetector::m_capture_section_begin(ompt_thread_id_t tid){
  m_pop_step_node(tid);
  GraphType async_index = m_create_node(PARALLEL, tid);
  m_set_nodeparent_info(async_index, tid);
  m_last_nodes[tid].push_back(async_index);
  m_add_step_node(tid);
}

void 
OpenmpRaceDetector::m_capture_section_end(ompt_thread_id_t tid){
  m_pop_step_node(tid);
  m_last_nodes[tid].pop_back();
  m_add_step_node(tid);
}

//record the task dependencies of the task
void 
OpenmpRaceDetector::m_capture_task_dependences(
        ompt_thread_id_t tid, 
        ompt_task_id_t task_id, 
        const ompt_dependence_t *deps, 
        int ndeps)
{

  //TODO: memory leak. reclaim memory upon task end
  std::unordered_map<size_t,char>* new_map = new std::unordered_map<size_t,char>(16);
  
  for (int i=0; i<ndeps; i++){
    auto d_type = static_cast<ompt_dependence_type_t>(deps[i].dependence_type);
    size_t d_addr = deps[i].variable.value;
    new_map->insert(std::pair<size_t, char>(d_addr, (char)d_type));
  }
  std::vector<TaskDependence> dependence_v; 
  for (int i=0; i<ndeps; i++){
    
    auto d_flag = static_cast<ompt_dependence_type_t>(deps[i].dependence_type);
    #if DEBUG 
    report[tid] << "pushing dependency variable addr = " <<  deps[i].variable.value << "type = " 
    << d_flag << std::endl;
    #endif
    auto td =TaskDependence(deps[i].variable.value,d_flag);
    dependence_v.push_back(td);
  }
  #if DEBUG
	report[tid] << "[captureTaskDependence] done : new_task_id = " <<  task_id 
	<< std::endl;
	#endif
  
  pthread_mutex_lock(&report_map_mutex);
  #if DEBUG
  assert(m_task_map.find(task_id) != m_task_map.end());
  #endif
  GraphType async_node_index = m_task_map[task_id];
  Node async_node =  m_tgraph_nodes[async_node_index];
  
  assert(async_node.dependencies == nullptr);
  async_node.dependencies = new_map;
  pthread_mutex_unlock(&report_map_mutex);

  m_task_dependencies.insert(std::pair<ompt_task_id_t, std::vector<TaskDependence>>(task_id, dependence_v));
}


/** @brief called when an explicit task gets scheduled and starts running
 *  push the async node associated with explicit task
 */
void
OpenmpRaceDetector::m_capture_task_begin(
          ompt_thread_id_t tid,
					ompt_parallel_id_t parallel_id,
					ompt_task_id_t parent_task_id,
					ompt_task_id_t new_task_id){
	
  #if DEBUG
	report[tid] << "[captureTaskBegin] start : new_task_id = " <<  new_task_id 
	<< ", parallel id = " << parallel_id << ", parent_task_id: " << parent_task_id 
	<< std::endl;
	#endif
  
  pthread_mutex_lock(&report_map_mutex);
  #if DEBUG
  assert(m_task_map.find(new_task_id) != m_task_map.end());
  #endif
  GraphType async_node_index = m_task_map[new_task_id];
  m_task_map.erase(new_task_id);
  pthread_mutex_unlock(&report_map_mutex);
  m_last_nodes[tid].push_back(async_node_index);

  m_add_step_node(tid);
	#if DEBUG
	report[tid] << "[captureTaskBegin] done" << std::endl;
	#endif
}

/** @brief called when an explicit finishes execution
 * 	pop the async node associated with explicit task 
 */
void
OpenmpRaceDetector::m_capture_task_end(ompt_thread_id_t tid,
				       ompt_task_id_t task_id){
  #if DEBUG
  assert(!m_last_nodes[tid].empty());
  report[tid] << "[captureTaskEnd] start. m_last_nodes.size=" << m_last_nodes[tid].size() 
	<< ", m_last_nodes: " << m_last_nodes[tid] << std::endl;
  #endif

  
  m_pop_step_node(tid);
  //cleanup st-node if the current thread ran the task as an included task
  //TODO what happens if there is a taskwait here?
  m_cleanup_finish_node(tid);
  m_last_nodes[tid].pop_back();

  #if DEBUG
	report[tid] << "[captureTaskEnd] done. m_last_nodes.size=" << m_last_nodes[tid].size() 
	<< ", m_last_nodes: " << m_last_nodes[tid] << std::endl;
	#endif
}

/** @brief called when an implicit task starts execution
 * 	master thread: push async node
 * 	Other threads: push finish node associated with parallel region and current barrier to stack
 * 	then push async node
 */
void
OpenmpRaceDetector::m_capture_implicit_task_begin(
            ompt_thread_id_t tid,
						ompt_parallel_id_t parallel_id,
						ompt_task_id_t task_id,
						const char* loc){

  #if DEBUG
	report[tid] << "[captureImplicitTaskBegin] start parallel_id = " << parallel_id 
  << ", task_id= " << task_id << std::endl;
	#endif
	
	#if DEBUG
	report[tid] << "[captureImplicitTaskBegin] m_last_nodes size = " << m_last_nodes[tid].size() 
	<< " , m_last_nodes= " << m_last_nodes[tid] 
	<< ", m_last_nodes[0]= " << m_last_nodes[0] << std::endl;
	#endif

  //need to synchronize here to make sure the master is not already at the next parallel region
  //thus removing the Finish nodes associated with the previous parallel region from par_finish_index
	//before other worker threads can update their stack
  if (tid == 0){
		while(1){
			int count_tmp = __sync_fetch_and_add(&m_impl_end_count,0);
			if (count_tmp<m_num_threads){
				continue;
			}
			__sync_fetch_and_and(&m_impl_end_count,0);
			break;
		}
	}
  else{
    pthread_mutex_lock(&report_map_mutex);
		
    #if DEBUG
		assert(m_last_nodes[tid].size() == 0);
		assert(m_par_finish_index.size() == 2);
    #endif 
		
    GraphType grand_parent_index = m_par_finish_index.at(0);
		m_last_nodes[tid].push_back(grand_parent_index);
		GraphType parent_index = m_par_finish_index.at(1);
		m_last_nodes[tid].push_back(parent_index);
		pthread_mutex_unlock(&report_map_mutex);
  }

  GraphType async_index = m_create_node(PARALLEL, tid);
  m_set_nodeparent_info(async_index, tid);
  m_last_nodes[tid].push_back(async_index);

  m_add_step_node(tid);
  
  #if DEBUG
	report[tid] << "[captureImplicitTaskBegin] done. after async push: m_last_nodes.size() = " << m_last_nodes[tid].size() 
	<< " , m_last_nodes " << m_last_nodes[tid] << std::endl;
	#endif
}

/**	@brief called when an implicit task finishes execution
 *  all threads: pop step node. pop async node representing current parallel work by the thread. 
 *  pop finish node associated with current barrier
 * 	other threads: additionally pop the finish node associated with the current parallel region
 */
void
OpenmpRaceDetector:: m_capture_implicit_task_end(
            ompt_thread_id_t tid,
						ompt_parallel_id_t parallel_id,
						ompt_task_id_t task_id){

  #if DEBUG
	report[tid] << "[captureImplicitTaskEnd] start parallel_id = " 
  << parallel_id << ", task_id = " << task_id << std::endl;
	#endif

  m_pop_step_node(tid);
  #if DEBUG
  assert(!m_last_nodes[tid].empty());
  #endif
  //m_cleanup_finish_node(tid);
	#if DEBUG
	report[tid] << "[captureImplicitTaskEnd]. before trying to pop finish and async nodes m_lastNodes.size() = " 
	<< m_last_nodes[tid].size() << ", m_last_nodes: " << m_last_nodes[tid] << std::endl;
	#endif
	//pop async representing implicit task
	m_last_nodes[tid].pop_back();
	//pop finish associated with current barrier
	m_last_nodes[tid].pop_back();

	//pop finish associated with current parallel region
	if (tid!=0){
    #if DEBUG
		assert(!m_last_nodes[tid].empty());
    #endif
		//last_nodes[tid].pop_back();
		m_last_nodes[tid].pop_back();
	}

	int count_tmp = __sync_add_and_fetch(&m_impl_end_count,1);
	#if DEBUG
	report[tid] << "implEndCount = " << count_tmp << std::endl;
	#endif

	#if DEBUG
	report[tid] << "[captureImplicitTaskEnd]. done m_lastNodes.size() = " 
	<< m_last_nodes[tid].size() << ", m_last_nodes: " << m_last_nodes[tid] << std::endl;
	#endif
	#if DEBUG
	report[tid] << "[captureImplicitTaskEnd] done" << std::endl;
	#endif

}

/** @brief called when a barrier is encountered by current thread
 *  An openmp barrier is a schedling point where the thread becomes ready to execute spawned tasks
 *  all threads: pop async node and finish node associated with the region before the barrier
 *  afterwards: first thread reaching the barrier creates finish node assocaited with serial relation
 *  between continuation and work prior to barrier
 *  all threads will push new finish and per thread async
 */
void
OpenmpRaceDetector::m_capture_barrier_begin(
            ompt_thread_id_t tid,
					  ompt_parallel_id_t parallel_id,
					  ompt_task_id_t task_id)
{


  #if DEBUG
	report[tid] << "[captureBarrierBegin] start parallel_id = " << parallel_id 
	<< ", task_id = " << task_id << std::endl;
	#endif

  m_pop_step_node(tid);

  m_cleanup_finish_node(tid);
  // The last two nodes on the thread's stack have to be P-node (async) and S-node (finish)
  #if DEBUG
  assert(m_tgraph_nodes[m_last_nodes[tid].back()].m_type == NodeType::PARALLEL);
  #endif
  m_last_nodes[tid].pop_back();
  #if DEBUG
  //TODO why is this failing sometimes?
  //assert(m_tgraph_nodes[m_last_nodes[tid].back()].m_type == NodeType::SERIES);
  #endif
  m_last_nodes[tid].pop_back();

  /* m_barrier_count[i] counts the number of barriers encountered in
     the parallel block by thread i. If the number of barriers
     encountered by other threads is more than the current barrier
     count, then the thread to reach the barrier first has already
     created a finish node and has pushed it into pending_finish */

  pthread_mutex_lock(&bar_mutex);
  m_barrier_count[tid] += 1;
  int max_count = 0;
  for (int i = 0; i < m_num_threads; i++){
    /* get the count of barriers encountered by other threads */
    if(i == tid) continue;

    if(m_barrier_count[i] > max_count){
      max_count = m_barrier_count[i];
    }
  }

  if(m_barrier_count[tid] > max_count){
    // this is the first thread visting the barrier
    #if DEBUG
    assert(m_pending_finish.size() + 1 == m_barrier_count[tid]);
    #endif
    GraphType finish_index = m_create_node(SERIES, tid);
    m_set_nodeparent_info(finish_index, tid);
    m_last_nodes[tid].push_back(finish_index);
    m_pending_finish.push_back(finish_index);
    #if DEBUG
		report[tid] << "barrier begin first thread pushing finish. node_id = " << finish_index << std::endl;
		#endif
  }
  else {
    #if DEBUG
    assert(m_pending_finish.size() >= m_barrier_count[tid]);
    #endif
    GraphType index = m_barrier_count[tid] -1 ;
    GraphType pend_fini_node = m_pending_finish.at(index);
    #if DEBUG
		report[tid] << "barrier begin thread pushing finish. node_id = " << pend_fini_node << std::endl;
		#endif
    m_last_nodes[tid].push_back(pend_fini_node);
  }

  GraphType async_index = m_create_node(PARALLEL, tid);
  m_set_nodeparent_info(async_index, tid);
  m_last_nodes[tid].push_back(async_index);
  pthread_mutex_unlock(&bar_mutex);

  #if DEBUG
	report[tid] << "[captureBarrierBegin] done. m_last_nodes size = " << m_last_nodes[tid].size() 
	<< ", m_last_nodes: " << m_last_nodes[tid] << std::endl;
	#endif
}

/** @brief called when current thread exits the barrier
 *  all threads push new step node
 * 
 */
void
OpenmpRaceDetector:: m_capture_barrier_end(
            ompt_thread_id_t tid,
					  ompt_parallel_id_t parallel_id,
					  ompt_task_id_t task_id)
{
    #if DEBUG
	  report[tid] << "[captureBarrierEnd] start parallel_id = " << parallel_id 
	  << ", task_id = " << task_id << std::endl;
	  #endif
    m_add_step_node(tid);

}

void
OpenmpRaceDetector::m_capture_taskwait_begin(
  ompt_thread_id_t tid,
  ompt_parallel_id_t parallel_id,
  ompt_task_id_t task_id)
{
  #if DEBUG
  report[tid]<< "[captureTaskwaitBegin] start : parallel_id = " << parallel_id 
  << ", task_id= " << task_id << std::endl;
  #endif

  //assert(!m_last_nodes[tid].empty());
  //GraphType top = m_last_nodes[tid].back();
  /* a wrong use of taskwait where the programmer calls taskwait
     without spawning any tasks */
  
  m_pop_step_node(tid);
  #if DEBUG
  report[tid]<< "[captureTaskwaitBegin] done : m_last_nodes = "  
  << m_last_nodes[tid]  << std::endl;
  #endif
}

void
OpenmpRaceDetector::m_capture_taskwait_end(
  ompt_thread_id_t tid,
  ompt_parallel_id_t parallel_id,
  ompt_task_id_t task_id)
{
  #if DEBUG
  assert(!m_last_nodes[tid].empty());
  report[tid] << "[captureTaskwaitEnd] start : parallel_id = " 
	<< parallel_id << ", task_id = " << task_id << std::endl;
  #endif

  if (m_tgraph_nodes[m_last_nodes[tid].back()].m_finish_flag == true){
    #if DEBUG 
    report[tid] << "[captureTaskwaitEnd] setting st-node id=" 
    << m_last_nodes[tid].back() << " st_val to -1" << std::endl;
    #endif
    m_tgraph_nodes[m_last_nodes[tid].back()].st_val = -1;
    m_cleanup_finish_node(tid);
  }
  
  m_add_step_node(tid);

  #if DEBUG
  report[tid] << "[captureTaskwaitEnd] done : parallel_id = " 
  << parallel_id << ", task_id = " << task_id << std::endl;
  #endif
}

void
OpenmpRaceDetector::m_capture_task_alloc(
          ompt_thread_id_t tid,
					ompt_parallel_id_t parallel_id,
					ompt_task_id_t parent_task_id,
					ompt_task_id_t new_task_id,
					void *new_task_function,
          int has_dependences,
					const char* loc){

  #if DEBUG
	report[tid] << "[captureTaskAlloc] start : new_task_id = " <<  new_task_id 
	<< ", parallel id = " << parallel_id << ", parent_task_id = " << parent_task_id 
	<< std::endl;
	#endif

  m_pop_step_node(tid);
  // If the new node to be created already has an immediate async
  // sibling, then there is no need to create a implicit finish node

  GraphType top = m_last_nodes[tid].back();

  if(m_tgraph_nodes[top].m_young_ns_child == PARALLEL){
    GraphType async_index = m_create_node(PARALLEL, tid);
    m_set_nodeparent_info(async_index, tid);
    //set m_task_id if task has dependenes
    if (has_dependences){
      m_tgraph_nodes[async_index].m_task_id=new_task_id;
    }
    pthread_mutex_lock(&report_map_mutex);
    m_task_map[new_task_id] = async_index;
    
    
    pthread_mutex_unlock(&report_map_mutex);
  }
  else{
    GraphType new_finish = m_create_node(SERIES, tid);
    m_set_nodeparent_info(new_finish, tid);
    m_tgraph_nodes[new_finish].m_young_ns_child = PARALLEL;
    m_tgraph_nodes[new_finish].m_finish_flag = true;
    m_last_nodes[tid].push_back(new_finish);

    GraphType new_async = m_create_node(PARALLEL, tid);
    //set m_task_id if task has dependenes
    if (has_dependences){
      m_tgraph_nodes[new_async].m_task_id=new_task_id;
    }
    m_set_nodeparent_info(new_async, tid);

    pthread_mutex_lock(&report_map_mutex);
    m_task_map[new_task_id] = new_async;
    pthread_mutex_unlock(&report_map_mutex);
  }
  
  //the w-node (step) represents the continuation of the parent task
  m_add_step_node(tid);
  #if DEBUG
	report[tid] << "[captureTaskAlloc] done" << std::endl;
	#endif
}

//TODO fix output format
void OpenmpRaceDetector::m_print_dpst(){

  std::ofstream report_int_nodes;
  std::ofstream report_step_nodes;
  
  report_int_nodes.open("log/tree_0.csv");
  report_step_nodes.open("log/step_work_0.csv");

  //loop through all nodes. Root node allocated at index 1. Hence loop starts from 1

  for (GraphType i = 1; i < m_last_allocated_node; i++) {
    Node& node = m_tgraph_nodes[i];

    if (node.m_type == PARALLEL || node.m_type == SERIES) {
      report_int_nodes << i << "," << node.m_type << "," << node.m_parent << std::endl;
    } else if (node.m_type == WORK) {      
      report_step_nodes << i << "," << node.m_parent << std::endl;
    } else {
      std::cout << "Task node with invalid type node id =" << i << "type= " << node.m_type << std::endl;
    }
  }
  report_int_nodes.close();
  report_step_nodes.close();

}

// m_get_curr_step returns the index to the step node that is
// currently executing on the running thread
// return the top node on the m_last_nodes stack
 
inline GraphType
OpenmpRaceDetector::m_get_curr_step(){

  // Invariant: ompt_thread_id_t is omp_get_thread_num
  // header issue forcing us to use omp_get_thread_num. To make this
  // robust, I should use a map between ompt_thread_ids and
  // omp_thread_num to make the mapping explicit.
  
  
  ompt_thread_id_t tid = omp_get_thread_num();

  //ompt_thread_id_t tid = ompt_get_thread_id();
  GraphType curr_step = m_last_nodes[tid].back();

  //important invariant that the top of the stack is always the current step node
  #if DEBUG
  assert(m_tgraph_nodes[curr_step].m_type == WORK);
  #endif
  return curr_step;
}


void OpenmpRaceDetector::m_capture_taskgroup_begin(
            ompt_thread_id_t tid,
					  ompt_parallel_id_t parallel_id,
					  ompt_task_id_t task_id)
{
  
  #if DEBUG
	report[tid] << "[captureTaskgroupBegin] start. parallel_id = " << parallel_id 
	<< ", task_id = " << task_id << std::endl;
	#endif

  m_pop_step_node(tid);

  m_cleanup_finish_node(tid);
  
  //now we should be within a work-sharing construct
  #if DEBUG
  assert(m_tgraph_nodes[m_last_nodes[tid].back()].m_type == NodeType::PARALLEL);
  #endif
  
  GraphType new_finish = m_create_node(SERIES, tid);
  m_set_nodeparent_info(new_finish, tid);
  
  m_last_nodes[tid].push_back(new_finish);

  //the w-node (step) represents the first fragment of work within the taskgroup
  m_add_step_node(tid);

  #if DEBUG
	report[tid] << "[captureTaskgroupBegin] done. m_last_nodes size = " << m_last_nodes[tid].size() 
	<< ", m_last_nodes: " << m_last_nodes[tid] << std::endl;
	#endif
}

void OpenmpRaceDetector::m_capture_taskgroup_end(
            ompt_thread_id_t tid,
					  ompt_parallel_id_t parallel_id,
					  ompt_task_id_t task_id)
{    
  
  #if DEBUG
	report[tid] << "[captureTaskgroupEnd] start. parallel_id = " << parallel_id 
	<< ", task_id = " << task_id << std::endl;
	#endif

  m_pop_step_node(tid);
  //may have st-nodes due to task spawns within the taskgroup
  m_cleanup_finish_node(tid);
  //pop the s-node (finish) that was created at the taskgroup begin
  #if DEBUG
  assert(m_tgraph_nodes[m_last_nodes[tid].back()].m_type == NodeType::SERIES);
  #endif
  report[tid] << "taskgroup popping finish node_id=" << m_last_nodes[tid].back() << std::endl;
  m_last_nodes[tid].pop_back();

  //the w-node (step) represents the first fragment of work after the taskgroup
  m_add_step_node(tid);

  #if DEBUG
	report[tid] << "[captureTaskgroupEnd] done. m_last_nodes size = " << m_last_nodes[tid].size() 
	<< ", m_last_nodes: " << m_last_nodes[tid] << std::endl;
	#endif
      
}
  

void OpenmpRaceDetector::m_fini_race_detector(){

  m_print_violations();
  #if DUMP_INST_MEM
  for(unsigned int i = 0; i < NUM_THREADS; i++){
    report[i].flush();
		report[i].close();
  }
  #endif 
  #if DEBUG
  //std::cout<<"Total allocated nodes:"<<m_last_allocated_node<<std::endl;;
  m_print_dpst();
  for(unsigned int i = 0; i < NUM_THREADS; i++){
    report[i].flush();
		report[i].close();
  }
  #endif

  #if __OMP_RACER_NESTED
  if (taskwaits_nested.load() == true){
    std::cout << "All taskwaits nested = True" << std::endl;
  }
  else{
    std::cout << "All taskwaits nested = False" << std::endl;
  }
  #endif
}

GraphType
OpenmpRaceDetector::m_lca(GraphType curr_task,
			   GraphType remote_task){

  GraphType new_curr_task = curr_task;
  GraphType new_remote_task = remote_task;

  if(m_tgraph_nodes[new_curr_task].m_depth >
     m_tgraph_nodes[new_remote_task].m_depth) {

    while(m_tgraph_nodes[new_curr_task].m_depth !=
	  m_tgraph_nodes[new_remote_task].m_depth){

      new_curr_task = m_tgraph_nodes[new_curr_task].m_parent;
    }
  }
  else if(m_tgraph_nodes[new_remote_task].m_depth >
	  m_tgraph_nodes[new_curr_task].m_depth) {
    new_remote_task = m_tgraph_nodes[new_remote_task].m_parent;
  }

  while(new_remote_task != new_curr_task){
    new_remote_task = m_tgraph_nodes[new_remote_task].m_parent;
    new_curr_task = m_tgraph_nodes[new_curr_task].m_parent;
  }
  return new_curr_task;
}

inline bool
OpenmpRaceDetector::m_isparallel(GraphType curr_step, GraphType meta_access){
  #if NOLCA
  return false;
  #endif
  //0 indicates that the meta data is not set yet. 
  //Hence, cur_step is the first access to the current address
  assert(curr_step != 0);
  if(meta_access == 0 || (meta_access == curr_step))
    return false;

  //check cache first make it work in the presence of dependencies
  //perhaps make the cache per thread if contention is an issue
  //int cache_res;
  //pthread_mutex_lock(&cache_mutex);
  //cache_res = lcaCache[tid].get(make_pair(curr_step, meta_access));
  //pthread_mutex_unlock(&cache_mutex);
  
  /*if (cache_res == 1){
    #if STAT
    stat_count.pt_stat[tid].m_lca_cache_hit++;
    #endif 
    return true;
  }
  else if (cache_res == 0){
    return false;
  }*/
  //otherwise cache_res is -1 which indicates a miss
  #if STAT
  int tid = omp_get_thread_num();
  stat_count.pt_stat[tid].m_lca_cache_miss++;
  stat_count.pt_stat[tid].m_lca_calls++;
  #endif
  
  // Check if the two nodes are at the same depth
  GraphType curr_depth = m_tgraph_nodes[curr_step].m_depth;
  GraphType meta_depth = m_tgraph_nodes[meta_access].m_depth;
  int cur_count = 0;
  int remote_count = 0;
  GraphType new_curr_task = curr_step;
  GraphType new_remote_task = meta_access;
  GraphType prev_curr_task = 0;
  GraphType prev_remote_task = 0;
 
  
  if(curr_depth > meta_depth){
    while(m_tgraph_nodes[new_curr_task].m_depth != m_tgraph_nodes[new_remote_task].m_depth){
      prev_curr_task = new_curr_task;
      cur_count += m_tgraph_nodes[new_curr_task].st_val;
      new_curr_task = m_tgraph_nodes[new_curr_task].m_parent;

      #if STAT
      stat_count.pt_stat[tid].m_lca_height++;
      #endif 
    }
  }
  else if (meta_depth > curr_depth){
    while(m_tgraph_nodes[new_remote_task].m_depth != m_tgraph_nodes[new_curr_task].m_depth){
      prev_remote_task = new_remote_task;
      remote_count += m_tgraph_nodes[new_remote_task].st_val;
      new_remote_task = m_tgraph_nodes[new_remote_task].m_parent;
      #if STAT
      stat_count.pt_stat[tid].m_lca_height++;
      #endif 
    }
  }

  while(new_remote_task != new_curr_task){
    prev_remote_task = new_remote_task;
    remote_count += m_tgraph_nodes[new_remote_task].st_val;
    new_remote_task = m_tgraph_nodes[new_remote_task].m_parent;
    prev_curr_task = new_curr_task;
    cur_count += m_tgraph_nodes[new_curr_task].st_val;
    new_curr_task = m_tgraph_nodes[new_curr_task].m_parent;   
    #if STAT
    stat_count.pt_stat[tid].m_lca_height+=2;
    #endif 
  }

  if (prev_curr_task == 0){
    std::cout << "should not happen" << std::endl;
  }

  // Look at the child of the LCA 
  size_t curr_seq = m_tgraph_nodes[prev_curr_task].m_seqnum;
  size_t rem_seq = m_tgraph_nodes[prev_remote_task].m_seqnum;
  bool ret_val = false;

  //remote_task is left child
  if(rem_seq < curr_seq){
    //adjust the st_val sum when left w-node parent is an st-node
    GraphType w_l_parent = m_tgraph_nodes[meta_access].m_parent;
    if (m_tgraph_nodes[w_l_parent].m_finish_flag == true) {
      remote_count -= m_tgraph_nodes[w_l_parent].st_val;
    }
    assert(prev_remote_task != 0);
    if(m_tgraph_nodes[prev_remote_task].m_type == WORK ||
      (m_tgraph_nodes[prev_remote_task].m_type == SERIES && m_tgraph_nodes[prev_remote_task].m_finish_flag == false)) {
      ret_val = false;
    }
    else if(m_tgraph_nodes[prev_remote_task].m_type == PARALLEL){
      if(m_tgraph_nodes[prev_curr_task].m_type == PARALLEL && 
         remote_count == 1 && 
         m_tgraph_nodes[new_remote_task].m_finish_flag == true &&
         m_are_dependent(prev_curr_task, prev_remote_task)) {
        ret_val = false;
      }
      ret_val = true;
    }
    else{//left child of lca is an st-node
      if (remote_count > 0){
        ret_val = true;
      }
      ret_val = false;
    }
  }
  else {
    //adjust the st_val sum when left w-node parent is an st-node
    GraphType w_l_parent = m_tgraph_nodes[curr_step].m_parent;
    if (m_tgraph_nodes[w_l_parent].m_finish_flag == true) {
      cur_count -= m_tgraph_nodes[w_l_parent].st_val;
    }
    assert(prev_curr_task != 0);
    if(m_tgraph_nodes[prev_curr_task].m_type == WORK ||
      (m_tgraph_nodes[prev_curr_task].m_type == SERIES && m_tgraph_nodes[prev_curr_task].m_finish_flag == false)) {
      ret_val = false;
    }
    else if(m_tgraph_nodes[prev_curr_task].m_type == PARALLEL){
      if(m_tgraph_nodes[prev_remote_task].m_type == PARALLEL &&
         cur_count == 1 &&
         m_tgraph_nodes[new_curr_task].m_finish_flag == true &&
         m_are_dependent(prev_remote_task, prev_curr_task)){
        ret_val = false;
      }
      ret_val = true;
    }
    else{//left child of lca is an st-node
      if (cur_count > 0){
        ret_val = true;
      }
      ret_val = false;
    }

  }
  //pthread_mutex_lock(&cache_mutex);
  //lcaCache[tid].put(make_pair(curr_step, meta_access), ret_val ? 1 : 0);
  //pthread_mutex_unlock(&cache_mutex);
  return ret_val;
}

inline bool 
OpenmpRaceDetector::m_are_dependent(GraphType curr_async, GraphType left_async){
  //both nodes have to have dependencies to be dependent
  auto left_task_id = m_tgraph_nodes[left_async].m_task_id;
  auto cur_task_id = m_tgraph_nodes[curr_async].m_task_id;
  if (left_task_id == 0 || cur_task_id == 0){
    return false;
  }
  auto l_deps = m_task_dependencies.at(left_task_id); 
  auto c_deps = m_task_dependencies.at(cur_task_id); 
  for(auto c_dep: c_deps){
    for(auto l_dep: l_deps){
      // first check that they operate on the same addr
      if(c_dep.addr == l_dep.addr){
        if((c_dep.dep_type == ompt_dependence_type_t::ompt_dependence_type_inout) || 
           (c_dep.dep_type == ompt_dependence_type_t::ompt_dependence_type_out)){
              //regardless of left nodes dependency type we have a dependency
              return true;
            }
            else if(c_dep.dep_type == ompt_dependence_type_t::ompt_dependence_type_in){
              if((l_dep.dep_type == ompt_dependence_type_t::ompt_dependence_type_inout) || 
                 (l_dep.dep_type == ompt_dependence_type_t::ompt_dependence_type_out)){
                return true;
              }
            }
            else{
              std::cout << "[in function HasDependence] unexpected dependency type. exiting" << std::endl;
              exit(1);
            }
          }
      }
  }  
  return false;
}

//this version uses lockset
#if LOCKSET_META
void 
OpenmpRaceDetector::m_instrument_access(uint64_t tid,void* addr, bool type)
{
  #if BASELINE
  return;
  #endif 
  
  // do not instrument if datarace detector is not in a parallel region
  if (!omprace_in_parallel.load())
    return;

  size_t saddr = (size_t) addr;
  
  #if STAT
  stat_count.pt_stat[tid].m_enter_instrument_mem++;
  #endif
  
  #if DUMP_INST_MEM
    report[tid] << addr << "," << type << "," << par_id << std::endl;
  #endif 

  
  size_t cur_lockset = (m_lockset[tid].top())->getLockset();
  
  // Maintain metadata per word (i.e., 4 bytes) 
  //assert(saddr>=appBase);
  
  
  saddr = saddr >> 2;
  size_t primary_index = (saddr >> 24) & 0x3fffff;

  struct Metadata* primary_ptr = m_primary_trie[primary_index];
  if(primary_ptr == NULL){
    // Allocate secondary entry on demand 

    size_t sec_length = (SS_SEC_TABLE_ENTRIES) * sizeof(struct Metadata);
    //since sizeof(struct Metadata) == 16
    //size_t sec_length = (SS_SEC_TABLE_ENTRIES) << 4;
    struct Metadata* new_primary_ptr = (struct Metadata *)mmap(0, sec_length, PROT_READ| PROT_WRITE,
						MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);

    //first thread will succeed and return true
    if(cas_gcc<Metadata*>(&primary_ptr, 0, new_primary_ptr)){//first thread will eval to true
      //TODO Examine:this next line is redundant, right?
      m_primary_trie[primary_index] = primary_ptr;  
      //Sanity check that the mmap is at page boundary
      //assert(((size_t)primary_ptr % 4096) == 0);
      #if MEM_RECLAIM
      //std::cout << "inserting into active directories, primary_index = " << primary_index << std::endl;
      //active_directories.insert(std::pair<size_t, bool>(primary_index, true));
      #endif
      #if STAT
      stat_count.pt_stat[tid].m_shadow_mem_buckets++;
      #endif
    }
    else{//others should unmap otherwise we'll have memory leak
      munmap(new_primary_ptr, sec_length);
      //munmap(primary_ptr, sec_length);
    }
  
  }

  //24bits offset
  size_t offset = (saddr) & 0xffffff;
  struct Metadata* addr_vec = &primary_ptr[offset];


  #if DEBUG
  assert(addr_vec != NULL);
  #endif
  
  GraphType curr_step = m_last_nodes[tid].back();

  pthread_mutex_lock(&(addr_vec->metadata_lock));
  if (addr_vec->entries[0].lock_set == 0){
    if (type == READ){
      (addr_vec->entries[0]).lock_set = cur_lockset;
      (addr_vec->entries[0]).read1 = curr_step;
    }
    else{
      (addr_vec->entries[0]).lock_set = cur_lockset;
      (addr_vec->entries[0]).write1 = curr_step;
    }
    //addr_vec->entry_count++;
  }
  else{
    //if (addr_vec->entry_count <= NUM_META_ENTRIES){
      //check for data race 

      bool race_detected = false;
      int entry_insert_index = -1;
      for (int i=0 ; i < NUM_META_ENTRIES; i++){
        //why?
        if(race_detected && entry_insert_index != -1) break;

        struct MetadataEntry& cur_entry = addr_vec->entries[i];

        if (entry_insert_index == -1 && cur_entry.lock_set == 0) {
	        entry_insert_index = i;
	        break;
        }
        //empty lockset intersection
        if ((cur_entry.lock_set != 0) && (((~cur_lockset) & (~cur_entry.lock_set)) == 0)){
	      //check for data race
	        if (cur_entry.write1 != 0 && m_isparallel(curr_step, cur_entry.write1)){
	            m_add_violations(addr, curr_step, WRITE, cur_entry.write1, WRITE, "loc");
	            race_detected = true;
	            break;
	        }
          if (cur_entry.write2 != 0 && m_isparallel(curr_step, cur_entry.write2)){
	            m_add_violations(addr, curr_step, WRITE, cur_entry.write2, WRITE, "loc");
	            race_detected = true;
	            break;
	        }

          if(type==WRITE){
            if (cur_entry.read1 != 0 && m_isparallel(curr_step, cur_entry.read1)){
	            m_add_violations(addr, curr_step, WRITE, cur_entry.read1, READ, "loc");
	            race_detected = true;
	            break;
	          }
            if (cur_entry.read2 != 0 && m_isparallel(curr_step, cur_entry.read2)){
	            m_add_violations(addr, curr_step, WRITE, cur_entry.read2, READ, "loc");
	            race_detected = true;
	            break;
	          }


          }
	      }
        if (cur_entry.lock_set != 0 && cur_lockset == cur_entry.lock_set){
	        entry_insert_index = i;
        }      

      }
      if (!race_detected){
        int list_insert_index = -1;
        //now check entry linked list
        std::vector<struct MetadataEntry>* access_list = addr_vec->entry_list;
        if (access_list != nullptr){
          for (std::vector<struct MetadataEntry>::iterator it=access_list->begin();it!=access_list->end(); ++it) {
            struct MetadataEntry& l_entry = *it;
            //check if lockset intersection is empty
            if (((~cur_lockset) & (~l_entry.lock_set)) == 0){
              if (l_entry.write1 != 0 && m_isparallel(curr_step, l_entry.write1)){
	              m_add_violations(addr, curr_step, WRITE, l_entry.write1, WRITE, "loc");
	              race_detected = true;
	              break;
	            }
              if (l_entry.write2 != 0 && m_isparallel(curr_step, l_entry.write2)){
                m_add_violations(addr, curr_step, WRITE, l_entry.write2, WRITE, "loc");
                race_detected = true;
                break;
	            }
      
              if(type==WRITE){
                if (l_entry.read1 != 0 && m_isparallel(curr_step, l_entry.read1)){
                  m_add_violations(addr, curr_step, WRITE, l_entry.read1, READ, "loc");
                  race_detected = true;
                  break;
                }
                if (l_entry.read2 != 0 && m_isparallel(curr_step, l_entry.read2)){
                  m_add_violations(addr, curr_step, WRITE, l_entry.read2, READ, "loc");
                  race_detected = true;
                  break;
                }
              }
            }
            if (cur_lockset == l_entry.lock_set){
              list_insert_index = it - access_list->begin();
            }
          }
        }
        
        if(!race_detected){
          //check if we can insert into fast entries
          if (entry_insert_index!=-1){
            struct MetadataEntry& cur_entry = addr_vec->entries[entry_insert_index];
            if(cur_entry.lock_set == 0){
              cur_entry.lock_set = cur_lockset;
              if(type == READ){
                cur_entry.read1 = curr_step;
              }
              else{
                cur_entry.write1 = curr_step;
              }
            }
            else{
              if(type == WRITE){
                bool par_w1 = false, par_w2 = false;
                if (cur_entry.write1){
                  par_w1 = m_isparallel(curr_step, cur_entry.write1);
                }
                if (cur_entry.write2){
                  par_w2 = m_isparallel(curr_step, cur_entry.write2);
                }
                
                //current write is series to prior 2 writes
                if ((cur_entry.write1 == 0 && cur_entry.write2 == 0) ||
		            (cur_entry.write1 == 0 && !par_w2) ||
		            (cur_entry.write2 == 0 && !par_w1) ||
		            (!par_w1 && !par_w2)) {
                  cur_entry.write1 = curr_step;
                  cur_entry.write2 = 0;
                }
                //current write is parallel to prior 2 writes
                else if (par_w1 && par_w2){
                  GraphType lca12 = m_lca(cur_entry.write1, cur_entry.write2);
                  GraphType lca1s = m_lca(cur_entry.write1, curr_step);
                  
                  if (m_tgraph_nodes[lca1s].m_depth < m_tgraph_nodes[lca12].m_depth ){
                    cur_entry.write1 = curr_step;
                  }
                  else if (cur_entry.write2 == 0 && par_w1) {
		                cur_entry.write2 = curr_step;
	                }
                }
              }
              else{//read access
                bool par_r1 = false, par_r2 = false;
                if (cur_entry.read1){
                  par_r1 = m_isparallel(curr_step, cur_entry.read1);
                }
                if (cur_entry.read2){
                  par_r2 = m_isparallel(curr_step, cur_entry.read2);
                }

                //current read is series to prior 2 reads
                if ((cur_entry.read1 == 0 && cur_entry.read2 == 0) ||
		            (cur_entry.read1 == 0 && !par_r2) ||
		            (cur_entry.read2 == 0 && !par_r1) ||
		            (!par_r1 && !par_r2)) {
                  cur_entry.read1 = curr_step;
                  cur_entry.read2 = 0;
                }
                //current read is parallel to prior 2 read
                else if (par_r1 && par_r2){
                  GraphType lca12 = m_lca(cur_entry.read1, cur_entry.read2);
                  GraphType lca1s = m_lca(cur_entry.read1, curr_step);
                  
                  if (m_tgraph_nodes[lca1s].m_depth < m_tgraph_nodes[lca12].m_depth ){
                    cur_entry.read1 = curr_step;
                  }
                  else if (cur_entry.read2 == 0 && par_r1) {
		                cur_entry.read2 = curr_step;
	                }
                }
              }
            }
          }
          else{
            //lockset not found in list
            if (list_insert_index == -1){
              std::vector<struct MetadataEntry>* access_list = addr_vec->entry_list;
              if (access_list == nullptr) {
	              addr_vec->entry_list = new std::vector<struct MetadataEntry>();
	              access_list = addr_vec->entry_list;
	            }
              struct MetadataEntry new_meta_entry;
              new_meta_entry.lock_set = cur_lockset;
              if (type == READ) {
                new_meta_entry.read1 = curr_step;	      
              } else {
                new_meta_entry.write1 = curr_step;
              }
	            // add to access history
	            access_list->push_back(new_meta_entry);
            }
            //there exists metadata in the entry_list with current lockset
            else{
              struct MetadataEntry& update_list_entry = access_list->at(list_insert_index);

              if (type == WRITE){
                bool par_w1 = false, par_w2 = false;
                if (update_list_entry.write1){
                  par_w1 = m_isparallel(curr_step, update_list_entry.write1);
                }
                if (update_list_entry.write2){
                  par_w2 = m_isparallel(curr_step, update_list_entry.write2);
                }
                
                //current write is series to prior 2 writes
                if ((update_list_entry.write1 == 0 && update_list_entry.write2 == 0) ||
		            (update_list_entry.write1 == 0 && !par_w2) ||
		            (update_list_entry.write2 == 0 && !par_w1) ||
		            (!par_w1 && !par_w2)) {
                  update_list_entry.write1 = curr_step;
                  update_list_entry.write2 = 0;
                }
                //current write is parallel to prior 2 writes
                else if (par_w1 && par_w2){
                  GraphType lca12 = m_lca(update_list_entry.write1, update_list_entry.write2);
                  GraphType lca1s = m_lca(update_list_entry.write1, curr_step);
                  
                  if (m_tgraph_nodes[lca1s].m_depth < m_tgraph_nodes[lca12].m_depth ){
                    update_list_entry.write1 = curr_step;
                  }
                  else if (update_list_entry.write2 == 0 && par_w1) {
		                update_list_entry.write2 = curr_step;
	                }
                }
              }
              else{//read access 
                bool par_r1 = false, par_r2 = false;
                if (update_list_entry.read1){
                  par_r1 = m_isparallel(curr_step, update_list_entry.read1);
                }
                if (update_list_entry.read2){
                  par_r2 = m_isparallel(curr_step, update_list_entry.read2);
                }

                //current read is series to prior 2 reads
                if ((update_list_entry.read1 == 0 && update_list_entry.read2 == 0) ||
		            (update_list_entry.read1 == 0 && !par_r2) ||
		            (update_list_entry.read2 == 0 && !par_r1) ||
		            (!par_r1 && !par_r2)) {
                  update_list_entry.read1 = curr_step;
                  update_list_entry.read2 = 0;
                }
                //current read is parallel to prior 2 read
                else if (par_r1 && par_r2){
                  GraphType lca12 = m_lca(update_list_entry.read1, update_list_entry.read2);
                  GraphType lca1s = m_lca(update_list_entry.read1, curr_step);
                  
                  if (m_tgraph_nodes[lca1s].m_depth < m_tgraph_nodes[lca12].m_depth ){
                    update_list_entry.read1 = curr_step;
                  }
                  else if (update_list_entry.read2 == 0 && par_r1) {
		                update_list_entry.read2 = curr_step;
	                }
                }

              }

            }

          }

        }
        
      }

    }
  pthread_mutex_unlock(&(addr_vec->metadata_lock));
  return;
}
#else
//this version is lockfree and uses atomics
#if DUMP_INST_MEM
void 
//OpenmpRaceDetector::m_instrument_access(uint64_t tid, uint64_t par_id, void* addr, bool type)
OpenmpRaceDetector::m_instrument_access(uint64_t tid,uint64_t par_id,void* addr, bool type,const std::string& loc)
#elif LOC_INFO
void 
OpenmpRaceDetector::m_instrument_access(uint64_t tid,void* addr, bool type,const std::string& loc)
#else
void
OpenmpRaceDetector::m_instrument_access(uint64_t tid,void* addr, bool type)
#endif
{
  #if BASELINE
  return;
  #elif  __OMP_RACER_NESTED
  return;
  #endif

  // do not instrument if datarace detector is not in a parallel region
  if (!omprace_in_parallel.load())
    return;

  size_t saddr = (size_t) addr;
  
  #if STAT
  stat_count.pt_stat[tid].m_enter_instrument_mem++;
  #endif
  
  #if DUMP_INST_MEM
    report[tid] << addr << "," << type << "," << par_id << std::endl;
  #endif 


  // Maintain metadata per word (i.e., 4 bytes) 
  //assert(saddr>=appBase);
  
  #if TRIESHADOW
  saddr = saddr >> 2;
  size_t primary_index = (saddr >> 24) & 0x3fffff;

  struct Metadata* primary_ptr = m_primary_trie[primary_index];
  if(primary_ptr == NULL){
    // Allocate secondary entry on demand 

    size_t sec_length = (SS_SEC_TABLE_ENTRIES) * sizeof(struct Metadata);
    //since sizeof(struct Metadata) == 16
    //size_t sec_length = (SS_SEC_TABLE_ENTRIES) << 4;
    struct Metadata* new_primary_ptr = (struct Metadata *)mmap(0, sec_length, PROT_READ| PROT_WRITE,
						MAP_PRIVATE|MAP_ANONYMOUS|MAP_NORESERVE, -1, 0);

    //first thread will succeed and return true
    if(cas_gcc<Metadata*>(&primary_ptr, 0, new_primary_ptr)){//first thread will eval to true
      //TODO Examine:this next line is redundant, right?
      m_primary_trie[primary_index] = primary_ptr;  
      //Sanity check that the mmap is at page boundary
      //assert(((size_t)primary_ptr % 4096) == 0);
      #if MEM_RECLAIM
      //std::cout << "inserting into active directories, primary_index = " << primary_index << std::endl;
      //active_directories.insert(std::pair<size_t, bool>(primary_index, true));
      #endif
      #if STAT
      stat_count.pt_stat[tid].m_shadow_mem_buckets++;
      #endif
    }
    else{//others should unmap otherwise we'll have memory leak
      munmap(new_primary_ptr, sec_length);
      //munmap(primary_ptr, sec_length);
    }
  
  }

  #endif

  #if DIRECTSHADOW
  size_t target = ((((size_t)saddr & appMask) << 4) + kShadowBeg);
  struct Metadata* addr_vec = (struct Metadata*) target;
  #elif TRIESHADOW
  //24bits offset
  size_t offset = (saddr) & 0xffffff;
  struct Metadata* meta = &primary_ptr[offset];
  #endif

  #if DEBUG
  assert(meta != NULL);
  #endif
  //TODO optimize get_curr_step
  //  GraphType curr_step = m_get_curr_step();
  
  //pthread_mutex_lock(&metadata_mutex);
  GraphType cur_w_node = m_last_nodes[tid].back();
  

  pthread_mutex_lock(&(meta->metadata_lock));
  //iterate over the first level (fast) metadata entries
  for (int i=0; i < meta->entry_count; ++i){
    GraphType write1 = meta->entries[i].write1;
    GraphType read1 = meta->entries[i].read1;
    GraphType read2 = meta->entries[i].read2;
    if ((write1) && (m_isparallel(cur_w_node, write1))) {
      //report race
      #if LOC_INFO
      m_add_violations(addr, cur_w_node, type, write1, WRITE, loc);  
      #else
      m_add_violations(addr, cur_w_node, type, write1, WRITE);  
      #endif
    }
    //if current access is a write we need to compare with two prior parallel reads
    if (type == WRITE) {
      if ((read1) && (m_isparallel(cur_w_node, read1))) {
        #if LOC_INFO
        m_add_violations(addr, cur_w_node, WRITE, read1, READ, loc);
        #else
        m_add_violations(addr, cur_w_node, WRITE, read1, READ);
        #endif
        //report race
      }
      if ((read2) && (m_isparallel(cur_w_node, read2))) {
        #if LOC_INFO
        m_add_violations(addr, cur_w_node, WRITE, read2, READ, loc);
        #else
        m_add_violations(addr, cur_w_node, WRITE, read2, READ);
        #endif
        //report race
      }
    }
  }
  //iterate over the second level (slow) entries
  if (meta->entry_map != nullptr){
    for (auto& elem: *(meta->entry_map)){
      if ((elem.second.write1) && (m_isparallel(cur_w_node, elem.second.write1))) {
        //report race
        #if LOC_INFO
        m_add_violations(addr, cur_w_node, type, elem.second.write1, WRITE, loc);  
        #else
        m_add_violations(addr, cur_w_node, type, elem.second.write1, WRITE);  
        #endif
      }
      //if current access is a write we need to compare with two prior parallel reads
      if (type == WRITE) {
        if ((elem.second.read1) && (m_isparallel(cur_w_node, elem.second.read1))) {
          #if LOC_INFO
          m_add_violations(addr, cur_w_node, WRITE, elem.second.read1, READ, loc);
          #else
          m_add_violations(addr, cur_w_node, WRITE, elem.second.read1, READ);
          #endif
          //report race
        }
        if ((elem.second.read2) && (m_isparallel(cur_w_node, elem.second.read2))) {
          #if LOC_INFO
          m_add_violations(addr, cur_w_node, WRITE, elem.second.read2, READ, loc);
          #else
          m_add_violations(addr, cur_w_node, WRITE, elem.second.read2, READ);
          #endif
          //report race
        }
      }  
    }
  }

  //update the metadata for the program
  //maintain invariant that root node (program) is always the first entry in 
  //the metadata
  if (meta->entry_count == 0) {
    meta->entry_count+=1;
  }
  
  //find st_nodes on the path to parent
  std::unordered_map<GraphType, bool> st_nodes;
  GraphType parent_index = m_tgraph_nodes[cur_w_node].m_parent;
  while (parent_index != ROOT_INDEX){
    if (m_tgraph_nodes[parent_index].m_finish_flag == true){
      st_nodes[parent_index] = true;
    }
    parent_index = m_tgraph_nodes[parent_index].m_parent;
  }
  //update first level entries ST-nodes
  for(int i=0; i < meta->entry_count; ++i) {
    if (type == WRITE) {
      meta->entries[i].write1 = cur_w_node;
    }
    else {// a read
      if (meta->entries[i].read1 == 0) {  
        meta->entries[i].read1= cur_w_node;
      } 
      else if (meta->entries[i].read2 == 0) {
        meta->entries[i].read2= cur_w_node;
      }
      //is this safe or should it be read again?
      bool cur_r1 = m_isparallel(cur_w_node, meta->entries[i].read1);
      bool cur_r2 = m_isparallel(cur_w_node, meta->entries[i].read2);
      if (!cur_r1 && !cur_r2) {
        meta->entries[i].read1 = cur_w_node;
        meta->entries[i].read2 = 0;
      } 
      else if (cur_r1 && cur_r2) {
        GraphType lca12 = m_lca(meta->entries[i].read1, meta->entries[i].read2);
        GraphType lca1s = m_lca(meta->entries[i].read1, cur_w_node);
        if (m_tgraph_nodes[lca1s].m_depth < m_tgraph_nodes[lca12].m_depth) {
          meta->entries[i].read1 = cur_w_node;        
        }
      }
    }
    if (st_nodes.find(meta->entries[i].id) != st_nodes.end()){
      //update entry in place and remove from st-nodes
      st_nodes.erase(meta->entries[i].id);
    }
  }

  if (st_nodes.size() > 0) {
    int remaining_entries = st_nodes.size();
    int first_lvl_free = NUM_META_ENTRIES - meta->entry_count;
    int start_index = meta->entry_count;
    if (first_lvl_free > 0){
      for (int i = start_index; i < NUM_META_ENTRIES; ++i) {
        if (remaining_entries == 0){//no entries left, done
          break;
        }
        GraphType new_entry_index = (*st_nodes.begin()).first;
        //promote entry from level 2 to level 1
        if ((meta->entry_map != nullptr) && 
            (meta->entry_map->find(new_entry_index) != meta->entry_map->end())) {
              meta->entries[i].id = new_entry_index;
              meta->entries[i].write1 = meta->entry_map->at(new_entry_index).write1;
              meta->entries[i].read1 = meta->entry_map->at(new_entry_index).read1;
              meta->entries[i].read2 = meta->entry_map->at(new_entry_index).read2;
              meta->entry_map->erase(new_entry_index);
        }
        if (type == WRITE) {
          meta->entries[i].write1 = cur_w_node;
        }
        else {// a read
          if (meta->entries[i].read1 == 0) {  
            meta->entries[i].read1= cur_w_node;
          } 
          else if (meta->entries[i].read2 == 0) {
            meta->entries[i].read2= cur_w_node;
          }
          //is this safe or should it be read again?
          bool cur_r1 = m_isparallel(cur_w_node, meta->entries[i].read1);
          bool cur_r2 = m_isparallel(cur_w_node, meta->entries[i].read2);
          if (!cur_r1 && !cur_r2) {
            meta->entries[i].read1 = cur_w_node;
            meta->entries[i].read2 = 0;
          }  
          else if (cur_r1 && cur_r2) {
            GraphType lca12 = m_lca(meta->entries[i].read1, meta->entries[i].read2);
            GraphType lca1s = m_lca(meta->entries[i].read1, cur_w_node);
            if (m_tgraph_nodes[lca1s].m_depth < m_tgraph_nodes[lca12].m_depth) {
              meta->entries[i].read1 = cur_w_node;        
            }
          }
        }
        st_nodes.erase(new_entry_index);
        meta->entry_count+=1;
        remaining_entries-=1;
      }
    }
  }
  //we've exhausted first level entries, put remaining st-nodes in second level
  if (st_nodes.size() > 0) {
    assert(meta->entry_count == NUM_META_ENTRIES);
    if (meta->entry_map == nullptr){
      meta->entry_map = new std::unordered_map<GraphType,struct MetadataEntry>();
    }
    while((st_nodes.size() > 0)) {
      GraphType new_entry_index = (*st_nodes.begin()).first;
      //insert new entry to the map
      if (meta->entry_map->find(new_entry_index) == meta->entry_map->end()){
        struct MetadataEntry e1{new_entry_index,0,0,0};
        meta->entry_map->insert(std::pair<GraphType,struct MetadataEntry>(new_entry_index, e1));
      }

      if (type == WRITE) {
        meta->entry_map->at(new_entry_index).write1 = cur_w_node;
      }
      else {// a read
        if (meta->entry_map->at(new_entry_index).read1 == 0) {  
          meta->entry_map->at(new_entry_index).read1= cur_w_node;
        } 
        else if (meta->entry_map->at(new_entry_index).read2 == 0) {
          meta->entry_map->at(new_entry_index).read2= cur_w_node;
        }
        
        bool cur_r1 = m_isparallel(cur_w_node, meta->entry_map->at(new_entry_index).read1);
        bool cur_r2 = m_isparallel(cur_w_node, meta->entry_map->at(new_entry_index).read2);
        if (!cur_r1 && !cur_r2) {
          meta->entry_map->at(new_entry_index).read1 = cur_w_node;
          meta->entry_map->at(new_entry_index).read2 = 0;
        }  
        else if (cur_r1 && cur_r2) {
          GraphType lca12 = m_lca(meta->entry_map->at(new_entry_index).read1, 
                                  meta->entry_map->at(new_entry_index).read2);
          GraphType lca1s = m_lca(meta->entry_map->at(new_entry_index).read1, 
                                  cur_w_node);
          if (m_tgraph_nodes[lca1s].m_depth < m_tgraph_nodes[lca12].m_depth) {
            meta->entry_map->at(new_entry_index).read1 = cur_w_node;        
          }
        }
      }
      st_nodes.erase(new_entry_index);
    }
    
  }
  
  pthread_mutex_unlock(&(meta->metadata_lock));
  #if STAT
    stat_count.pt_stat[tid].m_exit_instrument_mem++;
  #endif
  return;
}

#endif

void
OpenmpRaceDetector::clear_metadata(void* start_addr, void* end_addr){
  
  for(size_t saddr = (size_t) start_addr; saddr<(size_t)end_addr; saddr+=4){
    size_t saddr_c = saddr >> 2;
    size_t primary_index = (saddr_c >> 24) & 0x3fffff;
    struct Metadata* primary_ptr = m_primary_trie[primary_index];
    
    //this assertion holds when instrument_access is called
    //assert (primary_ptr!=NULL);
    //for now since we want to have runs with no instrument_access (debugging and ospg creation costs)
    //we return if primary_ptr is NULL
    if (primary_ptr == NULL){
      return;
    }
    size_t offset = (saddr_c) & 0xffffff;
    struct Metadata* metadata_entry = &primary_ptr[offset];
    //clear out entry
    #if LOCKSET_META
    //TODO
    #elif PRECISE_META
    //TODO if needed
    #else
    metadata_entry->setRead12Write(0,0,0);
    #endif
  }

}

void
OpenmpRaceDetector::m_report_race(struct violation* viol){

  m_report << "*******************************" << std::endl;
  m_report << "Data race detected at addr = " << viol->m_addr << " **\n";
  m_report << "location = " << viol->loc << " **\n";
  m_report << "access 1 step id: "<< viol->m_task1 << (viol->m_access1 == 0? " Read": " Write") << "\n";
  m_report << "access 2 step id: "<< viol->m_task2 << (viol->m_access2 == 0? " Read": " Write") << "\n";
  m_report << "*******************************" << std::endl;
}


void
OpenmpRaceDetector:: m_print_violations(){

  m_report.open("violations.out");
  int count = 0;
  for (int i = 0; i < __OMP_RACER_MAX_VIOLATIONS; i++){
    count++;
    if (!m_violations[i].valid) {
      break;
    }
    m_report_race(&m_violations[i]);
  }
  std::cout << "Number of violations = " << m_violations_count << std::endl;
  m_report.close();
  #if STAT
  stat_count.m_ospg_nodes_created += m_last_allocated_node;
  stat_count.display();
  #endif
}