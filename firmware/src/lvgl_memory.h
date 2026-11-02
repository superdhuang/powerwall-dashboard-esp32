#pragma once

#include <stdlib.h>

#if defined(ESP32)
#include <esp_heap_caps.h>

// LVGL objects are not DMA descriptors. Prefer PSRAM even for small objects,
// leaving internal contiguous blocks available for Wi-Fi and mbedTLS buffers.
static inline void* powerwall_lvgl_alloc(size_t size) {
  void* memory = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return memory ? memory : malloc(size);
}

static inline void* powerwall_lvgl_realloc(void* memory, size_t size) {
  if (size == 0) {
    free(memory);
    return NULL;
  }
  void* resized = heap_caps_realloc(memory, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  return resized ? resized : realloc(memory, size);
}
#else
#define powerwall_lvgl_alloc malloc
#define powerwall_lvgl_realloc realloc
#endif
