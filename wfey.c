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

#include <linux/perf_event.h>
#include <asm/unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#define NYI { fprintf(stderr, "%s: %d: NYI\n", __func__, __LINE__); assert(0); }

//#define BUSY_POLL
//#define USE_DOORBELL
//#define USE_MONITOR

//#define SOCKETSPLIT

//#define VERBOSE

#define CLOCK_SOURCE CLOCK_MONOTONIC
#define NSEC_IN_SECOND (1000000000)
#define NULL_WORK_COUNT (10000)

#ifdef COHERENCY_LINE_SIZE
// passed at compile time 
#define CACHE_LINE_SIZE COHERENCY_LINE_SIZE
#else
// hardcoded default
#define CACHE_LINE_SIZE 64
#error

#endif

#define USAGE "%s <events per sec> <event processor cpu> <# of source cpu>\n"

// Idling Macros
#define EVENT_CPU_ID 0
#define NUM_EVENT_CPUS 1 // currently locked in at 1, but should be dynamic in the future
#define META_THREAD 1 // just for clarity

pthread_barrier_t idlebarrier;

// Time  handling code
typedef struct timespec ts_t;

// Exit Condition
int flag = 0;

// Sources Random Delay
#define DELAYADD (10) // max percentage of delay to +- delay
#define SECTORUN (30.0)
#define TIMETORUN (SECTORUN*NSEC_IN_SECOND)

#define TOTAL_PERF_EVENTS 2

struct read_format {
  uint64_t nr;      // number of events
  struct {
    uint64_t value; // value of event
    uint64_t id;
  } values[TOTAL_PERF_EVENTS];
};

static long
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
  pe->read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;    // values returned in read
  pe->disabled = 1;                                        // off by default
  pe->exclude_kernel = 1;                                  // don't count kernel
}

static inline int
ts_now(ts_t *now)
{
  if (clock_gettime(CLOCK_SOURCE, now) == -1) {
    perror("clock_gettime");
    NYI;
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
  ts_t start_ts;
  ts_t end_ts;
  uint64_t totalns;
  uint64_t minns;
  uint64_t maxns;
  uint64_t count;
  union Event event;
};
_Static_assert(sizeof(union Event)==CACHE_LINE_SIZE,
	       "union eventdesc bad size");

typedef struct Source * source_t;
void source_event_activate(source_t this)  {
  // FIXME: add start of event time here
  ts_now(&(this->start_ts));
  __atomic_store_n(&(this->event.state), SRC_EVENT_ACTIVE,
		   __ATOMIC_SEQ_CST);
}

bool source_event_isActive(source_t this)   {
  return __atomic_load_n(&(this->event.state),
			 __ATOMIC_SEQ_CST) == SRC_EVENT_ACTIVE;
}

void source_event_reset(source_t this) {
  __atomic_store_n(&(this->event.state), SRC_EVENT_RESET,
		   __ATOMIC_SEQ_CST);
  // TODO there should probably~ be a lock involved here :p
  // min might be wrong! or this process can be really fast
  
  if (source_event_isActive(this)) { // Give up if this is true
    return;
  }
  
  ts_now(&(this->end_ts));

  uint64_t ns = ts_diff(this->start_ts, this->end_ts);

  if ( (int64_t)ns <= 0 ) { // Give up again -- shenanigans occured
    return;
  }

  this->totalns += ns;
  this->count++;

  if ( ns < this->minns ) { this->minns = ns; }
  if ( ns > this->maxns ) { this->maxns = ns; }
#ifdef VERBOSE
  fprintf(stderr, "%d: Diff=%ld, TotalNS=%ld, Count=%ld, Min=%lu, Max=%lu\n",
	  this->id, ns, this->totalns, this->count, this->minns, this->maxns);
#endif
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
  int id;
};

static inline void nullwork() {
  for (uint64_t i=0; i<NULL_WORK_COUNT; i++) {
    asm volatile ("nop");
  }
}

void
EP_handle_event(ep_t this, source_t src)
{
#ifdef VERBOSE
  fprintf(stderr, "%s: this=%p(%d) src=%p(%d)\n",
	  __FUNCTION__, this, this->id, src, src->id);
#endif
  // working hard on the event ;-)
  nullwork();
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
  uint64_t wakeups           = 0;
  uint64_t spurious          = 0;
  uint64_t events            = 0;
  char     name[80];

  ts_t start_time;
  ts_t current_time;
  uint64_t elapsed_time = 0.0;

  ts_t active_cycle_start;
  ts_t active_cycle_end;
  uint64_t active_cycle_total = 0.0;
    
  ts_t inactive_cycle_start;
  ts_t inactive_cycle_end;
  uint64_t inactive_cycle_total = 0.0;

#ifdef BUSY_POLL
  (void) inactive_cycle_start;
  (void) inactive_cycle_end;
#endif

  /* ------- Setting up PERF counters ------- */

  int pe_fd[TOTAL_PERF_EVENTS];                     // pe_fd[0] will be the group leader file descriptor
  int pe_id[TOTAL_PERF_EVENTS];                     // event pe_ids for file descriptors
  uint64_t pe_val[TOTAL_PERF_EVENTS];            // Counter value array corresponding to fd/id array.
  struct perf_event_attr pe[TOTAL_PERF_EVENTS];  // Configuration structure for perf events 
  struct read_format counter_results;

  // Configure the group of PMUs to count
  configure_perf_event(&pe[0], PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  configure_perf_event(&pe[1], PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);

  // Create event group leader
  pe_fd[0] = perf_event_open(&pe[0], 0, eparg->cpu, -1, 0);
  ioctl(pe_fd[0], PERF_EVENT_IOC_ID, &pe_id[0]);
  // Create the rest of the events -- with pe_fd[0] as the group leader
  for(int i = 1; i < TOTAL_PERF_EVENTS; i++){
    pe_fd[i] = perf_event_open(&pe[i], 0, eparg->cpu, pe_fd[0], 0);
    ioctl(pe_fd[i], PERF_EVENT_IOC_ID, &pe_id[i]);
  }

  ioctl(pe_fd[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  
  /* ---------------------------------------- */
  
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

  ts_now(&(active_cycle_start));
  ts_now(&(start_time));

  ioctl(pe_fd[0], PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
  
  while ( elapsed_time < TIMETORUN ) {
#ifndef BUSY_POLL
    ts_now(&(inactive_cycle_start));
    WFE();
    ts_now(&(inactive_cycle_end));
    inactive_cycle_total += ts_diff(inactive_cycle_start, inactive_cycle_end);
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
    
    ts_now(&(current_time));
    elapsed_time = ts_diff(start_time, current_time);
  }
  // all done

  // Stop perf counters
  ioctl(pe_fd[0], PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  
  ts_now(&(active_cycle_end));

  active_cycle_total = ts_diff(active_cycle_start, active_cycle_end);
  
  fprintf(stderr, "%s:%p:%d wakeups=%ld spurious=%ld events=%ld\n",
	  __FUNCTION__,this, id, wakeups, spurious, events);
  fprintf(stderr, "total active cycles:%ld, total inactive cycles:%ld, diff:%ld\n", active_cycle_total, inactive_cycle_total, active_cycle_total-inactive_cycle_total);


  // Read the group of perf counters
  read(pe_fd[0], &counter_results, sizeof(struct read_format));
  printf("Num events captured: %"PRIu64"\n", counter_results.nr);
  for(int i = 0; i < counter_results.nr; i++) {
    for(int j = 0; j < TOTAL_PERF_EVENTS ;j++){
      if(counter_results.values[i].id == pe_id[j]){
        pe_val[i] = counter_results.values[i].value;
      }
    }
  }
  printf("CPU cycles: %"PRIu64"\n", pe_val[0]);
  printf("Instructions retired: %"PRIu64"\n", pe_val[1]);

  // Close counter file descriptors
  for(int i = 0; i < TOTAL_PERF_EVENTS; i++){
    close(pe_fd[i]);
  }
  
  pthread_barrier_wait(eparg->barrier);
  return NULL;
}

struct SourceThreadArg {
  source_t src;
  int      cpu;
  pthread_barrier_t *barrier;
};

void * sourceThread(void *arg) {
  struct SourceThreadArg *sarg = arg;
  source_t this                = sarg->src;
  double   delay               = this->sleep;
  ts_t     thedelay            = { .tv_sec = 0, .tv_nsec = 0 };
  ts_t     ndelay              = { .tv_sec = 0, .tv_nsec = 0 };
  ts_t     nrem                = { .tv_sec = 0, .tv_nsec = 0 };
  int      id                  = this->id;
  char     name[80];

#ifdef USE_DOORBELL
  ep_t     ep                  = this->ep;
  doorbell_t *db = &(ep->eventSignal.db);
#endif

  // Add random delay to sleep time -- ?: should this delay be randomized every time? too much float math i think
  double mod_perc = (double)(rand()%(DELAYADD + 1))/100.0; // rand()%((max+1)-min) + min
  double delay_modifier = delay * mod_perc;
  delay = ( (rand()%2) == 0 ) ? delay + delay_modifier : delay - delay_modifier;
  
  
  if (delay >= 1.0) {
    thedelay.tv_sec = (time_t)delay;
    delay = delay - thedelay.tv_sec;
  }
  thedelay.tv_nsec = delay * (double)NSEC_IN_SECOND;

  snprintf(name, 80, "SRC:%p:%d",this,id);
  
  pinCpu(sarg->cpu, name);

  pthread_barrier_wait(sarg->barrier); // waits for all srcs to be ready then sends events
  
  while ( !flag ) {
    ndelay = thedelay; 
    while (nanosleep(&ndelay,&nrem)<0) {
      ndelay = nrem;
    }
    source_event_activate(this);
#ifdef USE_DOORBELL
    doorbell_press(db);
#ifndef USE_MONITOR // To use the monitor the doorbell must be on
    SEV();
#endif
#endif
  }
  free(sarg);
  return NULL;
}


struct IdleThreadArg {
  int                cpu;
  pthread_barrier_t  *barrier;
};

void * idleThread(void *arg) {
  struct IdleThreadArg *iarg = (struct IdleThreadArg *)arg;
  char name[80];

  snprintf(name, 80, "IDLE:%d",iarg->cpu);

  pinCpu(iarg->cpu, name);

  while ( !flag ) {
    WFI();
  }

  pthread_barrier_wait(&idlebarrier);
  free(iarg);
  return NULL;
}

static inline void getCPUInfo( uint64_t  *cps, uint64_t *socks ) {
  FILE *cps_pipe, *socks_pipe;
  char temp[256];

  cps_pipe = popen("./scripts/find_cores_per_socket.sh", "r");
  socks_pipe = popen("./scripts/find_num_sockets.sh", "r");

  if ( (fgets(temp, 255, cps_pipe)) == NULL ) {
    err(1, "ERROR reading cores per socket\n");
  }
  *cps = atoi(temp);

  if ( (fgets(temp, 255, socks_pipe)) == NULL ) {
    err(1, "ERROR reading number of sockets\n");
  }
  *socks = atoi(temp);
}

int
main(int argc, char **argv)
{
  /* ------- Variables ------- */
  int                     id=0;
  pthread_t               tid;
  pthread_barrier_t       epbarrier;
  struct EventProcessor   ep;
  struct EPThreadArg      eparg;
  struct SourceThreadArg *sarg;
  double                  srcsleep;
  pthread_barrier_t       srcbarrier;
  uint64_t cores_per_socket, sockets;
  uint64_t source_end;
  uint64_t runner_cpu, total_cpu;
  
  if (argc < 4) {
    fprintf(stderr, USAGE, argv[0]);
    return(-1);
  }

  getCPUInfo(&cores_per_socket, &sockets);
  total_cpu = cores_per_socket * sockets;

  /* ------- Init Event Processor ------- */
  // initalize event processor (only one right now)
  ep.id        = id; id++;
  // eventSignal initalization will be handled by the EP thread
  // intialize event processing thread
  pthread_barrier_init(&epbarrier, NULL, 2);
  eparg.ep      = &ep;
  eparg.barrier = &epbarrier;
  eparg.cpu     = EVENT_CPU_ID; //atoi(argv[2]);


  /* ------- Init Sources ------- */
  Num_Sources = atoi(argv[3]);   // create sources but don't start them
  Sources     = malloc(sizeof(struct Source)*Num_Sources);

  // Rate of Events per source is NumEventsPerSec == argv[1]
  // divided by the number of sources to get per event rate
  // srcsleep    = strtod(argv[3],NULL);
  uint64_t event_rate            = atoi(argv[1]);
  uint64_t event_rate_per_source = event_rate / Num_Sources;
  srcsleep                       = 1.0/event_rate_per_source;
  
  srand((unsigned)time(NULL)); // uniquely setting rand val seed
  
  for (int i=0; i<Num_Sources; i++) {
    source_t src = &Sources[i];
    src->ep      = &ep;
    src->sleep   = srcsleep;
    src->id      = id; id++;
    source_event_reset(src);
    src->totalns = 0;
    src->minns   = (uint64_t)-1;
    src->maxns   = 0;
    src->count   = 0;
  }


  /* ------- Init and Start Idle Cores ------- */
  struct IdleThreadArg *iarg;
  uint64_t num_idle, cpuid;

  num_idle = total_cpu - ( NUM_EVENT_CPUS + Num_Sources + META_THREAD );
  
#ifdef SOCKETSPLIT
  /* This splits the event processor(s) onto 1 socket and sources into another */
  /*
      ----------------   ----------------
      |  Event Proc   | |  Runner+Main  |
      |  Idle Threads | |  Sources      |
      ----------------  |  Idle Threads |
                        -----------------
   */
  
  // first node should have only event handlers on it, rest should be idle
  // second node will have the sources and one node that will run main + scripts
  if ( sockets < 2 ) {
    fprintf( stderr, "There is not another socket to split work across\nTurn off SOCKETSPLIT functionality\n");
    exit(1);
  } else if ( sockets > 2 ) {
    printf( "The case where there are more than 2 sockets has not been thought of in depth -- proceed with caution\n");
  }

  /* Error Checking */
  if ( NUM_EVENT_CPUS > cores_per_socket ) {
    fprintf( stderr, "There are too many event processors to fit on one socket -- Try <%ld\n", cores_per_socket);
    exit(1);
  }
  if ( Num_Sources > (cores_per_socket-META_THREAD) ) {
    fprintf( stderr, "There are too many source threads to fit on one socket -- Try <%ld\n", (cores_per_socket-1));
    exit(1);
  }

  // Starting point -- What cores should the source threads run on
  cpuid = cores_per_socket + META_THREAD;
  // Ending point -- Where the idle threads should start
  source_end = cpuid + Num_Sources;
  // Where Main and Runners should run
  runner_cpu = cores_per_socket; // first core on 2nd socket

  if (num_idle != 0) { pthread_barrier_init(&idlebarrier, NULL, num_idle); }
  
  /* Running Idle Threads on Socket 1 */
  // All but the event processors on the first node
  for (int i = NUM_EVENT_CPUS; i < cores_per_socket; i++){
    iarg = malloc(sizeof(struct IdleThreadArg));
    iarg->cpu = i;
    iarg->barrier = &idlebarrier;
    pthread_create(&tid, NULL, idleThread, iarg);
  }
  /* Running Idle Threads on Socket 2 */
  // start at sock2 & after all other after sources + runner
  for (int i = source_end; i < total_cpu; i++){
    iarg = malloc(sizeof(struct IdleThreadArg));
    iarg->cpu = i;
    iarg->barrier = &idlebarrier;
    pthread_create(&tid, NULL, idleThread, iarg);
  }
  
#else
  // All the events and the sources occur on a single core (*ish)

  if ( sockets < 2 ) { // There is only one socket so everything has to do work here
    /*
      -------------------  
      |  Event Proc     |
      |  Source Threads |
      |  Idle Threads   |
      |  Runner/Main    |
      -------------------  
    */

    /* Error Checking */
    if ( (NUM_EVENT_CPUS + Num_Sources) > (cores_per_socket - META_THREAD) ) {
      fprintf( stderr, "There are too many event processors & source threads to fit on one socket together -- Try a total of <%ld\n", cores_per_socket-META_THREAD);
      exit(1);
    }
    
    cpuid = NUM_EVENT_CPUS;
    source_end = cpuid + Num_Sources;
    runner_cpu = cores_per_socket - 1; // last core on socket

    if (num_idle != 0) { pthread_barrier_init(&idlebarrier, NULL, num_idle); }
    
    for (int i = source_end; i < (total_cpu-META_THREAD); i++){ 
      iarg = malloc(sizeof(struct IdleThreadArg));
      iarg->cpu = i;
      iarg->barrier = &idlebarrier;
      pthread_create(&tid, NULL, idleThread, iarg);
    }
    
  } else { // Can put runner scripts on different socket
      /*
      ----------------   ----------------
      |  Event Proc   | |  Runner+Main  |
      |  Sources      | |  Idle Threads |
      |  Idle Threads | -----------------
      ----------------  
   */

    /* Error Checking */
    if ( (NUM_EVENT_CPUS + Num_Sources) > cores_per_socket ) {
      fprintf( stderr, "There are too many event processors & source threads to fit on one socket -- Try a total of <%ld\n", cores_per_socket);
      exit(1);
    }
    if ( META_THREAD > cores_per_socket ) {
      fprintf( stderr, "There are too many runner threads to fit on one socket (HOW DID THIS HAPPEN) -- Try <%ld\n", cores_per_socket);
      exit(1);
    }
    
    cpuid = NUM_EVENT_CPUS;
    source_end = cpuid + Num_Sources;
    runner_cpu = cores_per_socket; // first core on 2nd socket
    
    
    if (num_idle != 0) { pthread_barrier_init(&idlebarrier, NULL, num_idle); }

    for (int i = source_end; i < cores_per_socket; i++){ // All but the event processors on the first node
      iarg = malloc(sizeof(struct IdleThreadArg));
      iarg->cpu = i;
      iarg->barrier = &idlebarrier;
      pthread_create(&tid, NULL, idleThread, iarg);
    }
    /* Running Idle Threads on Socket 2 */
    for (int i = (cores_per_socket + META_THREAD); i < total_cpu; i++){
      iarg = malloc(sizeof(struct IdleThreadArg));
      iarg->cpu = i;
      iarg->barrier = &idlebarrier;
      pthread_create(&tid, NULL, idleThread, iarg);
    }
  }
#endif
  
  pinCpu(runner_cpu, "main thread"); 


  /* ------- Run Event Processor ------- */
  pthread_create(&tid, NULL, epThread, &eparg);

  // wait for event processor to be ready
  pthread_barrier_wait(&epbarrier);
  pthread_barrier_init(&epbarrier, NULL, 2);  // reset the barrier 

  /* ------- Run Source Threads ------- */
  pthread_barrier_init(&srcbarrier, NULL, Num_Sources); // set up barrier for all src to init
  for (int i=0; i<Num_Sources; i++) {
    sarg      = malloc(sizeof(struct SourceThreadArg));
    sarg->src = &(Sources[i]);
    //sarg->cpu = atoi(argv[4+i]);
    sarg->cpu = cpuid++;
    sarg->barrier = &srcbarrier;
    pthread_create(&tid, NULL, sourceThread, sarg);
  }
  // wait for event processor to finish
  pthread_barrier_wait(&epbarrier);

  flag = 1;
  pthread_barrier_wait(&idlebarrier);
  
  /* ------- Latency Logging ------- */
  fprintf(stdout, "ID,Min,Max,Mean\n");
  for (int i=0; i<Num_Sources; i++) {
    source_t src = &Sources[i];
    if(src->count == 0) { continue; } // never got a chance to finish event
    uint64_t mean = (src->totalns / src->count);
    fprintf(stdout, "%d,%"PRIu64",%"PRIu64",%"PRIu64"\n",
	    src->id, src->minns, src->maxns, mean);
  }
  
  free(Sources);
  return 0;
}
 
