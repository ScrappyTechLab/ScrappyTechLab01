#include <stdio.h>
#include <pico/stdlib.h>
#include "pinconstants.h"
#include "hardware/adc.h"

#define REFERENCE_ADC_INDEX 0
#define PAYLOAD_ADC_INDEX 1
#define NUMBER_OF_SAMPLES_ADC 100000
#define PAYLOAD_VS_REFERENCE_OFFSET 37                      // standard offset when both the sensors are at ambient
#define PAYLOAD_VS_REFERENCE_THRESHOLD 3                    // threshold on top of the offset to allow for the values to float
#define PAYLOAD_TEMP_DIFF_ALERT_THRESHOLD 3                 // threshold temperature drop / increase for hot / cold respectively, that we need to alert on
#define PAYLOAD_IS_OFFSET_VALUE_GREATER_THAN_REFERENCE true  // to optimize, we are pre determining which sensor is greater than the other one, for the offset value
#define TEMPERATURE_DIFF_AFTER_WHICH_WE_ALERT_TEMPERATURE_SHIFT 0.5f // number of times we evaluate the current temperature to start shifting away from the base, after which we alert
#define RED_LED_PIN 16
#define BLUE_LED_PIN 17
#define GREEN_LED_PIN 18
#define BOARD_SLEEP_MS_BETWEEN_CYCLES 1000

uint led_level = 0;
uint ms_without_toggle = 0;
bool pinStatus[30] = {false};

struct ADCReadings
{
    float referenceValue;
    float payloadValue;
};


enum PayloadType { AMBIENT = 0, HOT = 1, COLD = 2 };

/*
    setup the required GPIO pins
*/
void pinsSetup()
{
    gpio_init(BOARD_LED_PIN);
    gpio_set_dir(BOARD_LED_PIN, GPIO_OUT);
    gpio_init(BLUE_LED_PIN);
    gpio_init(RED_LED_PIN);
    gpio_init(GREEN_LED_PIN);
    gpio_set_dir(BLUE_LED_PIN, GPIO_OUT);
    gpio_set_dir(RED_LED_PIN, GPIO_OUT);
    gpio_set_dir(GREEN_LED_PIN, GPIO_OUT);
    gpio_put(BLUE_LED_PIN, 0);
    gpio_put(RED_LED_PIN, 0);
    gpio_put(GREEN_LED_PIN, 0);
}


void toggleLED(uint8_t pinNum)
{
    pinStatus[pinNum] = !pinStatus[pinNum];
    gpio_put(pinNum, pinStatus[pinNum]);
}

struct ADCReadings getADCReadings()
{
    uint32_t referenceSum = 0;
    uint32_t payloadSum = 0;
    for (uint32_t i = 0; i < NUMBER_OF_SAMPLES_ADC; ++i)
    {
        adc_select_input(REFERENCE_ADC_INDEX);
        referenceSum += adc_read();

        adc_select_input(PAYLOAD_ADC_INDEX);
        payloadSum += adc_read();
    }
    struct ADCReadings readings = { ( (float) referenceSum / (float) NUMBER_OF_SAMPLES_ADC), ( (float) payloadSum / (float) NUMBER_OF_SAMPLES_ADC)};
    return readings;
}


float calcuateTemperatureDifference(struct ADCReadings readings)
{
    float referenceValue = readings.referenceValue;
    float payloadValue = readings.payloadValue;
    // first bring both the values to the same range by using the offset value
    if (PAYLOAD_IS_OFFSET_VALUE_GREATER_THAN_REFERENCE)
    {
        payloadValue -= PAYLOAD_VS_REFERENCE_OFFSET;
    }
    else
    {
        referenceValue -= PAYLOAD_VS_REFERENCE_OFFSET;
    }
    return payloadValue - referenceValue;
}


bool isTemperatureDifferenceWithinThreshold(float temperatureDifference)
{
    float absTemperatureDifference = temperatureDifference;
    if (temperatureDifference < 0)
    {
        absTemperatureDifference *= -1;
    }
    return absTemperatureDifference <= PAYLOAD_VS_REFERENCE_THRESHOLD;
}

enum PayloadType getPayloadType(float temperatureDifference)
{
    if (isTemperatureDifferenceWithinThreshold(temperatureDifference))
        return 0;
    if (temperatureDifference > 0)
        return 1;
    else if (temperatureDifference < 0)
        return 2;
}

/*
    sets the value for the LEDs based on the PayloadType and state
*/
void setIndicatorLED(enum PayloadType payloadType)
{
    if (payloadType == HOT)
    {
        printf("Payload identified: HOT\n");
        gpio_put(GREEN_LED_PIN, 0);
        gpio_put(BLUE_LED_PIN, 0);
        gpio_put(RED_LED_PIN, 1);
    }
    else if (payloadType == COLD)
    {
        printf("Payload identified: COLD\n");
        gpio_put(RED_LED_PIN, 0);
        gpio_put(GREEN_LED_PIN, 0);
        gpio_put(BLUE_LED_PIN, 1);
    }
    else
    {
        printf("Payload identified: AMBIENT\n");
        gpio_put(BLUE_LED_PIN, 0);
        gpio_put(RED_LED_PIN, 0);
        gpio_put(GREEN_LED_PIN, 1);
    }
}


void alertTempDrop(enum PayloadType payloadType)
{
    uint8_t ledToToggle;
    if (payloadType == HOT)
        ledToToggle = RED_LED_PIN;
    else if (payloadType == COLD)
        ledToToggle = BLUE_LED_PIN;

    for (int i = 0; i < 10; ++i)
    {
        toggleLED(ledToToggle);
        sleep_ms(1000);
    }
}

/*
    tracks the temperature state of the coaster
    if there is a change in state (say from hot to cold), then it updates the indicator LEDs.
    returns if there is a state change
*/
bool handleState(struct ADCReadings adcReadings, enum PayloadType payloadType)
{
    static float stableTemperature;
    static enum PayloadType lastState = AMBIENT;
    float currentTemperature = adcReadings.payloadValue;
    if (payloadType == AMBIENT)
    {
        // set some default values
        lastState = payloadType;
        stableTemperature = currentTemperature;
    }

    if (lastState != payloadType)
    {
        lastState = payloadType;

        // set MIN and MAX values, so when we encouter actual temp readings, we update with the highes or lowest values appropriately
        if (lastState == HOT)
            stableTemperature = 0.0f;
        else if (lastState == COLD)
            stableTemperature = 10000.0f;
    }

    if (lastState == HOT)
    {
        if (currentTemperature >= stableTemperature)
        {
            stableTemperature = currentTemperature;
            printf("temperature increasing to: %f\n", stableTemperature);
        }
        else
        {
            float temperatureDifference = stableTemperature - currentTemperature;
            printf("stable temp: %f . temperature difference from stable: %f", stableTemperature, temperatureDifference);
            if (temperatureDifference >= TEMPERATURE_DIFF_AFTER_WHICH_WE_ALERT_TEMPERATURE_SHIFT)
            {
                stableTemperature = currentTemperature;
                printf("\t temp difference is beyond threshold. alerting");
                alertTempDrop(HOT);
            }
            printf("\n");
        }

    }
    else if (lastState == COLD)
    {
        if (currentTemperature <= stableTemperature)
        {
            stableTemperature = currentTemperature;
            printf("temperature decreasing to: %f", stableTemperature);
        }
        else
        {
            float temperatureDifference = currentTemperature - stableTemperature;
            printf("stable temp: %f . temperature difference from stable: %f", stableTemperature, temperatureDifference);
            if (temperatureDifference >= TEMPERATURE_DIFF_AFTER_WHICH_WE_ALERT_TEMPERATURE_SHIFT)
            {
                stableTemperature = currentTemperature;
                printf("\t temp difference is beyond threshold. alerting");
                alertTempDrop(COLD);
            }
            printf("\n");
        }
    }

    setIndicatorLED(payloadType);
}

int main()
{
    stdio_init_all();
    printf("starting");
    pinsSetup();
    adc_init();
    
    while(1)
    {
        toggleLED(BOARD_LED_PIN);   // to indicate the start of processing

        struct ADCReadings adcReadings = getADCReadings();
        float temperatureDifference = calcuateTemperatureDifference(adcReadings);
        enum PayloadType payloadType = getPayloadType(temperatureDifference);
        printf("reference temp: %f\tpayload temp: %f\ttemperature difference after offset: %f\n", adcReadings.referenceValue, adcReadings.payloadValue, temperatureDifference);
        handleState(adcReadings, payloadType);
        
        toggleLED(BOARD_LED_PIN);   // to indicate the stop of processing
        sleep_ms(BOARD_SLEEP_MS_BETWEEN_CYCLES);
    }
}