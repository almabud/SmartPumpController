# Create and Use Custom Font Awesome / TTF Fonts with TFT_eSPI

This guide explains how to convert a custom `.ttf` font, such as Font Awesome, into a TFT_eSPI smooth font and use it in a PlatformIO ESP32 / ESP32-S3 project.

The example uses:

* PlatformIO
* ESP32 / ESP32-S3
* TFT_eSPI
* Font Awesome 7
* Processing IDE
* Generated `.h` font file

---

## 1. Install Processing IDE

TFT_eSPI provides a Processing sketch that can convert TrueType fonts into TFT_eSPI smooth fonts.

### macOS using Homebrew

```bash
brew install --cask processing
```

Then open it:

```bash
open -a Processing
```

### Manual installation

Download Processing IDE from:

```text
https://processing.org/download/
```

Then install it normally.

On macOS:

```text
Move Processing.app to Applications
```

---

## 2. Open TFT_eSPI Font Generator

Inside the TFT_eSPI library, find this file:

```text
TFT_eSPI/Tools/Create_Smooth_Font/Create_font/Create_font.pde
```

Open this `.pde` file with **Processing IDE**.

Important: make sure Processing is in **Java mode**, not C++ mode.

If you see errors like:

```text
GL/glew.h file not found
```

then you are probably using C++ mode. Change it to:

```text
Java Mode
```

---

## 3. Add Your `.ttf` Font File

Open the sketch folder from Processing:

```text
Sketch → Show Sketch Folder
```

Create a folder named:

```text
data
```

Put your `.ttf` file inside it.

Example:

```text
Create_font/
├── Create_font.pde
└── data/
    └── Font Awesome 7 Free-Solid-900.ttf
```

---

## 4. Configure the Font Name and Size

Inside `Create_font.pde`, find the font configuration section.

For Font Awesome 7 Free Solid, use:

```java
String fontName = "Font Awesome 7 Free-Solid-900";
String fontType = ".ttf";
int fontSize = 14;
```

Do not include `.ttf` in `fontName` if `fontType` is already `.ttf`.

Wrong:

```java
String fontName = "Font Awesome 7 Free-Solid-900.ttf";
String fontType = ".ttf";
```

Correct:

```java
String fontName = "Font Awesome 7 Free-Solid-900";
String fontType = ".ttf";
```

---

## 5. Select the Unicode Icons

**Read this before you regenerate anything: the font already in the project
almost certainly has the icon you want.** `include/FontAwesomesolid9006.h`
carries **348 unique glyphs covering the whole U+F013–U+F1EB range** — gear,
wifi, cloud, droplet, bolt, ban, lock, plug, link, and everything else Font
Awesome 7 Solid defines in that span. Regeneration is only needed for a
codepoint *outside* U+F013–U+F1EB (the F2xx/F3xx/F5xx+ ranges).

> **`unicodeBlocks` is a list of (start, end) PAIRS, not a list of icons.**
> This is the single most misleading thing about the Processing sketch. Two
> entries mean one range; four entries mean two ranges. It is why this project's
> font is 80 KB and 348 glyphs rather than the handful its list looks like it
> asks for.

So this list:

```java
static final int[] unicodeBlocks = {
  0xF013, 0xF1EB,   // range 1: gear .. wifi — everything in between comes too
  0xF015, 0xF0C2    // range 2: house .. cloud — a subset of range 1
};
```

means *"every glyph from U+F013 to U+F1EB, plus every glyph from U+F015 to
U+F0C2 again"* — which is exactly what the shipped font contains, duplicates and
all (492 table entries for 348 unique codepoints).

To pick out **individual** icons, give each one as its own single-codepoint
range:

```java
static final int[] unicodeBlocks = {
  0xF013, 0xF013,   // gear
  0xF1EB, 0xF1EB,   // wifi
  0xF015, 0xF015,   // home / house
  0xF0C2, 0xF0C2    // cloud
};
```

Common Font Awesome Solid codepoints:

| Icon         | Unicode | Icon              | Unicode |
| ------------ | ------- | ----------------- | ------- |
| Gear         | `f013`  | Ban / no-entry    | `f05e`  |
| WiFi         | `f1eb`  | Bolt              | `f0e7`  |
| Home / House | `f015`  | Droplet / tint    | `f043`  |
| Cloud        | `f0c2`  | Lock              | `f023`  |

Absent from Font Awesome 7 Solid's U+F013–U+F1EB range, so no amount of
regeneration will find them there: thermometer, faucet, power-off, plain check.

If your sketch already has a Unicode list, replace the existing list instead of creating a duplicate.

**Trimming is the bigger win.** 80 KB of PROGMEM for 348 glyphs when six are
actually drawn. If flash ever gets tight, narrow the ranges rather than adding
to them.

---

## 6. Generate the Font

Click the **Run** button in Processing.

The sketch should generate font files in a folder like:

```text
FontFiles/
```

Depending on the TFT_eSPI version, it may generate:

```text
FontAwesome14.vlw
```

and also:

```text
FontAwesome14.h
```

For PlatformIO, using the `.h` file is very convenient because you do not need SPIFFS or LittleFS.

---

## 7. Add the Generated `.h` File to PlatformIO

Put the generated `.h` file inside your PlatformIO `include/` folder.

Example:

```text
your_project/
├── include/
│   └── FontAwesome14.h
├── src/
│   └── main.cpp
└── platformio.ini
```

---

## 8. Enable Smooth Font in TFT_eSPI

In your TFT_eSPI setup file, make sure this is enabled:

```cpp
#define SMOOTH_FONT
```

Without this, `loadFont()` for generated smooth fonts will not work.

---

## 9. Use the Font in Code

Open your generated `.h` file and check the actual array name.

It may look like this:

```cpp
const uint8_t FontAwesome14[] PROGMEM = {
  ...
};
```

The array name is what you pass to `tft.loadFont()`.

Example:

```cpp
#include <TFT_eSPI.h>
#include "FontAwesome14.h"

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.loadFont(FontAwesome14);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.drawString("\uf013", 10, 10);   // gear
  tft.drawString("\uf1eb", 40, 10);   // wifi
  tft.drawString("\uf015", 70, 10);   // home / house
  tft.drawString("\uf0c2", 100, 10);  // cloud

  tft.unloadFont();
}

void loop() {
}
```

---

## 10. Position Icons Properly

The `drawString()` function uses `x` and `y` position:

```cpp
tft.drawString("\uf1eb", 40, 10);
```

For a 160×128 display:

```text
x range: 0 to 159
y range: 0 to 127
```

You can also use `setTextDatum()` for alignment.

### Top-left alignment

```cpp
tft.setTextDatum(TL_DATUM);
tft.drawString("\uf015", 5, 5);
```

### Center alignment

```cpp
tft.setTextDatum(MC_DATUM);
tft.drawString("\uf0c2", 80, 64);
```

### Top-right alignment

```cpp
tft.setTextDatum(TR_DATUM);
tft.drawString("\uf1eb", 155, 5);
```

Example layout:

```cpp
tft.loadFont(FontAwesome14);
tft.setTextColor(TFT_WHITE, TFT_BLACK);

// Top-left home
tft.setTextDatum(TL_DATUM);
tft.drawString("\uf015", 5, 5);

// Top-right wifi
tft.setTextDatum(TR_DATUM);
tft.drawString("\uf1eb", 155, 5);

// Center cloud
tft.setTextDatum(MC_DATUM);
tft.drawString("\uf0c2", 80, 64);

// Bottom-left gear
tft.setTextDatum(BL_DATUM);
tft.drawString("\uf013", 5, 123);

tft.unloadFont();
```

---

## 11. Font Size Notes

Smooth fonts generated by TFT_eSPI are fixed-size.

This means this does not work like normal scalable fonts:

```cpp
tft.setTextSize(2);
```

For smooth fonts, the size is decided when generating the font in Processing:

```java
int fontSize = 14;
```

If the icon is too large or too small, generate another font size.

Recommended sizes for a 160×128 display:

```text
FontAwesome12.h
FontAwesome14.h
FontAwesome16.h
FontAwesome20.h
```

For text height around 14px, start with:

```java
int fontSize = 14;
```

If the icon looks smaller than the text, try:

```java
int fontSize = 16;
```

---

## 12. Using Multiple Font Sizes

You can generate multiple `.h` files:

```text
include/
├── FontAwesome14.h
├── FontAwesome16.h
└── FontAwesome20.h
```

Then use them like this:

```cpp
#include "FontAwesome14.h"
#include "FontAwesome20.h"

void drawSmallWifi() {
  tft.loadFont(FontAwesome14);
  tft.drawString("\uf1eb", 10, 10);
  tft.unloadFont();
}

void drawLargeWifi() {
  tft.loadFont(FontAwesome20);
  tft.drawString("\uf1eb", 10, 40);
  tft.unloadFont();
}
```

---

## 13. Mixing Normal Text and Icons

Example:

```cpp
#include <TFT_eSPI.h>
#include "FontAwesome14.h"

TFT_eSPI tft = TFT_eSPI();

void setup() {
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  // Normal TFT_eSPI text
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("WiFi", 10, 10);

  // Font Awesome icon
  tft.loadFont(FontAwesome14);
  tft.drawString("\uf1eb", 50, 10);
  tft.unloadFont();
}

void loop() {
}
```

For better vertical alignment, use middle-left datum:

```cpp
tft.setTextDatum(ML_DATUM);

tft.setTextFont(2);
tft.setTextSize(1);
tft.drawString("WiFi", 10, 20);

tft.loadFont(FontAwesome14);
tft.drawString("\uf1eb", 50, 20);
tft.unloadFont();

tft.setTextDatum(TL_DATUM);
```

---

## 14. Common Problems

### Problem: Icon shows as a box

Possible reasons:

* Wrong Font Awesome `.ttf` file
* Unicode was not included during generation
* Wrong icon Unicode
* The icon is not available in that Font Awesome style

Use the correct Font Awesome file, for example:

```text
Font Awesome 7 Free-Solid-900.ttf
```

For solid icons, do not use Brands or Regular font files.

---

### Problem: Font is too big

Generate a smaller size in Processing:

```java
int fontSize = 12;
```

or:

```java
int fontSize = 14;
```

---

### Problem: `loadFont()` does not compile

Check that your TFT_eSPI setup has:

```cpp
#define SMOOTH_FONT
```

Also check the actual array name inside the generated `.h` file.

If the array name is:

```cpp
const uint8_t FontAwesome_14[] PROGMEM = {
```

then use:

```cpp
tft.loadFont(FontAwesome_14);
```

not:

```cpp
tft.loadFont(FontAwesome14);
```

---

### Problem: Processing says `GL/glew.h` missing

You are probably using Processing C++ mode.

Switch to:

```text
Java Mode
```

The TFT_eSPI `Create_font.pde` sketch should be run in Java mode.

---

### Problem: Processing says `Syntax Error - Missing ';'?`

Usually the actual mistake is just above the highlighted line.

Check for:

* Missing `;`
* Missing `}`
* Missing `{`
* Unicode list not closed with `};`
* Code placed outside a function

Example correct Unicode list:

```java
static final int[] unicodeBlocks = {
  0xF013,
  0xF1EB,
  0xF015,
  0xF0C2
};
```

---

## 15. Complete Example for PlatformIO

```cpp
#include <TFT_eSPI.h>
#include "FontAwesome14.h"

TFT_eSPI tft = TFT_eSPI();

#define ICON_GEAR  "\uf013"
#define ICON_WIFI  "\uf1eb"
#define ICON_HOME  "\uf015"
#define ICON_CLOUD "\uf0c2"

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.loadFont(FontAwesome14);

  tft.setTextDatum(TL_DATUM);
  tft.drawString(ICON_HOME, 5, 5);

  tft.setTextDatum(TR_DATUM);
  tft.drawString(ICON_WIFI, 155, 5);

  tft.setTextDatum(MC_DATUM);
  tft.drawString(ICON_CLOUD, 80, 64);

  tft.setTextDatum(BL_DATUM);
  tft.drawString(ICON_GEAR, 5, 123);

  tft.unloadFont();

  tft.setTextDatum(TL_DATUM);
}

void loop() {
}
```

---

## Summary

The full flow is:

```text
Download/install Processing IDE
↓
Open TFT_eSPI Create_font.pde in Java mode
↓
Put custom .ttf file in the Processing sketch data/ folder
↓
Select font name, font size, and Unicode characters
↓
Run Processing sketch
↓
Copy generated .h file to PlatformIO include/
↓
Enable SMOOTH_FONT in TFT_eSPI setup
↓
Use tft.loadFont(FontAwesome14)
↓
Draw icons using Unicode strings like "\uf1eb"
```
