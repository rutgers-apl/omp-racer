#ifndef _BSD_SOURCE
#define _BSD_SOURCE
#endif
#define _DEFAULT_SOURCE
#include <stdio.h>
#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS
#endif
//TODO check if this is being used anywhere
#include <map>

#include <inttypes.h>
#include <omp.h>
#include <omp-tools.h>
#include "openmp_dpst.h"

// Used to detect architecture
#include "kmp_platform.h"

// Set to 1 to print debug logs
#define DEBUG 0

static const char* ompt_thread_t_values[] = {
  NULL,
  "ompt_thread_initial",
  "ompt_thread_worker",
  "ompt_thread_other"
};

//new task status values starting detach and removal of ompt_task_others == 4 
static const char* ompt_task_status_t_values[] = {
  NULL,
  "ompt_task_complete",       // 1
  "ompt_task_yield",          // 2
  "ompt_task_cancel",         // 3
  "ompt_task_detach",         // 4
  "ompt_task_early_fulfill",  // 5
  "ompt_task_late_fulfill",   // 6
  "ompt_task_switch"          // 7
};
//ompt_cancel_do -> ompt_cancel_loop
static const char* ompt_cancel_flag_t_values[] = {
  "ompt_cancel_parallel",
  "ompt_cancel_sections",
  "ompt_cancel_loop",
  "ompt_cancel_taskgroup",
  "ompt_cancel_activated",
  "ompt_cancel_detected",
  "ompt_cancel_discarded_task"
};

//TODO does may need updating with llvm 10
static const char* ompt_dependence_type_t_values[] = {
  NULL,
  "ompt_dependence_type_in",            //1
  "ompt_dependence_type_out",           //2
  "ompt_dependence_type_inout",         //3
  "ompt_dependence_type_mutexinoutset", //4
  "ompt_dependence_type_source",        //5
  "ompt_dependence_type_sink"           //6
};

static void format_task_type(int type, char *buffer) {
  char *progress = buffer;
  if (type & ompt_task_initial)
    progress += sprintf(progress, "ompt_task_initial");
  if (type & ompt_task_implicit)
    progress += sprintf(progress, "ompt_task_implicit");
  if (type & ompt_task_explicit)
    progress += sprintf(progress, "ompt_task_explicit");
  if (type & ompt_task_target)
    progress += sprintf(progress, "ompt_task_target");
  if (type & ompt_task_undeferred)
    progress += sprintf(progress, "|ompt_task_undeferred");
  if (type & ompt_task_untied)
    progress += sprintf(progress, "|ompt_task_untied");
  if (type & ompt_task_final)
    progress += sprintf(progress, "|ompt_task_final");
  if (type & ompt_task_mergeable)
    progress += sprintf(progress, "|ompt_task_mergeable");
  if (type & ompt_task_merged)
    progress += sprintf(progress, "|ompt_task_merged");
}

static ompt_set_callback_t ompt_set_callback;
static ompt_get_callback_t ompt_get_callback;
static ompt_get_state_t ompt_get_state;
static ompt_get_task_info_t ompt_get_task_info;
static ompt_get_task_memory_t ompt_get_task_memory;
static ompt_get_thread_data_t ompt_get_thread_data;
static ompt_get_parallel_info_t ompt_get_parallel_info;
static ompt_get_unique_id_t ompt_get_unique_id;
static ompt_finalize_tool_t ompt_finalize_tool;
static ompt_get_num_procs_t ompt_get_num_procs;
static ompt_get_num_places_t ompt_get_num_places;
static ompt_get_place_proc_ids_t ompt_get_place_proc_ids;
static ompt_get_place_num_t ompt_get_place_num;
static ompt_get_partition_place_nums_t ompt_get_partition_place_nums;
static ompt_get_proc_id_t ompt_get_proc_id;
static ompt_enumerate_states_t ompt_enumerate_states;
static ompt_enumerate_mutex_impls_t ompt_enumerate_mutex_impls;

static void print_ids(int level)
{
  int task_type, thread_num;
  ompt_frame_t *frame;
  ompt_data_t *task_parallel_data;
  ompt_data_t *task_data;
  int exists_task = ompt_get_task_info(level, &task_type, &task_data, &frame,
                                       &task_parallel_data, &thread_num);
  char buffer[2048];
  format_task_type(task_type, buffer);
  if (frame)
    printf("%" PRIu64 ": task level %d: parallel_id=%" PRIu64
           ", task_id=%" PRIu64 ", exit_frame=%p, reenter_frame=%p, "
           "task_type=%s=%d, thread_num=%d\n",
           ompt_get_thread_data()->value, level,
           exists_task ? task_parallel_data->value : 0,
           exists_task ? task_data->value : 0, frame->exit_frame.ptr,
           frame->enter_frame.ptr, buffer, task_type, thread_num);
}

#define get_frame_address(level) __builtin_frame_address(level)

#define print_frame(level)                                                     \
  printf("%" PRIu64 ": __builtin_frame_address(%d)=%p\n",                      \
         ompt_get_thread_data()->value, level, get_frame_address(level))

// clang (version 5.0 and above) adds an intermediate function call with debug flag (-g)
#if defined(TEST_NEED_PRINT_FRAME_FROM_OUTLINED_FN)
  #if defined(DEBUG) && defined(__clang__) && __clang_major__ >= 5
    #define print_frame_from_outlined_fn(level) print_frame(level+1)
  #else
    #define print_frame_from_outlined_fn(level) print_frame(level)
  #endif

  #if defined(__clang__) && __clang_major__ >= 5
    #warning "Clang 5.0 and later add an additional wrapper for outlined functions when compiling with debug information."
    #warning "Please define -DDEBUG iff you manually pass in -g to make the tests succeed!"
  #endif
#endif

// This macro helps to define a label at the current position that can be used
// to get the current address in the code.
//
// For print_current_address():
//   To reliably determine the offset between the address of the label and the
//   actual return address, we insert a NOP instruction as a jump target as the
//   compiler would otherwise insert an instruction that we can't control. The
//   instruction length is target dependent and is explained below.
//
// (The empty block between "#pragma omp ..." and the __asm__ statement is a
// workaround for a bug in the Intel Compiler.)
#define define_ompt_label(id) \
  {} \
  __asm__("nop"); \
ompt_label_##id:

// This macro helps to get the address of a label that is inserted by the above
// macro define_ompt_label(). The address is obtained with a GNU extension
// (&&label) that has been tested with gcc, clang and icc.
#define get_ompt_label_address(id) (&& ompt_label_##id)

// This macro prints the exact address that a previously called runtime function
// returns to.
#define print_current_address(id) \
  define_ompt_label(id) \
  print_possible_return_addresses(get_ompt_label_address(id))

#if KMP_ARCH_X86 || KMP_ARCH_X86_64
// On X86 the NOP instruction is 1 byte long. In addition, the compiler inserts
// a MOV instruction for non-void runtime functions which is 3 bytes long.
#define print_possible_return_addresses(addr) \
  printf("%" PRIu64 ": current_address=%p or %p for non-void functions\n", \
         ompt_get_thread_data()->value, ((char *)addr) - 1, ((char *)addr) - 4)
#elif KMP_ARCH_PPC64
// On Power the NOP instruction is 4 bytes long. In addition, the compiler
// inserts a second NOP instruction (another 4 bytes). For non-void runtime
// functions Clang inserts a STW instruction (but only if compiling under
// -fno-PIC which will be the default with Clang 8.0, another 4 bytes).
#define print_possible_return_addresses(addr) \
  printf("%" PRIu64 ": current_address=%p or %p\n", ompt_get_thread_data()->value, \
         ((char *)addr) - 8, ((char *)addr) - 12)
#elif KMP_ARCH_AARCH64
// On AArch64 the NOP instruction is 4 bytes long, can be followed by inserted
// store instruction (another 4 bytes long).
#define print_possible_return_addresses(addr) \
  printf("%" PRIu64 ": current_address=%p or %p\n", ompt_get_thread_data()->value, \
         ((char *)addr) - 4, ((char *)addr) - 8)
#elif KMP_ARCH_RISCV64
#if __riscv_compressed
// On RV64GC the C.NOP instruction is 2 byte long. In addition, the compiler
// inserts a J instruction (targeting the successor basic block), which
// accounts for another 4 bytes. Finally, an additional J instruction may
// appear (adding 4 more bytes) when the C.NOP is referenced elsewhere (ie.
// another branch).
#define print_possible_return_addresses(addr) \
  printf("%" PRIu64 ": current_address=%p or %p\n", \
         ompt_get_thread_data()->value, ((char *)addr) - 6, ((char *)addr) - 10)
#else
// On RV64G the NOP instruction is 4 byte long. In addition, the compiler
// inserts a J instruction (targeting the successor basic block), which
// accounts for another 4 bytes. Finally, an additional J instruction may
// appear (adding 4 more bytes) when the NOP is referenced elsewhere (ie.
// another branch).
#define print_possible_return_addresses(addr) \
  printf("%" PRIu64 ": current_address=%p or %p\n", \
         ompt_get_thread_data()->value, ((char *)addr) - 8, ((char *)addr) - 12)
#endif
#else
#error Unsupported target architecture, cannot determine address offset!
#endif


// This macro performs a somewhat similar job to print_current_address(), except
// that it discards a certain number of nibbles from the address and only prints
// the most significant bits / nibbles. This can be used for cases where the
// return address can only be approximated.
//
// To account for overflows (ie the most significant bits / nibbles have just
// changed as we are a few bytes above the relevant power of two) the addresses
// of the "current" and of the "previous block" are printed.
#define print_fuzzy_address(id) \
  define_ompt_label(id) \
  print_fuzzy_address_blocks(get_ompt_label_address(id))

// If you change this define you need to adapt all capture patterns in the tests
// to include or discard the new number of nibbles!
#define FUZZY_ADDRESS_DISCARD_NIBBLES 2
#define FUZZY_ADDRESS_DISCARD_BYTES (1 << ((FUZZY_ADDRESS_DISCARD_NIBBLES) * 4))
#define print_fuzzy_address_blocks(addr)                                       \
  printf("%" PRIu64 ": fuzzy_address=0x%" PRIx64 " or 0x%" PRIx64              \
         " or 0x%" PRIx64 " or 0x%" PRIx64 " (%p)\n",                          \
         ompt_get_thread_data()->value,                                        \
         ((uint64_t)addr) / FUZZY_ADDRESS_DISCARD_BYTES - 1,                   \
         ((uint64_t)addr) / FUZZY_ADDRESS_DISCARD_BYTES,                       \
         ((uint64_t)addr) / FUZZY_ADDRESS_DISCARD_BYTES + 1,                   \
         ((uint64_t)addr) / FUZZY_ADDRESS_DISCARD_BYTES + 2, addr)

//global variables used by omprace
OpenmpRaceDetector* omp_racer;
//omprace vars
static int omprace_init_flag = 0;
//instrument memory function
extern "C" void __omprace_instrument_access__(void* addr, int type, char* loc=nullptr);
//#if LOC_INFO
//extern "C" void __omprace_instrument_access__(void* addr, int type, char* loc);
//#else
//extern "C" void __omprace_instrument_access__(void* addr, int type, char* loc=nullptr);
//#endif

/** @brief used to generate OpenMP parallel ids starting from 1
 *  @return void
 */
static uint64_t omw_next_parallel_id()
{
  static uint64_t OMPRACE_PARALLEL_ID=1;
  uint64_t ret = __sync_fetch_and_add(&OMPRACE_PARALLEL_ID,1);
  return ret;
}

/** @brief used to generate OpenMP  task ids starting from 1
 *  @return void
 */
static uint64_t omw_next_task_id()
{
  static uint64_t OMPRACE_TASK_ID=1;
  uint64_t ret = __sync_fetch_and_add(&OMPRACE_TASK_ID,1);
  return ret;
}

typedef struct omprace_thread_data{
  uint64_t tid;
  void* stack_base_addr;
  void* stack_top_addr;
  void* active_task_exit_frame{nullptr};
  void* lowest_accessed_addr{(void*)0x7fffffffffff};
  //TODO add padding
  
  omprace_thread_data(uint64_t _tid)
  :tid(_tid)
  {
    pthread_attr_t attr; 
    int r =  pthread_getattr_np(pthread_self(), &attr);
    assert(r==0);
    size_t stack_size;
    void* stack_addr;
    r = pthread_attr_getstack(&attr, &stack_addr, &stack_size);
    assert(r==0);
  
    stack_base_addr = stack_addr;
    stack_top_addr = (void*)((uint64_t)stack_addr + (uint64_t)stack_size);
    
    //printf("thread create tid=%ld stack_base_addr=%p stack_top_addr=%p , stack_size=%lx\n", 
    //tid ,stack_base_addr, stack_top_addr, stack_size);
  }

  inline void set_lowest_accessed_addr(void* addr){
      if ((size_t)addr < (size_t)lowest_accessed_addr){
        lowest_accessed_addr = addr;    
      }
  }

  inline void reset_lowest_accessed_addr(){
    lowest_accessed_addr = (void*)0x7fffffffffff;
  }
} omprace_thread_data;

typedef struct omprace_task_data{
  uint64_t id;
  //lockset
  omprace_task_data(uint64_t id)
  :id(id){
  }
} omprace_task_data;

inline uint64_t allocate_and_set_thread_data(ompt_data_t* t_data){
  uint64_t tid= omp_get_thread_num();
  
  t_data->ptr = new omprace_thread_data(tid);
  
  return ((omprace_thread_data*)t_data->ptr)->tid;

}

inline void delete_thread_data(ompt_data_t* data){
  delete ((omprace_thread_data*)data->ptr);
  data->ptr = nullptr;
}
inline uint64_t allocate_and_set_task_data(ompt_data_t* data){
  data->ptr = new omprace_task_data(omw_next_task_id());
  return ((omprace_task_data*)data->ptr)->id;
}

inline void delete_task_data(ompt_data_t* data){
  delete ((omprace_task_data*)data->ptr);
  data->ptr = nullptr;
}

/* not used atm
static void
on_ompt_callback_mutex_acquire(
  ompt_mutex_t kind,
  unsigned int hint,
  unsigned int impl,
  ompt_wait_id_t wait_id,
  const void *codeptr_ra)
{
  switch(kind)
  {
    case ompt_mutex_lock:
      printf("%" PRIu64 ": ompt_event_wait_lock: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    case ompt_mutex_nest_lock:
      printf("%" PRIu64 ": ompt_event_wait_nest_lock: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    case ompt_mutex_critical:
      printf("%" PRIu64 ": ompt_event_wait_critical: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    case ompt_mutex_atomic:
      printf("%" PRIu64 ": ompt_event_wait_atomic: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    case ompt_mutex_ordered:
      printf("%" PRIu64 ": ompt_event_wait_ordered: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    default:
      break;
  }
}
*/

//TODO
//get current task. add the lock to its lockset
static void
on_ompt_callback_mutex_acquired(
  ompt_mutex_t kind,
  ompt_wait_id_t wait_id,
  const void *codeptr_ra)
{
  switch(kind)
  {
    case ompt_mutex_lock:
      printf("%" PRIu64 ": ompt_event_acquired_lock: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_nest_lock:
      printf("%" PRIu64 ": ompt_event_acquired_nest_lock_first: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_critical:
      printf("%" PRIu64 ": ompt_event_acquired_critical: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_atomic:
      printf("%" PRIu64 ": ompt_event_acquired_atomic: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_ordered:
      printf("%" PRIu64 ": ompt_event_acquired_ordered: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    default:
      break;
  }
}

//TODO
//get current task. remove the lock from its lockset
static void
on_ompt_callback_mutex_released(
  ompt_mutex_t kind,
  ompt_wait_id_t wait_id,
  const void *codeptr_ra)
{
  switch(kind)
  {
    case ompt_mutex_lock:
      printf("%" PRIu64 ": ompt_event_release_lock: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_nest_lock:
      printf("%" PRIu64 ": ompt_event_release_nest_lock_last: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_critical:
      printf("%" PRIu64 ": ompt_event_release_critical: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_atomic:
      printf("%" PRIu64 ": ompt_event_release_atomic: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_ordered:
      printf("%" PRIu64 ": ompt_event_release_ordered: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    default:
      break;
  }
}

static void
on_ompt_callback_nest_lock(
  ompt_scope_endpoint_t endpoint,
  ompt_wait_id_t wait_id,
  const void *codeptr_ra)
{
  switch(endpoint)
  {
    case ompt_scope_begin:
      printf("%" PRIu64 ": ompt_event_acquired_nest_lock_next: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_scope_end:
      printf("%" PRIu64 ": ompt_event_release_nest_lock_prev: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
  }
}

/** @brief called when a task reaches a synchronization construct including 
 * implicit/explicit barriers.
 *  @param kind determines the synchronization construct encountered.
 *  @param parallel_data denotes the data associated with the current parallel region. null for barrier-end
 *  @param task_data is the data associated with the current task
 *  @param endpoint denotes if its the beginning or the end of the construct
 *  @return void
 */
static void
on_ompt_callback_sync_region(
  ompt_sync_region_t kind,
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  const void *codeptr_ra)
{
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t current_tid = thread_data->tid;
  omprace_task_data *t_data = (omprace_task_data*)task_data->ptr;
  switch(endpoint)
  {
    case ompt_scope_begin:
      switch(kind)
      {
        case ompt_sync_region_barrier:
        case ompt_sync_region_barrier_implicit:
        case ompt_sync_region_barrier_explicit:
        case ompt_sync_region_barrier_implementation:
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_barrier_begin: parallel_id=%" PRIu64 
          ", task_id=%" PRIu64 ", codeptr_ra=%p\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra);
          #endif
          //print_ids(0);
          omp_racer->m_capture_barrier_begin(current_tid, parallel_data->value, t_data->id);
          break;
        case ompt_sync_region_taskwait:
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_taskwait_begin: parallel_id=%" PRIu64 
          ", task_id=%" PRIu64 ", codeptr_ra=%p\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra);
          #endif
          omp_racer->m_capture_taskwait_begin(current_tid, parallel_data->value, t_data->id);
          break;
        case ompt_sync_region_taskgroup:
          //TODO
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_taskgroup_begin: parallel_id=%" PRIu64 
          ", task_id=%" PRIu64 ", codeptr_ra=%p\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra);
          #endif
          omp_racer->m_capture_taskgroup_begin(current_tid, parallel_data->value, t_data->id);
          break;
        case ompt_sync_region_reduction:
          printf("ompt_sync_region_reduction should never be passed to "
                 "on_ompt_callback_sync_region\n");
          exit(-1);
          break;
      }
      break;
    case ompt_scope_end:
      switch(kind)
      {
        case ompt_sync_region_barrier:
        case ompt_sync_region_barrier_implicit:
        case ompt_sync_region_barrier_explicit:
        case ompt_sync_region_barrier_implementation:
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_barrier_end: parallel_id=%" PRIu64 
          ", task_id=%" PRIu64 ", codeptr_ra=%p\n", 
          current_tid, (parallel_data)?parallel_data->value:0, t_data->id, codeptr_ra);
          #endif
          omp_racer->m_capture_barrier_end(current_tid, (parallel_data)?parallel_data->value:0, t_data->id);
          break;
        case ompt_sync_region_taskwait:
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_taskwait_end: parallel_id=%" PRIu64 
          ", task_id=%" PRIu64 ", codeptr_ra=%p\n",
          current_tid, (parallel_data)?parallel_data->value:0, t_data->id, codeptr_ra);
          #endif
          omp_racer->m_capture_taskwait_end(current_tid, (parallel_data)?parallel_data->value:0, t_data->id);
          break;
        case ompt_sync_region_taskgroup:
          //TODO
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_taskgroup_end: parallel_id=%" PRIu64 
          ", task_id=%" PRIu64 ", codeptr_ra=%p\n", 
          current_tid, (parallel_data)?parallel_data->value:0, t_data->id, codeptr_ra);
          #endif
          omp_racer->m_capture_taskgroup_end(current_tid, (parallel_data)?parallel_data->value:0, t_data->id);
          break;
        case ompt_sync_region_reduction:
          printf("ompt_sync_region_reduction should never be passed to "
                 "on_ompt_callback_sync_region\n");
          exit(-1);
          break;
      }
      break;
  }
}

/* unused since we don't need to consider time spent withing a synchronization consruct
static void
on_ompt_callback_sync_region_wait(
  ompt_sync_region_t kind,
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  const void *codeptr_ra)
{
  switch(endpoint)
  {
    case ompt_scope_begin:
      switch(kind)
      {
        case ompt_sync_region_barrier:
        case ompt_sync_region_barrier_implicit:
        case ompt_sync_region_barrier_explicit:
        case ompt_sync_region_barrier_implementation:
          printf("%" PRIu64 ": ompt_event_wait_barrier_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", codeptr_ra=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
          break;
        case ompt_sync_region_taskwait:
          printf("%" PRIu64 ": ompt_event_wait_taskwait_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", codeptr_ra=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
          break;
        case ompt_sync_region_taskgroup:
          printf("%" PRIu64 ": ompt_event_wait_taskgroup_begin: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", codeptr_ra=%p\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, codeptr_ra);
          break;
        case ompt_sync_region_reduction:
          printf("ompt_sync_region_reduction should never be passed to "
                 "on_ompt_callback_sync_region_wait\n");
          exit(-1);
          break;
      }
      break;
    case ompt_scope_end:
      switch(kind)
      {
        case ompt_sync_region_barrier:
        case ompt_sync_region_barrier_implicit:
        case ompt_sync_region_barrier_explicit:
        case ompt_sync_region_barrier_implementation:
          printf("%" PRIu64 ": ompt_event_wait_barrier_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", codeptr_ra=%p\n", ompt_get_thread_data()->value, (parallel_data)?parallel_data->value:0, task_data->value, codeptr_ra);
          break;
        case ompt_sync_region_taskwait:
          printf("%" PRIu64 ": ompt_event_wait_taskwait_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", codeptr_ra=%p\n", ompt_get_thread_data()->value, (parallel_data)?parallel_data->value:0, task_data->value, codeptr_ra);
          break;
        case ompt_sync_region_taskgroup:
          printf("%" PRIu64 ": ompt_event_wait_taskgroup_end: parallel_id=%" PRIu64 ", task_id=%" PRIu64 ", codeptr_ra=%p\n", ompt_get_thread_data()->value, (parallel_data)?parallel_data->value:0, task_data->value, codeptr_ra);
          break;
        case ompt_sync_region_reduction:
          printf("ompt_sync_region_reduction should never be passed to "
                 "on_ompt_callback_sync_region_wait\n");
          exit(-1);
          break;
      }
      break;
  }
}
*/
// new callback added in llvm 10
static void on_ompt_callback_reduction(ompt_sync_region_t kind,
                                       ompt_scope_endpoint_t endpoint,
                                       ompt_data_t *parallel_data,
                                       ompt_data_t *task_data,
                                       const void *codeptr_ra) {
  switch (endpoint) {
  case ompt_scope_begin:
    printf("%" PRIu64 ": ompt_event_reduction_begin: parallel_id=%" PRIu64
           ", task_id=%" PRIu64 ", codeptr_ra=%p\n",
           ompt_get_thread_data()->value,
           (parallel_data) ? parallel_data->value : 0, task_data->value,
           codeptr_ra);
    break;
  case ompt_scope_end:
    printf("%" PRIu64 ": ompt_event_reduction_end: parallel_id=%" PRIu64
           ", task_id=%" PRIu64 ", codeptr_ra=%p\n",
           ompt_get_thread_data()->value,
           (parallel_data) ? parallel_data->value : 0, task_data->value,
           codeptr_ra);
    break;
  }
}

/* Not used atm
static void
on_ompt_callback_flush(
    ompt_data_t *thread_data,
    const void *codeptr_ra)
{
  printf("%" PRIu64 ": ompt_event_flush: codeptr_ra=%p\n", thread_data->value, codeptr_ra);
}
*/

/* not used atm
static void
on_ompt_callback_cancel(
    ompt_data_t *task_data,
    int flags,
    const void *codeptr_ra)
{
  const char* first_flag_value;
  const char* second_flag_value;
  if(flags & ompt_cancel_parallel)
    first_flag_value = ompt_cancel_flag_t_values[0];
  else if(flags & ompt_cancel_sections)
    first_flag_value = ompt_cancel_flag_t_values[1];
  else if(flags & ompt_cancel_loop)
    first_flag_value = ompt_cancel_flag_t_values[2];
  else if(flags & ompt_cancel_taskgroup)
    first_flag_value = ompt_cancel_flag_t_values[3];

  if(flags & ompt_cancel_activated)
    second_flag_value = ompt_cancel_flag_t_values[4];
  else if(flags & ompt_cancel_detected)
    second_flag_value = ompt_cancel_flag_t_values[5];
  else if(flags & ompt_cancel_discarded_task)
    second_flag_value = ompt_cancel_flag_t_values[6];

  printf("%" PRIu64 ": ompt_event_cancel: task_data=%" PRIu64 ", flags=%s|%s=%" PRIu32 ", codeptr_ra=%p\n", ompt_get_thread_data()->value, task_data->value, first_flag_value, second_flag_value, flags,  codeptr_ra);
}
*/

/** @brief called when an implicit task is created/completed
 *  @param endpoint denotes whether its a task begin/end
 *  @param parallel_data is the data associated with the current parallel region. its 0 for implicit-task-end event
 *  @param task_data is the data associated with the current implicit task. needs to be set up in begin
 *  @param team_size is the size of the current team
 *  @param thread_num is the thread id of the thread running the implicit task within the current time
 */
static void
on_ompt_callback_implicit_task(
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  unsigned int team_size,
  unsigned int thread_num,
  int flags)
{
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t current_tid = thread_data->tid;
  
  
  //void *addr = NULL;
  //size_t size = 0;
  //int result = ompt_get_task_memory(&addr, &size, 0);
  
  switch(endpoint)
  {
    case ompt_scope_begin:
    {
      //DONE updated due to a change in llvm-9 ompt
      if(task_data->ptr)
        printf("%s\n", "0: task_data initially not null");
      
      //there is no parallel_begin callback for implicit parallel region
      //thus it is initialized in initial task
      if(flags & ompt_task_initial)
      {
      
      
        if(team_size == 1 && thread_num == 1 && parallel_data->ptr)
          printf("%s\n", "0: parallel_data initially not null");
        
        parallel_data->value = omw_next_parallel_id();
        auto new_task_id = allocate_and_set_task_data(task_data);
        #if DEBUG
        printf("%" PRIu64 ": ompt_event_initial_task_begin: parallel_id=%" PRIu64 
        ", task_id=%" PRIu64 ", actual_parallelism=%" PRIu32 ", index=%" PRIu32 
        ", flags=%" PRIu32 "\n", ompt_get_thread_data()->value, parallel_data->value, task_data->value, team_size, thread_num, flags);
        #endif
        return;
      }

      //task_data->value = ompt_get_unique_id();
      //note that the encoutering task could not have held any locks or even if it could it would be held among all tasks in the parallel region
      //either way no need to add to lockset
      auto new_task_id = allocate_and_set_task_data(task_data);
      //printf("ompt_event_implicit_task_begin: task_id=%" PRIu64
      //   ", memory_addr=%p, memory_size=%lu, result=%d\n",
      //   new_task_id, addr, size, result);
      //TEMP
      omp_racer->m_capture_implicit_task_begin(current_tid, parallel_data->value, new_task_id, "no source");
      #if DEBUG
      printf("%" PRIu64 ": ompt_event_implicit_task_begin: parallel_id=%" PRIu64 
      ", task_id=%" PRIu64 ", team_size=%" PRIu32 ", thread_num=%" PRIu32 "\n", ompt_get_thread_data()->value, parallel_data->value, new_task_id, team_size, thread_num);
      #endif
      break;
    }
    case ompt_scope_end:
    {
      if(flags & ompt_task_initial)
      {
        //printf("%s\n", "!!!!implicit task create end  for initial task");
        #if DEBUG
        printf("%" PRIu64 ": ompt_event_initial_task_end: parallel_id=%" PRIu64
               ", task_id=%" PRIu64 ", actual_parallelism=%" PRIu32
               ", index=%" PRIu32 "\n",
               ompt_get_thread_data()->value,
               (parallel_data) ? parallel_data->value : 0, task_data->value,
               team_size, thread_num);
        #endif 
        delete_task_data(task_data);
        return;
      }
      omprace_task_data *t_data = (omprace_task_data*)task_data->ptr;
      //TEMP
      omp_racer->m_capture_implicit_task_end(current_tid, (parallel_data)?parallel_data->value:0, t_data->id);
      #if DEBUG
      printf("%" PRIu64 ": ompt_event_implicit_task_end: parallel_id=%" PRIu64 
      ", task_id=%" PRIu64 ", team_size=%" PRIu32 ", thread_num=%" PRIu32 "\n", 
      current_tid, (parallel_data)?parallel_data->value:0, t_data->id, team_size, thread_num);
      #endif
      delete_task_data(task_data);
      break;
    }
  }
}

static void
on_ompt_callback_lock_init(
  ompt_mutex_t kind,
  unsigned int hint,
  unsigned int impl,
  ompt_wait_id_t wait_id,
  const void *codeptr_ra)
{
  switch(kind)
  {
    case ompt_mutex_lock:
      printf("%" PRIu64 ": ompt_event_init_lock: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    case ompt_mutex_nest_lock:
      printf("%" PRIu64 ": ompt_event_init_nest_lock: wait_id=%" PRIu64 ", hint=%" PRIu32 ", impl=%" PRIu32 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, hint, impl, codeptr_ra);
      break;
    default:
      break;
  }
}

static void
on_ompt_callback_lock_destroy(
  ompt_mutex_t kind,
  ompt_wait_id_t wait_id,
  const void *codeptr_ra)
{
  switch(kind)
  {
    case ompt_mutex_lock:
      printf("%" PRIu64 ": ompt_event_destroy_lock: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    case ompt_mutex_nest_lock:
      printf("%" PRIu64 ": ompt_event_destroy_nest_lock: wait_id=%" PRIu64 ", codeptr_ra=%p \n", ompt_get_thread_data()->value, wait_id, codeptr_ra);
      break;
    default:
      break;
  }
}

/** @brief called when a work sharing construct begins/ends
 *  @param endpoint denotes whether it's the construct's begin/end
 *  @param wstype determines the type of the work sharing construct
 */
static void
on_ompt_callback_work(
  ompt_work_t wstype,
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  uint64_t count,
  const void *codeptr_ra)
{
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t current_tid = thread_data->tid;
  omprace_task_data *t_data = (omprace_task_data*)task_data->ptr;
  switch(endpoint)
  {
    case ompt_scope_begin:
      switch(wstype)
      {
        case ompt_work_loop:
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_loop_begin: parallel_id=%" PRIu64 
          ", parent_task_id=%" PRIu64 ", codeptr_ra=%p, count=%" PRIu64 "\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra, count);
          #endif
          omp_racer->m_capture_loop_begin(current_tid, parallel_data->value, t_data->id, "psource");
          break;
        case ompt_work_sections:
          //Basically this is a more restricted version of loop. However, we need to add
          //extra calls to identify a section's start and end since the dispatch callback 
          //is not implemented yet in llvm
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_sections_begin: parallel_id=%" PRIu64 
          ", parent_task_id=%" PRIu64 ", codeptr_ra=%p, count=%" PRIu64 "\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra, count);
          #endif
          break;
        case ompt_work_single_executor:
          //TODO do we need this anymore?
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_single_in_block_begin: parallel_id=%" PRIu64 
          ", parent_task_id=%" PRIu64 ", codeptr_ra=%p, count=%" PRIu64 "\n", 
          current_tid, parallel_data->value, t_data->id, 
	  codeptr_ra, count);
          #endif
          omp_racer->m_capture_single_begin(current_tid, parallel_data->value, t_data->id, "psource");
          break;
        case ompt_work_single_other:
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_single_others_begin: parallel_id=%" PRIu64 
          ", task_id=%" PRIu64 ", codeptr_ra=%p, count=%" PRIu64 "\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra, count);
          #endif
          break;
        //this cosntruct is fortran only 
        case ompt_work_workshare:
          //impl
          break;
        case ompt_work_distribute:
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_distribute_begin: parallel_id=%" PRIu64 
          ", parent_task_id=%" PRIu64 ", codeptr_ra=%p, count=%" PRIu64 "\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra, count);
          #endif
          break;
        case ompt_work_taskloop:
          //impl TODO is this implemented by llvm 9? the code seems to be there
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_taskloop_begin: parallel_id=%" PRIu64 
          ", parent_task_id=%" PRIu64 ", codeptr_ra=%p, count=%" PRIu64 "\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra, count);
          #endif
          break;
      }
      break;
    case ompt_scope_end:
      switch(wstype)
      {
        case ompt_work_loop:
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_loop_end: parallel_id=%" PRIu64 
          ", task_id=%" PRIu64 ", codeptr_ra=%p, count=%" PRIu64 "\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra, count);
          #endif
          
          omp_racer->m_capture_loop_end(current_tid, parallel_data->value, t_data->id);
          break;
        case ompt_work_sections:
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_sections_end: parallel_id=%" PRIu64 
          ", task_id=%" PRIu64 ", codeptr_ra=%p, count=%" PRIu64 "\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra, count);
          #endif
          break;
        case ompt_work_single_executor:
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_single_in_block_end: parallel_id=%" PRIu64 
          ", task_id=%" PRIu64 ", codeptr_ra=%p, count=%" PRIu64 "\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra, count);
          #endif

          omp_racer->m_capture_single_end(current_tid, parallel_data->value, t_data->id);
          break;
        //Don't need to handle for datarace detection
        case ompt_work_single_other:
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_single_others_end: parallel_id=%" PRIu64 
          ", task_id=%" PRIu64 ", codeptr_ra=%p, count=%" PRIu64 "\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra, count);
          #endif
          break;
        //this construct is fortran only
        case ompt_work_workshare:
          //impl
          break;
        case ompt_work_distribute:
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_distribute_end: parallel_id=%" PRIu64 
          ", parent_task_id=%" PRIu64 ", codeptr_ra=%p, count=%" PRIu64 "\n", 
          current_tid, parallel_data->value, t_data->id, codeptr_ra, count);
          #endif
          break;
        //todo
        case ompt_work_taskloop:
          //impl
          #if DEBUG
          printf("%" PRIu64 ": ompt_event_taskloop_end: parallel_id=%" PRIu64 
          ", parent_task_id=%" PRIu64 ", codeptr_ra=%p, count=%" PRIu64 "\n", 
          ompt_get_thread_data()->value, parallel_data->value, t_data->id, codeptr_ra, count);
          #endif
          break;
      }
      break;
  }
}

/** @brief called when a master construct begins/ends
 *  @param endpoint denotes whether it's the construct's begin/end
 */
//TODO we probably don't need this along with single
static void
on_ompt_callback_master(
  ompt_scope_endpoint_t endpoint,
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  const void *codeptr_ra)
{
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t current_tid = thread_data->tid;
  omprace_task_data *t_data = (omprace_task_data*)task_data->ptr;
  switch(endpoint)
  {
    case ompt_scope_begin:
      #if DEBUG
      printf("%" PRIu64 ": ompt_event_master_begin: parallel_id=%" PRIu64 
      ", task_id=%" PRIu64 ", codeptr_ra=%p\n", 
      current_tid, parallel_data->value, t_data->id, codeptr_ra);
      #endif

      omp_racer->m_capture_master_begin(current_tid, parallel_data->value, t_data->id, "psource");
      break;
    case ompt_scope_end:
      #if DEBUG
      printf("%" PRIu64 ": ompt_event_master_end: parallel_id=%" PRIu64 
      ", task_id=%" PRIu64 ", codeptr_ra=%p\n", 
      current_tid, parallel_data->value, t_data->id, codeptr_ra);
      #endif
      omp_racer->m_capture_master_end(current_tid, parallel_data->value, t_data->id);
      break;
  }
}

/** @brief called when a task reaches a parallel directive
 *  @param encountering_task_data task data associated with the task reaching
 *  parallel directive. I think I need to set it up with data in task create 
 *  @return void
 */
static void 
on_ompt_callback_parallel_begin(
  ompt_data_t *encountering_task_data,
  const ompt_frame_t *encountering_task_frame, 
  ompt_data_t *parallel_data,
  uint32_t requested_team_size, 
  int flag, 
  const void *codeptr_ra) 
{
  if(parallel_data->ptr)
    printf("0: parallel_data initially not null\n");
  parallel_data->value = omw_next_parallel_id();
  omprace_task_data *e_t_data = (omprace_task_data*)encountering_task_data->ptr;
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t tid = thread_data->tid;
  #if DEBUG
  printf("%" PRIu64 ": ompt_event_parallel_begin: parent_task_id=%" PRIu64
         ", parent_task_frame.exit=%p, parent_task_frame.reenter=%p, "
         "parallel_id=%" PRIu64 ", requested_team_size=%" PRIu32
         ", codeptr_ra=%p, invoker=%d\n",
         tid, e_t_data->id,
         encountering_task_frame->exit_frame.ptr,
         encountering_task_frame->enter_frame.ptr, parallel_data->value,
         requested_team_size, codeptr_ra, flag);
  #endif

  omp_racer->m_capture_parallel_begin(tid,
    e_t_data->id,
    parallel_data->value, 
    requested_team_size,
    "psource");//this is a don't care value an artifact of reusing the profiler code
}

static void
on_ompt_callback_parallel_end(
  ompt_data_t *parallel_data,
  ompt_data_t *encountering_task_data,
  int flag, 
  const void *codeptr_ra) 
{
  omprace_task_data *e_t_data = (omprace_task_data*)encountering_task_data->ptr;
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t current_tid = thread_data->tid;
  #if DEBUG
  printf("%" PRIu64 ": ompt_event_parallel_end: parallel_id=%" PRIu64
         ", task_id=%" PRIu64 ", invoker=%d, codeptr_ra=%p\n",
         current_tid, parallel_data->value,
         e_t_data->id, flag, codeptr_ra);
  #endif

  omp_racer->m_capture_parallel_end(current_tid, 
                                  parallel_data->value, 
                                  e_t_data ? e_t_data->id : 0);
}

/** @brief called when an implicit/explicit task is created
 *  @param psource is the task's directive's source code information unless its the initial task
 *  @return void
 *  Note that we don't use this callback for the profiler anymore since we need a customized callback
 *  to measure tasking overhead
 */
static void
on_ompt_callback_task_create(
  ompt_data_t *encountering_task_data,
  const ompt_frame_t *encountering_task_frame,
  ompt_data_t* new_task_data,
  int type,
  int has_dependences,
  const void *codeptr_ra)
{
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t current_tid = thread_data->tid;
  
  if(new_task_data->ptr)
    printf("0: new_task_data initially not null\n");
  //new_task_data->value = ompt_get_unique_id();
  

  //there is no parallel_begin callback for implicit parallel region
  //thus it is initialized in initial task
  if(type & ompt_task_initial)
  {
    //this should not happen in llvm 9
    assert(false);
    ompt_data_t *parallel_data;
    ompt_get_parallel_info(0, &parallel_data, NULL);
    if(parallel_data->ptr)
      printf("%s\n", "0: parallel_data initially not null");
    parallel_data->value = omw_next_parallel_id();
    //printf("%s\n", "!!!!task create for initial task");
    auto new_task_id = allocate_and_set_task_data(new_task_data);

    //return;
  }

  omprace_task_data *e_t_data = encountering_task_data ? (omprace_task_data*)encountering_task_data->ptr : nullptr;

  //check for explicit task here and call task_alloc
  if(type & ompt_task_explicit){
    auto new_task_id = allocate_and_set_task_data(new_task_data);
    omprace_task_data *e_t_data = encountering_task_data ? (omprace_task_data*)encountering_task_data->ptr : nullptr;
    ompt_data_t *parallel_data;
    ompt_get_parallel_info(0, &parallel_data, NULL);
    omp_racer->m_capture_task_alloc(current_tid, parallel_data->value,
    e_t_data ? e_t_data->id : 0, 
    new_task_id,
    //NULL, has_dependences, psource);
    NULL, has_dependences, "psource");
  }
  #if DEBUG
  omprace_task_data *t_data = (omprace_task_data*)new_task_data->ptr;
  char buffer[2048];
  format_task_type(type, buffer);
  printf("%" PRIu64 ": ompt_event_task_create: parent_task_id=%" PRIu64 
  ", parent_task_frame.exit=%p, parent_task_frame.reenter=%p, new_task_id=%" PRIu64 
  ", codeptr_ra=%p, task_type=%s=%d, has_dependences=%s\n", 
  current_tid, e_t_data ? e_t_data->id : 0, 
  encountering_task_frame ? encountering_task_frame->exit_frame.ptr : NULL, 
  encountering_task_frame ? encountering_task_frame->enter_frame.ptr : NULL, 
  t_data->id, codeptr_ra, buffer, type, has_dependences ? "yes" : "no");
  #endif
}

/** @brief called when a task gets scheduled
 *  used to idenfity an explicit task's begin/end
 */
//TODO what happens to locksets here
static void
on_ompt_callback_task_schedule(
  ompt_data_t *first_task_data,
  ompt_task_status_t prior_task_status,
  ompt_data_t *second_task_data)
{
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t current_tid = thread_data->tid;
  omprace_task_data *first_t_data = (omprace_task_data*)first_task_data->ptr;
  omprace_task_data *second_t_data = (omprace_task_data*)second_task_data->ptr;

  #if DEBUG
  printf("%" PRIu64 ": ompt_event_task_schedule: first_task_id=%" PRIu64 
  ", second_task_id=%" PRIu64 ", prior_task_status=%s=%d\n", 
  current_tid, first_t_data->id, second_t_data->id, ompt_task_status_t_values[prior_task_status], prior_task_status);
  #endif
  //this works if explicit tasks are always tied as is the case with all benchmarks we could find
  //check to refine the exact condition for taskBegin
  //pass second task data to omp_profiler->captureTaskBegin but let it return if taskmap does not include the taskId
  //This should should not occur so there is an assertion for this in omp_profiler->captureTaskBegin
  //parent/prior task is only used for logging purposes
  //this can be set to ompt_task_switch
  if(prior_task_status != ompt_task_complete){
    ompt_data_t *parallel_data;
    ompt_get_parallel_info(0, &parallel_data, NULL);

    /*
    printf("ompt_event_task_schedule: task_id=%" PRIu64
         ", child_task_id=%" PRIu64 ", memory_addr=%p, memory_size=%lu, result=%d\n",
         first_t_data->id, second_t_data->id, addr, size, result);
    */
    omp_racer->m_capture_task_begin(current_tid,
     		 parallel_data ? parallel_data->value : 0,
     		 first_t_data->id,
     		 second_t_data->id);
               
  }

  if(prior_task_status == ompt_task_complete)
  {
    #if DEBUG
    printf("%" PRIu64 ": ompt_event_task_end: task_id=%" PRIu64 
    "\n", current_tid, first_t_data->id);
    #endif
    void *addr = NULL;
    size_t size = 0;
    int result = ompt_get_task_memory(&addr, &size, 0);
    assert(result == 1);
    #if DEBUG
    printf("ompt_event_task_end: task_id=%" PRIu64
         ", memory_addr=%p, memory_size=%lu, result=%d\n",
         first_t_data->id, addr, size, result);
    #endif
    //mark currents thread lowest_accesses_addr to active_task_exit_frame as deallocated
    
    void* stack_high = thread_data->active_task_exit_frame; 
    void* stack_low  = thread_data->lowest_accessed_addr;
    omp_racer->clear_metadata(stack_low, stack_high);

    //printf("cleared thread stack id=%" PRIu64
    //     ", memory_addr_low=%p, memory_size_high=%p\n",
    //     current_tid, stack_low, stack_high);
    void* task_heap_low = addr;
    void* task_heap_high = (void*)((size_t)addr + size);

    omp_racer->clear_metadata(task_heap_low, task_heap_high);

    //printf("cleared task heap stack id=%" PRIu64
    //     "task id=%" PRIu64 ", memory_addr_low=%p, memory_size_high=%p\n",
    //     current_tid, first_t_data->id,task_heap_low, task_heap_high);
    
    //mark current tasks heap as deallocated
    omp_racer->m_capture_task_end(current_tid, first_t_data->id);
    delete_task_data(first_task_data);
  }

  
  ompt_frame_t* omptTaskFrame;
  //TODO check this for llvm-9
  ompt_get_task_info(0, NULL, (ompt_data_t**) NULL, &omptTaskFrame, NULL, NULL);
  
  assert(omptTaskFrame!=NULL);
  //TODO it seems the exit frame can be null for some tasks which causes correctness issues
  //update the current threads address range
  thread_data->active_task_exit_frame = omptTaskFrame->exit_frame.ptr;
  thread_data->lowest_accessed_addr = omptTaskFrame->exit_frame.ptr;
  
  
  //update lowest addr
}

/*from omp-tools.h

typedef struct ompt_dependence_t {
  ompt_data_t variable;
  ompt_dependence_type_t dependence_type;
} ompt_dependence_t;


typedef enum ompt_dependence_type_t {
  ompt_dependence_type_in              = 1,
  ompt_dependence_type_out             = 2,
  ompt_dependence_type_inout           = 3,
  ompt_dependence_type_mutexinoutset   = 4,
  ompt_dependence_type_source          = 5,
  ompt_dependence_type_sink            = 6
} ompt_dependence_type_t;

*/

/** @brief called when new tasks with depedencies are created
 *  Not Implemented in llvm 8.0.1 called when ordered constructs are encountered
 *  TODO check if the above statement is correct in llvm-9
 */
//TODO what happens to locksets here. Datarace only
static void
on_ompt_callback_dependences(
  ompt_data_t *task_data,
  const ompt_dependence_t *deps,
  int ndeps)
{
  omprace_task_data *t_data = (omprace_task_data*)task_data->ptr;
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t current_tid = thread_data->tid;
  #if DEBUG
  printf("%" PRIu64 ": ompt_event_task_dependences: task_id=%" PRIu64 
  ", deps=%p, ndeps=%d\n", current_tid, t_data->id, (void *)deps, ndeps);

  for(int i=0; i< ndeps; i++){
    printf("task_id= = %" PRIu64 ", dep var= 0x%" PRIXPTR " type=%s %" PRIu32 "\n", 
    t_data->id, (uintptr_t)deps[i].variable.value, 
    ompt_dependence_type_t_values[deps[i].dependence_type], deps[i].dependence_type);
  }
  #endif

  omp_racer->m_capture_task_dependences(current_tid, t_data->id, deps, ndeps);
}

//not used atm
static void
on_ompt_callback_task_dependence(
  ompt_data_t *first_task_data,
  ompt_data_t *second_task_data)
{
  printf("%" PRIu64 ": ompt_event_task_dependence_pair: first_task_id=%" PRIu64 ", second_task_id=%" PRIu64 "\n", ompt_get_thread_data()->value, first_task_data->value, second_task_data->value);
}

/** @brief callback called when a new thread gets created by the openmp runtime
 *  @param thread_type indicates the type of thread created intial/worker/other/unknown
 *  @param thread_data pointer to the associated thread data. gets set here
 *  @return void
 */
static void
on_ompt_callback_thread_begin(
  ompt_thread_t thread_type,
  ompt_data_t *thread_data)
{
  if(thread_data->ptr)
    printf("%s\n", "0: thread_data initially not null");

 uint64_t new_tid = allocate_and_set_thread_data(thread_data);
  
  //not used atm
  //if (thread_type == ompt_thread_type_t::ompt_thread_initial){
    //initial thread start
  //}
  #if DEBUG
  printf("%" PRIu64 ": ompt_event_thread_begin: thread_type=%s=%d\n"
  , new_tid, ompt_thread_t_values[thread_type], thread_type);
  #endif
  omp_racer->m_capture_thread_begin(new_tid);
}

/** @brief called when a thread ends
 *  @param thread_data pointer to associated thread data set up during thread_begin event
 *  @return void
 */ 
static void
on_ompt_callback_thread_end(
  ompt_data_t *thread_data)
{
  omprace_thread_data* t_data = (omprace_thread_data*) thread_data->ptr;
  uint64_t current_tid = t_data->tid;
  #if DEBUG
  printf("%" PRIu64 ": ompt_event_thread_end: thread_id=%" PRIu64 "\n", 
  current_tid, thread_data->value);
  #endif
  delete_thread_data(thread_data);
}

/** Not used atm
 */
/*static int
on_ompt_callback_control_tool(
  uint64_t command,
  uint64_t modifier,
  void *arg,
  const void *codeptr_ra)
{
  ompt_frame_t* omptTaskFrame;
  ompt_get_task_info(0, NULL, (ompt_data_t**) NULL, &omptTaskFrame, NULL, NULL);
  printf("%" PRIu64 ": ompt_event_control_tool: command=%" PRIu64 ", modifier=%" PRIu64 ", arg=%p, codeptr_ra=%p, current_task_frame.exit=%p, current_task_frame.reenter=%p \n", ompt_get_thread_data()->value, command, modifier, arg, codeptr_ra, omptTaskFrame->exit_frame.ptr, omptTaskFrame->enter_frame.ptr);
  return 0; //success
}*/

//not available on llvm 9.0. added the callback to ompt manually 
static void 
on_ompt_callback_dispatch(
  ompt_data_t *parallel_data,
  ompt_data_t *task_data,
  ompt_dispatch_t kind,
  ompt_data_t instance)
{
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t current_tid = thread_data->tid;
  #if DEBUG
  printf("[temp log] kmp dispatch tid= %lu, iteration=%" PRIu64 "\n", current_tid, instance.value);
  #endif
  omp_racer->m_capture_loop_chunk(current_tid);

}

//called to record memory access 
#if LOC_INFO
void __omprace_instrument_access__(void* addr, int type, char* loc)
#else
void __omprace_instrument_access__(void* addr, int type, char* loc)
//void __omprace_instrument_access__(void* addr, int type)
#endif
{
  //return immediately to test the overhead of instrumentation
  //return;
  if (omprace_init_flag == 0)
    return;
  //TODO cleanup all these preprocessors very hard to remember what are the differences
  
  //for location info when the llvm provides them
  #if LOC_INFO
  std::string loc_str(loc);
  //printf("instrument access loc=%s\n", loc);
  #endif
  #if DUMP_INST_MEM
  ompt_data_t *parallel_data;
  ompt_get_parallel_info(0, &parallel_data, NULL);
  //temp
  std::string loc_str(loc);
  #endif
  

  //TODO consider a TLS solution instead of ompt_get_thread_data
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  //access is within current thread's stack 
  if (((size_t)thread_data->stack_base_addr <= (size_t)addr)&&
      ((size_t)addr <= (size_t)thread_data->stack_top_addr)){
        thread_data->set_lowest_accessed_addr(addr);
        //returning here alleviates the stack not getting deallocated is certain cases
        //However it introduces False negatives
        //but it can significantly speedup task based apps such as fft
        //return;
      }
  if (type == 0){ //read
    #if DUMP_INST_MEM
    //omp_racer->m_instrument_access(thread_data->tid,parallel_data->value,addr, false);
    omp_racer->m_instrument_access(thread_data->tid,parallel_data->value,addr, false, loc_str);
    #elif LOC_INFO
    omp_racer->m_instrument_access(thread_data->tid,addr, false, loc_str);
    #else
    omp_racer->m_instrument_access(thread_data->tid,addr, false);
    #endif
    
  }
  else if(type ==1){
    
    #if DUMP_INST_MEM
    //omp_racer->m_instrument_access(thread_data->tid,parallel_data->value,addr, true);
    omp_racer->m_instrument_access(thread_data->tid,parallel_data->value,addr, true,loc_str);
    #elif LOC_INFO
    omp_racer->m_instrument_access(thread_data->tid,addr, true, loc_str);
    #else
    omp_racer->m_instrument_access(thread_data->tid,addr, true);
    #endif
  }
  else{
    printf("[ERROR] invalid instrument access, exiting omprace\n");
    exit(1);
  }
    
}


#define register_callback_t(name, type)                                        \
  do {                                                                         \
    type f_##name = &on_##name;                                                \
    if (ompt_set_callback(name, (ompt_callback_t)f_##name) == ompt_set_never)  \
      printf("0: Could not register callback '" #name "'\n");                  \
  } while (0)

#define register_callback(name) register_callback_t(name, name##_t)

int ompt_initialize(
  ompt_function_lookup_t lookup,
  int initial_device_num,
  ompt_data_t *tool_data)
{
  printf("ompt_initialize initializing omprace\n");

  omp_racer = new OpenmpRaceDetector();
  
  ompt_set_callback = (ompt_set_callback_t) lookup("ompt_set_callback");
  ompt_get_callback = (ompt_get_callback_t) lookup("ompt_get_callback");
  ompt_get_state = (ompt_get_state_t) lookup("ompt_get_state");
  ompt_get_task_info = (ompt_get_task_info_t) lookup("ompt_get_task_info");
  ompt_get_task_memory = (ompt_get_task_memory_t)lookup("ompt_get_task_memory");
  ompt_get_thread_data = (ompt_get_thread_data_t) lookup("ompt_get_thread_data");
  ompt_get_parallel_info = (ompt_get_parallel_info_t) lookup("ompt_get_parallel_info");
  ompt_get_unique_id = (ompt_get_unique_id_t) lookup("ompt_get_unique_id");
  ompt_finalize_tool = (ompt_finalize_tool_t)lookup("ompt_finalize_tool");

  ompt_get_num_procs = (ompt_get_num_procs_t) lookup("ompt_get_num_procs");
  ompt_get_num_places = (ompt_get_num_places_t) lookup("ompt_get_num_places");
  ompt_get_place_proc_ids = (ompt_get_place_proc_ids_t) lookup("ompt_get_place_proc_ids");
  ompt_get_place_num = (ompt_get_place_num_t) lookup("ompt_get_place_num");
  ompt_get_partition_place_nums = (ompt_get_partition_place_nums_t) lookup("ompt_get_partition_place_nums");
  ompt_get_proc_id = (ompt_get_proc_id_t) lookup("ompt_get_proc_id");
  ompt_enumerate_states = (ompt_enumerate_states_t) lookup("ompt_enumerate_states");
  ompt_enumerate_mutex_impls = (ompt_enumerate_mutex_impls_t) lookup("ompt_enumerate_mutex_impls");

  //register_callback(ompt_callback_mutex_acquire);
  //register_callback_t(ompt_callback_mutex_acquired, ompt_callback_mutex_t);
  //register_callback_t(ompt_callback_mutex_released, ompt_callback_mutex_t);
  //register_callback(ompt_callback_nest_lock);
  register_callback(ompt_callback_sync_region);
  //register_callback_t(ompt_callback_sync_region_wait, ompt_callback_sync_region_t);
  register_callback_t(ompt_callback_reduction, ompt_callback_sync_region_t);
  //register_callback(ompt_callback_control_tool);
  //register_callback(ompt_callback_flush);
  //register_callback(ompt_callback_cancel);
  register_callback(ompt_callback_implicit_task);
  //register_callback_t(ompt_callback_lock_init, ompt_callback_mutex_acquire_t);
  //register_callback_t(ompt_callback_lock_destroy, ompt_callback_mutex_t);
  register_callback(ompt_callback_work);
  register_callback(ompt_callback_master);
  register_callback(ompt_callback_parallel_begin);
  register_callback(ompt_callback_parallel_end);
  register_callback(ompt_callback_task_create);
  register_callback(ompt_callback_task_schedule);
  register_callback(ompt_callback_dependences);
  //register_callback(ompt_callback_task_dependence);
  register_callback(ompt_callback_thread_begin);
  register_callback(ompt_callback_thread_end);
  
  register_callback(ompt_callback_dispatch);
  
  return 1; //success
}

void ompt_finalize(ompt_data_t *tool_data)
{
  printf("0: ompt_event_runtime_shutdown\n");
  //TEMP
  omp_racer->m_fini_race_detector();  
  delete omp_racer;
}

void omprace_init(){
  printf("omprace_init start\n");
  // force initialization of the openmp runtime system
  omp_get_max_threads();  
  // if not forced this line might fail
  
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t serial_thread_id = thread_data->tid;
  //omprace_task_data *t_data = (omprace_task_data*)task_data->ptr;
  ompt_data_t* task_data;
  int exists_task = ompt_get_task_info(0, NULL, &task_data, NULL, NULL, NULL);
  //at this point there is no parallel region
  uint64_t serial_parallel_id = omw_next_parallel_id();
  //however there is a task_data associated with the task_create for the initial master task
  omprace_task_data *s_t_data = (omprace_task_data*)task_data->ptr;

  omp_racer->m_init_racer(serial_thread_id, serial_parallel_id, s_t_data->id);
  omprace_init_flag = 1;
}


void omprace_fini(){
  printf("omprace_fini \n");
  //omp_racer->m_fini_race_detector();  
}

void omprace_section_dispatch_begin(){
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t current_tid = thread_data->tid;
  omp_racer->m_capture_section_begin(current_tid);
  #if DEBUG
  printf("[temp log] section begin dispatch tid= %lu\n", current_tid);
  #endif
}
void omprace_section_dispatch_end(){
  omprace_thread_data* thread_data = (omprace_thread_data*) ompt_get_thread_data()->ptr;
  uint64_t current_tid = thread_data->tid;
  omp_racer->m_capture_section_end(current_tid);
  #if DEBUG
  printf("[temp log] section end dispatch tid= %lu\n", current_tid);
  #endif
}


#ifdef __cplusplus
extern "C" {
#endif
ompt_start_tool_result_t* ompt_start_tool(
  unsigned int omp_version,
  const char *runtime_version)
{
  static ompt_start_tool_result_t ompt_start_tool_result = {&ompt_initialize,&ompt_finalize, 0};
  return &ompt_start_tool_result;
}
#ifdef __cplusplus
}
#endif
