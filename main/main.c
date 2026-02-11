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
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO (7)
#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY (50)
#define LEDC_DUTY_MIN 307
#define LEDC_DUTY_MAX 944 

bool dSense = false;
bool dsbelt = false;
bool pSense = false;
bool psbelt = false;
static adc_oneshot_unit_handle_t oneShotHandler; 
static adc_cali_handle_t adcWiperPotenHandle;  
static hd44780_t lcd = {
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

static void example_ledc_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 50 Hz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // Set duty to 0%
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
    adc_cali_create_scheme_curve_fitting(&caliPotenConfig, &adcWiperPotenHandle);
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
 * oneshot read for the potentiometer in our system
 */

int potentiometerRead(void){
    int adcBitsPoten;
    int adcMVPoten;
    adc_oneshot_read(oneShotHandler, wiperPoten, &adcBitsPoten);
    adc_cali_raw_to_voltage(adcWiperPotenHandle, adcBitsPoten, &adcMVPoten); 
    return adcMVPoten;
}

/**
 * defines a function that will return an integer corresponding to one of the headlight mode the user selects
 * 0 = OFF,
 * 1 = ON,
 * 2 = AUTO
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
    bool initial_message = true;
    bool engineRunning = false;
    bool killEngine = false;
    lcdINIT(NULL);
    while(1){
        bool ignitEn = debounce(ignitionEn);
        int wiperSpeed = speedSelection(potentiometerRead());
        bool ready = IgnitionReady();
        hd44780_clear(&lcd);
        if (wiperSpeed == 0){
            hd44780_clear(&lcd);
            hd44780_puts(&lcd, "WIPERS OFF");
        }
        if (wiperSpeed == 1 && engineRunning){
            hd44780_clear(&lcd);
            hd44780_puts(&lcd, "WIPERS INTERMITTENT");
            for (int i = 0; i <= 1800; i++){
                if (i > 600 && i < 1200){
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_MIN);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                }
                if ( i >= 1200 && i < 1800){
                ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY_MAX);
                ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
                }
                if (i >= 1800){
                    i = 0;
                }
                vTaskDelay(25 / portTICK_PERIOD_MS);
            }
        }
        if (wiperSpeed == 2 && engineRunning){
            hd44780_clear(&lcd);
            hd44780_puts(&lcd, "WIPERS LOW");
        }
        if (wiperSpeed == 3 && engineRunning){
            hd44780_clear(&lcd);
            hd44780_puts(&lcd, "WIPERS HIGH");
        }

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
                engineRunning = false;
                killEngine = false;
                vTaskDelay (2000/ portTICK_PERIOD_MS);
            }
        }
        else {
            gpio_set_level (Alarm, 0);
        }
        vTaskDelay(25 / portTICK_PERIOD_MS);
    }
}
