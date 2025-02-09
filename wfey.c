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
#include <time.h>

//#define BUSY_POLL
//#define USE_DOORBELL
//#define USE_MONITOR 

#define NSEC_IN_SECOND 1000000000

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

enum SourceEventState { SRC_EVENT_RESET=0, SRC_EVENT_ACTIVE=1 };
union Event {
  char padding[CACHE_LINE_SIZE];
  volatile int state;
};

typedef struct EventProcessor * ep_t;

struct Source {
  ep_t ep;
  double sleep;
  int id;
  union Event event;
};
_Static_assert(sizeof(union Event)==CACHE_LINE_SIZE,
	       "union eventdesc bad size");

typedef struct Source * source_t;
void source_event_reset(source_t this) {
  __atomic_store_n(&(this->event.state), SRC_EVENT_RESET,
		   __ATOMIC_SEQ_CST);
  // FIXME: add end of event time here
}

void source_event_activate(source_t this)  {
  // FIXME: add start of event time here
  __atomic_store_n(&(this->event.state), SRC_EVENT_ACTIVE,
		   __ATOMIC_SEQ_CST);
}

bool source_event_isActive(source_t this)   {
  return __atomic_load_n(&(this->event.state),
			 __ATOMIC_SEQ_CST) == SRC_EVENT_ACTIVE;
}
int            Num_Sources = 0;
struct Source *Sources     = NULL;

typedef volatile uint64_t doorbell_t;
enum { DOORBELL_RESET=0, DOORBELL_PRESSED=1 };
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

union EventSignal {
  char       padding[CACHE_LINE_SIZE];
#ifdef USE_DOORBELL
  doorbell_t db;
#endif
};
_Static_assert(sizeof(union EventSignal)==CACHE_LINE_SIZE,
	       "union EventSignal bad size");

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
  char     name[80];
  snprintf(name, 80, "EP:%p:%d",this,id);
  pinCpu(eparg->cpu, name);
  
#ifdef USE_DOORBELL
  volatile uint64_t *db = &(this->eventSignal.db);
  doorbell_reset(db);
#endif
  
#ifdef USE_MONITOR  
  armMonitor(db);
#endif

  // we are now ready to accept events
  pthread_barrier_wait(eparg->barrier);
  
  while (events < maxevents) {
#ifndef BUSY_POLL
    WFE();
#ifdef USE_MONITOR
    // re-arm immediately to ensure do no loose
    // event signals that happen after process
    // but before going halting on WFE
    armMonitor(db);                     
#endif
#endif
    wakeups++;
#ifdef USE_DOORBELL    
    if (!doorbell_isPressedAndReset(db)) {
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
  source_t this                = sarg->src;
  double   delay               = this->sleep;
  struct timespec thedelay     = { .tv_sec = 0, .tv_nsec = 0 };
  struct timespec ndelay       = { .tv_sec = 0, .tv_nsec = 0 };
  struct timespec nrem         = { .tv_sec = 0, .tv_nsec = 0 };
  int      id                  = this->id;
  char     name[80];

#ifdef USE_DOORBELL
  ep_t     ep                  = this->ep;
  doorbell_t *db = &(ep->eventSignal.db);
#endif
  if (delay >= 1.0) {
    thedelay.tv_sec = (time_t)delay;
    delay = delay - thedelay.tv_sec;
  }
  thedelay.tv_nsec = delay * (double)NSEC_IN_SECOND;

  snprintf(name, 80, "SRC:%p:%d",this,id);
  pinCpu(sarg->cpu, name);
  source_event_reset(this);
  
  while (1) {
    ndelay = thedelay; 
    while (nanosleep(&ndelay,&nrem)<0) {
      ndelay = nrem;
    }
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
  double                  srcsleep;
  
  if (argc < 5) {
    fprintf(stderr, USAGE, argv[0]);
    return(-1);
  }

  // create sources but don't start them 
  Num_Sources = argc - 4;
  Sources     = malloc(sizeof(struct Source)*Num_Sources);
  // create sources
  srcsleep    = strtod(argv[3],NULL);
  for (int i=0; i<Num_Sources; i++) {
    source_t src = &Sources[i];
    src->ep      = &ep;
    src->sleep   = srcsleep;
    src->id      = id; id++;
    source_event_reset(src);
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

  for (int i=0; i<Num_Sources; i++) {
    sarg      = malloc(sizeof(struct SourceThreadArg));
    sarg->src = &(Sources[i]);
    sarg->cpu = atoi(argv[4+i]);
    pthread_create(&tid, NULL, sourceThread, sarg);
  }
  // wait for event processor to finish
  pthread_barrier_wait(&epbarrier);
  return 0;
}
 
