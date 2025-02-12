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
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void sleep(void);
    
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
    void reset(void);
    void sendCommand(uint8_t command);
    void sendData(uint8_t data);
    void waitUntilIdle();
    void setMemoryArea(int x_start, int y_start, int x_end, int y_end);
    void setMemoryPointer(int x, int y);
};

// Implementations
MT_EPD::MT_EPD(int8_t cs, int8_t dc, int8_t rst, int8_t busy) :
  _cs_pin(cs), _dc_pin(dc), _rst_pin(rst), _busy_pin(busy),
  _width(800), _height(480)
{
}

void MT_EPD::begin() {
  // Initialize pins
  pinMode(_cs_pin, OUTPUT);
  pinMode(_dc_pin, OUTPUT);
  pinMode(_rst_pin, OUTPUT);
  pinMode(_busy_pin, INPUT);
  
  digitalWrite(_cs_pin, HIGH);
  digitalWrite(_dc_pin, HIGH);
  digitalWrite(_rst_pin, HIGH);
  
  // Hardware reset first
  reset();
  
  // 1. Panel Setting
  sendCommand(0x00);    // PSR - Panel Setting
  sendData(0x0F);      // KW/R mode, Gate scan up, shift right, booster on
  
  // 2. Power Setting
  sendCommand(0x01);    // PWR
  sendData(0x03);      // VDS_EN, VDG_EN
  sendData(0x00);      // VCOM_HV, VGHL_LV=16V
  sendData(0x2B);      // VDH
  sendData(0x2B);      // VDL
  sendData(0x09);      // VDHR
  delay(10);

  // 3. Booster Soft Start
  sendCommand(0x06);     // BTST - Booster Setting
  sendData(0x07);       // Phase A
  sendData(0x07);       // Phase B
  sendData(0x17);       // Phase C
  delay(10);

  // 4. Power ON
  sendCommand(0x04);     // Power on
  delay(100);           // Wait 100ms for power to stabilize
  waitUntilIdle();
  
  // 5. Panel Setting (again, after power on)
  sendCommand(0x00);
  sendData(0x0F);

  // 6. PLL Control
  sendCommand(0x30);
  sendData(0x06);      // 50Hz
  delay(10);
  
  // 7. VCOM and data interval setting
  sendCommand(0x50);
  sendData(0x77);      // Default value

  // 8. Set RAM X address
  sendCommand(0x11);
  sendData(0x00);
  
  // 9. Set RAM Y address
  sendCommand(0x12);
  sendData(0x00);

  delay(100);
}

void MT_EPD::reset() {
  digitalWrite(_rst_pin, HIGH);
  delay(20);
  digitalWrite(_rst_pin, LOW);    // Pull low to reset
  delay(2);                       // At least 2ms delay
  digitalWrite(_rst_pin, HIGH);
  delay(20);                      // At least 20ms delay
  
  // Wait for busy
  int timeout = 1000;   // 1 second timeout
  int start = millis();
  while(digitalRead(_busy_pin) == HIGH && (millis() - start < timeout)) {
    delay(1);
  }
}

void MT_EPD::sendCommand(uint8_t command) {
  digitalWrite(_dc_pin, LOW);      // Command mode
  digitalWrite(_cs_pin, LOW);      // Select display
  delayMicroseconds(10);          // Allow signals to settle
  SPI.transfer(command);
  delayMicroseconds(10);          // Allow signals to settle
  digitalWrite(_cs_pin, HIGH);     // Deselect display
  delayMicroseconds(10);          // CS high time
}

void MT_EPD::sendData(uint8_t data) {
  digitalWrite(_dc_pin, HIGH);     // Data mode
  digitalWrite(_cs_pin, LOW);      // Select display
  delayMicroseconds(10);          // Allow signals to settle
  SPI.transfer(data);
  delayMicroseconds(10);          // Allow signals to settle
  digitalWrite(_cs_pin, HIGH);     // Deselect display
  delayMicroseconds(10);          // CS high time
}

void MT_EPD::waitUntilIdle() {
  while(digitalRead(_busy_pin) == HIGH) {
    delay(10);
  }
}

void MT_EPD::setMemoryArea(int x_start, int y_start, int x_end, int y_end) {
  // Set X addresses
  sendCommand(0x44);
  sendData(x_start & 0xFF);
  sendData((x_start >> 8) & 0xFF);
  sendData(x_end & 0xFF);
  sendData((x_end >> 8) & 0xFF);
  
  // Set Y addresses
  sendCommand(0x45);
  sendData(y_start & 0xFF);
  sendData((y_start >> 8) & 0xFF);
  sendData(y_end & 0xFF);
  sendData((y_end >> 8) & 0xFF);
}

void MT_EPD::setMemoryPointer(int x, int y) {
  sendCommand(0x4E);
  sendData(x & 0xFF);
  sendData((x >> 8) & 0xFF);
  sendCommand(0x4F);
  sendData(y & 0xFF);
  sendData((y >> 8) & 0xFF);
}

void MT_EPD::clearDisplay() {
  // Set memory window
  sendCommand(0x44);    // Set RAM X start/end
  sendData(0x00);      // XStart = 0
  sendData(0x00);
  sendData(0x31);      // XEnd = 799 (0x31F)
  sendData(0x03);
  
  sendCommand(0x45);    // Set RAM Y start/end
  sendData(0x00);      // YStart = 0
  sendData(0x00);
  sendData(0xDF);      // YEnd = 479 (0x1DF)
  sendData(0x01);
  
  // Set cursor to beginning
  sendCommand(0x4E);    // Set RAM X counter
  sendData(0x00);
  sendData(0x00);
  
  sendCommand(0x4F);    // Set RAM Y counter
  sendData(0x00);
  sendData(0x00);
  
  // Write Black/White RAM
  sendCommand(0x24);
  for(uint32_t i = 0; i < 48000; i++) {  // 800 * 480 / 8
    sendData(0xFF);  // White
  }
  
  // Write Red RAM
  sendCommand(0x26);
  for(uint32_t i = 0; i < 48000; i++) {
    sendData(0x00);  // No red
  }
  
  // Update display
  sendCommand(0x22);
  sendData(0xF7);    // Full update with LUT from OTP
  sendCommand(0x20);
  waitUntilIdle();
}

void MT_EPD::display() {
  sendCommand(0x22); // DISPLAY UPDATE CONTROL
  sendData(0xF7);    // Full update with LUT from OTP
  sendCommand(0x20); // MASTER ACTIVATION
  waitUntilIdle();
}

void MT_EPD::drawPixel(int16_t x, int16_t y, uint16_t color) {
  if(x < 0 || x >= _width || y < 0 || y >= _height) return;
  
  sendCommand(0x4E);
  sendData(x & 0xFF);
  
  sendCommand(0x4F);
  sendData(y & 0xFF);
  
  sendCommand(0x24);  // WRITE_RAM
  sendData(color == EPD_WHITE ? 0xFF : 0x00);
  
  if(color == EPD_RED) {
    sendCommand(0x26);
    sendData(0xFF);
  }
}

void MT_EPD::sleep() {
  sendCommand(0x10);  // DEEP_SLEEP_MODE
  sendData(0x01);
}

#endif