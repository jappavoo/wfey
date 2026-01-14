#ifndef __WFEY_HWMON_H__
#define __WFEY_HWMON_H__

#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>

extern FILE * wfey_hwmon_joules_fopen(int core);

inline static uint64_t wfey_hwmon_joules_read(FILE *jf) {
  uint64_t joules;
  fscanf(jf, "%" SCNu64, &joules);
  return joules;
}

#endif 
