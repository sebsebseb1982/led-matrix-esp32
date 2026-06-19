#include "colors.h"

uint16_t Colors::black(MatrixPanel_I2S_DMA* dma_display) {
  return dma_display->color565(0, 0, 0);
}

uint16_t Colors::grey(MatrixPanel_I2S_DMA* dma_display) {
  return dma_display->color565(50, 50, 50);
}

uint16_t Colors::lightGrey(MatrixPanel_I2S_DMA* dma_display) {
  return dma_display->color565(100, 100, 100);
}

uint16_t Colors::white(MatrixPanel_I2S_DMA* dma_display) {
  return dma_display->color565(255, 255, 255);
}

uint16_t Colors::red(MatrixPanel_I2S_DMA* dma_display) {
  return dma_display->color565(255, 0, 0);
}

uint16_t Colors::green(MatrixPanel_I2S_DMA* dma_display) {
  return dma_display->color565(0, 255, 0);
}

uint16_t Colors::blue(MatrixPanel_I2S_DMA* dma_display) {
  return dma_display->color565(0, 0, 255);
}

uint16_t Colors::blueShade(MatrixPanel_I2S_DMA* dma_display, int shade) {
  return dma_display->color565(0, 0, shade);
}

uint16_t Colors::rgb(MatrixPanel_I2S_DMA* dma_display, int r, int g, int b) {
  return dma_display->color565(r, g, b);
}