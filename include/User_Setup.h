// TFT_eSPI User Setup for Waveshare ESP32-S3-Touch-LCD-1.28
// Display: GC9A01A, 240x240 round IPS, SPI interface
// Board:   ESP32-S3R8, 16MB flash, PSRAM
//
// Pin mapping matches DEV_Config.h from Waveshare sample code:
//   LCD_DC    -> GPIO 8
//   LCD_CS    -> GPIO 9
//   LCD_CLK   -> GPIO 10
//   LCD_MOSI  -> GPIO 11
//   LCD_MISO  -> GPIO 12  (not used by display, -1 below)
//   LCD_RST   -> GPIO 14
//   LCD_BL    -> GPIO 2   (backlight, driven separately in DEV_Config)

#define GC9A01_DRIVER

// ESP32-S3 + Arduino 3.x: FSPI=0 causes NULL SPI register access in TFT_eSPI.
// USE_HSPI_PORT forces SPI3; custom pins still work via spi.begin().
#define USE_HSPI_PORT

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

#define TFT_MISO -1   // not connected
#define TFT_MOSI 11
#define TFT_SCLK 10
#define TFT_CS    9
#define TFT_DC    8
#define TFT_RST  14

#define TFT_BL 2
#define TFT_BACKLIGHT_ON HIGH

// Fonts to load (comment out to save flash)
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// SPI speed — 80 MHz halves transfer time (115 KB → ~11.5 ms) to reduce screen tearing.
// The GC9A01A supports up to 100 MHz; if signal integrity issues arise, try 60000000 first.
#define SPI_FREQUENCY       80000000
#define SPI_READ_FREQUENCY  20000000

// TE (Tearing Effect) pin — full anti-tear fix.
// The GC9A01A has a TE output and the Waveshare firmware enables it (0x35 in LCD_1in28.cpp).
// However the TE pin does not appear to be routed to any GPIO on this board variant;
// DEV_Config.h defines no pin for it, and the schematic shows no connection.
// If a future board revision exposes TE (e.g. GPIO 40), uncomment the line below.
// TFT_eSPI will then automatically sync each pushImage call to the display's VBL.
// #define TFT_TE 40
