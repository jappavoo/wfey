#include <unistd.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wfey_hwmon.h"

#define USE_ALTRA_HWMON

#define MAX_PATH_LEN 1024

#define HWMON_ROOT_PATH "/sys/class/hwmon"

#ifdef USE_ALTRA_HWMON
#define JOULES_HWMON_NAME "altra_hwmon"
#define JOULES_PATH_LABEL_TEMPLATE "energy%d_label"
#define JOULES_LABEL_SCANF "Core Energy %d"
#define JOULES_PATH_INPUT_TEMPLATE "energy%d_input"
#endif

char **wfey_hwmon_joules_paths = NULL;


void wfey_hwmon_joules_cache_path(int core)
{
  char path[1024];
  
  if (!wfey_hwmon_joules_paths) {
     long n = sysconf(_SC_NPROCESSORS_CONF);
     wfey_hwmon_joules_paths = malloc(sizeof(char *) * n);
     bzero(wfey_hwmon_joules_paths, sizeof(char *) * n);
  }

  // FIXME: CJ this is bad code 
  if (wfey_hwmon_joules_paths[core]) return;
  
  struct dirent *de;
  DIR *dr = opendir(HWMON_ROOT_PATH);
  
  if (dr == NULL) {
    fprintf(stderr, "ERROR: %s: could not open %s\n", __func__, HWMON_ROOT_PATH);
    goto done;
  }
  
  while ((de = readdir(dr)) != NULL) {
    if (strncmp(de->d_name, ".", 1) == 0 ||
	strncmp(de->d_name, "..", 2) == 0 ) continue;
    snprintf(path, sizeof(path), HWMON_ROOT_PATH "/%s/name", de->d_name);
    // fprintf(stderr, "%s %s\n", de->d_name, path);
    
    char name[80];
    FILE *f = fopen(path, "r");
    if (f == NULL) continue;
    bzero(name, sizeof(name));
    fread(name, sizeof(name),1,f);
    fclose(f);
    
    // fprintf(stderr, "%s", name);
    
    if (strncmp(name, JOULES_HWMON_NAME, 11)==0) {
      char label[160];
      int  n = core + 1;
      int ln;
      
      snprintf(path, sizeof(path), HWMON_ROOT_PATH "/%s/" JOULES_PATH_LABEL_TEMPLATE, de->d_name, n);
      //  fprintf(stderr, "%s\n", path);
      
      f=fopen(path, "r");
      if (f == NULL) {
	fprintf(stderr, "ERROR: %s: could not open %s\n", __func__, path);
	goto done;
      }
      bzero(label, sizeof(label));
      fgets(label, sizeof(label), f);
      fclose(f);
      
      // fprintf(stderr, "%s", label)
      ;
      if (sscanf(label, JOULES_LABEL_SCANF, &ln)!=1) {
	fprintf(stderr, "ERROR: %s: bad label %s\n", __func__, label);
	goto done;
      }
      
      // fprintf(stderr, "%d\n", ln);
      
      if ( ln != core ) {
	fprintf(stderr, "ERROR: %s: label %s core %d != core %d\n", __func__, label, ln, core);
	goto done;
      }
      snprintf(path, sizeof(path), HWMON_ROOT_PATH "/%s/" JOULES_PATH_INPUT_TEMPLATE, de->d_name, n);
      
      // fprintf(stderr, "%s\n", path);
      wfey_hwmon_joules_paths[core] = strndup(path, sizeof(path)); 
      break;
    }	
  }
 done:
  closedir(dr);
}


#ifdef __STAND_ALONE__
int main(int argc, char **argv)
{

  int core = (argc>1) ? atoi(argv[1]) : 0;

  uint64_t j1= wfey_hwmon_joules_read(core);
  sleep(10);
  uint64_t j2 = wfey_hwmon_joules_read(core);
  printf("j1=%" PRIu64 " j2=%" PRIu64 "\n", j1, j2);
  return 0;
}
  
#endif  
