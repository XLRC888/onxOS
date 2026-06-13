#ifndef MEMORY_H
#define MEMORY_H
#include "kernel.h"
void memory_init(void *heap_start, uint32_t heap_size);
void *malloc(uint32_t size);
void free(void *ptr);
void *realloc(void *ptr, uint32_t size);
#endif
