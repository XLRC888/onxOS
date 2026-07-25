#ifndef PIT_H
#define PIT_H
#include "kernel.h"
void pit_init(void);
uint32_t pit_get_ticks(void);
#endif
