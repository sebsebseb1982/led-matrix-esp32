#include "colors.h"
#include "bitmap.h"
#include <WiFi.h>

#include <HTTPClient.h>

Bitmap::Bitmap(LEDPanel *ledPanel) {
  this->ledPanel = ledPanel;
}

void Bitmap::setup() {
}

void Bitmap::loop() {
  downloadAndDisplayImage();
}

void Bitmap::downloadAndDisplayImage() {

  String imageUrl = "http://sebastienblondy.com/images/fez.bmp";

  HTTPClient http;
  http.begin(imageUrl);
  int httpCode = http.GET();
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      int len = http.getSize();

      // create buffer for read
      uint8_t buff[3] = { 0 };

      // get tcp stream
      WiFiClient *stream = http.getStreamPtr();


      for (int i = 0; i < HEADER_BYTES_SIZE; i++) {
        while (!stream->available()) {}
        stream->read();
      }

      for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
          /*while(stream->available()<sizeof(buff)){
            delay(1);
          }*/
          size_t size = stream->available();
          if (true) {
            //if (size) {
            int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
            if (len > 0) {
              len -= c;
            }

            uint8_t r = buff[2];
            uint8_t g = buff[1];
            uint8_t b = buff[0];

            this->ledPanel->dma_display->drawPixel(
              SCREEN_WIDTH - 1 - x,
              SCREEN_HEIGHT - 1 - y,
              Colors::rgb(this->ledPanel->dma_display, r, g, b));
          }
        }
      }
    }
  } else {
  }

  Serial.println("");

  http.end();
}
