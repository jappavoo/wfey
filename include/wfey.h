#ifndef __WFEY_H__
#define __WFEY_H__

#include <err.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

// ----- Doorbell Logic ----- //
typedef volatile uint64_t doorbell_t;
enum { DOORBELL_RESET = 0, DOORBELL_PRESSED = 1 };

static inline doorbell_t doorbell_isPressedAndReset(doorbell_t *db)
{
  return __atomic_exchange_n(db, DOORBELL_RESET, __ATOMIC_SEQ_CST);
}

static inline void doorbell_reset(doorbell_t *db)
{
  __atomic_store_n(db, DOORBELL_RESET, __ATOMIC_SEQ_CST);
}

static inline void doorbell_press(doorbell_t *db)
{
  __atomic_store_n(db, DOORBELL_PRESSED, __ATOMIC_SEQ_CST);
}

// ----- BENCHMARK Logic ----- //
// 
//#define BUSY_POLL
//#define USE_DOORBELL
//#define USE_MONITOR

//#define SOCKETSPLIT

enum WorkType { COMP_WORK = 1, MEM_WORK = 2, IO_WORK = 3, NULL_WORK = -1 };

const char* printWorkType(enum WorkType wt) {
  switch (wt) {
  case 1:
    return "comp work";
  case 2:
    return "mem work";
  case 3:
    return "io work";
  case -1:
    return "null work";
  default:
    return "ERROR";
  }
}

enum SourceEventState { SRC_EVENT_RESET = 0, SRC_EVENT_ACTIVE = 1 };

union Event {
  char padding[CACHE_LINE_SIZE];
  volatile int state;
};
union EventSignal {
  char       padding[CACHE_LINE_SIZE];
#ifdef USE_DOORBELL
  doorbell_t db;
#endif
};
_Static_assert(sizeof(union Event) == CACHE_LINE_SIZE,
               "union eventdesc bad size");
_Static_assert(sizeof(union EventSignal) == CACHE_LINE_SIZE,
               "union EventSignal bad size");

int Num_Sources = 0;
int Num_Processors = 0;

struct Source *Sources            = NULL;
struct EventProcessor *Processors = NULL;

typedef struct EventProcessor *ep_t;
struct EventProcessor {
  union EventSignal eventSignal;
  int id;
  pthread_t tid;
  volatile sig_atomic_t end_flag;
};
struct EPThreadArg {
  ep_t  ep;
  int cpu;
  pthread_barrier_t *barrier;
};

typedef struct Source *source_t;
struct SourceThreadArg {
  source_t src;
  int      cpu;
  pthread_barrier_t *barrier;
};
struct Source {
  ep_t ep;
  double sleep;
  int id;
  pthread_t tid;
  enum WorkType work_type;
  void (*work_func)();
  volatile sig_atomic_t end_flag;
  ts_t start_ts;
  ts_t end_ts;
  uint64_t totalns;
  uint64_t minns;
  uint64_t maxns;
  uint64_t count;
  union Event event;
};

struct IdleThreadArg {
  int                cpu;
  pthread_barrier_t  *barrier;
};

// ----- WFE Logic ----- //

static inline void
SEV()
{
  asm volatile("sev \n\t");
}

static inline void
WFE() 
{
  asm volatile("wfe \n\t");
}

static inline void
WFI()
{
  asm volatile("wfi \n\t");
}

static inline uint64_t 
armMonitor(volatile void *addr)
{
  uint64_t value;
  asm volatile( "ldaxr %0, [%1]\n\t"
		 : "=&r" (value) : "r" (addr));
  return value;
}

static inline void getCPUInfo( uint64_t  *cps, uint64_t *socks ) {
  FILE *cps_pipe, *socks_pipe;
  char temp[256];

  if ((cps_pipe = popen("./scripts/find_cores_per_socket.sh", "r")) == NULL) {
    handle_error("Cores Per Socket Pipe open failed");
  }
  if ((socks_pipe = popen("./scripts/find_num_sockets.sh", "r")) == NULL) {
    handle_error("Num Sockets Pipe opebn failed");
  }

  if ( (fgets(temp, 255, cps_pipe)) == NULL ) {
    err(1, "ERROR reading cores per socket\n");
  }
  *cps = atoi(temp);

  if ( (fgets(temp, 255, socks_pipe)) == NULL ) {
    err(1, "ERROR reading number of sockets\n");
  }
  *socks = atoi(temp);

  if (pclose(cps_pipe) == -1) {
    handle_error("Closing CPS Pipe failed");
  }
  if (pclose(socks_pipe) == -1) {
    handle_error("Closing Socks Pipe failed");
  }
}

#endif
