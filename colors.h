#ifndef COLORS_H
#define COLORS_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

class Colors {
  public:
    static uint16_t black(MatrixPanel_I2S_DMA* dma_display);
    static uint16_t grey(MatrixPanel_I2S_DMA* dma_display);
    static uint16_t lightGrey(MatrixPanel_I2S_DMA* dma_display);
    static uint16_t white(MatrixPanel_I2S_DMA* dma_display);
    static uint16_t red(MatrixPanel_I2S_DMA* dma_display);
    static uint16_t green(MatrixPanel_I2S_DMA* dma_display);
    static uint16_t blue(MatrixPanel_I2S_DMA* dma_display);
    static uint16_t blueShade(MatrixPanel_I2S_DMA* dma_display, int shade);
    static uint16_t rgb(MatrixPanel_I2S_DMA* dma_display, int r, int g, int b);
};

#endif