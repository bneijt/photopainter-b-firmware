#include "EPD_Test.h" // Examples
#include "run_File.h"
#include <hardware/watchdog.h>
#include <pico/rand.h>
#include <unistd.h>
#include "led.h"
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
#define NEXT_IMAGE_EVERY 720

int MAX_PICTURE_INDEX = 9999;

float measure_battery_voltage(void)
{
    float voltage = 0.0;
    const float conversion_factor = 3.3f / (1 << 12);
    uint16_t result = adc_read();
    voltage = result * conversion_factor * 3;
    printf("Raw battery measurement: 0x%03x, voltage: %f V\n", result, voltage);
    return voltage;
}

int rand_between(int min, int max)
{
    return (get_rand_32() % (max - min + 1)) + min;
}

void charge_state_callback()
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
    return access(path, F_OK) == 0;
}

void display_next_image()
{
    run_mount();
    char picture_path[20];
    do
    {
        int next_picture_idx = rand_between(1, MAX_PICTURE_INDEX);
        sprintf(picture_path, "0:/pic/%04d.bmp", next_picture_idx);
        printf("Max idx is %i, checking for image: %s\n", MAX_PICTURE_INDEX, picture_path);
    } while (!file_exists(picture_path) && MAX_PICTURE_INDEX-- > 0);

    printf("Displaying image: %s\n", picture_path);
    EPD_7in3e_display_bmp(picture_path);
    run_unmount();
}

int main(void)
{
    printf("Start\n");

    if (DEV_Module_Init() != 0)
    {
        printf("Init failed\n");
        return -1;
    }

    if (BAT_AVAILABLE)
    {
        if (measure_battery_voltage() < 3.1)
        {
            printf("Low power\n");
            ledLowPower(); // LED flash for Low power
            powerOff();    // BAT off
            return 0;
        }
        gpio_set_irq_enabled_with_callback(CHARGE_STATE, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, charge_state_callback);
    }
    if (MISSING_SD_CARD)
    {
        printf("No SD Card found\n");
        EPD_7in3e_display_message("No SD Card found");
        powerOff(); // BAT off
        return 0;
    }
    printf("Enable watchdog\n");
    watchdog_enable(2 * LOOP_DELAY_MS, true);
    watchdog_update();

    int loop_counter = NEXT_IMAGE_EVERY; // Force first loop to display image
    while (1)
    {
        if (BAT_AVAILABLE)
        {
            if (measure_battery_voltage() < 3.3)
            {
                EPD_7in3e_display_message("Low battery, please charge.");
                break;
            }
        }
        if (++loop_counter >= NEXT_IMAGE_EVERY)
        {
            printf("Displaying next image\n");
            display_next_image();
        }
        watchdog_update();
        sleep_ms(LOOP_DELAY_MS);
        watchdog_update();
    }

    printf("finish\n");
    powerOff();
    return 0;
}
