#ifndef __PERF_COUNTER_H_
#define __PERF_COUNTER_H_

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#define NUM_THREADS 16

typedef uint64_t THREADID;

//make sure that /proc/sys/kernel/perf_event_paranoid is set to 1,0, or -1
//this can be done via
//sudo sh -c 'echo 1 >/proc/sys/kernel/perf_event_paranoid'

int perf_event_open_wrapper(struct perf_event_attr *hw_event, pid_t pid,
        int cpu, int group_fd, unsigned long flags);
long long stop_n_get_count (THREADID threadid);
void start_count(THREADID threadid);

static int perf_fds[NUM_THREADS];

int perf_event_open_wrapper(struct perf_event_attr *hw_event, 
			    pid_t pid,
    			    int cpu, 
			    int group_fd, 
			    unsigned long flags)
{
int ret;
ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
    group_fd, flags);
return ret;
}

long long stop_n_get_count (THREADID threadid) {
    #if DEBUG
        return 1;
    #else
        //assert_count--;       
        if (perf_fds[threadid] == 0){
            //std::cout << "stop and get count on already closed perf_fd tid= " << threadid << " will return 0 step" << std::endl;
            return 0;
        }
        assert(perf_fds[threadid] != 0); 
        ioctl(perf_fds[threadid], PERF_EVENT_IOC_DISABLE);
        long long count = 0;
        read(perf_fds[threadid], &count, sizeof(long long));
        close(perf_fds[threadid]);
        perf_fds[threadid] = 0;
        return count;
    #endif
}

void start_count(THREADID threadid) {
    #if DEBUG
        return;
    #else
        //assert_count++;
        if (perf_fds[threadid]!=0){
            //std::cout << "start count for already started counter! tid = " << threadid << std::endl;
            return;
        }
        assert(perf_fds[threadid]==0);
        struct perf_event_attr pe;
        
        memset(&pe, 0, sizeof(struct perf_event_attr));
        pe.type = PERF_TYPE_HARDWARE; 
        pe.size = sizeof(struct perf_event_attr);
        pe.config = PERF_COUNT_HW_INSTRUCTIONS;
        //pe.config = PERF_COUNT_HW_CPU_CYCLES;
        pe.disabled = 1;
        pe.exclude_kernel = 1;
        pe.exclude_hv = 1;
        pe.exclude_idle = 1;
        
        int fd;
        fd = perf_event_open_wrapper(&pe, 0, -1, -1, 0);
        if (fd == -1) {
            fprintf(stderr, "Unable to read performance counters. Linux perf event API not supported on the machine. Error number: %d\n", errno);
            exit(EXIT_FAILURE);
        }
        perf_fds[threadid] = fd;
        
        ioctl(fd, PERF_EVENT_IOC_RESET);
        ioctl(fd, PERF_EVENT_IOC_ENABLE);
    #endif
}

#endif
