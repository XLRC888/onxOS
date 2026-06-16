#include "memory.h"
typedef struct block {
    uint32_t size;
    struct block *next;
    int free;
} block_t;
#define ALIGN 4
#define BLOCK_SIZE sizeof(block_t)
#define HEAP_MAGIC 0xDEADBEEF
static void *heap_base = 0;
static uint32_t heap_total = 0;
static block_t *free_list = 0;
void memory_init(void *heap_start, uint32_t heap_size) {
    heap_base = heap_start;
    heap_total = heap_size;
    free_list = (block_t *)heap_base;
    free_list->size = heap_size - BLOCK_SIZE;
    free_list->next = 0;
    free_list->free = 1;
}
static void split_block(block_t *block, uint32_t size) {
    if (size > 0xFFFFFFEF || block->size <= size + BLOCK_SIZE + ALIGN) return;
    block_t *new_block = (block_t *)((uint8_t *)block + BLOCK_SIZE + size);
    new_block->size = block->size - size - BLOCK_SIZE;
    new_block->next = block->next;
    new_block->free = 1;
    block->size = size;
    block->next = new_block;
}
void *malloc(uint32_t size) {
    if (!size) return 0;
    if (size % ALIGN) size += ALIGN - (size % ALIGN);
    block_t *curr = free_list;
    while (curr) {
        if (curr->free && curr->size >= size) {
            split_block(curr, size);
            curr->free = 0;
            return (uint8_t *)curr + BLOCK_SIZE;
        }
        curr = curr->next;
    }
    return 0;
}
void free(void *ptr) {
    if (!ptr) return;
    block_t *block = (block_t *)((uint8_t *)ptr - BLOCK_SIZE);
    block->free = 1;
    block_t *curr = free_list;
    while (curr && curr->next) {
        if (curr->free && curr->next->free) {
            curr->size += BLOCK_SIZE + curr->next->size;
            curr->next = curr->next->next;
            continue;
        }
        curr = curr->next;
    }
}
void *realloc(void *ptr, uint32_t size) {
    if (!ptr) return malloc(size);
    if (!size) { free(ptr); return 0; }
    block_t *block = (block_t *)((uint8_t *)ptr - BLOCK_SIZE);
    if (block->size >= size) return ptr;
    void *new_ptr = malloc(size);
    if (!new_ptr) return 0;
    uint32_t copy_size = block->size < size ? block->size : size;
    uint8_t *src = ptr, *dst = new_ptr;
    for (uint32_t i = 0; i < copy_size; i++) dst[i] = src[i];
    free(ptr);
    return new_ptr;
}
