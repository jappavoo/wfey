#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <err.h>

#include "papi.h"

void
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
  
  fprintf(stderr, "%s: PINNED TO CPU: %d\n", str, cpu);

}


void main() {
  PAPI_option_t opts;
  int EventSet = PAPI_NULL;
  long long values[2];


  pinCpu(1, "test cpu");
  
  // Initialize the PAPI library
  int retval = PAPI_library_init(PAPI_VER_CURRENT);
  if (retval != PAPI_VER_CURRENT) {
    fprintf(stderr, "PAPI library init error!\n");
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
  }

    // Create the Event Set
  if (PAPI_create_eventset(&EventSet) != PAPI_OK) {
    fprintf(stderr, "Error creating event set\n");
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
  }

  // Force event set to be associated with component 0 (perf_events component provides all core events)
  // CJ Notes -- wasn't able to find exactly why it is necessary before attach CPU
  retval = PAPI_assign_eventset_component( EventSet, 0 );
  if (retval != PAPI_OK ) {
    fprintf(stderr, "PAPI_assign_eventset_component\n");
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
  }

  // Attach this event set to source cpu
  opts.cpu.eventset = EventSet;
  opts.cpu.cpu_num = 1;

  retval = PAPI_set_opt( PAPI_CPU_ATTACH, &opts ); 
  if ( retval != PAPI_OK ) {
    // fprintf(stderr, "Can't PAPI_CPU_ATTACH: %s\n", PAPI_strerror(retval));
    fprintf(stderr, "Error attaching event set to CPU\n");
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
  }

  // Add Total Cycles
  retval = PAPI_add_event(EventSet, PAPI_TOT_CYC);
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error adding total cycles  event to event set\n");
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
  }
  
  // Add Total Instructions Executed to the Event Set
  retval = PAPI_add_event(EventSet, PAPI_TOT_INS);
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error adding total instructions event to event set\n");
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
  }

  // Start counting events in the Event Set
  retval = PAPI_start(EventSet);
  if (retval != PAPI_OK) {
    fprintf(stderr, "Error starting event counting\n");
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
  }

  sleep(5);

  if ( PAPI_read( EventSet, values ) != PAPI_OK ) {
    fprintf(stderr, "Error reading event counting\n");
    exit(1);
  }

  printf("Papi check : total cycles:%lld, total instructions:%lld\n", values[0], values[1]);
  
  retval = PAPI_stop(EventSet, values);
  if ( retval != PAPI_OK) {
    fprintf(stderr, "Error stopping event counting\n");
    printf("PAPI error %d: %s\n", retval, PAPI_strerror(retval));
    exit(1);
  }

  
  printf("finished fine\n");
  return;
}

// gcc -g -O3 test.c -I/home/cjpar01/wfey/pmu_logging/papi/src/install/include -L/home/cjpar01/wfey/pmu_logging/papi/src/install/lib -o test -lpapi
