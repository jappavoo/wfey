#ifndef __WFEY_HWMON_H__
#define __WFEY_HWMON_H__

#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>

extern char **wfey_hwmon_joules_paths;

extern void wfey_hwmon_joules_cache_path(int core);

inline static uint64_t wfey_hwmon_joules_read(int core) {

  if (!wfey_hwmon_joules_paths) {
    wfey_hwmon_joules_cache_path(core);
  }
 
  char *path = wfey_hwmon_joules_paths[core];
  if (!path) wfey_hwmon_joules_cache_path(core);
  
  FILE *f = fopen(path, "r");
  if (f == NULL) {
    fprintf(stderr, "ERROR: %s: failed to open %s\n", __func__, path);
    return -1;
  }
  uint64_t joules;
  fscanf(f, "%" SCNu64, &joules);
  fclose(f);
  
  return joules;
}

#endif 
