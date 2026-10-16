/* Registry of ported routines and their test adapters (see ports.c). */
#pragma once
#include "replay.h"
typedef struct { uint32_t pc; const char *name; void (*run)(const Record *r); } Port;
extern const Port ports[];
/* global labels that are only branch targets inside a ported routine (a loop's labels): no C
 * function of their own; their recorded "calls" are the routine's own work */
extern const uint32_t inner_labels[];
void expect_reg16(const Record *r, int reg, uint32_t got, const char *what);
