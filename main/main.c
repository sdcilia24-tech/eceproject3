#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <sdkconfig.h>
#include <stdbool.h>
#include "esp_adc/adc_oneshot.h"
#include <hd44780.h>
#include <esp_idf_lib_helpers.h>
#include <inttypes.h>
#include "driver/ledc.h"

#define ignitionLED 11
#define engineLED 3
#define ignitionEn 2
#define driverSeatBelt 42
#define passSeatBelt 41
#define driveSeat 40
#define passSeat 39
#define Alarm 12
#define adcAtten ADC_ATTEN_DB_12
#define bitWidth ADC_BITWIDTH_12
#define wiperPoten ADC_CHANNEL_3
#define intermittentChan ADC_CHANNEL_5
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO 7
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY 50
#define LEDC_DUTY_NINETY 580
#define LEDC_DUTY_ZERO 944 

bool dSense = false;
bool dsbelt = false;
bool pSense = false;
bool psbelt = false;
static adc_oneshot_unit_handle_t oneShotHandler; 
static adc_cali_handle_t adcWiperPotenHandle;  
static adc_cali_handle_t intermittentHandle;
static const hd44780_t lcd = {
        .write_cb = NULL,
        .font = HD44780_FONT_5X8,
        .lines = 2,
        .pins = {
            .rs = GPIO_NUM_5,
            .e  = GPIO_NUM_37,
            .d4 = GPIO_NUM_36,
            .d5 = GPIO_NUM_35,
            .d6 = GPIO_NUM_48,
            .d7 = GPIO_NUM_47,
            .bl = HD44780_NOT_USED
        }
    };

/*
initalizes the PWM in our system
*/
static void example_ledc_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY, 
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0,
        .hpoint         = 0
    };
ledc_channel_config(&ledc_channel);
}


/**
 * Configures the adc conversions within our system
 */
void adcConfig(void){
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };

    adc_oneshot_new_unit(&init_config1, &oneShotHandler);
     adc_oneshot_chan_cfg_t config = {
        .atten = adcAtten,
        .bitwidth = bitWidth
    };

    adc_oneshot_config_channel(oneShotHandler, wiperPoten, &config);
        adc_cali_curve_fitting_config_t caliPotenConfig = {
        .unit_id = ADC_UNIT_1,
        .chan = wiperPoten,
        .atten = adcAtten,
        .bitwidth = bitWidth
    };
        adc_oneshot_config_channel(oneShotHandler, intermittentChan , &config);
        adc_cali_curve_fitting_config_t caliPotenConfig2 = {
        .unit_id = ADC_UNIT_1,
        .chan = intermittentChan,
        .atten = adcAtten,
        .bitwidth = bitWidth
    };
    adc_cali_create_scheme_curve_fitting(&caliPotenConfig, &adcWiperPotenHandle);
    adc_cali_create_scheme_curve_fitting(&caliPotenConfig2, &intermittentHandle);
}


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

/**
 * oneshot read for the speed Potentiometer in our system 
 * returns the value in milivolts 
 */
int wiperPotentiometerRead(void){
    int adcBitsPoten;
    int adcMVPoten;
    adc_oneshot_read(oneShotHandler, wiperPoten, &adcBitsPoten);
    adc_cali_raw_to_voltage(adcWiperPotenHandle, adcBitsPoten, &adcMVPoten); 
    return adcMVPoten;
}
/*
reading for the intermittent mode in our system
returns the value in milivolts
*/
int intermittentPotenRead(void){
    int adcBitsPoten;
    int adcMVPoten;
    adc_oneshot_read(oneShotHandler, intermittentChan, &adcBitsPoten);
    adc_cali_raw_to_voltage(adcWiperPotenHandle, adcBitsPoten, &adcMVPoten); 
    return adcMVPoten;
}

/**
 * defines a function that will allow the user to select a delay time in the intermittent mode
 * 0 - short
 * 1 - medium
 * 2 - long
 */
int intermDelaySelection(int adcMV){
if (adcMV < 1000){
    return 0;
}
if (adcMV < 2000){
    return 1;
}
else{
    return 2;
}
}

/**n
 * defines a function that will return an integer corresponding to one of the wiper 
 * speeed mode selection within our system
 * 0 = off
 * 1 = intermittent
 * 2 = low
 * 3 = high
 */
int speedSelection(int adcMV){
    if (adcMV <= 500){
        return 0;
    }
    if (adcMV < 1500){
        return 1;
    }
    else if (adcMV < 3000){
        return 2;
    }
    else{
        return 3;
    }

}
/**
 * initializes our LCD display
 */
void lcdINIT(void *pvParameters)
{
    ESP_ERROR_CHECK(hd44780_init(&lcd));
    hd44780_gotoxy(&lcd, 0,0);
}

void app_main(void) {

    adcConfig();
    pinConfig();
    example_ledc_init();
    lcdINIT(NULL);
    int lastSelectionSpeed = -1;
    int lastSelecInter = -1;
    bool initialMessage = true;
    bool engineRunning = false;
    bool alarmOff = true;
    int counter = 0;
    int Threshold = 14;

    while(1){
        bool ignitEn = debounce(ignitionEn);
        int wiperSpeed = speedSelection(wiperPotentiometerRead());
        int interSelect = intermDelaySelection(intermittentPotenRead());
        bool ready = IgnitionReady();
        if (!engineRunning){
            hd44780_clear(&lcd);
        }
        if ((lastSelectionSpeed != wiperSpeed) && engineRunning){
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_ZERO);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            counter = 0;
            hd44780_gotoxy(&lcd, 0, 0);
            if (wiperSpeed == 0){
            hd44780_clear(&lcd);
            hd44780_puts(&lcd, "WIPERS OFF");
            }
            if (wiperSpeed == 1){
                hd44780_clear(&lcd);
                hd44780_puts(&lcd, "WIPERS INTERMITTENT");
            }
            if (wiperSpeed == 2){
                hd44780_clear(&lcd);
                hd44780_puts(&lcd, "WIPERS LOW");
            }
            if (wiperSpeed == 3){
                hd44780_clear(&lcd);
                hd44780_puts(&lcd, "WIPERS HIGH");
            }
            lastSelectionSpeed = wiperSpeed;
        }

        if ((lastSelecInter != interSelect) && wiperSpeed == 1){
            ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_ZERO);
            ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
            hd44780_gotoxy(&lcd, 0, 1);
            hd44780_puts(&lcd, "                     ");
            if (interSelect == 0){
                hd44780_gotoxy(&lcd, 0, 1);
                hd44780_puts(&lcd, "SHORT");
            }
            else if (interSelect == 1){
                hd44780_gotoxy(&lcd, 0, 1);
                hd44780_puts(&lcd, "MED");
            }
            else{
                hd44780_gotoxy(&lcd, 0, 1);
                hd44780_puts(&lcd, "LONG");                    
            }
            lastSelecInter = interSelect;
        }

        if (engineRunning){
            if (wiperSpeed == 0){
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_ZERO);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
        }
        if (wiperSpeed == 1 || wiperSpeed == 2)
        {
            if (counter < Threshold){
                    int LEDC_TURN_1 = LEDC_DUTY_NINETY+ (LEDC_DUTY_ZERO-LEDC_DUTY_NINETY)*(((float)counter+1)/Threshold);
                    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_TURN_1);
                    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                }
            else if (counter >= Threshold && counter < Threshold * 2){
                    int LEDC_TURN_2;
                    if(counter<Threshold*2)
                    {
                        LEDC_TURN_2 = LEDC_DUTY_ZERO-LEDC_DUTY_ZERO*(((float)counter-Threshold+1)/(Threshold))+
                        LEDC_DUTY_NINETY*(((float)counter-Threshold+1)/(Threshold));
                    }
                    else
                    {
                        LEDC_TURN_2 = LEDC_DUTY_NINETY;
                    }
                    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_TURN_2);
                    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                }
            if (wiperSpeed == 1 && interSelect == 0){
                    if (counter >= (int)(Threshold * 2.66)){
                        counter = 0;
                     }
                }
            if (wiperSpeed == 1 && interSelect == 1){
                     if (counter >= Threshold * 4){
                    counter = 0;
                     }
                }
            if (wiperSpeed == 1 && interSelect == 2){
                    if (counter >= (int)(Threshold * 5.33)){
                        counter = 0;
                    }
                }
            if(wiperSpeed == 2 && counter >= Threshold*2)
            {
                counter=0;
            }
            counter++;
        }

        if (wiperSpeed == 3){
            for (int i = LEDC_DUTY_ZERO; i > LEDC_DUTY_NINETY; i -= 20){
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, i);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                vTaskDelay(25 / portTICK_PERIOD_MS);
            }
            for (int j = LEDC_DUTY_NINETY; j < LEDC_DUTY_ZERO; j += 20){
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, j);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                vTaskDelay(25 / portTICK_PERIOD_MS);
            }

        }

        }

        /*
        if (wiperSpeed == 2 && engineRunning){
            if (counter < Threshold){
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_MIN);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                }
            else if (counter >= Threshold && counter < Threshold * 2){
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_MAX);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                }
            if (counter >= Threshold * 5){
                    counter = 0;
            }
            counter++;
        }

        if (wiperSpeed == 3 && engineRunning){
            if (counter < Threshold){
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_MIN);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                }
            else if (counter >= Threshold && counter < Threshold * 2){
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_MAX);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                }
            if (counter >= Threshold * 2){
                    counter = 0;
            }
            counter++;
        }
        */

        if (dSense && initialMessage){
            printf("Welcome to enhanced Alarm system model 218 -W25\n");
            initialMessage = false;
        }

        if (ready && !engineRunning){
            gpio_set_level(ignitionLED, 1);
        }
        else{
            gpio_set_level(ignitionLED, 0);
        }
    
        if(ignitEn){
            vTaskDelay (25 / portTICK_PERIOD_MS);
            if (engineRunning){
            printf("engine stopping...\n");
            gpio_set_level(engineLED, 0);
            engineRunning = false;
            alarmOff = true;
            lastSelectionSpeed = -1;
            lastSelecInter = -1;
            continue;
            }
            if (ready) {
                lastSelectionSpeed = -1;
                lastSelecInter = -1;
                engineRunning = true;
                alarmOff = true;
                printf("engine starting...\n");
                gpio_set_level(ignitionLED, 0);
                gpio_set_level(engineLED, 1);
                continue;
            }
            else{
                alarmOff = false;
            }
            if (!alarmOff){
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
                alarmOff = true;
            }
        }
        else {
            gpio_set_level (Alarm, 0);
        }
        vTaskDelay(25 / portTICK_PERIOD_MS);

    }
}
