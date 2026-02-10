#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <sdkconfig.h>
#include <stdbool.h>
#include "esp_adc/adc_oneshot.h"

// difference between the last project, enable the pulldown 

#define ignitionLED 3
#define engineLED 11
#define ignitionEn 2
#define driverSeatBelt 42
#define passSeatBelt 41
#define driveSeat 40
#define passSeat 39
#define Alarm 12

bool dSense = false;
bool dsbelt = false;
bool pSense = false;
bool psbelt = false;
    

/**
 * Defines a debouncing function that will return the value of the button input after a delay of 25MS
 */

bool debounce(int buttonInput){
    int button = gpio_get_level(buttonInput);
    vTaskDelay(25/ portTICK_PERIOD_MS);
    int new = gpio_get_level(buttonInput);
    if (new == button){
        return new;
    }
    return false;
}

/**
 * returns a boolean determining whether all of the car alarms systems have been satisifed ie:
 * driver seat belt, driver is seated etc.
 */
bool IgnitionReady(void){
    bool dSeatBelt = debounce(driverSeatBelt);
    bool pSeatBelt = debounce(passSeatBelt);
    bool dSeat = debounce(driveSeat);
    bool passengerSeat = debounce(passSeat);
    if (dSeatBelt){
        dsbelt = true;
    }
    else{dsbelt = false;}
    if (pSeatBelt){
        psbelt = true;
    }
    else{psbelt = false;}
    if (dSeat){
        dSense = true;
    }
    else{
        dSense = false;
    }
    if (passengerSeat){
        pSense = true;
    }
    else {pSense = false;}
    return (dsbelt && psbelt && dSense && pSense);
    }

    /**
     * Will configure all of the pins within the design resetting all of the pins within the design to a known state
     * setting the direction to either input or output 
     * enabling pullup resistors within the ESP
     * And finally setting all of the output pins to zero
     */

void pinConfig(void){
    gpio_reset_pin(ignitionLED);
    gpio_reset_pin(engineLED);
    gpio_reset_pin(ignitionEn);
    gpio_reset_pin(driverSeatBelt);
    gpio_reset_pin(passSeatBelt);
    gpio_reset_pin(driveSeat);
    gpio_reset_pin(passSeat);
    gpio_reset_pin(Alarm);

    gpio_set_direction(ignitionLED, GPIO_MODE_OUTPUT);
    gpio_set_direction(engineLED, GPIO_MODE_OUTPUT);
    gpio_set_direction(Alarm, GPIO_MODE_OUTPUT);
    gpio_set_direction(ignitionEn, GPIO_MODE_INPUT);
    gpio_set_direction(driverSeatBelt, GPIO_MODE_INPUT);
    gpio_set_direction(driveSeat, GPIO_MODE_INPUT);
    gpio_set_direction(passSeat, GPIO_MODE_INPUT);
    gpio_set_direction(passSeatBelt, GPIO_MODE_INPUT);

    gpio_pulldown_en(ignitionEn);
    gpio_pulldown_en(driverSeatBelt);
    gpio_pulldown_en(driveSeat);
    gpio_pulldown_en(passSeat);
    gpio_pulldown_en(passSeatBelt);

    gpio_set_level(ignitionLED, 0);
    gpio_set_level(engineLED, 0);
    gpio_set_level(Alarm, 0);

}


void app_main(void) {
    pinConfig();
    bool initial_message = true;
    bool engineRunning = false;
    bool killEngine = false;
    while(1){
        bool ignitEn = debounce(ignitionEn);
        bool ready = IgnitionReady();
        if (dSense && initial_message){
            printf("Welcome to enhanced Alarm system model 218 -W25\n");
            initial_message = false;
        }

        if (ready && !engineRunning){
            gpio_set_level(ignitionLED, 1);
        }
        else{gpio_set_level(ignitionLED, 0);}
    

        if(ignitEn){
            vTaskDelay (25 / portTICK_PERIOD_MS);
            if (engineRunning){
            printf("engine stopping...\n");
            gpio_set_level(engineLED, 0);
            engineRunning = false;
            killEngine = true;
            }
            if (ready) {
                engineRunning = true;
                killEngine = false;
                printf("engine starting...\n");
                gpio_set_level(ignitionLED, 0);
                gpio_set_level(engineLED, 1);
            }
            if (!engineRunning && !killEngine){
                gpio_set_level(Alarm, 1);
                if (!dSense){
                    printf("Driver seat not occupied\n");
                }
                
                if (!dsbelt){
                    printf("Driver seatbelt not fastened\n");
                }

                if (!pSense){
                    printf("Passenger seat not occupied\n");
                }
                if (!psbelt){
                    printf("Passenger seatbelt not fastened\n");
                }
                vTaskDelay (2000/ portTICK_PERIOD_MS);
            }
        }
        else {
            gpio_set_level (Alarm, 0);
        }
        vTaskDelay(25 / portTICK_PERIOD_MS);

    }
}