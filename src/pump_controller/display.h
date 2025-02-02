#ifndef DISPLAY_H
#define DISPLAY_H

#include <Adafruit_GFX.h> 
#include <Adafruit_ST7735.h> 
#include <Fonts/FreeSerif18pt7b.h>

class Display {
public:
    void initDisplay();
    void showOnDisplay();
};

#endif