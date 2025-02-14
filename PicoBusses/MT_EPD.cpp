#include "MT_EPD.h"

// Define static variables
const uint16_t MT_EPD::EPD_BLACK = 0x0000;
const uint16_t MT_EPD::EPD_WHITE = 0xFFFF;
const uint16_t MT_EPD::EPD_RED = 0xF800;

const uint8_t MT_EPD::LUT_VCOM[] = {
    0x00, 0x17, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x17, 0x17, 0x00, 0x00, 0x02,
    0x00, 0x0A, 0x01, 0x00, 0x00, 0x01,
    0x00, 0x0E, 0x0E, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

const uint8_t MT_EPD::LUT_WW[] = {
    0x40, 0x17, 0x00, 0x00, 0x00, 0x02,
    0x90, 0x17, 0x17, 0x00, 0x00, 0x02,
    0x40, 0x0A, 0x01, 0x00, 0x00, 0x01,
    0xA0, 0x0E, 0x0E, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t MT_EPD::LUT_BW[] = {
    0x40, 0x17, 0x00, 0x00, 0x00, 0x02,
    0x90, 0x17, 0x17, 0x00, 0x00, 0x02,
    0x40, 0x0A, 0x01, 0x00, 0x00, 0x01,
    0xA0, 0x0E, 0x0E, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t MT_EPD::LUT_WB[] = {
    0x80, 0x17, 0x00, 0x00, 0x00, 0x02,
    0x90, 0x17, 0x17, 0x00, 0x00, 0x02,
    0x80, 0x0A, 0x01, 0x00, 0x00, 0x01,
    0x50, 0x0E, 0x0E, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const uint8_t MT_EPD::LUT_BB[] = {
    0x80, 0x17, 0x00, 0x00, 0x00, 0x02,
    0x90, 0x17, 0x17, 0x00, 0x00, 0x02,
    0x80, 0x0A, 0x01, 0x00, 0x00, 0x01,
    0x50, 0x0E, 0x0E, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

// Constructor
MT_EPD::MT_EPD(int8_t cs, int8_t dc, int8_t rst, int8_t busy)
    : _cs_pin(cs), _dc_pin(dc), _rst_pin(rst), _busy_pin(busy),
      _width(800), _height(480) {}

void MT_EPD::begin(void) {
    // Hardware reset
    reset();
    delay(100);
    
    // Init sequence from datasheet
    sendCommand(0x01);    // POWER SETTING
    sendData(0x07);
    sendData(0x07);      // VGH=20V,VGL=-20V
    sendData(0x3f);      // VDH=15V
    sendData(0x3f);      // VDL=-15V
    
    sendCommand(0x06);    // Booster Soft Start
    sendData(0x17);
    sendData(0x17);
    sendData(0x17);

    sendCommand(0x04);    // Power on
    waitUntilIdle();
    
    sendCommand(0x00);    // Panel Setting
    sendData(0x0f);      // KW mode
    
    sendCommand(0x61);    // Resolution setting
    sendData(0x03);      // 800
    sendData(0x20);
    sendData(0x01);      // 480
    sendData(0xE0);
    
    // Set LUT
    sendCommand(0x20);    // VCOM LUT
    for(int i = 0; i < 44; i++) {
        sendData(LUT_VCOM[i]);
    }
    
    sendCommand(0x21);    // WW LUT
    for(int i = 0; i < 42; i++) {
        sendData(LUT_WW[i]);
    }
    
    sendCommand(0x22);    // BW LUT
    for(int i = 0; i < 42; i++) {
        sendData(LUT_BW[i]);
    }
    
    sendCommand(0x23);    // WB LUT
    for(int i = 0; i < 42; i++) {
        sendData(LUT_WB[i]);
    }
    
    sendCommand(0x24);    // BB LUT
    for(int i = 0; i < 42; i++) {
        sendData(LUT_BB[i]);
    }

    sendCommand(0x50);    // VCOM AND DATA INTERVAL SETTING
    sendData(0x11);
    sendData(0x07);

    Serial.println("Power on complete");
}

void MT_EPD::clearDisplay(void) {
    Serial.println("Clearing display...");
    
    // Calculate number of bytes needed
    uint32_t bytes_per_line = _width / 8;
    uint8_t* line_buffer = new uint8_t[bytes_per_line];
    memset(line_buffer, 0xFF, bytes_per_line);  // Set all bits to 1 for white
    
    // First data transmission - old data
    sendCommand(0x10);
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_cs_pin, LOW);
    
    for(int y = 0; y < _height; y++) {
        for(int x = 0; x < bytes_per_line; x++) {
            SPI.transfer(line_buffer[x]);
        }
    }
    digitalWrite(_cs_pin, HIGH);

    // Second data transmission - new data
    sendCommand(0x13);
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_cs_pin, LOW);
    
    for(int y = 0; y < _height; y++) {
        for(int x = 0; x < bytes_per_line; x++) {
            SPI.transfer(line_buffer[x]);
        }
    }
    digitalWrite(_cs_pin, HIGH);
    
    delete[] line_buffer;

    // Refresh display
    sendCommand(0x12);
    delay(100);
    waitUntilIdle();
    
    Serial.println("Clear complete");
}

void MT_EPD::display() {
    sendCommand(0x22);    // Display Update Control
    sendData(0xF7);      // Load temperature value, Load LUT
    sendCommand(0x20);    // Activate Display Update Sequence
    waitUntilIdle();
}

void MT_EPD::sleep(void) {
    sendCommand(0x02);  // Power off
    waitUntilIdle();
    sendCommand(0x07);  // Deep sleep
    sendData(0xA5);     // Deep sleep check code
}

void MT_EPD::drawBox(int x_start, int y_start, int width, int height, uint16_t color) {
    Serial.println("Drawing box...");

    // Calculate which bit in the byte we start with
    int start_bit = x_start % 8;
    int start_byte = x_start / 8;
    
    // Calculate bytes needed to cover the width
    int byte_width = (width + start_bit + 7) / 8;

    // Create a buffer for one line
    uint8_t* line_buffer = new uint8_t[_width/8];
    
    // First data transmission - old data
    sendCommand(0x10);
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_cs_pin, LOW);
    
    for(int y = 0; y < _height; y++) {
        // Fill buffer with white
        memset(line_buffer, 0xFF, _width/8);
        
        // If we're in the box's y-range
        if(y >= y_start && y < (y_start + height)) {
            // Set the appropriate bits in each affected byte
            for(int i = 0; i < byte_width; i++) {
                uint8_t mask = 0xFF;
                
                // Handle first byte
                if(i == 0 && start_bit > 0) {
                    mask = 0xFF >> start_bit;
                }
                
                // Handle last byte
                if(i == byte_width-1) {
                    int end_bit = (x_start + width) % 8;
                    if(end_bit > 0) {
                        mask &= 0xFF << (8 - end_bit);
                    }
                }
                
                // Set or clear bits based on color
                if(color == EPD_BLACK) {
                    line_buffer[start_byte + i] &= ~mask;  // Clear bits for black
                } else {
                    line_buffer[start_byte + i] |= mask;   // Set bits for white
                }
            }
        }
        
        // Send the line
        for(int x = 0; x < _width/8; x++) {
            SPI.transfer(line_buffer[x]);
        }
    }
    digitalWrite(_cs_pin, HIGH);

    // Second data transmission - new data
    sendCommand(0x13);
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_cs_pin, LOW);
    
    for(int y = 0; y < _height; y++) {
        memset(line_buffer, 0xFF, _width/8);
        
        if(y >= y_start && y < (y_start + height)) {
            for(int i = 0; i < byte_width; i++) {
                uint8_t mask = 0xFF;
                
                if(i == 0 && start_bit > 0) {
                    mask = 0xFF >> start_bit;
                }
                
                if(i == byte_width-1) {
                    int end_bit = (x_start + width) % 8;
                    if(end_bit > 0) {
                        mask &= 0xFF << (8 - end_bit);
                    }
                }
                
                if(color == EPD_BLACK) {
                    line_buffer[start_byte + i] &= ~mask;
                } else {
                    line_buffer[start_byte + i] |= mask;
                }
            }
        }
        
        for(int x = 0; x < _width/8; x++) {
            SPI.transfer(line_buffer[x]);
        }
    }
    digitalWrite(_cs_pin, HIGH);
    
    delete[] line_buffer;

    // Refresh display
    sendCommand(0x12);
    delay(100);
    waitUntilIdle();
    
    Serial.println("Display refresh complete");
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

void MT_EPD::waitUntilIdle(void) {
    Serial.println("Waiting for busy signal...");
    unsigned long startTime = millis();
    while(digitalRead(_busy_pin) == HIGH) {  // HIGH means busy
        delay(100);  // Don't tight loop
        if(millis() - startTime > 10000) {  // 10 second timeout
            Serial.println("Busy timeout!");
            return;
        }
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