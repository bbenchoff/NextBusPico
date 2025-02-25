#include "MT_EPD.h"
#include <Arduino.h>

MT_EPD::MT_EPD(int8_t cs, int8_t dc, int8_t rst, int8_t busy)
    : Adafruit_GFX(800, 480),  // Always initialize with the physical dimensions
      _cs_pin(cs), _dc_pin(dc), _rst_pin(rst), _busy_pin(busy),
      _width(800), _height(480) {

    _orientation = 0;  // Default orientation
    
    // Calculate buffer size (1 bit per pixel for each plane)
    _buffer_size = _width * _height / 8;
    
    // Allocate memory for buffers
    _buffer_bw = (uint8_t*)malloc(_buffer_size);
    _buffer_red = (uint8_t*)malloc(_buffer_size);

    // Initialize buffers to white (0xFF for B/W, 0x00 for red)
    if (_buffer_bw && _buffer_red) {
        memset(_buffer_bw, 0xFF, _buffer_size);
        memset(_buffer_red, 0x00, _buffer_size);
    }
    
    // Start with rotation 0
    setRotation(0);
}

MT_EPD::~MT_EPD() {
    // Free allocated memory
    if (_buffer_bw) free(_buffer_bw);
    if (_buffer_red) free(_buffer_red);
}


void MT_EPD::begin(void) {
    digitalWrite(_cs_pin, HIGH);
    digitalWrite(_dc_pin, HIGH);
    digitalWrite(_rst_pin, HIGH);
    
    Serial.println("Initializing display...");
    
    // Check if memory allocation was successful
    if (!_buffer_bw || !_buffer_red) {
        Serial.println("Error: Failed to allocate display buffers!");
        return;
    }
    
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

void MT_EPD::transformCoordinates(int16_t &x, int16_t &y) {
    // Transform from logical (0,0 at top-left) to physical coordinates
    int16_t temp_x = 799 - x;  // Flip X (799 = display width - 1)
    int16_t temp_y = 479 - y;  // Flip Y (479 = display height - 1)
    
    x = temp_x;
    y = temp_y;
}

void MT_EPD::setRotation(uint8_t r) {
    // Call parent implementation to handle width/height swapping
    Adafruit_GFX::setRotation(r);
    
    // No need for additional work - the parent class handles logical width/height
    // and our drawPixel implementation will handle the transformation
}

void MT_EPD::drawPixel(int16_t x, int16_t y, uint16_t color) {
    // Handle rotation (converting logical to physical coordinates)
    int16_t t;
    switch (getRotation()) {
        case 1:  // 90 degrees clockwise
            t = x;
            x = WIDTH - 1 - y;
            y = t;
            break;
        case 2:  // 180 degrees
            x = WIDTH - 1 - x;
            y = HEIGHT - 1 - y;
            break;
        case 3:  // 270 degrees clockwise
            t = x;
            x = y;
            y = HEIGHT - 1 - t;
            break;
    }

    // Bounds check with physical dimensions
    if (x < 0 || x >= 800 || y < 0 || y >= 480) return;
    
    // Calculate byte index and bit position as before
    uint32_t byte_idx = (y * 100) + (x / 8);
    uint8_t bit_position = 7 - (x % 8);
    
    if (byte_idx >= _buffer_size) return;
    
    // Set pixel according to color
    if (color == EPD_BLACK) {
        _buffer_bw[byte_idx] &= ~(1 << bit_position);
        _buffer_red[byte_idx] &= ~(1 << bit_position);
    } 
    else if (color == EPD_RED) {
        _buffer_bw[byte_idx] |= (1 << bit_position);
        _buffer_red[byte_idx] |= (1 << bit_position);
    }
    else { // white
        _buffer_bw[byte_idx] |= (1 << bit_position);
        _buffer_red[byte_idx] &= ~(1 << bit_position);
    }
}

void MT_EPD::drawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, 
                         int16_t w, int16_t h, uint16_t color, uint16_t bg_color) {
    // Iterate through each row
    for (int16_t j = 0; j < h; j++) {
        // Iterate through each byte in the row
        for (int16_t i = 0; i < (w + 7) / 8; i++) {
            uint8_t byte = pgm_read_byte(&bitmap[j * ((w + 7) / 8) + i]);
            
            // Process each bit in the byte
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

void MT_EPD::setDisplayOrientation(uint8_t orientation) {
    _orientation = orientation & 3;  // 0-3
    
    // Update the logical dimensions based on orientation
    if (_orientation & 1) {  // Rotated 90 or 270 degrees
        // Swap width and height for the logical dimensions
        Adafruit_GFX::_width = 480;   // Physical height becomes logical width
        Adafruit_GFX::_height = 800;  // Physical width becomes logical height
    } else {  // Original or 180 degrees
        Adafruit_GFX::_width = 800;   // Original width
        Adafruit_GFX::_height = 480;  // Original height
    }
}

void MT_EPD::clearDisplay(void) {
    Serial.println("Clearing display...");
    
    if (!_buffer_bw || !_buffer_red) return;
    
    // Fill black/white buffer with white (0xFF)
    memset(_buffer_bw, 0xFF, _buffer_size);
    
    // Fill red buffer with no red (0x00)
    memset(_buffer_red, 0x00, _buffer_size);
}

void MT_EPD::drawBox(int x_start, int y_start, int width, int height, uint16_t color) {
    // Use Adafruit_GFX's fillRect instead
    fillRect(x_start, y_start, width, height, color);
}

void MT_EPD::display(void) {
    Serial.println("Updating display...");
    
    if (!_buffer_bw || !_buffer_red) return;
    
    // Send black/white buffer
    sendCommand(0x10);
    for (uint32_t i = 0; i < _buffer_size; i++) {
        sendData(_buffer_bw[i]);
        // Process system tasks every 100 bytes
        if ((i % 100) == 0) yield();
    }
    
    // Send red buffer
    sendCommand(0x13);
    for (uint32_t i = 0; i < _buffer_size; i++) {
        sendData(_buffer_red[i]);
        // Process system tasks every 100 bytes
        if ((i % 100) == 0) yield();
    }
    
    // Update display
    sendCommand(0x12);
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

void MT_EPD::setPartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    // Bounds check
    if (x >= 800) return;
    if (y >= 480) return;
    if (x + w > 800) w = 800 - x;
    if (y + h > 480) h = 480 - y;
    
    // Send partial window command
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

void MT_EPD::updatePartialWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    // Ensure coordinates are within bounds
    if (x >= 800) return;
    if (y >= 480) return;
    if (x + w > 800) w = 800 - x;
    if (y + h > 480) h = 480 - y;
    
    // Calculate buffer size for the region (bytes)
    // Each byte contains 8 pixels
    size_t bufferSize = 0;
    uint8_t* partialBuffer = createPartialBuffer(x, y, w, h, &bufferSize);
    
    if (!partialBuffer) {
        Serial.println("Failed to create partial buffer");
        return;
    }
    
    // Set partial window parameters
    sendCommand(0x91); // PARTIAL_IN command - Enter partial mode
    
    // Set partial window parameters
    sendCommand(0x90); // PARTIAL_WINDOW
    sendData(x & 0xFF);  // X start low byte
    sendData((x >> 8) & 0xFF);  // X start high byte
    sendData((x + w - 1) & 0xFF);  // X end low byte
    sendData(((x + w - 1) >> 8) & 0xFF);  // X end high byte
    sendData(y & 0xFF);  // Y start low byte
    sendData((y >> 8) & 0xFF);  // Y start high byte
    sendData((y + h - 1) & 0xFF);  // Y end low byte
    sendData(((y + h - 1) >> 8) & 0xFF);  // Y end high byte
    sendData(0x01);  // Set gate scan direction

    // Send black/white buffer for partial area
    sendCommand(0x10);
    for (size_t i = 0; i < bufferSize; i++) {
        sendData(partialBuffer[i]);
        if ((i % 100) == 0) yield();
    }
    
    // In a tri-color display, you would send the red buffer here
    // We're using a blank red buffer for partial update
    sendCommand(0x13);
    for (size_t i = 0; i < bufferSize; i++) {
        sendData(0x00);  // No red for partial update
        if ((i % 100) == 0) yield();
    }
    
    // Update the display
    sendCommand(0x12);  // DISPLAY_REFRESH
    waitUntilIdle();
    
    // Exit partial mode
    sendCommand(0x92);  // PARTIAL_OUT
    
    // Free the temporary partial buffer
    free(partialBuffer);
}

uint8_t* MT_EPD::createPartialBuffer(uint16_t x, uint16_t y, uint16_t w, uint16_t h, size_t* bufferSize) {
    // Calculate buffer size for the region (bytes)
    // Round up width to nearest byte boundary (8 pixels)
    uint16_t widthBytes = (w + 7) / 8;
    *bufferSize = widthBytes * h;
    
    // Allocate a temporary buffer for the partial area
    uint8_t* partialBuffer = (uint8_t*)malloc(*bufferSize);
    if (!partialBuffer) {
        return NULL;
    }
    
    // Initialize the buffer to white (0xFF for black/white buffer)
    memset(partialBuffer, 0xFF, *bufferSize);
    
    // Copy data from the full buffer to the partial buffer
    for (uint16_t ry = 0; ry < h; ry++) {
        for (uint16_t rx = 0; rx < w; rx++) {
            // Get the actual pixel coordinates in the display
            uint16_t px = x + rx;
            uint16_t py = y + ry;
            
            // Get the bit value from the full buffer
            uint32_t fullBufIndex = (py * 100) + (px / 8);  // 100 = 800/8 bytes per row
            uint8_t bitPosition = 7 - (px % 8);
            
            if (fullBufIndex >= _buffer_size) continue;  // Safety check
            
            bool pixelIsBlack = !(_buffer_bw[fullBufIndex] & (1 << bitPosition));
            
            // Set the corresponding bit in the partial buffer
            uint32_t partialBufIndex = (ry * widthBytes) + (rx / 8);
            bitPosition = 7 - (rx % 8);
            
            if (pixelIsBlack) {
                // Set the bit to 0 for black
                partialBuffer[partialBufIndex] &= ~(1 << bitPosition);
            } else {
                // Set the bit to 1 for white (already initialized to 0xFF)
            }
        }
    }
    
    return partialBuffer;
}

void MT_EPD::displayPartial() {
    // Send black/white buffer
    sendCommand(0x10);
    for (uint32_t i = 0; i < _buffer_size; i++) {
        sendData(_buffer_bw[i]);
        if ((i % 100) == 0) yield();
    }
    
    // Send red buffer
    sendCommand(0x13);
    for (uint32_t i = 0; i < _buffer_size; i++) {
        sendData(_buffer_red[i]);
        if ((i % 100) == 0) yield();
    }
    
    // Display update
    sendCommand(0x12); // DISPLAY_REFRESH
    waitUntilIdle();
    
    // Reset partial mode - return to full update mode
    sendCommand(0x92); // PARTIAL_IN
    sendCommand(0x00); // PANEL_SETTING - normal mode
}