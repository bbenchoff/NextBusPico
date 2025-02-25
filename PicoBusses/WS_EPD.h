#ifndef WS_EPD_H
#define WS_EPD_H

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>

class WS_EPD : public Adafruit_GFX {
  public:
    // Constructor
    WS_EPD(int8_t cs, int8_t dc, int8_t rst, int8_t busy);
    ~WS_EPD();
    
    // Display control functions
    void begin(void);
    void clearDisplay(void);
    void display(void);
    void sleep(void);
    void reset(void);
    
    // Required by Adafruit_GFX
    void drawPixel(int16_t x, int16_t y, uint16_t color) override;
    void setRotation(uint8_t r) override;
    
    // Extended drawing functions
    void drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, 
                uint16_t color, uint16_t bg_color = EPD_WHITE);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    
    // Command and data management
    void sendCommand(uint8_t command);
    void sendData(uint8_t data);
    void waitUntilIdle();
    
    // Partial update functions
    void setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void displayPartial();
    void updatePartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    
    // For backward compatibility with MT_EPD class
    void drawBox(int x_start, int y_start, int width, int height, uint16_t color) {
        fillRect(x_start, y_start, width, height, color);
    }
    
    // Color definitions - keeping these public and static to match MT_EPD
    static const uint16_t EPD_BLACK = 0x0000;
    static const uint16_t EPD_WHITE = 0xFFFF;
    
    // Buffer access for direct manipulation
    uint8_t* _buffer_bw;
    uint8_t* _buffer_red;
    uint32_t _buffer_size;
    
  private:
    int8_t _cs_pin;
    int8_t _dc_pin;
    int8_t _rst_pin;
    int8_t _busy_pin;
    int16_t _width;
    int16_t _height;
    uint8_t _rotation;
    
    // Helper functions
    void initDisplay();
    void powerOn();
    void powerOff();
    uint8_t* createPartialBuffer(uint16_t x, uint16_t y, uint16_t w, uint16_t h, size_t* bufferSize);
};

#endif // WS_EPD_H