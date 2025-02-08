#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <sched.h>
#include <err.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>

#define USE_DOORBELL
#define USE_MONITOR 

#ifdef COHERENCY_LINE_SIZE
// passed at compile time 
#define CACHE_LINE_SIZE COHERENCY_LINE_SIZE
#else
// hardcoded default
#define CACHE_LINE_SIZE 64
#error

#endif

#define USAGE "%s <max events> <event processor cpu> <source cpu> [[source cpu]...]\n"
#define VERBOSE

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

static inline uint64_t 
armMonitor(volatile void *addr)
{
  uint64_t value;
  asm volatile( "ldaxr %0, [%1]\n\t"
		 : "=&r" (value) : "r" (&addr));
  return value;
}

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

enum SourceEventState { RESET=0, ACTIVE=1 };
union Event {
  char padding[CACHE_LINE_SIZE];
  volatile int state;
};

typedef struct EventProcessor * ep_t;

struct Source {
  ep_t ep;
  int id;
  union Event event;
};
_Static_assert(sizeof(union Event)==CACHE_LINE_SIZE,
	       "union eventdesc bad size");

typedef struct Source * source_t;
void source_event_reset(source_t this) {
  assert(__sync_bool_compare_and_swap(&(this->event.state),ACTIVE,RESET));
  // FIXME: add end of event time here
}

void source_event_activate(source_t this)  {
  // FIXME: add start of event time here
  assert(__sync_bool_compare_and_swap(&(this->event.state),RESET,ACTIVE));
}

bool source_event_isActive(source_t this)   {
  return this->event.state == ACTIVE;
}
int            Num_Sources = 0;
struct Source *Sources     = NULL;

typedef volatile uint64_t doorbell_t;
enum { DOORBELL_RESET=0, DOORBELL_PRESSED=1 };
static inline doorbell_t doorbell_reset(doorbell_t *db)
{
  return __sync_val_compare_and_swap(db, DOORBELL_PRESSED, DOORBELL_RESET);
}

static inline doorbell_t doorbell_press(doorbell_t *db)
{
  return __sync_val_compare_and_swap(db, DOORBELL_RESET, DOORBELL_PRESSED);
}

union EventSignal {
  char       padding[CACHE_LINE_SIZE];
#ifdef USE_DOORBELL
  doorbell_t db;
#endif
};

//_Static_assert(sizeof(union EventSignal)==CACHE_LINE_SIZE),
//	       "union EventSignal bad size");

struct EventProcessor {
  union EventSignal eventSignal;
  int maxevents;
  int id;
};

void
EP_handle_event(ep_t this, source_t src)
{
#ifdef VERBOSE
  fprintf(stderr, "%s: this=%p(%d) src=%p(%d)\n",
	  __FUNCTION__, this, this->id, src, src->id);
#endif
  // working hard on the event ;-)
  
  source_event_reset(src);
}

struct EPThreadArg {
  ep_t  ep;
  pthread_barrier_t *barrier;
  int   cpu;
};
  
void *
epThread(void *arg)
{
  struct EPThreadArg * eparg = arg;
  ep_t     this              = eparg->ep;
  int      id                = this->id;
  int      maxevents         = this->maxevents;
  uint64_t wakeups           = 0;
  uint64_t spurious          = 0;
  uint64_t events            = 0;
  uint64_t dbval;
  
  pinCpu(eparg->cpu);
  
#ifdef USE_DOORBELL
  volatile uint64_t *db = &(this->eventSignal.db);
  (void)doorbell_reset(db);
#endif
  
#ifdef USE_MONITOR  
  assert(armMonitor(db)==0);
#endif

  // we are now ready to accept events
  pthread_barrier_wait(eparg->barrier);
  
  while (events < maxevents) {
#ifndef BUSY_POLL
    WFE();
#ifdef USE_MONITOR
    armMonitor(db); // re-arm immediately
#endif
#endif
    wakeups++;
#ifdef USE_DOORBELL    
    dbval = doorbell_reset(db);
    if (dbval != DOORBELL_PRESSED) {
      // spurious wakeup
      spurious++;
      continue;
    }
#endif
    for (int i=0; i<Num_Sources; i++) {
      source_t src = &(Sources[i]);
      if (source_event_isActive(src)) {
	events++;
	EP_handle_event(this, src);
      }
    }
  }
  // all done
  fprintf(stderr, "%s:%p:%d wakeups=%ld spurious=%ld events=%ld\n",
	  __FUNCTION__,this, id, wakeups, spurious, events);
  pthread_barrier_wait(eparg->barrier);
  return NULL;
}

struct SourceThreadArg {
  source_t src;
  int      cpu;
};

void * sourceThread(void *arg) {
  struct SourceThreadArg *sarg = arg;
  source_t this  = sarg->src;
  ep_t     ep    = this->ep;
#ifdef USE_DOORBELL
  doorbell_t *db = &(ep->eventSignal.db);
#endif
  
  pinCpu(sarg->cpu);

  while (1) {
    sleep(1.0);
    source_event_activate(this);
#ifdef USE_DOORBELL
    doorbell_press(db);
#endif
#ifndef USE_MONITOR
    SEV();
#endif
  }
  
  return NULL;
}


int
main(int argc, char **argv)
{
  int                     id=0;
  pthread_t               tid;
  pthread_barrier_t       epbarrier;
  struct EventProcessor   ep;
  struct EPThreadArg      eparg;
  struct SourceThreadArg *sarg;
  
  if (argc < 4) {
    fprintf(stderr, USAGE, argv[0]);
    return(-1);
  }

  // initalize event processor (only one right now)
  ep.id        = id; id++;
  ep.maxevents = atoi(argv[1]);
  if (ep.maxevents<=0) ep.maxevents = 1; 
  // eventSignal initalization will be handled by the EP thread

  // intialize event processing thread
  pthread_barrier_init(&epbarrier, NULL, 2);
  eparg.ep      = &ep;
  eparg.barrier = &epbarrier;
  eparg.cpu     = atoi(argv[2]);
  pthread_create(&tid, NULL, epThread, &eparg);

  // wait for event processor to be ready
  pthread_barrier_wait(&epbarrier);
  pthread_barrier_init(&epbarrier, NULL, 2);  // reset the barrier 

  // create sources
  Num_Sources = argc - 3;
  Sources = malloc(sizeof(struct Source)*Num_Sources);
  for (int i=0; i<Num_Sources; i++) {
    source_t src = &Sources[i];
    src->ep      = &ep;
    src->id      = id; id++;
    // source thread will initlize its event;
    sarg      = malloc(sizeof(struct SourceThreadArg));
    sarg->src = src;
    sarg->cpu = atoi(argv[3+i]);
    pthread_create(&tid, NULL, sourceThread, &sarg);
  }
  // wait for event processor to finish
  pthread_barrier_wait(&epbarrier);
  return 0;
}
 
