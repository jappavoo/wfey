#define _GNU_SOURCE

#include <fcntl.h>
#include <stdbool.h>
#include <math.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <assert.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/shm.h>

#include "include/wfey.h"
#include "include/perf.h"
#include "include/wfey_hwmon.h"
#include "include/logging.h"


//#define VERBOSE


bool source_event_isActive(volatile int state)
{
  return __atomic_load_n(&state,
			 __ATOMIC_SEQ_CST) == SRC_EVENT_ACTIVE;
}

void source_event_activate(union Event *event)  {
  // if already active then don't continue
  // already processing an event
  // relay this back to source so no signal sent
  // 
  // ts_now(&(this->start_ts));
  //__atomic_store_n(&(this->event.state), SRC_EVENT_ACTIVE, __ATOMIC_SEQ_CST);
  __atomic_store_n(&event->state, SRC_EVENT_ACTIVE, __ATOMIC_SEQ_CST);
}

void source_event_reset(union Event *event) {
  // todo make local then add to log array per source
  // this->log[i].start
  // end if 0 if miss -- 
  __atomic_store_n(&(event->state), SRC_EVENT_RESET,
		   __ATOMIC_SEQ_CST);
  // TODO there should probably~ be a lock involved here :p
  // min might be wrong! or this process can be really fast
  
  /* if (source_event_isActive(this)) { // Give up if this is true */
  /*   return; */
  /* } */
  

/*   uint64_t ns = ts_diff(this->start_ts, this->end_ts); */

/*   // become an assert TODO */
/*   if ( (int64_t)ns <= 0 ) { // Give up again -- shenanigans occured */
/*     return; */
/*   } */

/*   this->totalns += ns; */
/*   this->count++; */

/*   if ( ns < this->minns ) { this->minns = ns; } */
/*   if ( ns > this->maxns ) { this->maxns = ns; } */
/* #ifdef VERBOSE */
/*   fprintf(stderr, "%d: Diff=%ld, TotalNS=%ld, Count=%ld, Min=%lu, Max=%lu\n", */
/* 	  this->id, ns, this->totalns, this->count, this->minns, this->maxns); */
/* #endif */
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

//#define MEMCHECK
#define BYTESTOKB (1024)
#define BYTESTOMB (1024 * 1024)
// TO NOTE: Decreasing the data size doesn't not have the expected results
// perhaps due to overheads or math misktake but should be double checked 
#define STRIDE 11 // in bytes // todo take cache line size and divide
// #define DATA_SIZE 5 // in MB
#define DATA_SIZE (2*BYTESTOMB) // in B

static inline uint64_t memwork() {

  // uint64_t arrsize = (DATA_SIZE * 1024 * 1024); // 5MB to be larger than the
  // L2 can hold
  uint64_t arrsize = (DATA_SIZE); // in bytes for lower testing
  uint64_t elem = arrsize / sizeof(uint64_t);

  uint64_t  sx2 = STRIDE * 2, sx3 = STRIDE * 3, sx4 = STRIDE * 4;
  uint64_t acc0 = 0, acc1 = 0, acc2 = 0, acc3 = 0;

  uint64_t limit = elem - sx4;

  int rand_fd;
  uint64_t *memarr;
  uint64_t bytes_read = 0;
  ssize_t result = 0;
  if (( rand_fd = open("/dev/urandom", O_RDONLY)) == -1) {
      handle_error("opening urandom failed");
  }
  if ( (memarr = mmap(NULL, arrsize, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0)) == (void *) -1) {
    handle_error("mmapping failed");
  }

  while (bytes_read < arrsize) {
    if ( (result = read(rand_fd, (char *)memarr + bytes_read, arrsize - bytes_read)) == -1) {
      handle_error("rand read failed");
    } else {
      bytes_read += (uint64_t) result;
    }
  }

  /* int i; */
  /* fprintf(stderr, "memarr:\n"); */
  /* for ( i = 0; i<arrsize; i++) { */
  /*   fprintf(stderr, "Element %d: %u\n", i, memarr[i]); */
  /* } */

#ifdef MEMCHECK  
  double_t time_spent_tot = 0;
  double_t loops = 0;
#endif
  // do i want to do this for work amount of times or is once okay?
  for (uint64_t i = 0; i < WORK_COUNT; i++) {
#ifdef MEMCHECK
    clock_t begin = clock();
#endif
    for (uint64_t j = 0; j < limit; j += sx4) {
      acc0 = acc0 + memarr[j];
      acc1 = acc1 + memarr[j + STRIDE];
      acc2 = acc2 + memarr[j + sx2];
      acc3 = acc3 + memarr[j + sx3];
    }

#ifdef MEMCHECK
    clock_t end = clock();
    time_spent_tot += ((double)(end - begin) / CLOCKS_PER_SEC);
    loops+=1;
#endif
  }
#ifdef MEMCHECK 
  // Per bytes = total elem / stride = num of elem processed (then mul 8 bc 64t)
  double avgmemloop = (time_spent_tot / loops)*NSEC_IN_SECOND;
  double bytesproc = ((float)limit / (float)STRIDE) * 8.0;
  double avgtimeb = bytesproc/avgmemloop;
  fprintf(stderr, "avg mem loop(ns):%f\n", avgmemloop);
  fprintf(stderr, "bytes processed per loop:%f\n", bytesproc);
  fprintf(stderr, "avg time per byte(b/ns):%f\n", avgtimeb);
#endif
  close(rand_fd);
  munmap(memarr, arrsize);
  
  return (acc0 + acc1 + acc2 + acc3);
}


static inline void iowork() {
  int fd, rand_fd;
  char randomdata[50];
  ssize_t datalen = 0, result = 0;
  
  for (uint64_t i = 0; i < WORK_COUNT; i++) {
    if ((fd = open("foo", O_TMPFILE | O_RDWR, S_IRUSR | S_IWUSR)) == -1) {
      handle_error("opening memwork file failed");
    }
    if ((rand_fd = open("/dev/urandom", O_RDONLY)) == -1) {
      handle_error("opening urandom failed");
    }

    while (datalen < sizeof(randomdata)) {
      if ( (result = read(rand_fd, randomdata + datalen, sizeof(randomdata) - datalen)) == -1) {
        handle_error("rand read failed");
      } else {
        datalen += result;
      }
    }
    
    datalen = 0;
    while (datalen < sizeof(randomdata)) {
      if ( (result = write(fd, randomdata+datalen, sizeof(randomdata) - datalen)) == -1) {
        handle_error("rand write failed");
      } else {	
        datalen += result;
      }
    }

    if (close(rand_fd) == -1) { handle_error("closing urandom failed"); }
    if (close(fd) == -1) { handle_error("closing memwork file failed"); } 
  }
}

static inline void nullwork() {
  for (uint64_t i = 0; i < WORK_COUNT; i++) {
    asm volatile ("nop");
  }
}

static inline void *
work_translate(enum WorkType wt) {
  switch (wt) {
  case 1:
    return &compwork;
  case 2:
    return &memwork;
  case 3:
    return &iowork;
  case -1:
    return &nullwork;
  default:
    fprintf(stderr, "No such work type:%d\n", wt);
    exit(EXIT_FAILURE);
  }
}
      
static inline enum WorkType choose_work(int work_perc) {
  if (work_perc == -1) {
    return NULL_WORK;
  }
  int wt = rand() % 100;
  if (wt >= work_perc) {
    return COMP_WORK;
  } else {
    return MEM_WORK;
  }
}

// static inline int choose_ep(ep_t proc_list) {
static inline int choose_ep() {
  int ep = rand() % Num_Processors; // check math
  // return &proc_list[ep];
  return ep;
}

void
EP_handle_event(ep_t this, struct MailBox_Slot mb)
{
#ifdef VERBOSE
  fprintf(stderr, "%s: this=%p(%d) src=%p(%d)\n",
	  __FUNCTION__, this, this->id, src, src->id);
#endif
  mb.work_func();

  ts_t timestamp;
  ts_now(&timestamp);
  //fprintf(stderr, "Processor writing event num:%d\n", mb.event_num);
  buffer_write(mb.event_num, mb.id, this->id, timestamp);
  source_event_reset(mb.event);
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

  snprintf(name, 80, "EP:%p:%d",this,id);
  pinCpu(eparg->cpu, name);

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
    handle_error("reset");
  }

  /* ---------------------------------------- */
#endif // WITHPERF >= 1
    
#ifdef USE_DOORBELL
  // volatile uint64_t *db = &(this->eventSignal.db);
  doorbell_t *db = &(this->mb->db.db);
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
    handle_error("enable");
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
      //source_t src = &(Sources[i]);
      struct MailBox_Slot mb_slot = this->mb->mb[i];// slot for the current source
      if (mb_slot.event == NULL) { // mailbox hasn't been set up yet
        continue;
      }
      if (source_event_isActive(mb_slot.event->state)) {
	events++;
	EP_handle_event(this, mb_slot);
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
    handle_error("disable");
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
  /* ep_t     ep                  = this->ep; */
  /* doorbell_t *db = &(ep->eventSignal.db); */
  doorbell_t *db = &(this->mb->db.db);
#endif
  struct MailBox_Slot *mb_slot = &(this->mb->mb[(this->id)-1]); // srcs start at 1 index
  mb_slot->id = this->id;
  mb_slot->work_func = this->work_func;
  mb_slot->event = &this->event;

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

  int eventnum = 1;
  ts_t timestamp;
  
  while ( !this->end_flag ) {
    ndelay = thedelay;
    while (nanosleep(&ndelay,&nrem)<0) {
      if (this->end_flag) {
	break;
      }
      ndelay = nrem;
    }
    mb_slot->event_num = eventnum;
    if ( source_event_isActive(this->event.state) == true) {
      continue;
    } else {
      source_event_activate(&this->event);
      ts_now(&timestamp);
      //fprintf(stderr, "Source writing event num:%d\n", mb_slot->event_num);
      buffer_write(mb_slot->event_num, mb_slot->id, this->ep_id, timestamp);
      eventnum++;
    }
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

  if (pthread_detach(pthread_self()) != 0) {
    handle_error("pthread for idle detach failed\n");
  }
  
  free(iarg);
  pthread_barrier_wait(&endbarrier);
  return NULL;
}



int
main(int argc, char **argv)
{
  /* ------- Variables ------- */
  pthread_barrier_t       epbarrier;
  pthread_barrier_t       srcbarrier;
  struct EPThreadArg      *eparg;
  struct SourceThreadArg  *sarg;
  struct IdleThreadArg    *iarg;

  int                     opt;
  pthread_t               tid;
  uint64_t cores_per_socket, sockets, total_cpu;
  double event_rate;
  int id = 0;
  uint64_t num_idle = 0, cpuid = 1;
  int work_perc = 100; // percent memwork

  srand((unsigned)time(NULL)); // uniquely setting rand val seed
  
  while ((opt = getopt(argc, argv, "he:p:s:w:")) != -1) {
    switch ((char)opt) {
    case 'h':
      USAGE(argv[0]);
      exit(EXIT_FAILURE);
    case 'e':
      // Rate of Events per source is NumEventsPerSec
      // divided by the number of sources to get per event rate
      event_rate = atof(optarg);
      if ( event_rate < 0.0 ) {
	fprintf(stderr, "Error: Cannot have a negative event rate\n");
	exit(EXIT_FAILURE);
      }
      
      if ( event_rate == 0.0) {
	event_rate = 0.00000001; // Very small number that will never send events
      }
      break;
    case 's':
      Num_Sources = atoi(optarg);
      break;
    case 'p':
      Num_Processors = atoi(optarg);
      if (Num_Processors > 1) {
        fprintf(stderr, "Only 1 EP can be run currently\n");
        exit(EXIT_FAILURE);
      }
      break;
    case 'w':
      work_perc = atoi(optarg);
      if ((work_perc > 100 || (work_perc < 0)) && work_perc != -1) {
        fprintf(stderr,
                "-w flag is the perc of mem work; enter a number btn 0-100\n");
        exit(EXIT_FAILURE);
      }
      break;
    default:
      USAGE(argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if ((Num_Processors == 0) || (Num_Sources == 0)) {
    fprintf(stderr, "Please define the number of sources and processors\n");
    USAGE(argv[0]);
    exit(EXIT_FAILURE);
  }

  double event_rate_per_source = event_rate / (double)Num_Sources;
  double srcsleep                       = 1.0/event_rate_per_source;
  
  
  getCPUInfo(&cores_per_socket, &sockets);
  total_cpu = cores_per_socket * sockets;
  

  /* ------- Init Signal Handler ------- */
  struct sigaction sigact;
  struct sigaction old_sigact;
  
  memset(&sigact, 0, sizeof(sigact));
  
  sigact.sa_sigaction = empty_handler;
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags = SA_SIGINFO;   // not including SA_RESTART bc dont want WFE/SLEEP to continue 
  sigaction(SIGUSR1, &sigact, &old_sigact);

  /* ------- Init Mailboxes ------- */
  Mailboxes = malloc(sizeof(struct MailBox) * Num_Processors);
  
  /* ------- Init Event Processors ------- */
  // eventSignal initalization will be handled by the EP thread
  struct EventProcessor *Processors = malloc(sizeof(struct EventProcessor) * Num_Processors);

  for (int i=0; i<Num_Processors; i++) {
    ep_t eproc = &Processors[i];
    eproc->id = id; id++;
    eproc->mb = &Mailboxes[eproc->id];

    eproc->mb->mb = malloc(sizeof(struct MailBox_Slot) * Num_Sources);
    
    eproc->eventSignal = eproc->mb->db;
    eproc->end_flag = 0;
  }

  /* ------- Run Event Processor ------- */
  pthread_barrier_init(&epbarrier, NULL, Num_Processors + 1);
  for (int i = 0; i < Num_Processors; i++) {
    eparg = malloc(sizeof(struct EPThreadArg));
    eparg->ep = &(Processors[i]);
    eparg->cpu = cpuid++;
    eparg->barrier = &epbarrier;
    wfey_hwmon_joules_read(eparg->cpu); // cache the paths

    pthread_create(&eparg->ep->tid, NULL, epThread, eparg);
    if (pthread_detach(eparg->ep->tid) != 0) {
      handle_error("pthread for ep detach failed\n");
    }
  }

  // wait for event processor to be ready
  pthread_barrier_wait(&epbarrier);
  pthread_barrier_init(&epbarrier, NULL, Num_Processors+1);  // reset the barrier


  /* ------- Init Sources ------- */
  struct Source *Sources            = NULL;
  Sources = malloc(sizeof(struct Source) * Num_Sources);
  
  for (int i=0; i<Num_Sources; i++) {
    source_t src = &Sources[i];
    src->id      = id; id++;
    // sarg->src->ep = choose_ep(Processors); //&ep;
    src->ep_id = choose_ep(); // ep id --> Num_Proc needs to be global
    src->sleep   = srcsleep;
    src->work_type = choose_work(work_perc);
    src->work_func = work_translate(src->work_type);
    src->mb = &Mailboxes[src->ep_id];
    
    source_event_reset(&src->event);
    src->end_flag = 0;
    src->totalns = 0;
    src->minns   = (uint64_t)-1;
    src->maxns   = 0;
    src->count   = 0;
  }

  /* ------- Run Source Threads ------- */
  pthread_barrier_init(&srcbarrier, NULL, Num_Sources+1); // set up barrier for all src to init
  for (int i=0; i<Num_Sources; i++) {
    sarg      = malloc(sizeof(struct SourceThreadArg));
    sarg->src = &(Sources[i]);
    sarg->cpu = cpuid++;
    sarg->barrier = &srcbarrier;

    pthread_create(&sarg->src->tid, NULL, sourceThread, sarg);
    if (pthread_detach(sarg->src->tid) != 0) {
      handle_error("pthread for src detach failed\n");
    }
  }
  
  /* ------- Init and Run Idle Cores ------- */
  /*
    -------------------
    |  Runner/Main    |
    |  Event Proc     |
    |  Source Threads |
    |  Idle Threads   |
    -------------------  
  */
  num_idle = total_cpu - (Num_Processors + Num_Sources + META_THREAD);
  
  /* Error Checking */
  if ((Num_Processors + Num_Sources) > (total_cpu - META_THREAD) ) {
    fprintf( stderr, "There are too many event processors & source threads to fit on all cores across the socket(s) -- Try a total of <%ld\n", total_cpu-META_THREAD);
    exit(EXIT_FAILURE);
  }

  int source_end = cpuid; // all events and sources allocated first (might need +1)

  if (num_idle != 0) { pthread_barrier_init(&endbarrier, NULL, num_idle + Num_Sources + 1); }
    
  for (int i = source_end; i < total_cpu; i++){ 
    iarg = malloc(sizeof(struct IdleThreadArg));
    iarg->cpu = i;
    iarg->barrier = &endbarrier; // TODO -- is this used in struct?
    pthread_create(&tid, NULL, idleThread, iarg);
  } 

  pinCpu(RUNNER_CPU, "main thread");

  // Create Buffer File
  FILE *latency_stream;
  if ( (latency_stream = fdopen(LATENCY_FD, "w")) == NULL) {
    handle_error("opening latency stream\n");
  }

  fprintf(latency_stream, "EventID, SrcID, EpID, TS\n");
  // Start logging thread
  pthread_t logger_thread;
  int end_flag = 0;
  struct Log arg = {latency_stream, &end_flag};
  //int log_pid;
  pthread_create(&logger_thread, NULL, consumer, (void *) &arg);
  
  // Start the Benchmark (Sources)
  pthread_barrier_wait(&srcbarrier);
  
  usleep(TIMETORUN);

  for (int i=0; i < Num_Processors; i++) {
    ep_t ep = &Processors[i];
    ep->end_flag = 1;
    // I believe I send this jic ep stuck waiting for event that never comes
    pthread_kill(ep->tid, SIGUSR1);
  }
  
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

  // End Logging Thread
  end_flag = 1;
  pthread_join(logger_thread, NULL);
  
  /* ------- Latency Logging ------- */

  /* float mean; */
  
  /* fprintf(stdout, "SRC_ID,WorkType,Latency_Min,Latency_Max,Latency_Mean\n"); */
  /* for (int i=0; i<Num_Sources; i++) { */
  /*   source_t src = &Sources[i]; */
  /*   if(src->count == 0) { // never got a chance to finish event */
  /*     src->minns = 0; src->maxns = 0; mean = -1; */
  /*   } else { */
  /*     mean = (src->totalns / (double)src->count); */
  /*   } */
  /*   fprintf(stdout, "%d,%s,%"PRIu64",%"PRIu64",%.2f\n", */
  /* 	    src->id, printWorkType(src->work_type), src->minns, src->maxns, mean); */
  /* } */

  fflush(latency_stream);
  fclose(latency_stream);
  //fflush(stdout);

  free(Sources);
  free(Processors);
  exit(EXIT_SUCCESS);
}
 
