#ifndef __LOGGING_H__
#define __LOGGING_H__

#include <pthread.h>
#include <stdio.h>

#define BUFFER_SIZE 5

struct Log_Item {
  int eventID;
  int srcID;
  int epID;
  struct timespec ts;
};

struct Log {
  // struct Log_Item *buffer;
  FILE *latency_stream;
  int *end_flag;
};

void* producer(void *arg);
void *consumer(void *arg);

void buffer_write(int eventID, int srcID, int epID, struct timespec ts);

static inline void write_to_log(FILE *log, struct Log_Item data) {
  fprintf(log, "%d %d %d %ld:%ld\n", data.eventID, data.srcID, data.epID, data.ts.tv_sec, data.ts.tv_nsec);
}

#endif
