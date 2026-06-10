#include <stddef.h>
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
#include <signal.h>
#include <sys/mman.h>
#include <math.h>
#include <fcntl.h>

#include "include/wfey.h"
#include "include/perf.h"
#include "include/wfey_hwmon.h"


//#define VERBOSE

void source_event_activate(source_t this)  {
  // if already active then don't continue
  // already processing an event
  // relay this back to source so no signal sent
  ts_now(&(this->start_ts));
  __atomic_store_n(&(this->event.state), SRC_EVENT_ACTIVE,
		   __ATOMIC_SEQ_CST);
}

bool source_event_isActive(source_t this)   {
  return __atomic_load_n(&(this->event.state),
			 __ATOMIC_SEQ_CST) == SRC_EVENT_ACTIVE;
}

void source_event_reset(source_t this) {
  ts_now(&(this->end_ts));
  // todo make local then add to log array per source
  // this->log[i].start
  // end if 0 if miss -- 
  __atomic_store_n(&(this->event.state), SRC_EVENT_RESET,
		   __ATOMIC_SEQ_CST);
  // TODO there should probably~ be a lock involved here :p
  // min might be wrong! or this process can be really fast
  
  /* if (source_event_isActive(this)) { // Give up if this is true */
  /*   return; */
  /* } */
  

  uint64_t ns = ts_diff(this->start_ts, this->end_ts);

  // become an assert TODO
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

static inline double compwork() {
  int x, y;
  int retval;
  for (uint64_t i = 0; i < WORK_COUNT; i++) {
    x = rand();
    y = rand();
    volatile double output_1 = 0.5 * (sin(x + y) - sin(x - y));
    volatile double output_2 = cos(rand()) * sin(y);

    if ((output_1 == output_2) && (retval == 0)) {
      retval = 1;
    }
  }
  return retval;
}

static inline void iowork() {
  int fd, rand_fd;
  char randomdata[50];
  ssize_t datalen, result = 0;
  
  for (uint64_t i = 0; i < WORK_COUNT; i++) {
    if ((fd = open("foo", O_TMPFILE | O_RDWR, S_IRUSR | S_IWUSR)) == -1) {
      perror("opening memwork file failed");
    }
    if ((rand_fd = open("/dev/urandom", O_RDONLY)) == -1) {
      perror("opening urandom failed");
    }

    while (datalen < sizeof(randomdata)) {
      if ( (result = read(rand_fd, randomdata, sizeof(randomdata) - datalen)) == -1) {
        perror("rand read failed");
      } else {
        datalen += result;
      }
    }
    
    datalen = 0;
    while (datalen < sizeof(randomdata)) {
      if ( (result = write(fd, randomdata, sizeof(randomdata) - datalen)) == -1) {
        perror("rand write failed");
      } else {	
        datalen += result;
      }
    }

    if (close(rand_fd) == -1) { perror("closing urandom failed"); }
    if (close(fd) == -1) { perror("closing memwork file failed"); } 
  }
}

static inline void nullwork() {
  for (uint64_t i = 0; i < WORK_COUNT; i++) {
    asm volatile ("nop");
  }
}


static inline void* choose_work() {
  int wt = rand() % 100;
  if (wt >= WORK_PERC) {
    return &compwork;
  } else {
    return &memwork;
  }
}

void
EP_handle_event(ep_t this, source_t src)
{
#ifdef VERBOSE
  fprintf(stderr, "%s: this=%p(%d) src=%p(%d)\n",
	  __FUNCTION__, this, this->id, src, src->id);
#endif
  src->work_func();
  source_event_reset(src);
}

  
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

  double start_energy = 0.0;
  double end_energy = 0.0;
  
  
#if WITHPERF == 2
  ts_t active_time_start;
  ts_t active_time_end;
    
  ts_t inactive_time_start;
  ts_t inactive_time_end;
  
#ifdef BUSY_POLL
  (void) inactive_time_start;
  (void) inactive_time_end;
#endif // BUSY_POLL
  
#endif // WITHPERF 2

  uint64_t active_time_total = 0.0;
  uint64_t inactive_time_total = 0.0;

  /* ------- Setting up PERF counters ------- */

  
  uint64_t pe_val[TOTAL_PERF_EVENTS];            // Counter value array corresponding to fd/id array.
  memset(pe_val, 0, sizeof(pe_val));
  
#if WITHPERF >= 1
  int rc;
  uint64_t pe_fd[TOTAL_PERF_EVENTS];                  // pe_fd[0] will be the group leader file descriptor
  uint64_t pe_id[TOTAL_PERF_EVENTS];                  // event pe_ids for file descriptors
  struct perf_event_attr pe[TOTAL_PERF_EVENTS];  // Configuration structure for perf events 
  struct read_format counter_results;

  // Configure the group of PMUs to count
  configure_perf_event(&pe[0], PERF_TYPE_HARDWARE, PERF_COUNT_HW_CPU_CYCLES);
  configure_perf_event(&pe[1], PERF_TYPE_HARDWARE, PERF_COUNT_HW_INSTRUCTIONS);

  // Create event group leader
  pe_fd[0] = perf_event_open(&pe[0], 0, -1, -1, PERF_FLAG_FD_CLOEXEC);
  ioctl(pe_fd[0], PERF_EVENT_IOC_ID, &pe_id[0]);
  // Create the rest of the events -- with pe_fd[0] as the group leader

  for(int i = 1; i < TOTAL_PERF_EVENTS; i++){
    pe_fd[i] = perf_event_open(&pe[i], 0, -1, pe_fd[0], PERF_FLAG_FD_CLOEXEC);
    ioctl(pe_fd[i], PERF_EVENT_IOC_ID, &pe_id[i]);
  }

  if ( ioctl(pe_fd[0], PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP) == -1) {
    perror("reset");
  }

  /* ---------------------------------------- */
#endif // WITHPERF >= 1
  
  snprintf(name, 80, "EP:%p:%d",this,id);
  pinCpu(eparg->cpu, name);
  
#ifdef USE_DOORBELL
  volatile uint64_t *db = &(this->eventSignal.db);
  doorbell_reset(db);
#endif
  
#ifdef USE_MONITOR  
  armMonitor(db);
#endif

  // TODO -- fixing energy collecting
  /* -- maybe pass in prefix path
    open( /sys/class/hwmon/hwmon5/energy{core no+1}_input )
    read({long int}) -- gives back energy value
    (read instead of scanf to reduce time from conversion -- of hot path)
  */
  // we are now ready to accept events
  pthread_barrier_wait(eparg->barrier);

#if WITHPERF == 2
  ts_now(&(active_time_start));
#endif // WITHPERF 2

  // TODO above
  // do the read here to get the string value of energy for this core
  
#if WITHPERF >= 1
  rc = ioctl(pe_fd[0], PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
  if (rc == -1){
    perror("enable");
  }
#endif // WITHPERF 1

  start_energy = wfey_hwmon_joules_read(eparg->cpu);
  
  while ( !this->end_flag ) {    
#ifndef BUSY_POLL

#if WITHPERF == 2
    ts_now(&(inactive_time_start));
#endif // WITHPERF 2

    WFE();

#if WITHPERF == 2
    ts_now(&(inactive_time_end));
    inactive_time_total += ts_diff(inactive_time_start, inactive_time_end);
#endif // WITHPERF 2
    
#ifdef USE_MONITOR
    // re-arm immediately to ensure do no loose
    // event signals that happen after process
    // but before going halting on WFE
    armMonitor(db);                     
#endif // USE_MONITOR
#endif // NOT BUSYPOLL
    
    wakeups++;
#ifdef USE_DOORBELL    
    if (!doorbell_isPressedAndReset(db)) {
      // spurious wakeup
      spurious++;
      continue;
    }    
#endif // USE_DOORBELL
    for (int i=0; i<Num_Sources; i++) {
      source_t src = &(Sources[i]);
      if (source_event_isActive(src)) {
	events++;
	EP_handle_event(this, src);
      }
    }
  }
  // all done

  // TODO change macro
  end_energy = wfey_hwmon_joules_read(eparg->cpu);
  
#if WITHPERF >= 1
  // Stop perf counters
  rc = ioctl(pe_fd[0], PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);
  if (rc == -1) {
    perror("disable");
  }
#endif // WITHPERF >= 1

#if WITHPERF == 2
  ts_now(&(active_time_end));

  active_time_total = ts_diff(active_time_start, active_time_end);
#endif // WITHPERF 2

#if WITHPERF >= 1
  // Read the group of perf counters
  rc = read(pe_fd[0], &counter_results, sizeof(counter_results));

  for(int i = 0; i < counter_results.nr; i++) {
    for(int j = 0; j < TOTAL_PERF_EVENTS ;j++){
      if(counter_results.values[i].id == pe_id[j]){
        pe_val[i] = counter_results.values[i].value;
      }
    }
  }

#endif // WITHPERF >= 1
  
  // Printing event processor stats

  start_energy = (double)start_energy/USEC_IN_SECOND;
  end_energy = (double)end_energy/USEC_IN_SECOND;
  fprintf(stderr, "%s=%p, ID=%d, Core=%d, TimeRan=%f, "			\
	  "Start Energy=%f, End Energy=%f, Energy Diff=%f, "		\
	  "Total Wakeups=%ld, Spurious Wakeups=%ld, Events=%ld, "	\
	  "Active Cycles=%ld, Inactive Cycles=%ld, Cycle Diff=%ld, "	\
	  "CPU Cycles=%"PRIu64", Instructions Retired=%"PRIu64"\n",	\
	  __FUNCTION__, this, id, eparg->cpu, SECTORUN,			\
	  start_energy, end_energy, (end_energy-start_energy),		\
	  wakeups, spurious, events,					\
	  active_time_total, inactive_time_total, active_time_total-inactive_time_total, \
	  pe_val[0], pe_val[1]);

#if WITHPERF >= 1
  // Close counter file descriptors
  for(int i = 0; i < TOTAL_PERF_EVENTS; i++){
    close(pe_fd[i]);
  }
#endif
  
  pthread_barrier_wait(eparg->barrier);
  return NULL;
}


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

  
  while ( !this->end_flag ) {
    ndelay = thedelay;
    while (nanosleep(&ndelay,&nrem)<0) {
      if (this->end_flag) {
	break;
      }
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
  pthread_barrier_wait(&endbarrier);
  return NULL;
}


void * idleThread(void *arg) {
  struct IdleThreadArg *iarg = (struct IdleThreadArg *)arg;
  char name[80];

  snprintf(name, 80, "IDLE:%d",iarg->cpu);

  pinCpu(iarg->cpu, name);

  free(iarg);
  pthread_barrier_wait(&endbarrier);
  return NULL;
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

  //volatile const void *mem_ptr = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_HUGETLB|MAP_SHARED, NULL, 0);
  

  /* ------- Init Signal Handler ------- */
  struct sigaction sigact;
  struct sigaction old_sigact;
  
  memset(&sigact, 0, sizeof(sigact));
  
  sigact.sa_sigaction = empty_handler;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = SA_SIGINFO;   // not including SA_RESTART bc dont want WFE/SLEEP to continue 
  sigaction(SIGUSR1, &sigact, &old_sigact);
  
  /* ------- Init Event Processor ------- */
  // initalize event processor (only one right now)
  ep.id        = id; id++;
  // eventSignal initalization will be handled by the EP thread
  // intialize event processing thread
  pthread_barrier_init(&epbarrier, NULL, 2);
  eparg.ep          = &ep;
  eparg.ep->end_flag = 0;
  eparg.barrier     = &epbarrier;
  eparg.cpu         = EVENT_CPU_ID; //atoi(argv[2]);
  wfey_hwmon_joules_read(eparg.cpu);
  
  /* ------- Init Sources ------- */
  Num_Sources = atoi(argv[3]);   // create sources but don't start them
  Sources     = malloc(sizeof(struct Source)*Num_Sources);

  // Rate of Events per source is NumEventsPerSec == argv[1]
  // divided by the number of sources to get per event rate
  double event_rate            = atof(argv[1]);
  if ( event_rate < 0.0 ) {
    fprintf(stderr, "Error: Cannot have a negative event rate\n");
    return(-1);
  }
  
  if ( event_rate == 0.0) {
    event_rate = 0.00000001; // Very small number that will never send events
  }
  double event_rate_per_source = (double)event_rate / (double)Num_Sources;
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

  // HUGE TODO!!!!!!!!!! -- sanity check num sources, the number of idle threads on the barriers, and add num sources to barrier count for all cases
  // might be idle to rename barrier
  // where runner and main are should be fixed as well
  
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

  // TODO -- idle barrier to end barrier and add the sources to that barrier
  NYI;
  // if (num_idle != 0) { pthread_barrier_init(&idlebarrier, NULL, num_idle); }
  
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

    // TODO double check this addition
    if (num_idle != 0) { pthread_barrier_init(&endbarrier, NULL, num_idle + Num_Sources + 1); }
    
    for (int i = source_end; i < (total_cpu-META_THREAD); i++){ 
      iarg = malloc(sizeof(struct IdleThreadArg));
      iarg->cpu = i;
      iarg->barrier = &endbarrier; // TODO -- is this used in struct?
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
    
    // TODO idle to end
    NYI;
    //if (num_idle != 0) { pthread_barrier_init(&idlebarrier, NULL, num_idle); }

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
  pthread_create(&eparg.ep->tid, NULL, epThread, &eparg);

  // wait for event processor to be ready
  pthread_barrier_wait(&epbarrier);
  pthread_barrier_init(&epbarrier, NULL, 2);  // reset the barrier 

  /* ------- Run Source Threads ------- */
  pthread_barrier_init(&srcbarrier, NULL, Num_Sources+1); // set up barrier for all src to init
  for (int i=0; i<Num_Sources; i++) {
    sarg      = malloc(sizeof(struct SourceThreadArg));
    sarg->src = &(Sources[i]);
    //sarg->cpu = atoi(argv[4+i]);
    sarg->cpu = cpuid++;
    sarg->barrier = &srcbarrier;

    sarg->src->work_func = choose_work();
    sarg->src->end_flag = 0;
    pthread_create(&sarg->src->tid, NULL, sourceThread, sarg);
  }
  pthread_barrier_wait(&srcbarrier);
  
  usleep(TIMETORUN);

  // TODO : have a list of event processors like we do for sources
  eparg.ep->end_flag = 1;
  pthread_kill(eparg.ep->tid, SIGUSR1);
  
  // Wait for event processor to finish
  pthread_barrier_wait(&epbarrier);
  
  // Extra verification sources not stuck in sleep
  // Individual flags sent because dont want to send a signal
  // to an already cleaned up source
  for (int i=0; i<Num_Sources; i++) {
    source_t src = &Sources[i];
    src->end_flag=1;
    pthread_kill(src->tid, SIGUSR1);
  }
  
  // Wait for all source and idle threads to clean up and return
  pthread_barrier_wait(&endbarrier);
  
  /* ------- Latency Logging ------- */
  /// TODO -- possible segfault occuring with the latency
  float mean;
  
  fprintf(stdout, "ID,Min,Max,Mean\n");
  for (int i=0; i<Num_Sources; i++) {
    source_t src = &Sources[i];
    if(src->count == 0) { // never got a chance to finish event
      src->minns = 0; src->maxns = 0; mean = -1;
    } else {
      mean = (src->totalns / (double)src->count);
    }
    fprintf(stdout, "%d,%"PRIu64",%"PRIu64",%.2f\n",
	    src->id, src->minns, src->maxns, mean);
  }
  
  free(Sources);
  return 0;
}
 
