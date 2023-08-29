/**
 * @file main.c
 * @author Giovani (giovani.hiroshi@uel)
 * @brief O algoritmo controla a temperatura do ferro. Funciona atraves de 3 tarefas e mais uma para printar os valores de temperatura
 * 
 * task_read_temp - le a temperatura a cada 0,5s (tempo de delay da propria funcao de leitura) e permite a execucao da tarefas: verifica_tempo e 
 * control_pwm. O bit que realiza essa permicao e limpo pelas tarefas que a esperam. 
 * 
 * control_pwm - Realiza calculo do PID. Esse calculo depende da temperatura atual e do setpoint (Temperatura desejada). Configura o pwm de acordo 
 * com a saida do PID.
 * 
 * verifica_tempo - Atualizado a cada 1s. Altera o setpoint dependendo do estagio do perfil de temperatura. Armazena a temperatura durante todo o
 * processo. Inicia em pelo pre aquecimento que aumenta a temp do ferro ate 150 graus. Chegando em 150, passa para o estagio de imersao termica.
 * Imersao termica mantem essa temp por 120s. Depois passa para o proximo estagio, refluxo. Refluxo é dividido em duas partes. Parte 1, aumenta a temp
 * do ferro ate 240 graus e dps passa para parte 2. Parte 2, mantem essa temp por 30s e dps passa para o resfriamento. No resfriamento, o ferro fica 
 * desligado por 60s e dps permite a execucao da proxima tarefa. Essa tarefa é deletada no fim, pois nao vai ser utilizada mais e para parar de armazenar
 * os valores de temp
 * 
 * printar_task - Ocorre apos o fim do processo da solda por refluxo. Printa a temperatura ideal que o ferro deveria seguir e a temperatura real que
 * o ferro seguiu
 * 
 * @version 0.1
 * @date 2023-06-02
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "rele.h"
#include "max6675.h"
#include "sys/time.h"
#include <time.h>

#define TEMP_BIT BIT0
#define PRINTAR_BIT BIT1
#define CONTROL_BIT BIT2

// handle do dispositivo SPI
spi_device_handle_t spi;

// variáveis de controle
int i,temp = 0;
float T = 0.5; //periodo em s 
int setpoint = 0;
float current_time, erro , erro_ant = 0;
float kp = 3;
float ki = 24;
float kd = 4;
float PID_P,PID_I, PID_D , PID_Output,PID_Output_ant = 0;

int temperatura_ideal[3000] = {0};
int temperatura_real[3000] = {0};

static const char *TAG = "MAIN";

// handle do grupo de eventos
EventGroupHandle_t LD_event_group;


void obterHoraLocal() {
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    ESP_LOGI(TAG, "s: %d ", timeinfo.tm_sec);
}


/**
 * @brief Esperar acabar o processo de solda por refluxo e printa as temperaturas desse o processo
 * 
 * @param pvParameters 
 */
void printar_task(void *pvParameters)
{
    
    while (1)
    {
        // espera pelo bit 
        xEventGroupWaitBits(
            LD_event_group,         // grupo de eventos a ser esperado
            PRINTAR_BIT,            // bit que está sendo esperado
            pdTRUE,                 //  limpa os bits após a leitura
            pdFALSE,                // espera por um dos bits
            portMAX_DELAY           // tempo máximo para esperar os bits
        );
        
        int i;
        //Logica para printar as temperaturas
        printf("Temperatura ideal: ");
        for (i = 0; i < 3000; i++) {
            printf("%d ", temperatura_ideal[i]);
        }
        printf("Temperatura real: ");
        for (i = 0; i < 3000; i++) {
            printf("%d ", temperatura_real[i]);
        }
        
    }
}

/**
 * @brief Verifica em qual estagio esta o perfil de temperatura. Altera o setpoint de acordo com o estagio.
 * Ocorre a cada 1s(0,5s de delay da propria task e 0,5s do delay da leitura)
 * 
 * @param pvParameters 
 */
void verifica_tempo(void *pvParameters)
{
    int modo_operacao = 0;
    int t_atual = 0;
    int t_anterior = 0;
    while (1)
    {
        // espera pelo bit de controle
        xEventGroupWaitBits(
            LD_event_group,         // grupo de eventos a ser esperado
            CONTROL_BIT,            // bit que está sendo esperado
            pdFALSE,                // não limpa os bits após a leitura
            pdFALSE,                // espera por um dos bits
            portMAX_DELAY           // tempo máximo para esperar os bits
        );


        switch (modo_operacao)
        {
        //Aquece ate 100 graus e espera por 2 min    
        case 0: 
            /*temperatura_ideal armazena a temperatura que o perfil de temperatura deveria seguir*/
            temperatura_ideal[t_atual] = 100;
            temperatura_real[t_atual] = temp;
            t_atual++;
            setpoint = 100;        
            if(t_atual>360){
                ESP_LOGI(TAG, "Pre aquecimento");
                modo_operacao = 1;
            }
            break;
        //pre aquecimento - Aumenta do temperatura do ferro ate 150 graus    
        case 1:
            /*temperatura_ideal armazena a temperatura que o perfil de temperatura deveria seguir*/
            temperatura_ideal[t_atual] = 150;
                        
            /*temperatura_real armazena os valores de temperatura da leitura do MAX6675*/
            temperatura_real[t_atual] = temp;
            /*Incrementa t_atual a cada 0.5s*/
            t_atual++;
            /*setpoint é a temperatura que o perfil deve atingir*/
            setpoint = 150;
            /*Se a temperatura do ferro passar de 150, passa para o proximo estagio*/
            if(temp > (150 - 20)){
                ESP_LOGI(TAG, "Imersao termica");
                modo_operacao = 2;
                //armazena tempo que mudou de estagio
                t_anterior = t_atual;
            }         
            break;
        //Imersao termica - Manter a temperatura em 150 graus
        case 2:
            //Estagio p manter a temperatura do ferro em 150 graus
            setpoint = 150;  
            //temperatura_ideal armazena a temperatura que o perfil de temperatura deveria seguir
            temperatura_ideal[t_atual] = 150;
            //temperatura_real armazena os valores de temperatura da leitura do MAX6675
            temperatura_real[t_atual] = temp;            
            t_atual++;

            //O estagio dura 120s e passa para o proximo estagio    
            if((t_atual - t_anterior)>240){
                ESP_LOGI(TAG, "Refluxo parte 1");
                modo_operacao = 3;
                t_anterior = t_atual;
            }
            break;
        //pre aquecimento do refluxo
        case 3:
            //temperatura_ideal armazena a temperatura que o perfil de temperatura deveria seguir
            temperatura_ideal[t_atual] = 195;
            temperatura_real[t_atual] = temp;
            t_atual++;
            //Manter a temp em 195
            setpoint = 195;   
            if((t_atual - t_anterior)>120){
                ESP_LOGI(TAG, "Refluxo parte 2");
                modo_operacao = 4;
                t_anterior = t_atual;
            }
            break;
        //Refluxo parte 2 - Aumentar a temperatura ate 240 
        case 4:
            //temperatura_ideal armazena a temperatura que o perfil de temperatura deveria seguir
            temperatura_ideal[t_atual] = 240;

            //temperatura_real armazena os valores de temperatura da leitura do MAX6675
            temperatura_real[t_atual] = temp;            
            t_atual++;
            //Temperatura alvo e 240 graus
            setpoint = 240;
            //Quando a temperatura atingir 240, passa p o proximo estagio
            if((temp)>(240-20)){
                ESP_LOGI(TAG, "Resfriamento");
                modo_operacao = 5;
                t_anterior=t_atual;
            }
            break;
        //Refluxo parte 3 - Manter a temperatura em 240  
        case 5:
            //Manter a temperatura em 240   
            setpoint = 240;
            //temperatura_ideal armazena a temperatura que o perfil de temperatura deveria seguir
            temperatura_ideal[t_atual] = 240;
            //temperatura_real armazena os valores de temperatura da leitura do MAX6675
            temperatura_real[t_atual] = temp;         
            t_atual++;
            //A duracao do estagio deve ser 30s. Se passa disso, vai para proximo estagio
            if(t_atual - t_anterior>60){
                ESP_LOGI(TAG, "Resfriamento");
                modo_operacao = 6;
                t_anterior = t_atual;
            }
            break;
        //Resfriamento - deixar ferro desligado
        case 6: 
            //Temperatura alvo
            setpoint = 0;
            //temperatura_ideal armazena a temperatura que o perfil de temperatura deveria seguir
            temperatura_ideal[t_atual] = temperatura_ideal[t_atual -1] - 2;
            //temperatura_real armazena os valores de temperatura da leitura do MAX6675
            temperatura_real[t_atual] = temp;
            t_atual++; 
            //A duracao do estagio deve ser ate esfriar, mas para espera somente 60s para fins praticos
            if(t_atual - t_anterior>240){
                //permite a execucao da tarefa printar_task
                xEventGroupSetBits(LD_event_group, PRINTAR_BIT);
                //Apaga esta tarefa (verifica_tempo)
                vTaskDelete(NULL);
            }
            
        }

        printf("Temperatura: %d \n", temp);
        printf("PID: %f \n", PID_Output);
        // espera por 0,5 segundo
        obterHoraLocal();
        
    }
}


/**
 * @brief Cacula a saida do PID e altera a largura do pulso do PWM.
 * 
 * @param pvParameters 
 */
void control_pwm(void *pvParameters)
{
    while (1)
    {
        // espera pelo bit 
        xEventGroupWaitBits(
            LD_event_group,         // grupo de eventos a ser esperado
            CONTROL_BIT,            // bit que está sendo esperado
            pdTRUE,                 // limpa os bits após a leitura
            pdFALSE,                // espera por um dos bits
            portMAX_DELAY           // tempo máximo para esperar os bits
        );

        //Calcula o erro
        erro = setpoint - temp;
        //Calculo do PID
        PID_P = kp * erro;
        PID_I = ((ki*T)/2)*(erro + erro_ant) + PID_Output_ant;
        PID_D = (kd*(2/T))*(erro - erro_ant) - PID_Output_ant;
        PID_Output = PID_P + PID_I + PID_D ;
        PID_Output_ant = PID_Output;
        erro_ant = erro;
        //Controla o PWM do relé com o valor de saido do PID
        rele_d_altera(PID_Output);  
    }
}
/**
 * @brief Realiza a leitura da temperatura
 *  
 * @param pvParameters 
 */
void task_read_temp(void *pvParameters)
{

    printf("Aquecendo...\n");
    
    while (1)
    {
      //Realiza a temperatura e armazena em temp  
      temp = readMax6675(spi);
      
      //Permite a ação das tarefas: control_pwm e verifica_tempo
      xEventGroupSetBits(LD_event_group, CONTROL_BIT);
    }
}

/**
 * @brief Inicializa o sensor MAX6675 e cria as tarefas
 * 
 * @return * void 
 */
void app_main() {
  
  /*Inicializa o MAX6675 e o barramento SPI*/
  max6675_set();

  /*Configura o pwm*/
  rele_pwm_set();
  /*Duty Cycle = 0*/
  rele_d_altera(0);

  /*Cria o evento*/
  LD_event_group = xEventGroupCreate();
  /*Limpo os bits utilizados*/
  xEventGroupClearBits(LD_event_group, CONTROL_BIT);
  xEventGroupClearBits(LD_event_group, TEMP_BIT);


  /*Cria tarefa para ler a temperatura*/
  xTaskCreate(task_read_temp, "read_temp", 2048, NULL, 5, NULL); 
  /*Cria tarefa para controle do pwm*/
  xTaskCreate(control_pwm, "control_pwm", configMINIMAL_STACK_SIZE * 3, NULL, 2, NULL);
  /*Cria tarefa que verifica o tempo para controlar o setpoint*/
  xTaskCreate(verifica_tempo, "verifica_tempo", configMINIMAL_STACK_SIZE * 3, NULL, 1, NULL);
  /*Cria a tarefa para printar as temperaturas durante o processo*/
  xTaskCreate(printar_task, "printar_task", configMINIMAL_STACK_SIZE * 3, NULL, 1, NULL);
  
  
  
  /*Evitar watchdog*/
  while(1) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }           

}




