#ifndef __WFEY_H__
#define __WFEY_H__

#include <err.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

// ----- Global Memory ----- //

int Num_Sources = 0;
int Num_Processors = 0;
// Mailboxes
// Doorbells

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
//#define BUSY_POLL
//#define USE_DOORBELL
//#define USE_MONITOR
//#define SOCKETSPLIT

/// --- Work Types --- ///
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

/// --- Event States --- ///
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

/// --- MailBox --- ///
struct MailBox_Slot {
  int id;
  int event_num;
  void (*work_func)();
  union Event *event;
};
struct MailBox {
  union EventSignal db;
  struct MailBox_Slot *mb;
};

struct shm_mem {
  int seg_id;
  //key_t shm_key;
  void *data;
};

// 1 mailbox per event processor
struct MailBox *Mailboxes = NULL;

/// --- Event Processors --- ///
typedef struct EventProcessor *ep_t;
struct EPThreadArg {
  ep_t  ep;
  int cpu;
  pthread_barrier_t *barrier;
};
struct EventProcessor {
  int id;
  pthread_t tid;
  struct MailBox *mb;
  struct shm_mem mem;
  union EventSignal eventSignal; //dont know what to do with this in the mailbox case
  volatile sig_atomic_t end_flag;
};


/// --- Event Sources --- ///
typedef struct Source *source_t;
struct SourceThreadArg {
  source_t src;
  int      cpu;
  pthread_barrier_t *barrier;
};
struct Source {
  int id;
  pthread_t tid;
  // ep_t ep;
  int ep_id;
  double sleep;
  enum WorkType work_type;
  void (*work_func)();
  union Event event;
  struct MailBox *mb;
  volatile sig_atomic_t end_flag;
  ts_t start_ts;
  ts_t end_ts;
  uint64_t totalns;
  uint64_t minns;
  uint64_t maxns;
  uint64_t count;
};

/// --- Idle Threads --- ///
struct IdleThreadArg {
  int                cpu;
  pthread_barrier_t *barrier;
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
		 : "=&r" (value) : "r" (&addr));
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
