#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <time.h>

// Number of samples per second
#define SAMPLES 2.0
#define NSEC_IN_SEC (1000000000)
#define MAXSAMPLES 10000
int main(int argc, char **argv){
  int testtime, totalread;
  uint64_t readbuf[MAXSAMPLES];
  FILE *fd;
  struct timespec sleeptime;

  int n = 0,out = 0;
  sleeptime.tv_nsec = (1.0/SAMPLES)*NSEC_IN_SEC;
  
  if (argc < 3) {
    fprintf(stderr, "%s: <hwmon file> <numcores * period>\n", argv[0]);
    return(-1);
  }

  char *hwmonpath = argv[1];
  testtime = atoi(argv[2]);
  totalread = testtime * SAMPLES;

  if ( totalread > MAXSAMPLES ) {
    fprintf(stderr, "ERROR: Unable to read that many samples -- buffer not big enough\n");
    exit(1);
  }

  fprintf(stderr, "testtime: %d, totalread:%d, hwmonpath:%s\n", testtime, totalread, hwmonpath);
  
  if ( (fd = fopen(hwmonpath, "rb")) == NULL ) {
    fprintf(stderr, "ERROR: Could not open hwmon file\n");
    exit(1);
  }

  while ( n < totalread ) {
    if ( fseek(fd, 0L, SEEK_SET) == -1 ) {
      fprintf(stderr, "ERROR: Setting file to start\n");
      exit(1);
    }
    if ( (out += fread(&readbuf[n++], sizeof(readbuf[0]), 1, fd)) == 0) {
      fprintf(stderr, "ERROR: Reading file\n");
      exit(1);
    }
    nanosleep(&sleeptime, NULL);
    fflush(fd);
  }

  
  for ( int i = 0; i < n; i++ ) {
    fwrite(&readbuf[i], sizeof(readbuf[i]), 1, stdout);
    printf("\n");
  }
  fflush(stdout);
  fclose(fd);
}
