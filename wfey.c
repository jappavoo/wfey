#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sched.h>
#include <err.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>

#ifdef COHERENCY_LINE_SIZE
// passed at compile time 
#define CACHE_LINE_SIZE COHERENCY_LINE_SIZE
#else
// hardcoded default
#define CACHE_LINE_SIZE 64
#error

#endif

#define USAGE "%s <iterations> <cpu0> <cpu1>\n"
#define VERBOSE

void
pinCpu(int cpu)
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
    fprintf(stderr, "PINNED TO CPU: %d\n", cpu);
#endif    
}

union ServerThreadArg {
  char padding[CACHE_LINE_SIZE];
  struct {
    volatile uint64_t ready;
    int cpu;
  };
};

void * serverThread(void *arg) {
  union ServerThreadArg *sarg = arg;
  pinCpu(sarg->cpu);

  sleep(0.5);
#if 0
  //    __sync_fetch_and_add(&(sarg->ready),-1);
  asm volatile("sev"              "\n\t");
#else
  sarg->ready = 0;
#endif
  while (1);
  
  return NULL;
}


int
main(int argc, char **argv)
{
  uint64_t count = 0;
  int iters;
  int cpu0, cpu1;
  pthread_t serverTid;
  union ServerThreadArg sarg;

  if (argc != 4) {
    fprintf(stderr, USAGE, argv[0]);
    return(-1);
  }
    
  iters = atoi(argv[1]);
  count = iters;
  cpu0 = atoi(argv[2]);
  cpu1 = atoi(argv[3]);
  
  pinCpu(cpu0);
  sarg.ready = 1; sarg.cpu = cpu1;
  
  pthread_create(&serverTid, NULL, serverThread, &sarg);

  uint64_t result;
  asm volatile("sevl"              "\n\t"
	       "__retry: wfe"      "\n\t"
	       "ldaxr %0, [%2]"    "\n\t"
	       "add   %1, %1, #1"      "\n\t"
	       "cbnz  %0, __retry" "\n\t"
	       : "=&r" (result),"=&r" (count) : "r" (&sarg.ready));
  fprintf(stderr, "GOT HERE %ld\n", count);
  //  while (sarg.ready==0) ;
  while(1) ;
  
  return 0;
}
 
