#include "WS_EPD.h"

WS_EPD::WS_EPD(int8_t cs, int8_t dc, int8_t rst, int8_t busy)
    : Adafruit_GFX(800, 480),  // Initialize with physical dimensions (7.5" e-Paper is 800x480)
      _cs_pin(cs), _dc_pin(dc), _rst_pin(rst), _busy_pin(busy),
      _width(800), _height(480) {
    
    _rotation = 0;  // Default orientation
    
    // Calculate buffer size (1 bit per pixel for each plane)
    _buffer_size = _width * _height / 8;
    
    // Allocate memory for buffers
    _buffer_bw = (uint8_t*)malloc(_buffer_size);
    _buffer_red = NULL; // Not using red buffer since this is a black and white display
    
    // Initialize buffer to white
    if (_buffer_bw) {
        memset(_buffer_bw, 0xFF, _buffer_size);  // 0xFF for white
    }
    
    // Start with rotation 0
    setRotation(0);
}

WS_EPD::~WS_EPD() {
    // Free allocated memory
    if (_buffer_bw) free(_buffer_bw);
    // _buffer_red is NULL so no need to free it
}

void WS_EPD::begin() {
    // Configure pins
    pinMode(_cs_pin, OUTPUT);
    pinMode(_dc_pin, OUTPUT);
    pinMode(_rst_pin, OUTPUT);
    pinMode(_busy_pin, INPUT);
    
    digitalWrite(_cs_pin, HIGH);
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_rst_pin, HIGH);
    
    // Hardware reset
    reset();
    delay(100);
    
    // Initialize the display with Waveshare-specific configuration
    initDisplay();
    
    Serial.println("Display initialization complete");
}

void WS_EPD::initDisplay() {
    // Based on Waveshare 7.5inch E-Paper V2 initialization sequence
    sendCommand(0x00); // PANEL SETTING
    sendData(0x0F);    // KW/R mode + LUT from OTP
    
    sendCommand(0x01); // POWER SETTING
    sendData(0x07);    // VGH=20V, VGL=-20V
    sendData(0x07);    // VGH=20V, VGL=-20V
    sendData(0x3F);    // VDH=15V
    sendData(0x3F);    // VDL=-15V
    
    sendCommand(0x06); // BOOSTER SOFT START
    sendData(0x17);
    sendData(0x17);
    sendData(0x17);
    
    sendCommand(0x04); // POWER ON
    waitUntilIdle();
    
    sendCommand(0x00); // PANEL SETTING
    sendData(0x0F);    // KW/R mode + LUT from OTP
    
    // Resolution setting
    sendCommand(0x61);
    sendData(0x03);    // 800px
    sendData(0x20);
    sendData(0x01);    // 480px
    sendData(0xE0);
    
    sendCommand(0x15); // DUAL SPI
    sendData(0x00);
    
    sendCommand(0x50); // VCOM AND DATA INTERVAL SETTING
    sendData(0x11);    // VCOM setting
    sendData(0x07);
    
    sendCommand(0x60); // TCON SETTING
    sendData(0x22);
    
    sendCommand(0x82); // VCM DC SETTING
    sendData(0x00);    // -0.1V
    
    sendCommand(0x30); // PLL CONTROL
    sendData(0x06);    // Default 50Hz
    
    clearDisplay();
}

void WS_EPD::reset() {
    digitalWrite(_rst_pin, LOW);    // Reset low
    delay(10);
    digitalWrite(_rst_pin, HIGH);   // Reset high
    delay(10);
}

void WS_EPD::clearDisplay() {
    if (!_buffer_bw) return;
    
    // Fill buffer for white display
    memset(_buffer_bw, 0xFF, _buffer_size);  // 0xFF for white in B/W buffer
}

void WS_EPD::sendCommand(uint8_t command) {
    digitalWrite(_dc_pin, LOW);     // Command mode
    digitalWrite(_cs_pin, LOW);     // Chip select low
    SPI.transfer(command);
    digitalWrite(_cs_pin, HIGH);    // Chip select high
}

void WS_EPD::sendData(uint8_t data) {
    digitalWrite(_dc_pin, HIGH);    // Data mode
    digitalWrite(_cs_pin, LOW);     // Chip select low
    SPI.transfer(data);
    digitalWrite(_cs_pin, HIGH);    // Chip select high
}

void WS_EPD::waitUntilIdle() {
    while (digitalRead(_busy_pin) == HIGH) {  // Waveshare BUSY is HIGH when busy
        delay(10);
    }
}

void WS_EPD::setRotation(uint8_t r) {
    // Call parent implementation to handle width/height swapping
    Adafruit_GFX::setRotation(r);
    _rotation = r % 4;
}

void WS_EPD::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= width() || y < 0 || y >= height()) return;
    
    // Handle rotation using GFX library's logical dimensions
    int16_t temp;
    switch (getRotation()) {
        case 1:  // 90 degrees clockwise
            temp = x;
            x = _width - 1 - y;
            y = temp;
            break;
        case 2:  // 180 degrees
            x = _width - 1 - x;
            y = _height - 1 - y;
            break;
        case 3:  // 270 degrees clockwise
            temp = x;
            x = y;
            y = _height - 1 - temp;
            break;
    }
    
    // Calculate byte index and bit position
    // Waveshare display is 800 pixels wide, so each row has 100 bytes (800/8)
    uint32_t byte_idx = (y * 100) + (x / 8);
    uint8_t bit_position = 7 - (x % 8);  // MSB format
    
    if (byte_idx >= _buffer_size) return;
    
    // Set pixel according to color - we only need to handle black and white
    // since this panel doesn't support red
    if (color == EPD_BLACK) {
        _buffer_bw[byte_idx] &= ~(1 << bit_position);  // Black
    } 
    else { // white or any other color
        _buffer_bw[byte_idx] |= (1 << bit_position);   // White
    }
}

void WS_EPD::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, int16_t w, int16_t h, 
                      uint16_t color, uint16_t bg_color) {
    for (int16_t j = 0; j < h; j++) {
        for (int16_t i = 0; i < (w + 7) / 8; i++) {
            uint8_t byte = pgm_read_byte(&bitmap[j * ((w + 7) / 8) + i]);
            
            for (int16_t b = 0; b < 8; b++) {
                if (i * 8 + b < w) {  // Ensure we're not going past the width
                    if (byte & (0x80 >> b)) {
                        drawPixel(x + i * 8 + b, y + j, color);
                    } else {
                        drawPixel(x + i * 8 + b, y + j, bg_color);
                    }
                }
            }
        }
    }
}

void WS_EPD::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    for (int16_t i = x; i < x + w; i++) {
        for (int16_t j = y; j < y + h; j++) {
            drawPixel(i, j, color);
        }
    }
}

void WS_EPD::display() {
    if (!_buffer_bw) return;
    
    // Power on if needed
    powerOn();
    
    // Send black/white data to display
    sendCommand(0x10); // DATA_START_TRANSMISSION_1 (Black/White data)
    
    for (uint32_t i = 0; i < _buffer_size; i++) {
        sendData(_buffer_bw[i]);
        if ((i % 100) == 0) yield(); // Allow time for system processes
    }
    
    // For black and white only display, we send the same data to both buffers
    sendCommand(0x13); // DATA_START_TRANSMISSION_2
    
    for (uint32_t i = 0; i < _buffer_size; i++) {
        sendData(0x00); // All zeros for second buffer
        if ((i % 100) == 0) yield(); // Allow time for system processes
    }
    
    // Update display
    sendCommand(0x12); // DISPLAY_REFRESH
    waitUntilIdle();
}

void WS_EPD::powerOn() {
    sendCommand(0x04); // POWER_ON
    waitUntilIdle();
}

void WS_EPD::powerOff() {
    sendCommand(0x02); // POWER_OFF
    waitUntilIdle();
}

void WS_EPD::sleep() {
    powerOff(); // First power off
    
    // Enter deep sleep mode
    sendCommand(0x07); // DEEP_SLEEP
    sendData(0xA5);    // Check code
}

void WS_EPD::setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    // Ensure coordinates are within bounds
    if (x >= _width) return;
    if (y >= _height) return;
    if (x + w > _width) w = _width - x;
    if (y + h > _height) h = _height - y;
    
    // Enter partial mode
    sendCommand(0x91); // PARTIAL_IN
    
    // Set partial window parameters
    sendCommand(0x90); // PARTIAL_WINDOW
    sendData(x & 0xFF);
    sendData((x >> 8) & 0xFF);
    sendData((x + w - 1) & 0xFF);
    sendData(((x + w - 1) >> 8) & 0xFF);
    sendData(y & 0xFF);
    sendData((y >> 8) & 0xFF);
    sendData((y + h - 1) & 0xFF);
    sendData(((y + h - 1) >> 8) & 0xFF);
    sendData(0x01); // Gates scan both inside and outside of the partial window
}

void WS_EPD::displayPartial() {
    // Similar to display() but optimized for partial updates
    // Send black/white buffer
    sendCommand(0x10); // DATA_START_TRANSMISSION_1
    for (uint32_t i = 0; i < _buffer_size; i++) {
        sendData(_buffer_bw[i]);
        if ((i % 100) == 0) yield();
    }
    
    // For black and white only display, send zeros for second buffer
    sendCommand(0x13); // DATA_START_TRANSMISSION_2
    for (uint32_t i = 0; i < _buffer_size; i++) {
        sendData(0x00); // All zeros
        if ((i % 100) == 0) yield();
    }
    
    // Display update
    sendCommand(0x12); // DISPLAY_REFRESH
    waitUntilIdle();
    
    // Exit partial mode
    sendCommand(0x92); // PARTIAL_OUT
}

uint8_t* WS_EPD::createPartialBuffer(uint16_t x, uint16_t y, uint16_t w, uint16_t h, size_t* bufferSize) {
    // Calculate buffer size (bytes)
    uint16_t widthBytes = (w + 7) / 8;
    *bufferSize = widthBytes * h;
    
    // Allocate buffer
    uint8_t* partialBuffer = (uint8_t*)malloc(*bufferSize);
    if (!partialBuffer) {
        return NULL; // Memory allocation failed
    }
    
    // Initialize to white
    memset(partialBuffer, 0xFF, *bufferSize);
    
    // Copy data from the full buffer to the partial buffer
    for (uint16_t row = 0; row < h; row++) {
        for (uint16_t col = 0; col < w; col++) {
            // Calculate positions in full and partial buffers
            uint16_t fullX = x + col;
            uint16_t fullY = y + row;
            
            uint32_t fullBufIndex = (fullY * 100) + (fullX / 8); // 100 = 800/8
            uint8_t fullBitPos = 7 - (fullX % 8);
            
            uint32_t partialBufIndex = (row * widthBytes) + (col / 8);
            uint8_t partialBitPos = 7 - (col % 8);
            
            // Check bounds
            if (fullBufIndex >= _buffer_size) continue;
            
            // Copy bit
            bool isBlack = !(_buffer_bw[fullBufIndex] & (1 << fullBitPos));
            if (isBlack) {
                partialBuffer[partialBufIndex] &= ~(1 << partialBitPos);
            }
        }
    }
    
    return partialBuffer;
}

void WS_EPD::updatePartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    // Ensure coordinates are within bounds
    if (x >= _width) return;
    if (y >= _height) return;
    if (x + w > _width) w = _width - x;
    if (y + h > _height) h = _height - y;
    
    // Create buffer for the region
    size_t bufferSize = 0;
    uint8_t* partialBuffer = createPartialBuffer(x, y, w, h, &bufferSize);
    
    if (!partialBuffer) {
        Serial.println("Failed to create partial buffer");
        return;
    }
    
    // Set partial window
    setPartialWindow(x, y, w, h);
    
    // Send buffer data
    sendCommand(0x10); // DATA_START_TRANSMISSION_1
    for (size_t i = 0; i < bufferSize; i++) {
        sendData(partialBuffer[i]);
        if ((i % 100) == 0) yield();
    }
    
    // Using no red data for partial update
    sendCommand(0x13); // DATA_START_TRANSMISSION_2
    for (size_t i = 0; i < bufferSize; i++) {
        sendData(0x00); // No red
        if ((i % 100) == 0) yield();
    }
    
    // Refresh display
    sendCommand(0x12); // DISPLAY_REFRESH
    waitUntilIdle();
    
    // Exit partial mode
    sendCommand(0x92); // PARTIAL_OUT
    
    // Free the buffer
    free(partialBuffer);
}