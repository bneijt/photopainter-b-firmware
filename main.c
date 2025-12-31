#include "EPD_Test.h" // Examples
#include "run_File.h"
#include <hardware/watchdog.h>
#include <pico/rand.h>
#include <unistd.h>
#include "led.h"
#include "ff.h"
#include "waveshare_PCF85063.h" // RTC
#include "DEV_Config.h"

#include <time.h>

// Actions
#define PWR_LED_ON gpio_put(LED_PWR, 1)
#define PWR_LED_OFF gpio_put(LED_PWR, 0)

#define BAT_IS_CHARGING gpio_get(CHARGE_STATE) == 0
#define BAT_AVAILABLE gpio_get(VBUS) >= 1

#define MISSING_SD_CARD sdTest()
// Sleep LOOP_DELAY_MS milliseconds per loop
#define LOOP_DELAY_MS 5000
// Display image every NEXT_IMAGE_EVERY loops
// 720 is approximately 1 hour
// 1440 is approximately 2 hours
// 2880 is approximately 4 hours
#define NEXT_IMAGE_EVERY 2880

// count=1; for file in *.bmp; do mv "$file" $(printf "%04d.bmp" $count); ((count++)); done
int MAX_PICTURE_INDEX = 9999;
int LOOP_COUNTER = 0;
int NEXT_BUTTON_PRESSED_THIS_CYCLE = 0;

float measure_battery_voltage(void)
{
    float voltage = 0.0;
    const float conversion_factor = 3.3f / (1 << 12);
    uint16_t result = adc_read();
    voltage = result * conversion_factor * 3;
    printf("Raw battery measurement: 0x%03x, voltage: %f V\n", result, voltage);
    return voltage;
}

uint32_t rand_number(uint32_t max)
{
    return 1 + (get_rand_32() % max);
}

void charge_state_callback(uint gpio, uint32_t event_mask)
{
    if (BAT_IS_CHARGING)
    {
        PWR_LED_ON;
    }
    else
    {
        PWR_LED_OFF;
    }
}

bool file_exists(const char *path)
{
    FILINFO fno;
    return f_stat(path, &fno) == FR_OK;
}

void display_next_image()
{
    run_mount();
    char picture_path[20];
    do
    {
        int next_picture_idx = rand_number(MAX_PICTURE_INDEX);
        sprintf(picture_path, "pic/%04d.bmp", next_picture_idx);
        printf("Max idx is %i, checking for image: %s\n", MAX_PICTURE_INDEX, picture_path);
    } while (!file_exists(picture_path) && --MAX_PICTURE_INDEX > 0);

    printf("Displaying image: %s\n", picture_path);
    EPD_7in3e_display_bmp(picture_path);
    run_unmount();
}

void next_button_pressed(uint gpio, uint32_t event_mask)
{
    printf("Next button pressed\n");
    LOOP_COUNTER = 0;
    ++NEXT_BUTTON_PRESSED_THIS_CYCLE;
}

int main(void)
{
    printf("Start\n");

    if (DEV_Module_Init() != 0)
    {
        printf("Init failed\n");
        return -1;
    }

    PCF85063_init(); // RTC init
    printf("Checking for battery\n");
    if (BAT_AVAILABLE)
    {
        printf("Battery found\n");
        if (measure_battery_voltage() < 3.1)
        {
            printf("Low power\n");
            ledLowPower(); // LED flash for Low power
            powerOff();    // BAT off
            return 0;
        }
        gpio_set_irq_enabled_with_callback(CHARGE_STATE, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, charge_state_callback);
    }
    else
    {
        printf("No battery connected\n");
    }

    if (MISSING_SD_CARD)
    {
        printf("No SD Card found\n");
        EPD_7in3e_display_message("No SD Card found");
        powerOff(); // BAT off
        return 0;
    }
    else
    {
        printf("SD Card found\n");
    }

    gpio_set_irq_enabled_with_callback(BAT_STATE, GPIO_IRQ_EDGE_FALL, true, next_button_pressed);

    printf("Starting main loop\n");
    sleep_ms(1000);

    PWR_LED_OFF;

    while (1)
    {
        printf("Main loop %i/%i\n", LOOP_COUNTER, NEXT_IMAGE_EVERY);
        if (BAT_AVAILABLE)
        {
            if (measure_battery_voltage() < 3.3)
            {
                EPD_7in3e_display_message("Low battery, please charge.");
                break;
            }
        }
        if(NEXT_BUTTON_PRESSED_THIS_CYCLE > 4)
        {
            printf("Next button pressed a lot of times, enable restart\n");
            watchdog_enable(100, 1);
            break;
        } else {
            NEXT_BUTTON_PRESSED_THIS_CYCLE = 0;
        }
        if (LOOP_COUNTER == 0)
        {
            printf("Displaying next image\n");
            display_next_image();
        }
        if (++LOOP_COUNTER >= NEXT_IMAGE_EVERY)
        {
            LOOP_COUNTER = 0;
        }
        sleep_ms(LOOP_DELAY_MS);
    }

    printf("finish\n");
    powerOff();
    return 0;
}
