#include "include/gpu_source.h"

#include <cuda/atomic>
#include <cuda_runtime.h>

#include <stdio.h>

static const char *last_error = "success";

static int cuda_ok(cudaError_t err)
{
  if (err == cudaSuccess) {
    last_error = "success";
    return 0;
  }

  last_error = cudaGetErrorString(err);
  return -1;
}

__device__ static inline uint64_t gpu_clock_ns(void)
{
  uint64_t now;
  asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(now));
  return now;
}

__global__ void gpu_source_kernel(uint64_t *doorbell, int *event_state,
                                  uint64_t events, uint64_t interval_ns)
{
  cuda::atomic_ref<uint64_t, cuda::thread_scope_system> bell(*doorbell);
  cuda::atomic_ref<int, cuda::thread_scope_system> state(*event_state);
  uint64_t next = gpu_clock_ns();

  for (uint64_t i = 0; i < events; i++) {
    while (gpu_clock_ns() < next) {
    }

    state.store(1, cuda::memory_order_release);
    bell.store(1, cuda::memory_order_release);
    next += interval_ns;
  }
}

extern "C" int gpu_managed_alloc(void **ptr, size_t size)
{
  return cuda_ok(cudaMallocManaged(ptr, size));
}

extern "C" int gpu_managed_free(void *ptr)
{
  return cuda_ok(cudaFree(ptr));
}

extern "C" int gpu_source_start(volatile uint64_t *doorbell,
                                 volatile int *event_state,
                                 uint64_t events,
                                 uint64_t interval_ns)
{
  uint64_t *db = (uint64_t *)doorbell;
  int *state = (int *)event_state;

  gpu_source_kernel<<<1, 1>>>(db, state, events, interval_ns);
  return cuda_ok(cudaGetLastError());
}

extern "C" int gpu_source_synchronize(void)
{
  return cuda_ok(cudaDeviceSynchronize());
}

extern "C" const char *gpu_source_last_error(void)
{
  return last_error;
}
