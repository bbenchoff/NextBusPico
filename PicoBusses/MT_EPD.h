#ifndef MT_EPD_H
#define MT_EPD_H

#include "WS_EPD.h"

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>

class MT_EPD : public Adafruit_GFX {
  public:
    MT_EPD(int8_t cs, int8_t dc, int8_t rst, int8_t busy);
    ~MT_EPD(); 
    
    void begin();
    void clearDisplay();
    void display();
    void sleep();
    void drawBox(int x_start, int y_start, int width, int height, uint16_t color);
    void sendCommand(uint8_t command);
    void sendData(uint8_t data);
    void transformCoordinates(int16_t &x, int16_t &y);
    // Required by Adafruit_GFX
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void setRotation(uint8_t r) override; // Add this in the public section
    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, 
                uint16_t color, uint16_t bg_color = EPD_WHITE);
    
    // Set physical display orientation
    void setDisplayOrientation(uint8_t orientation);
    void setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void displayPartial();

    /**
    * Sets up a partial update region and updates only that portion of the display
    * @param x X coordinate of the update region
    * @param y Y coordinate of the update region
    * @param w Width of the update region
    * @param h Height of the update region
    */
    void updatePartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

    /**
    * Creates a partial buffer for updates
    * @param x X coordinate of the buffer region
    * @param y Y coordinate of the buffer region
    * @param w Width of the buffer region
    * @param h Height of the buffer region
    * @return Pointer to the partial buffer
    */
    uint8_t* createPartialBuffer(uint16_t x, uint16_t y, uint16_t w, uint16_t h, size_t* bufferSize);

    void waitUntilIdle();
        
    static const uint16_t EPD_BLACK = 0x0000;
    static const uint16_t EPD_WHITE = 0xFFFF;
    static const uint16_t EPD_RED = 0xF800;

    // Buffers for black/white and red planes
    uint8_t* _buffer_bw;
    uint8_t* _buffer_red;
    uint32_t _buffer_size;

  private:
    int16_t _width;
    int16_t _height;
    int8_t _cs_pin;
    int8_t _dc_pin;
    int8_t _rst_pin;
    int8_t _busy_pin;
    uint8_t _orientation;  // Physical orientation

    void writeRAM(uint16_t xSize, uint16_t ySize, uint8_t* buffer, uint16_t offset, uint8_t command);

    

    void reset();
};

class MT_EPD : public WS_EPD {
public:
    // Inherit constructors from WS_EPD
    using WS_EPD::WS_EPD;
    
    // Define the same color constants as in MT_EPD
    static const uint16_t EPD_BLACK = WS_EPD::EPD_BLACK;
    static const uint16_t EPD_WHITE = WS_EPD::EPD_WHITE;
    static const uint16_t EPD_RED = WS_EPD::EPD_BLACK; // Map red to black since it's B&W display
};

#endif