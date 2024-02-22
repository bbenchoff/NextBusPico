/**
 *  @filename   :   epdif.cpp
 *  @brief      :   Implements EPD interface functions
 *                  Users have to implement all the functions in epdif.cpp
 *  @author     :   Yehui from Waveshare
 *
 *  Copyright (C) Waveshare     August 10 2017
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documnetation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to  whom the Software is
 * furished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "epdif.h"
#include <SPI.h>

// Define the SPI1 pins for the Pi Pico
#define SPI_MISO1 4
#define SPI_MOSI1 11
#define SPI_SCK1 10

// Create our own Mbed SPI1 object to access the SPI1 block
// Note, the 'NC' is because the MISO pin is not connected to the epaper
// display, and I could not find a 'benign' pin to assign it to, as both
// possible 'SPI1 RX' pins (GP8 and GP12) are already used by the e-paper
// board for other things:
//  - GP8 is DC
//  - GP12 is RST
//SPIClass SPI1(SPI_MOSI1, SPI_MISO1, SPI_SCK1);


EpdIf::EpdIf() {
};

EpdIf::~EpdIf() {
};

void EpdIf::DigitalWrite(int pin, int value) {
    digitalWrite(pin, value);
}

int EpdIf::DigitalRead(int pin) {
    return digitalRead(pin);
}

void EpdIf::DelayMs(unsigned int delaytime) {
    delay(delaytime);
}

void EpdIf::SpiTransfer(unsigned char data) {
    digitalWrite(EPAPER_CS_PIN, LOW);
    SPI1.transfer(data);
    digitalWrite(EPAPER_CS_PIN, HIGH);
}

int EpdIf::IfInit(void) {
    pinMode(EPAPER_CS_PIN, OUTPUT);
    pinMode(EPD_RST_PIN, OUTPUT);
    pinMode(EPD_DC_PIN, OUTPUT);
    //pinMode(EPD_BUSY_PIN, INPUT);  //Ethernet uses the busy pin
    SPI1.setTX(11);
    SPI1.setCS(EPAPER_CS_PIN);
    SPI1.setSCK(10);
    SPI1.begin();
    SPI1.beginTransaction(SPISettings(3600000, MSBFIRST, SPI_MODE0));
    
    return 0;
}
