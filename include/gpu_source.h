#ifndef __GPU_SOURCE_H__
#define __GPU_SOURCE_H__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int gpu_managed_alloc(void **ptr, size_t size);
int gpu_managed_free(void *ptr);
int gpu_source_start(volatile uint64_t *doorbell, volatile int *event_state,
                     uint64_t events, uint64_t interval_ns);
int gpu_source_synchronize(void);
const char *gpu_source_last_error(void);

#ifdef __cplusplus
}
#endif

#endif
