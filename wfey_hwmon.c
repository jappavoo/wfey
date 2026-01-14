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


FILE* wfey_hwmon_joules_fopen(int core)
{
  char path[MAX_PATH_LEN];
  struct dirent *de;
  FILE * f = NULL;
  
  DIR *dr = opendir(HWMON_ROOT_PATH);

  if (dr == NULL) {
    fprintf(stderr, "ERROR: %s: could not open %s\n", __func__, HWMON_ROOT_PATH);
    f = NULL; goto done;
  }

  while ((de = readdir(dr)) != NULL) {
    if (strncmp(de->d_name, ".", 1) == 0 ||
	strncmp(de->d_name, "..", 2) == 0 ) continue;
    snprintf(path, sizeof(path), HWMON_ROOT_PATH "/%s/name", de->d_name);
    // fprintf(stderr, "%s %s\n", de->d_name, path);

    char name[80];
    f = fopen(path, "r");
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
	f = NULL; goto done;
      }
      bzero(label, sizeof(label));
      fgets(label, sizeof(label), f);
      fclose(f);
      
      // fprintf(stderr, "%s", label)
	;
      if (sscanf(label, JOULES_LABEL_SCANF, &ln)!=1) {
	fprintf(stderr, "ERROR: %s: bad label %s\n", __func__, label);
	f = NULL; goto done;
      }
      
      // fprintf(stderr, "%d\n", ln);
      
      if ( ln != core ) {
	fprintf(stderr, "ERROR: %s: label %s core %d != core %d\n", __func__, label, ln, core);
	f = NULL; goto done;
      }
      snprintf(path, sizeof(path), HWMON_ROOT_PATH "/%s/" JOULES_PATH_INPUT_TEMPLATE, de->d_name, n);
      
      // fprintf(stderr, "%s\n", path);

      f = fopen(path, "r");
      if (f == NULL) {
	fprintf(stderr, "ERROR: %s: failed to open %s\n", __func__, path);
	f = NULL; goto done;
      }
      break;
    }
	
  }
 done:
  closedir(dr);
  return f;
}


#ifdef __STAND_ALONE__
int main(int argc, char **argv)
{
  FILE * jf= wfey_hwmon_joules_fopen(atoi(argv[1]));
  uint64_t joules = wfey_hwmon_joules_read(jf);
  printf("joules=%" PRIu64 "\n", joules);
  return 0;
}
  
#endif  
