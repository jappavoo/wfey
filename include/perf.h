#ifndef __PERF_WFEY_H__
#define __PERF_WFEY_H__

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <syscall.h>
#include <linux/perf_event.h>


// PERF ELIMINATION
/* 0 = no performance values, 1 = only PERF events, 2 = PERF events and
 * active/inactive timing */
#define WITHPERF 2
#define TOTAL_PERF_EVENTS 2

//#define BEAST ## If on beast (not dynamic pmu w/ sparks)

struct read_format {
  uint64_t nr;      // number of events
  uint64_t time_enabled;
  uint64_t time_running;
  struct {
    uint64_t value; // value of event
    uint64_t id;
    uint64_t lost;
  } values[TOTAL_PERF_EVENTS];
};

[[maybe_unused]] static long
perf_event_open(struct perf_event_attr *hw_event, pid_t pid,
                int cpu, int group_fd, unsigned long flags)
{
  int ret;

  ret = syscall(__NR_perf_event_open, hw_event, pid, cpu,
		group_fd, flags);

  if (ret == -1) {
    fprintf(stderr, "Error creating event\n");
    exit(EXIT_FAILURE);
  }
  
  return ret;
}

void
configure_perf_event(struct perf_event_attr *pe, uint32_t type, uint64_t config) {
  memset(pe, 0, sizeof(struct perf_event_attr));
  pe->type = type;                                         // type of event
  pe->size = sizeof(struct perf_event_attr); 
  pe->config = config;                                     // type specific config
  pe->read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID | PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING | PERF_FORMAT_LOST;    // values returned in read
  pe->disabled = 1;                                        // off by default
  pe->exclude_kernel = 1;                                  // don't count kernel
}

// Signal Handler for End Signaling
void empty_handler(int sig_no, siginfo_t* info, void *context){};


#endif
