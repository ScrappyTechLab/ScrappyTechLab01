/**
 * program that runs on the pico, to consume the colour information read via serial, and control the LED light strip
 */
#include <stdio.h>
#include <cmath>
#include "pico/stdlib.h"
#include "WS2812.hpp"   // https://github.com/ForsakenNGS/Pico_WS2812

#define LED_PIN 15
#define LED_LENGTH 60
#define MAX_LED_LENGTH LED_LENGTH
#define LED_STRIP_SUB_PIXEL_COUNT 3
#define LED_TOTAL_SUB_PIXEL_COUNT MAX_LED_LENGTH * LED_STRIP_SUB_PIXEL_COUNT
#define BOARD_LED_PIN 25

bool boardLedStatus = false;
uint8_t colour[LED_LENGTH][LED_STRIP_SUB_PIXEL_COUNT] = {0};   // dimensions: [number of pixels in the strip][colour {R,G,B}]

void toggleBoardLed()
{
    gpio_init(BOARD_LED_PIN);
    gpio_set_dir(BOARD_LED_PIN, GPIO_OUT);
    boardLedStatus = !boardLedStatus;
    gpio_put(BOARD_LED_PIN, boardLedStatus);
}

int readline(char *buffer, int size) {
  int index = 0;
  while (index < size) {
    char c;
    scanf("%c", &c);
    if (c == '\n' || c == '\r') {
      break;
    }
    buffer[index++] = c;
  }
  buffer[index] = '\0';
  return index;
}


int getNumber()
{
    char buffer[4] = {0};
    readline(buffer, LED_STRIP_SUB_PIXEL_COUNT);
    int number;
    sscanf(buffer, "%d", &number);

    /*
        the companion app can wait to output the next value until the same value is printed by the pico. this helps to synchornize
        1.pc passes a single subpixel colour information via serial
        2. pico will read the number
        3. pico will print back to serial the same value it received
        4. pc will verify that the number sent across is what is received
     */
    // sleep_ms(1000);  // uncomment to verify the above
    // printf("%03d\n", number);
    return number;
}

void getData()
{
    toggleBoardLed();
    for (uint8_t i = 0; i < LED_LENGTH; ++i)
    {
        for(uint8_t colourType = 0; colourType < LED_STRIP_SUB_PIXEL_COUNT; ++colourType)
        {
            colour[i][colourType] = getNumber();
        }
    }
    toggleBoardLed();
}


void getDataInBulkAndFill(WS2812 ledStrip)
{
    getData();
    toggleBoardLed();
    for (uint8_t i = 0; i < LED_LENGTH; ++i)
    {
        ledStrip.fill( WS2812::RGB(colour[i][0], colour[i][1], colour[i][2]), i, 1);
    }
    ledStrip.show();
    toggleBoardLed();
}


/*
 * retrieves the input, and as info is available for a pixel (RGB), then we fill it in
 */
void getAndFill(WS2812 ledStrip)
{
    while(1)
    {
        for (int i = 0; i < LED_LENGTH; ++i)
        {
            ledStrip.fill(WS2812::RGB(getNumber(), getNumber(), getNumber()), i, 1);
        }
        ledStrip.show();

    }



}


int main()
{
    stdio_init_all();

    // 0. Initialize LED strip
    printf("0. Initialize LED strip");
    WS2812 ledStrip(
        LED_PIN,            // Data line is connected to pin 0. (GP0)
        LED_LENGTH,         // Strip is 6 LEDs long.
        pio0,               // Use PIO 0 for creating the state machine.
        0,                  // Index of the state machine that will be created for controlling the LED strip
                            // You can have 4 state machines per PIO-Block up to 8 overall.
                            // See Chapter 3 in: https://datasheets.raspberrypi.org/rp2040/rp2040-datasheet.pdf
        WS2812::FORMAT_GRB  // Pixel format used by the LED strip
    );

    

    // clear out the LED strip
    for (uint8_t i = 0; i < MAX_LED_LENGTH; ++i)
        ledStrip.fill( WS2812::RGB(0, 0, 0), i, 1);
    ledStrip.show();

    while(1)
    {
        getAndFill(ledStrip);
    }

    return 0;
}
