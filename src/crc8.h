#pragma once

#include <stdint.h>

class Crc8
{
public:
    Crc8(uint8_t poly);
    uint8_t calc(uint8_t *data, uint8_t len);

    // One shot CRC over an arbitrary polynomial, computed a bit at a time.
    // Slower than calc() but needs no lookup table, so it suits polynomials
    // that are only used occasionally.
    static uint8_t calcPoly(const uint8_t *data, uint8_t len, uint8_t poly);

protected:
    uint8_t _lut[256];
    void init(uint8_t poly);
};