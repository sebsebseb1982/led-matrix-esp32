#ifndef COLORS_H
#define COLORS_H

#include <stdint.h>

// Packing RGB565 identique a MatrixPanel_I2S_DMA::color565, mais calcule a la
// compilation : pas de pointeur d'affichage a trimballer jusqu'aux appelants.
constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

namespace Colors {
  constexpr uint16_t BLACK      = rgb565(0, 0, 0);
  constexpr uint16_t WHITE      = rgb565(255, 255, 255);
  constexpr uint16_t LIGHT_GREY = rgb565(100, 100, 100);
  constexpr uint16_t RED        = rgb565(255, 0, 0);
  constexpr uint16_t GREEN      = rgb565(0, 255, 0);
  constexpr uint16_t VENT_BOX   = rgb565(150, 150, 150);
  constexpr uint16_t SUN        = rgb565(255, 220, 0);
  constexpr uint16_t MOON       = rgb565(180, 210, 255);
  constexpr uint16_t CLOCK_HAND = rgb565(255, 180, 0);
}

#endif
