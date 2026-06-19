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

  //String imageUrl = "http://sebastienblondy.com/images/fez.bmp";
  String imageUrl = "http://192.168.1.208:8080/images/http%3A%2F%2F192.168.1.229%2Fsnapshot.jpg?convertTo=bitmap&width=64&height=64";

  HTTPClient http;
  http.begin(imageUrl);
  int httpCode = http.GET();
  if (httpCode > 0) {
    if (httpCode == HTTP_CODE_OK) {
      int len = http.getSize();

      // create buffer for read
      uint8_t buff[3 * SCREEN_WIDTH] = { 0 };

      // get tcp stream
      WiFiClient *stream = http.getStreamPtr();


      for (int i = 0; i < HEADER_BYTES_SIZE; i++) {
        while (!stream->available()) {}
        stream->read();
      }

      for (int y = 0; y < SCREEN_HEIGHT; y++) {

        int retries = 0;
        while (stream->available() < sizeof(buff) && retries < 20) {
          Serial.print(stream->available());
          Serial.println("B");
          retries++;
        }
        stream->readBytes(buff, sizeof(buff));

        for (int x = 0; x < SCREEN_WIDTH; x++) {
          uint8_t r = buff[x * 3 + 2];
          uint8_t g = buff[x * 3 + 1];
          uint8_t b = buff[x * 3 + 0];

          this->ledPanel->dma_display->drawPixel(
            x,
            y,
            Colors::rgb(this->ledPanel->dma_display, r, g, b));
        }
      }
    }
  } else {
    Serial.println(httpCode);
  }

  http.end();
}
