// pump-controller/User_Setup.h
// TFT_eSPI configuration for ESP32-S3-WROOM-1 N16R8
// ST7735S 1.8" 128x160 display on dedicated SPI2/FSPI bus

// --- Display driver ---
#define ST7735_DRIVER
#define ST7735_BLACKTAB        // confirmed working in earlier test

// ESP32-S3: REG_SPI_BASE(i)=0 for i<2; valid only for i>=2.
// Default FSPI=0 gives SPI_USER_REG=0x10 → StoreProhibited crash on first write.
// USE_FSPI_PORT forces SPI_PORT=2 → REG_SPI_BASE(2)=0x60024000 (FSPI/SPI2).
#define USE_FSPI_PORT

// --- Display dimensions ---
#define TFT_WIDTH  128
#define TFT_HEIGHT 160

// --- SPI2 / FSPI pins (dedicated display bus) ---
#define TFT_MOSI   41
#define TFT_SCLK   40
#define TFT_CS     39
#define TFT_DC     38
#define TFT_RST    47
#define TFT_BL     21          // backlight

// --- SPI frequency ---
#define SPI_FREQUENCY      27000000   // 27MHz — safe for ST7735S
#define SPI_READ_FREQUENCY  5000000   // read speed (not used, display is write-only)

// --- PSRAM sprite support ---
// Sprites automatically use PSRAM on N16R8 — no extra config needed

// --- Fonts to include ---
#define LOAD_GLCD    // Font 1 — original Adafruit 8px font
#define LOAD_FONT2   // Font 2 — small (7px)
#define LOAD_FONT4   // Font 4 — medium (26px) — good for tank level %
#define LOAD_FONT6   // Font 6 — large (48px) numerics only
#define LOAD_FONT7   // Font 7 — 7-segment style (48px) numerics only
#define LOAD_GFXFF   // FreeFonts — enables custom font support
#define SMOOTH_FONT  // enables anti-aliased font rendering

#define USER_SETUP_LOADED