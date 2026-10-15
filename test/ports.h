/* Registry of ported routines and their test adapters (see ports.c). */
#pragma once
#include "replay.h"
typedef struct { uint32_t pc; const char *name; void (*run)(const Record *r); } Port;
extern const Port ports[];
void expect_reg16(const Record *r, int reg, uint32_t got, const char *what);
