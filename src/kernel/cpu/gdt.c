#include "gdt.h"
#include "port.h"
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed)) gdt_entry_t;
typedef struct {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_ptr_t;
static gdt_entry_t gdt_entries[5];
static gdt_ptr_t   gdt_ptr;
static void gdt_set_entry(int i, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt_entries[i].base_low    = base & 0xFFFF;
    gdt_entries[i].base_mid    = (base >> 16) & 0xFF;
    gdt_entries[i].base_high   = (base >> 24) & 0xFF;
    gdt_entries[i].limit_low   = limit & 0xFFFF;
    gdt_entries[i].granularity = (limit >> 16) & 0x0F;
    gdt_entries[i].granularity |= gran & 0xF0;
    gdt_entries[i].access      = access;
}
extern void gdt_flush(uint32_t);
void gdt_init(void) {
    gdt_ptr.limit = sizeof(gdt_entry_t) * 5 - 1;
    gdt_ptr.base  = (uint32_t)&gdt_entries;
    gdt_set_entry(0, 0, 0, 0, 0);
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    gdt_flush((uint32_t)&gdt_ptr);
}
