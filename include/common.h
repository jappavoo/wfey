#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <sched.h>
#include <err.h>
#include <pthread.h>
#include <stdint.h>

#define NYI                                                                    \
  {                                                                            \
    fprintf(stderr, "%s: %d: NYI\n", __func__, __LINE__);                      \
    assert(0);                                                                 \
  }
#define USAGE(func)							\
  do {									\
    fprintf(stderr, "%s -e <events per sec> -p <event processor cpu> -s <# of source cpu> (-w <perc memwork>)\n", \
      func);								\
  } while (0)
  
#define handle_error(msg)                                                      \
  do {                                                                         \
    perror(msg);                                                               \
    exit(EXIT_FAILURE);                                                        \
  } while (0)

#define CLOCK_SOURCE CLOCK_MONOTONIC
#define NSEC_IN_SECOND (1000000000)
#define USEC_IN_SECOND (1000000)

#define WORK_COUNT (10000)

// Idling Macros
#define EVENT_CPU_ID 1
#define NUM_EVENT_CPUS 1 // currently locked in at 1, but should be dynamic in the future
#define RUNNER_CPU 0 //core 0 for everything extra
#define META_THREAD 1 // just for clarity -- should be renamed to num meta threads

#define LATENCY_FD 3

// Sources Random Delay
#define DELAYADD (10) // max percentage of delay to +- delay
#define SECTORUN (30.0)
#define TIMETORUN (SECTORUN*USEC_IN_SECOND)


#ifdef COHERENCY_LINE_SIZE
// passed at compile time 
#define CACHE_LINE_SIZE COHERENCY_LINE_SIZE
#else
// hardcoded default
#define CACHE_LINE_SIZE 64
#error "Please specify the cache line size"
#endif

#ifdef COHERENCY_L1_SIZE
// passed at compile time 
#define L1_SIZE COHERENCY_L1_SIZE
#else
// hardcoded default (KB)
#define L1_SIZE 64
#endif

#ifdef COHERENCY_L2_SIZE
// passed at compile time 
#define L2_SIZE COHERENCY_L2_SIZE
#else
// hardcoded default (KB)
#define L2_SIZE 1024
#endif


pthread_barrier_t endbarrier;
pthread_barrier_t idlebarrier; // TODO remove when all instances removed
typedef struct timespec ts_t;


static inline int
ts_now(ts_t *now)
{
  if (clock_gettime(CLOCK_SOURCE, now) == -1) {
    perror("clock_gettime");
    return 0;
  }
  return 1;
}

// Return the difference between the times in nanoseconds
static inline uint64_t
ts_diff(ts_t start, ts_t end)
{
  uint64_t diff=((end.tv_sec - start.tv_sec)*NSEC_IN_SECOND) + (end.tv_nsec - start.tv_nsec);
  return diff;
}


static inline void
pinCpu(int cpu, char *str)
{
  cpu_set_t  mask;
  CPU_ZERO(&mask);

  if (cpu == -1 ) {
    cpu = sched_getcpu();
    if (cpu == -1) {
      err(1, "could not determine current cpu");
    }
  }

  CPU_SET(cpu, &mask);
  if (sched_setaffinity(0, sizeof(mask), &mask) != 0) {
    err(1, "could not pin to cpu=%d",cpu);
  }
  
#ifdef VERBOSE
  fprintf(stderr, "%s: PINNED TO CPU: %d\n", str, cpu);
#endif    
}

#endif
