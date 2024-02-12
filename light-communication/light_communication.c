#include <stdio.h>
#include <pico/stdlib.h>
#include "hardware/adc.h"

#define SENDER_INDICATION_PIN 15        // if this pin is set to high, then the board is intended to send the data, else receive
#define RESET_BUTTON_PIN 0              // the sender will start sending the data packet. receiver will clear out whatever it received so far and start again
#define RECEIVER_ADC_INDEX 2
#define NUMBER_OF_SAMPLES_ADC 1
#define BUTTON_DEBOUNCE_DELAY_MS 200
#define OUTPUT_LED_PIN 14
#define SLEEP_MS 10
#define ADC_HIGH_LEVEL_THRESHOLD 100.0f

uint32_t lastButtonPressedTimestamp = 0;
bool resetHappened = false;

// bool data[] = {0,1,1,0,1,0,0,0,0,1,1,0,0,1,0,1,0,1,1,0,1,1,0,0,0,1,1,0,1,1,0,0,0,1,1,0,1,1,1,1};    // "hello"
// #define DATA_LENGTH 40
bool data[8] = {0};
#define PACKET_SIZE 8

float getADCReadings()
{
    adc_select_input(RECEIVER_ADC_INDEX);
    uint32_t adcSum = 0;
    for (uint32_t i = 0; i < NUMBER_OF_SAMPLES_ADC; ++i)
    {
        adcSum += adc_read();
    }
    return ((float) adcSum / (float) NUMBER_OF_SAMPLES_ADC);
}

/*
    checks if the pin indicating whether the board is a sender is held high
*/
bool isSender()
{
    gpio_init(SENDER_INDICATION_PIN);
    gpio_set_dir(SENDER_INDICATION_PIN, GPIO_IN);
    return gpio_get(SENDER_INDICATION_PIN);
}


bool setGPIOLevel(uint8_t pin, bool level)
{
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_OUT);
    gpio_put(pin, level);
}

void sendData(bool* _data, uint32_t _length)
{
    uint32_t packetCount = _length / PACKET_SIZE;
    printf("packetCount: %d  PACKET_SIZE: %d\n", packetCount, PACKET_SIZE);
    for (uint32_t i = 0; i < (packetCount); ++i)
    {
        // send initial start bit
        setGPIOLevel(OUTPUT_LED_PIN, 1);
        busy_wait_ms(SLEEP_MS);

        for (uint32_t j = 0; j < PACKET_SIZE; ++j)
        {
            uint32_t bitIndex = (i * PACKET_SIZE) + j;
            printf("sending bit count: %d\tvalue: %d\n", bitIndex, _data[bitIndex]);
            setGPIOLevel(OUTPUT_LED_PIN, _data[bitIndex]);
            busy_wait_ms(SLEEP_MS);
        }

        // complete
        setGPIOLevel(OUTPUT_LED_PIN, 0);
        busy_wait_ms(SLEEP_MS);
    }
    printf("data send completed\n");
}

/*
    will read data from serial and send the same
*/
void readAndSendData()
{
    while(1)
    {
        printf("input: ");
        char c;
        scanf("%c", &c);
        printf("%c\n", c);
        for (int i = 7, j = 0; i >= 0 ; --i, ++j)
        {
            data[j] = (c & (1 << i) );
        }
        sendData(data, 8);
    }
}

void receiveData()
{
    uint32_t receivedDataInt = 0;
    bool receivedBit;
    bool receivedData[PACKET_SIZE] = {false};
    bool startBitReceived = false;
    do {
        float reading = getADCReadings();
        // printf("%f\n", reading);
        if (reading >= ADC_HIGH_LEVEL_THRESHOLD)
        {
            printf("start bit received: %f\n", reading);
            startBitReceived = true;
        }
    } while(!startBitReceived);
    busy_wait_ms(SLEEP_MS);
    
    for (uint32_t i = 0; i < PACKET_SIZE; ++i)
    {
        float reading = getADCReadings();
        receivedBit = (reading >= ADC_HIGH_LEVEL_THRESHOLD);
        receivedData[i] = receivedBit;
        receivedDataInt |= (receivedBit << (PACKET_SIZE - 1 - i) );
        printf("received bit count: %d\tvalue: %d  adc_reading: %f\n", i, receivedBit, reading);
        busy_wait_ms(SLEEP_MS);
    }
    printf("data receive completed %c\n", receivedDataInt);
}

void buttonPressed(uint gpio, uint32_t events)
{
    printf("interrupt registered\n");

    /*
        button debounce logic
    */
    uint32_t msSinceBoot = to_ms_since_boot(get_absolute_time());
    if (msSinceBoot - lastButtonPressedTimestamp <= BUTTON_DEBOUNCE_DELAY_MS)
    {
        printf("interrupt being ignored\n");
        return;
    }
    else
    {
        lastButtonPressedTimestamp = msSinceBoot;
    }

    resetHappened = true;

}

void process()
{
    
    if (isSender())
    {
        if (!resetHappened)
        {
            return;
        }
        printf("sending data\n");
        readAndSendData();
    }
    else
    {
        printf("receiving data\n");
        while(1)
        {
            // continuously receive data, since we have the start bit to indicate start of packet emission
            receiveData();
        }
    }

    // wait for the next button press.
    // set this up at the very end to ensure interrupts during this processing does not cause another iteration
    resetHappened = false;
}

void initButton()
{
    gpio_set_dir(RESET_BUTTON_PIN, GPIO_IN);
    gpio_pull_up(RESET_BUTTON_PIN);
}

int main()
{
    stdio_init_all();
    adc_init();
    initButton();
    gpio_set_irq_enabled_with_callback(RESET_BUTTON_PIN, GPIO_IRQ_EDGE_FALL, true, &buttonPressed);
    while(1)
    {
        process();
        sleep_ms(10);
    }
    
}
