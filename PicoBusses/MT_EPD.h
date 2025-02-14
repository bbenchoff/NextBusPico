#ifndef MT_EPD_H
#define MT_EPD_H

#include <Arduino.h>
#include <SPI.h>

class MT_EPD {
  public:
    MT_EPD(int8_t cs, int8_t dc, int8_t rst, int8_t busy);
    void begin(void);
    void clearDisplay(void);
    void display(void);
    void sleep(void);
    void drawBox(int x_start, int y_start, int width, int height, uint16_t color);
    
    static const uint16_t EPD_BLACK = 0x0000;
    static const uint16_t EPD_WHITE = 0xFFFF;
    static const uint16_t EPD_RED = 0xF800;
    
  private:
    int16_t _width;
    int16_t _height;
    int8_t _cs_pin;
    int8_t _dc_pin;
    int8_t _rst_pin;
    int8_t _busy_pin;
    void waitUntilIdle(void);
    void reset(void);
    void sendCommand(uint8_t command);
    void sendData(uint8_t data);
    void writeRAM(uint16_t xSize, uint16_t ySize, uint8_t* buffer, uint16_t offset, uint8_t command);
    void setPartialWindow(uint16_t xStart, uint16_t xEnd, uint16_t yStart, uint16_t yEnd);
};

MT_EPD::MT_EPD(int8_t cs, int8_t dc, int8_t rst, int8_t busy) :
  _cs_pin(cs), _dc_pin(dc), _rst_pin(rst), _busy_pin(busy),
  _width(800), _height(480)
{
}

void MT_EPD::reset() {
    digitalWrite(_rst_pin, HIGH);
    delay(10);
    digitalWrite(_rst_pin, LOW);
    delay(10);
    digitalWrite(_rst_pin, HIGH);
    delay(10);
}

void MT_EPD::sendCommand(uint8_t command) {
    digitalWrite(_dc_pin, LOW);
    digitalWrite(_cs_pin, LOW);
    SPI.transfer(command);
    digitalWrite(_cs_pin, HIGH);
}

void MT_EPD::sendData(uint8_t data) {
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_cs_pin, LOW);
    SPI.transfer(data);
    digitalWrite(_cs_pin, HIGH);
}

void MT_EPD::waitUntilIdle() {
    while(digitalRead(_busy_pin) == HIGH) {
        delay(10);
    }
}

void MT_EPD::begin() {
    // Hardware reset
    reset();
    
    // Init sequence
    sendCommand(0x01);    // POWER SETTING
    sendData(0x07);
    sendData(0x07);      // VGH=20V,VGL=-20V
    sendData(0x3f);      // VDH=15V
    sendData(0x3f);      // VDL=-15V
    
    sendCommand(0x06);    // Booster Soft Start
    sendData(0x17);      // A
    sendData(0x17);      // B
    sendData(0x17);      // C
    
    sendCommand(0x04);    // Power on
    waitUntilIdle();
    
    sendCommand(0x00);    // Panel setting
    sendData(0x0f);      // KW-3f   KWR-2F BWROTP 0f BWOTP 1f
    
    sendCommand(0x61);    // Resolution setting
    sendData(0x03);      // 800
    sendData(0x20);
    sendData(0x01);      // 480
    sendData(0xE0);
    
    sendCommand(0x15);    // Dual SPI
    sendData(0x00);
    
    sendCommand(0x50);    // VCOM AND DATA INTERVAL SETTING
    sendData(0x11);
    sendData(0x07);
    
    sendCommand(0x60);    // TCON SETTING
    sendData(0x22);
    
    delay(10);
}

void MT_EPD::writeRAM(uint16_t xSize, uint16_t ySize, uint8_t* buffer, uint16_t offset, uint8_t command) {
    waitUntilIdle();
    sendCommand(command);
    
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_cs_pin, LOW);
    
    for(uint16_t i = 0; i < ySize; i++) {
        for(uint16_t j = 0; j < xSize; j++) {
            SPI.transfer(command == 0x24 ? 0xFF : 0x00);  // White for BW, no red
        }
    }
    
    digitalWrite(_cs_pin, HIGH);
}

void MT_EPD::setPartialWindow(uint16_t xStart, uint16_t xEnd, uint16_t yStart, uint16_t yEnd) {
    sendCommand(0x90);    // Partial Window
    sendData(xStart & 0xFF);
    sendData((xStart >> 8) & 0xFF);
    sendData(xEnd & 0xFF);
    sendData((xEnd >> 8) & 0xFF);
    sendData(yStart & 0xFF);
    sendData((yStart >> 8) & 0xFF);
    sendData(yEnd & 0xFF);
    sendData((yEnd >> 8) & 0xFF);
    sendData(0x00);
}

void MT_EPD::clearDisplay() {
    // Full window
    sendCommand(0x90);    // Partial Window
    sendData(0x00);      // x-start 
    sendData(0x00);
    sendData(0x03);      // x-end: 799
    sendData(0x1F);
    sendData(0x00);      // y-start
    sendData(0x00);
    sendData(0x01);      // y-end: 479
    sendData(0xDF);
    sendData(0x00);
    
    // Write to BW RAM
    writeRAM(_width/8, _height, NULL, 0, 0x24);
    
    // Write to Red RAM
    writeRAM(_width/8, _height, NULL, 0, 0x26);
    
    display();
}

void MT_EPD::display() {
    sendCommand(0x22);    // Display Update Control
    sendData(0xF7);      // Load temperature value, Load LUT
    sendCommand(0x20);    // Activate Display Update Sequence
    waitUntilIdle();
}

void MT_EPD::drawBox(int x_start, int y_start, int width, int height, uint16_t color) {
    if(x_start < 0 || x_start + width > _width || y_start < 0 || y_start + height > _height) {
        return;
    }
    
    // Set partial window
    sendCommand(0x91);    // Enter Partial Mode
    setPartialWindow(x_start, x_start + width - 1, y_start, y_start + height - 1);
    
    // Write to BW RAM
    sendCommand(0x24);
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_cs_pin, LOW);
    
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < (width + 7)/8; x++) {
            SPI.transfer(color == EPD_BLACK ? 0x00 : 0xFF);
        }
    }
    
    digitalWrite(_cs_pin, HIGH);
    
    // Write to Red RAM
    sendCommand(0x26);
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_cs_pin, LOW);
    
    for(int y = 0; y < height; y++) {
        for(int x = 0; x < (width + 7)/8; x++) {
            SPI.transfer(color == EPD_RED ? 0xFF : 0x00);
        }
    }
    
    digitalWrite(_cs_pin, HIGH);
    
    display();
}

void MT_EPD::sleep() {
    sendCommand(0x02);    // Power off
    waitUntilIdle();
    sendCommand(0x07);    // Deep sleep
    sendData(0xA5);
}

#endif