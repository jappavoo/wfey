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
  struct Log_Item *buffer;
  int *end_flag;
};

void* producer(void *arg);
void *consumer(void *arg);

void log_write(int eventID, int srcID, int epID, struct timespec ts);

#endif
