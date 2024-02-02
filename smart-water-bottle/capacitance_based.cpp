/**
    ADC will be reading the values across two electrodes that are mounted on either side of the bottle
    the ADC reading will decrease when the water level increases
 */
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include <limits.h>

#define NUMBER_OF_SAMPLES_ADC 10000                                     // number of ADC samples taken back to back. then we take the average of all the values. This is done to get a more accurate representation of the value
#define ADC_INDEX 0                                                     // ADC index
#define NUMBER_OF_ITERATIONS_OF_ADC_READINGS 6                          // number of readings that are taken via the ADC. this is done to prevent taking a reading while the bottle is being used and in a tilted angle resulting in incorrect readings
#define DELAY_BETWEEN_ADC_READING_ITERATIONS_MS 10000                   // delay between iterations of ADC readings
#define ALLOWED_DELTA_BETWEEN_ADC_READING_ITERATIONS 0.5f               // when we are taking readings, we might not get the exact value. so we need to allow for some variance
#define MIN_VALUE_EXPECTED_TO_DROP_BETWEEN_READINGS 0.5f
#define BOARD_LED_PIN 25
#define BUTTON_PIN 18
#define LED_TOGGLE_DELAY_MS 1000
#define LOW_LEVEL_BOTTLE_LED_PIN 16
#define HIGH_LEVEL_BOTTLE_LED_PIN 17
#define MIN_FLOAT_VALUE -1.0f
#define MAX_FLOAT_VALUE 99999.0f
#define BUTTON_DEBOUNCE_DELAY_MS 200
#define MAX_ALLOWED_ITERATIONS_WITHOUT_GOOD_READING 5    // even if we do not get a proper reading for a while, we alarm. this is to ensure that shaking the bottle once in a while to prevent a proper reading does not end up being a hack to prevent alarm
#define BOARD_SLEEP_MS 60000 //1800000                                          // this is the delay with which we would try to evaluate the levels
#define BUZZER_PIN 14
#define NUMBER_OF_BEEPS_FOR_ALERT 10

float emptyBottleReading, fullBottleReading, lastReading = MAX_FLOAT_VALUE;
bool isIntializing = false;
uint32_t lastButtonPressedTimestamp = 0;
uint8_t numberOfAttemptsWithoutGoodReading = 0;


// timer objects for toggling the led without blocking the primary thread
struct repeating_timer ledToggleTimer;

// to track the status of output GPIO pins
bool gpioStatus[40] = {false};

enum GPIOStatus
{
    OFF = 0, ON = 1, TOGGLE = 2
};

/**
 * toggles output GPIO based on the args.
 * returns the final level of the GPIO
 */
bool toggleOutputGPIOStatus(uint8_t pin, GPIOStatus status = TOGGLE)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    if (status == TOGGLE)
    {
        gpioStatus[pin] = !gpioStatus[pin];
    }
    else
    {
        gpioStatus[pin] = GPIOStatus(status);
    }
    gpio_put(pin, gpioStatus[pin]);
    return gpioStatus[pin];
}


bool toggleLowLevelLED(struct repeating_timer *t)
{
    toggleOutputGPIOStatus(LOW_LEVEL_BOTTLE_LED_PIN);
    return true;
}

bool toggleHighLevelLED(struct repeating_timer *t)
{
    toggleOutputGPIOStatus(HIGH_LEVEL_BOTTLE_LED_PIN);
    return true;
}

float getADCReadings()
{
    adc_select_input(ADC_INDEX);
    uint32_t adcSum = 0;
    for (uint32_t i = 0; i < NUMBER_OF_SAMPLES_ADC; ++i)
    {
        adcSum += adc_read();
    }
    return ((float) adcSum / (float) NUMBER_OF_SAMPLES_ADC);
}

/**
 * to be called upon button press
 * this is the initialization workflow, where the lowest and highest levels of the bottle are calibrated
 */
void buttonPress(uint gpio, uint32_t events)
{
    printf("interrupt registered\n");
    uint32_t msSinceBoot = to_ms_since_boot(get_absolute_time());
    if (msSinceBoot - lastButtonPressedTimestamp <= BUTTON_DEBOUNCE_DELAY_MS)
    {
        // button debounce
        printf("interrupt being ignored\n");
        return;
    }
    else
    {
        lastButtonPressedTimestamp = msSinceBoot;
    }

    if (!isIntializing)
    {
        printf("initialization started\n");
        // start to initialize the levels
        toggleOutputGPIOStatus(LOW_LEVEL_BOTTLE_LED_PIN, OFF);
        toggleOutputGPIOStatus(HIGH_LEVEL_BOTTLE_LED_PIN, OFF);
        emptyBottleReading = fullBottleReading = MIN_FLOAT_VALUE;

        // start blinking of LOW level LED
        add_repeating_timer_ms(LED_TOGGLE_DELAY_MS, &toggleLowLevelLED, NULL, &ledToggleTimer);

        isIntializing = true;
        return;
    }

    // we are already in the initializing mode
    if (emptyBottleReading == MIN_FLOAT_VALUE)
    {
        emptyBottleReading = getADCReadings();
        printf("emptyBottleReading fixed: %f\n", emptyBottleReading);

        // cancel blinking of LOW level LED
        cancel_repeating_timer(&ledToggleTimer);
        // turn off LOW level LED incase the last level was high
        toggleOutputGPIOStatus(LOW_LEVEL_BOTTLE_LED_PIN, OFF);

        // add blinking of HIGH level LED
        add_repeating_timer_ms(LED_TOGGLE_DELAY_MS, &toggleHighLevelLED, NULL, &ledToggleTimer);
    }
    else if (fullBottleReading == MIN_FLOAT_VALUE)
    {
        fullBottleReading = getADCReadings();
        printf("fullBottleReading: %f\n", fullBottleReading);

        // we are done with initializing
        cancel_repeating_timer(&ledToggleTimer);
        // turn of both the level LEDs
        toggleOutputGPIOStatus(LOW_LEVEL_BOTTLE_LED_PIN, OFF);
        toggleOutputGPIOStatus(HIGH_LEVEL_BOTTLE_LED_PIN, OFF);

        isIntializing = false;
    }

}


void audioAlert()
{
    printf("alerting\n");
    for (uint8_t i = 0; i < NUMBER_OF_BEEPS_FOR_ALERT; ++i)
    {
        toggleOutputGPIOStatus(LOW_LEVEL_BOTTLE_LED_PIN, ON);
        toggleOutputGPIOStatus(HIGH_LEVEL_BOTTLE_LED_PIN, ON);
        toggleOutputGPIOStatus(BUZZER_PIN, ON);
        sleep_ms(1000);
        toggleOutputGPIOStatus(BUZZER_PIN, OFF);
        toggleOutputGPIOStatus(LOW_LEVEL_BOTTLE_LED_PIN, OFF);
        toggleOutputGPIOStatus(HIGH_LEVEL_BOTTLE_LED_PIN, OFF);
        sleep_ms(1000);
    }
}


/**
 * retrieve ADC reading multiple times with a delay
 * check if the values are within some allowed delta, to ensure that the bottle is not being moved or tilted, which might skew the reading
 * if readings are consistent, then check the difference between the last reading and the current one, to evaluate if there has been a drop in the level
 * returns true if there was successful processing
 */
bool getAndProcessReading()
{
    float min = MAX_FLOAT_VALUE;
    float max = MIN_FLOAT_VALUE;
    float sum = 0.0f;

    // we take ADC readings with a delay between each iteration. This is to ensure that we do not catch the reading during bottle consumption and tilt
    for (uint8_t i = 0; i < NUMBER_OF_ITERATIONS_OF_ADC_READINGS; ++i)
    {
        float reading = getADCReadings();
        sum += reading;
        printf("reading: %f\n", reading);
        if (reading > max)
        {
            max = reading;
        }
        if (reading < min)
        {
            min = reading;
        }
        sleep_ms(DELAY_BETWEEN_ADC_READING_ITERATIONS_MS);
    }

    float average = (sum == 0.0f) ? 0.0f : (sum / NUMBER_OF_ITERATIONS_OF_ADC_READINGS);
    printf("min: %f\tmax: %f\tavg: %f\n", min, max, average);
    if (max - min > ALLOWED_DELTA_BETWEEN_ADC_READING_ITERATIONS)
    {
        printf("difference greater than allowed threshold %f . unable to conclude on the current level\n", ALLOWED_DELTA_BETWEEN_ADC_READING_ITERATIONS);
        ++numberOfAttemptsWithoutGoodReading;
        if (numberOfAttemptsWithoutGoodReading > MAX_ALLOWED_ITERATIONS_WITHOUT_GOOD_READING)
        {
            // alert
            audioAlert();
        }
        return false;
    }
    else
    {
        printf("considering current level at: %f\n", average);
        numberOfAttemptsWithoutGoodReading = 0;
        uint32_t currentSuccessfulReadingTimestamp = to_ms_since_boot(get_absolute_time());

        if (average < lastReading - MIN_VALUE_EXPECTED_TO_DROP_BETWEEN_READINGS)
        {
            printf("water level has increased from %f to %f. updating current level\n", lastReading, average);
            lastReading = average;

        }
        else if (average > lastReading + ALLOWED_DELTA_BETWEEN_ADC_READING_ITERATIONS)
        {
            printf("water level has dropped from %f to %f\n", lastReading, average);
        }
        else
        {
            // water level has remained pretty similar
            // alert
            audioAlert();
        }
        return true;
    }

}


int main()
{
    stdio_init_all();
    adc_init();
    gpio_set_irq_enabled_with_callback(BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &buttonPress);

    // trigger initialization when the board boots up, but delay a few ms after boot to prevent being ignored as part of the button debounce logic
    sleep_ms(1000);
    buttonPress(BUTTON_PIN, 0);

    // to prevent going into reading mode until the initialization is successful
    while (isIntializing)
    {
        sleep_ms(1000);
    }

    while (1)
    {
        // printf("%f\n", getADCReadings());
        printf("board sleeping\n");
        sleep_ms(BOARD_SLEEP_MS);
        toggleOutputGPIOStatus(BOARD_LED_PIN, ON);
        bool result = false;
        while(!result)
        {
            // try till we get a good reading
            result = getAndProcessReading();
        }
        toggleOutputGPIOStatus(BOARD_LED_PIN, OFF);
    }
}
