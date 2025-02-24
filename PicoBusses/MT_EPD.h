#ifndef MT_EPD_H
#define MT_EPD_H

#include <Arduino.h>
#include <SPI.h>

class MT_EPD {
  public:
    MT_EPD(int8_t cs, int8_t dc, int8_t rst, int8_t busy);
    void begin();
    void clearDisplay();
    void display();
    void sleep();
    void drawBox(int x_start, int y_start, int width, int height, uint16_t color);
    void sendCommand(uint8_t command);
    void sendData(uint8_t data);

    static const uint16_t EPD_BLACK;
    static const uint16_t EPD_WHITE;
    static const uint16_t EPD_RED;

  private:
    static const uint8_t LUT_VCOM[44];
    static const uint8_t LUT_WW[42];
    static const uint8_t LUT_BW[42];
    static const uint8_t LUT_WB[42];
    static const uint8_t LUT_BB[42];

    int16_t _width;
    int16_t _height;
    int8_t _cs_pin;
    int8_t _dc_pin;
    int8_t _rst_pin;
    int8_t _busy_pin;

    void waitUntilIdle();
    void reset();
    void writeRAM(uint16_t xSize, uint16_t ySize, uint8_t* buffer, uint16_t offset, uint8_t command);
    void setPartialWindow(uint16_t xStart, uint16_t xEnd, uint16_t yStart, uint16_t yEnd);
};

#endif
