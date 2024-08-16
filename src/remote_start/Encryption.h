#ifndef ENCRIPTION_H
#define ENCRIPTION_H
#include <stdint.h>

static uint32_t f20(const uint64_t x);
static uint64_t hitag2_init(const uint64_t key, const uint32_t serial, const uint32_t IV);
static uint64_t hitag2_round(uint64_t *state);
static uint32_t hitag2_byte(uint64_t *x, uint8_t size);
uint32_t getKS(uint64_t KEY, uint32_t UID, uint32_t IV);
#endif