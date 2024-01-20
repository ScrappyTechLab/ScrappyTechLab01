#include <stdio.h>
#include <pico/stdlib.h>
#include "pinconstants.h"

uint led_level = 0;
uint ms_without_toggle = 0;


/*
    to indicate that the board has power
*/
void blinkLed()
{
    gpio_init(BOARD_LED_PIN);
    gpio_set_dir(BOARD_LED_PIN, GPIO_OUT);
    gpio_put(BOARD_LED_PIN, 1);
    sleep_ms(100);
    gpio_put(BOARD_LED_PIN, 0);
    sleep_ms(100);
}


/*
    the board is not powered on continously, but instead is powered based on using a mosfet
    the first seeding power comes based on some sort of a push button
*/
void turnOnBoard()
{
    gpio_init(BOARD_POWER_MOSFET_SIGNAL_PIN);
    gpio_set_dir(BOARD_POWER_MOSFET_SIGNAL_PIN, GPIO_OUT);
    gpio_put(BOARD_POWER_MOSFET_SIGNAL_PIN, 1);
}

void toggledLEDs(uint gpio, uint32_t events)
{
    ms_without_toggle = 0;
    printf("GPIO %d %d\n", gpio, events);
    gpio_init(LED_1_PIN);
    gpio_set_dir(LED_1_PIN, GPIO_OUT);
    gpio_put(LED_1_PIN, led_level);
    gpio_init(LED_2_PIN);
    gpio_set_dir(LED_2_PIN, GPIO_OUT);
    gpio_put(LED_2_PIN, !led_level);
    led_level = !led_level;

}


/*
    since the board is powered by a signal to the mosfet, we could turn off as well
*/
void turnOffBoard()
{
    gpio_init(BOARD_POWER_MOSFET_SIGNAL_PIN);
    gpio_set_dir(BOARD_POWER_MOSFET_SIGNAL_PIN, GPIO_OUT);
    gpio_put(BOARD_POWER_MOSFET_SIGNAL_PIN, 0);
}


int main()
{
    stdio_init_all();
    blinkLed();
    turnOnBoard();
    blinkLed();

    gpio_init(BUTTON_PIN);
    gpio_set_dir(BUTTON_PIN, GPIO_IN);
    gpio_pull_up(BUTTON_PIN);
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &toggledLEDs);

    while(1)
    {
        ms_without_toggle += 100;
        blinkLed();
        if (ms_without_toggle > 1000)
        {
            turnOffBoard();
        }
    }
 
}
