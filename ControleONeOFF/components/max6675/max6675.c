#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "max6675.h"

#define PIN_NUM_MISO 12                         //Master Input Slave Output (Do Slave para o Master)
#define PIN_NUM_CLK 14                          //Serial Clock
#define PIN_NUM_CS 15                           //Chip selector

#define CLK_FREQ_HZ 4*1000000

#ifndef HIGH
  #define HIGH 1
#endif
#ifndef LOW
  #define LOW 0
#endif  

int spi_init = 0;

/**
 * @brief Envia um sinal de clock para o MAX6675 e le os dados recebidos
 * 
 * @param spi 
 * @return uint16_t temp Temperatura lida do MAX6675 em graus Celsius
 */
float readMax6675 (spi_device_handle_t spi){

    spi_transaction_t trans_word;               //Estrutura de dados
    uint16_t data,rawtemp = 0;                  
    float temp = 0;

    gpio_set_level(PIN_NUM_CS,LOW);             //CS é colocado em LOW para iniciar a comunicacao 
    vTaskDelay(500 / portTICK_PERIOD_MS);       //Espera 500ms para certificar que o dispositivo está pronto para ser lido

    rawtemp = 0x000;                            //rawtemp: onde é armazenado o valor da leitura
    data = 0x000;                               //data: valor enviado para iniciar a leitura

    /*Configuracao das info de transacao*/
    trans_word.length = 16; // Tamanho do dado
    trans_word.rxlength = 0; // Número de bits a serem recebidos (0 significa transmitir apenas)
    trans_word.rx_buffer = &rawtemp; // Ponteiro para o buffer de recepção
    trans_word.tx_buffer = &data; // Ponteiro para o buffer de transmissão
    

    /*Transmite a transacao para o dispositivo*/
    ESP_ERROR_CHECK(spi_device_transmit(spi, &trans_word));

    gpio_set_level(PIN_NUM_CS,HIGH);            //CS é colocado em HIGH para finalizar a comunicacao

   
    temp = (((((rawtemp & 0x00FF) << 8) | ((rawtemp & 0xFF00) >> 8))>>3)*25)/100;
    return temp;
 


} 

/**
 * @brief Configura o SPI para utilização do max6675 e o inicializa
 * 
 */
void max6675_set(void){
    esp_err_t ret; 

    //spi_device_handle_t spi;

    /* Para não inicializar o barramento mais de uma vez*/
    if(!spi_init)
    {
        /* Definindo os barramentos do SPI*/
        spi_bus_config_t buscfg={
            .miso_io_num= PIN_NUM_MISO,             //Pino MISO
            .sclk_io_num= PIN_NUM_CLK,              //Pino CLK
            .quadwp_io_num= -1,                     //-1 para indicar ausencia do pino
            .quadhd_io_num= -1,                     
            .max_transfer_sz= 16                    //Tamanho máximo da transação (16 Bits)
            };

        /* Definindo as configurações do SPI*/
        spi_device_interface_config_t devcfg={
            .clock_speed_hz= CLK_FREQ_HZ,           //Clock em 10 kHz
            .mode=0,                                //Modo do SPI 0
            .spics_io_num=PIN_NUM_CS,               //pino CS (Chip selector) 
            .queue_size=3,                          //Tamanho da fila de transmissão para o dispositivo
            .pre_cb = NULL,                         //Ponteiro para uma função de callback                          
            }; 

        /* Inicializa o barramento do SPI*/
        ret=spi_bus_initialize(HSPI_HOST, &buscfg, 1);
        ESP_ERROR_CHECK(ret);

        /*Adiciona o dispositivo SPI configurado à interface HSPI*/
        ret=spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
        ESP_ERROR_CHECK(ret);

      
    }
    

}

