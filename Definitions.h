#ifndef DEFINITIONS_H
#define DEFINITIONS_H

//#define DEBUG
//#define TEST_BOARD
#define PROD_BOARD

#define MY_DEVICE_NAME    "KC868-A6 Kontroler integracyjny"
#define MY_WIFI_NAME      "SUPLA-KC868-A6"
#define MAX_SENSOR_AMOUNT 16

// Definicje I2C
#ifdef TEST_BOARD
  #define I2C_SDA 22 
  #define I2C_SCL 21
#endif

#ifdef PROD_BOARD
  #define I2C_SDA 4
  #define I2C_SCL 15
#endif

#define PCF_RELAYS_ADDR 0x20  // Ekspander wyjściowy (Przekaźniki)
#define PCF_INPUTS_ADDR 0x22  // Ekspander wejściowy (Wejścia cyfrowe)

#define ONE_WIRE_GPIO 33  // io nr 2
#define AIRVENT_HOODMODE_SENSOR 32  // io nr 1
#ifdef TEST_BOARD
   #define EXTRA_GPIO_1 2 // niebieska dioda
#endif
#ifdef PROD_BOARD
  #define EXTRA_GPIO_1 18 // spi clk
#endif
#define EXTRA_GPIO_2 19 // spi miso
#define EXTRA_GPIO_3 23 // spi mosi

/*
złącze nRF24L01 w KC668-A6 (orientacja od dołu przekaźniki, od góry wejścia):
GND                 -     3V
CE_S                -     SPI CS
SPI CLK (GPIO18)    -     SPI MOSI (GPIO23)
SPI MISO (GPIO19)   -     x 
*/

// przycisk sprzetowy wejscia w konfiguracje
#define BUTTON_CFG_RELAY_GPIO 0

// wybierz ekran
// ************************************************************
//#define USE_SH1106
#define USE_SSD1306
// ************************************************************
#define SCREEN_ADDRESS 0x3C
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1 // Brak pinu resetu w większości ekranów I2C
#define OLED_OFF 0xAE
#define OLED_ON 0xAF
#define DIM_TIME 60000
#define OFF_TIME 240000
#define EDIT_MODE_EXIT_TIME 10000
#define SENSOR_PAGE_TIME 5000
#define DISPLAY_INTERVAL 250


#endif // DEFINITIONS_H