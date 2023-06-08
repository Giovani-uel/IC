#include "driver/gpio.h"

#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_sleep.h"
#include "esp_wifi.h"
#include "esp_system.h"

#include "rele.h"

#include "driver/ledc.h"

#define PWM_CHANNEL     LEDC_CHANNEL_0
#define PWM_TIMER       LEDC_TIMER_0
#define GPIO_PWM_OUTPUT 2

int max_d = 256;//0.8*256;
int min_d = 0;
/**
 * @brief Funcao que altera o duty cycle do PWM
 * 
 * @param d O valor da largura do pulso. Ex: d = 1 -> 100%, d = 0.5 -> 50% ...
 */
void rele_d_altera(float d){

    // Altere o duty cycle do canal PWM
    ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, d*256);

    // Atualize o canal PWM
    ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);
}

void rele_pwm_set(void){
// Inicialize o módulo PWM com uma frequência de 1kHz e 10 bits de resolução
    ledc_timer_config_t pwm_timer = {
        .duty_resolution = LEDC_TIMER_8_BIT,
        .freq_hz = 10,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = PWM_TIMER
    };
    ledc_timer_config(&pwm_timer);

    // Configure o canal PWM
    ledc_channel_config_t pwm_channel = {
        .channel = PWM_CHANNEL,
        .duty = 0,
        .gpio_num = GPIO_PWM_OUTPUT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_sel = PWM_TIMER
    };
    ledc_channel_config(&pwm_channel);

    // Altere o duty cycle do canal PWM
    //ledc_set_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL, 512);

    // Atualize o canal PWM
    //ledc_update_duty(LEDC_LOW_SPEED_MODE, PWM_CHANNEL);
}