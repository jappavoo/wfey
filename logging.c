#include "include/logging.h"

pthread_mutex_t lock;
pthread_cond_t not_full;
pthread_cond_t not_empty;

struct Log_Item buffer[BUFFER_SIZE];
int read_index = 0, write_index = 0, count = 0;

struct timespec wait = {1.0, 0.0};

void *producer(void *arg) {
  struct Log_Item item = *(struct Log_Item *)arg;

  pthread_mutex_lock(&lock);

  // while ( ((write_index + 1) % BUFFER_SIZE) == read_index) {
  while ( count == BUFFER_SIZE ) { // FULL
    pthread_cond_wait(&not_full, &lock);
  }

  buffer[write_index] = item;
  //printf("Produced %d at %d\n", item.eventID, write_index);
  
  write_index = (write_index + 1) % BUFFER_SIZE;
  count++;

  pthread_cond_signal(&not_empty);
  pthread_mutex_unlock(&lock);

  return NULL;
}

void* consumer(void *arg) {
  struct Log *log = (struct Log *)arg;
  int *end_flag = log->end_flag;
  FILE *latency_stream = log->latency_stream;
  
  while (!(*end_flag)) {
    pthread_mutex_lock(&lock);

    while ((count == 0) && (*end_flag == 0)) {
      pthread_cond_timedwait(&not_empty, &lock, &wait);
    }
    if (*end_flag == 1) { break; }

    struct Log_Item data = buffer[read_index];
    //printf("Consumed: %d at %d\n", data.eventID, read_index);
    
    write_to_log(latency_stream, data);
    
    read_index = (read_index + 1) % BUFFER_SIZE;
    count--;
    
    pthread_cond_signal(&not_full);
    pthread_mutex_unlock(&lock);
  }
  pthread_join(pthread_self(), NULL);
  return NULL;
}
