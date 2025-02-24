#include "MT_EPD.h"
#include <Arduino.h>

const uint16_t MT_EPD::EPD_BLACK = 0x0000;
const uint16_t MT_EPD::EPD_WHITE = 0xFFFF;
const uint16_t MT_EPD::EPD_RED = 0xF800;

MT_EPD::MT_EPD(int8_t cs, int8_t dc, int8_t rst, int8_t busy)
    : _cs_pin(cs), _dc_pin(dc), _rst_pin(rst), _busy_pin(busy),
      _width(800), _height(480) {}

void MT_EPD::begin(void) {
    digitalWrite(_cs_pin, HIGH);
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_rst_pin, HIGH);
    
    Serial.println("Initializing display...");
    
    // Hardware reset
    reset();
    delay(100);

    // Panel setting
    sendCommand(0x00);    
    sendData(0x0f);      // KW/R mode, LUT from OTP
    
    // Power setting
    sendCommand(0x01);
    sendData(0x07);      // VGH=20V
    sendData(0x07);      // VGL=-20V
    sendData(0x3f);      // VDH=15V
    sendData(0x3f);      // VDL=-15V
    
    sendCommand(0x06);    // Booster soft start
    sendData(0x27);      
    sendData(0x27);   
    sendData(0x2F);  
    sendData(0x17);
    
    sendCommand(0x61);    // Resolution setting
    sendData(0x03);      // 800
    sendData(0x20);
    sendData(0x01);      // 480
    sendData(0xE0);
    
    sendCommand(0x15);
    sendData(0x00);
    
    sendCommand(0x50);    // VCOM and data interval
    sendData(0x11);      // VCOM
    sendData(0x07);
    
    sendCommand(0x60);
    sendData(0x22);
    
    sendCommand(0x04);    // Power on
    waitUntilIdle();

    Serial.println("Init complete");
}

void MT_EPD::clearDisplay(void) {
    Serial.println("Clearing display...");
    
    // Fill black/white plane with white
    sendCommand(0x10);    
    for(uint32_t i = 0; i < (_width * _height / 8); i++) {
        sendData(0xFF);  // White
    }

    // Fill red plane with no red
    sendCommand(0x13);    
    for(uint32_t i = 0; i < (_width * _height / 8); i++) {
        sendData(0x00);  // No red
    }

    // Update display
    display();
}

void MT_EPD::drawBox(int x_start, int y_start, int width, int height, uint16_t color) {
    // Input validation
    if (x_start < 0 || y_start < 0 || width <= 0 || height <= 0) return;
    if (x_start >= _width || y_start >= _height) return;
    
    // Adjust dimensions if they exceed display bounds
    if (x_start + width > _width) width = _width - x_start;
    if (y_start + height > _height) height = _height - y_start;
    
    Serial.printf("Drawing box: x=%d, y=%d, w=%d, h=%d, color=%04X\n", 
                  x_start, y_start, width, height, color);
    
    // Calculate byte positions for x coordinates
    int start_byte = x_start / 8;
    int end_byte = (x_start + width - 1) / 8;
    
    // Black/white plane
    sendCommand(0x10);
    for (int y = 0; y < _height; y++) {
        for (int byte_x = 0; byte_x < _width/8; byte_x++) {
            uint8_t data;
            
            // Determine if this byte is in the box area
            if (y >= y_start && y < (y_start + height) && 
                byte_x >= start_byte && byte_x <= end_byte) {
                
                // Handle potential partial bytes at the edges
                if (color == EPD_BLACK) {
                    data = 0x00;  // Black
                } else {
                    data = 0xFF;  // White (for both white and red colors)
                }
            } else {
                data = 0xFF;  // White outside the box
            }
            
            sendData(data);
        }
    }
    
    // Red plane
    sendCommand(0x13);
    for (int y = 0; y < _height; y++) {
        for (int byte_x = 0; byte_x < _width/8; byte_x++) {
            uint8_t data;
            
            // Determine if this byte is in the box area
            if (y >= y_start && y < (y_start + height) && 
                byte_x >= start_byte && byte_x <= end_byte) {
                
                if (color == EPD_RED) {
                    data = 0xFF;  // Red
                } else {
                    data = 0x00;  // No red
                }
            } else {
                data = 0x00;  // No red outside the box
            }
            
            sendData(data);
        }
    }
}

void MT_EPD::display(void) {
    Serial.println("Updating display...");
    sendCommand(0x12);    // Display refresh
    waitUntilIdle();
}

void MT_EPD::sleep(void) {
    Serial.println("Entering sleep mode...");
    sendCommand(0x02);    // Power off
    waitUntilIdle();
    sendCommand(0x07);    // Deep sleep
    sendData(0xA5);      // Check code
}

void MT_EPD::reset(void) {
    Serial.println("Resetting...");
    digitalWrite(_rst_pin, HIGH);
    delay(10);
    digitalWrite(_rst_pin, LOW);
    delay(10);
    digitalWrite(_rst_pin, HIGH);
    delay(10);
}

void MT_EPD::sendCommand(uint8_t command) {
    digitalWrite(_cs_pin, LOW);
    digitalWrite(_dc_pin, LOW);
    SPI.transfer(command);
    digitalWrite(_cs_pin, HIGH);
}

void MT_EPD::sendData(uint8_t data) {
    digitalWrite(_cs_pin, LOW);
    digitalWrite(_dc_pin, HIGH);
    SPI.transfer(data);
    digitalWrite(_cs_pin, HIGH);
}

void MT_EPD::waitUntilIdle(void) {
    Serial.println("Waiting for busy signal...");
    while(digitalRead(_busy_pin) == HIGH) {
        delay(100);
    }
    Serial.println("Busy signal cleared");
}

void MT_EPD::writeRAM(uint16_t xSize, uint16_t ySize, uint8_t* buffer, uint16_t offset, uint8_t command) {
    waitUntilIdle();
    sendCommand(command);
    
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_cs_pin, LOW);
    
    for(uint16_t i = 0; i < ySize; i++) {
        for(uint16_t j = 0; j < xSize; j++) {
            uint8_t data = buffer[offset + i * xSize + j];
            SPI.transfer(data);
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
    sendData(0x00);       // Don't care value
}