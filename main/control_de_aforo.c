#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_sleep.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt_thingsboard.h"
#include "mqtt_thingsboard.c"


#include "driver/gpio.h"
#include "driver/adc.h"
#include "hal/adc_types.h"

#include "pmod_oled_rgb.h"
#include "bme280_sensor.h"
#include "portmacro.h"

#include "softap_sta.c"


#define TAG "CDA"

/* IR */
#define PIN_IR_1 34
#define PIN_IR_2 35

/* sensor de sonido */
#define SOUND_ADC_CHANNEL   ADC_CHANNEL_5     /* sensor de sonido = gpio 33 */

/* barra de leds */
#define TOTAL_LEDS_BARRA    8      /* numero de leds en la barra de sonido */
const gpio_num_t led_pins[TOTAL_LEDS_BARRA] = {13, 12, 14, 27, 26, 25, 32, 15};   /* revisar el 0 y el 2 los usa el BME*/


/* global variables */
int aforo = 0;


/* init pin sonido */
void init_gpio_adc(void)    
{
    ESP_LOGI(TAG, "Inicializando ADC...");
    adc1_config_width(ADC_WIDTH_BIT_12);         // Configurar la resoluciÃ³n del ADC
    adc1_config_channel_atten(SOUND_ADC_CHANNEL, ADC_ATTEN_DB_0); // Configurar la atenuaciÃ³n. Ver otra opciÃ³n como 12.
}


/* init pines de la barra de leds */
void init_gpios_barra_sonido(void)  
{
    for (int i=0; i<TOTAL_LEDS_BARRA; i++)
    {
        gpio_reset_pin(led_pins[i]);
        gpio_set_direction(led_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(led_pins[i], 0);
    }
}


/* init pines IR */
void configure_IR(void)
{
    gpio_reset_pin(PIN_IR_1);
	gpio_reset_pin(PIN_IR_2);
    gpio_set_direction(PIN_IR_1, GPIO_MODE_INPUT);
    gpio_set_direction(PIN_IR_2, GPIO_MODE_INPUT);
}


/* enciende o apaga los leds de la barra de sonido */
void actualizar_barra_sonido(int n_on)
{
    for (int i=0; i<n_on; i++)
    {
        gpio_set_level(led_pins[i], 1);
    }

    for (int i=n_on+1; i<TOTAL_LEDS_BARRA; i++)
    {
        gpio_set_level(led_pins[i], 0);
    }

}


/* tarea que actualiza la pantalla cada 5s */
/* tambiÃ©n envÃ­a la informaciÃ³n a thingsboard */
void actualizar_pantalla_task()
{
	oled_clear(NEGRO);
    while (1)
    {
        write_line(0, "tempe = ", temperatura);
        write_line(1, "humed = ", humedad);
        write_line(2, "press = ", presion);

        write_line(7, "aforo = ", aforo);
        
        //send_data(temperatura, humedad, presion, aforo);
        
        vTaskDelay(5000 / portTICK_PERIOD_MS);  /* 5s */
    }
    
}


/* Tarea para actualizar el valor del sonido en la barra de leds. */
void mostrar_sonido_task()
{
    int adc_reading;    /* valor del sensor de sonido */

    while(1)
    {
        adc_reading = adc1_get_raw(SOUND_ADC_CHANNEL) / 4;  // 0-1023
        ESP_LOGI(TAG, "sonido: %d : %d", adc_reading, (adc_reading*10)/1023);
        actualizar_barra_sonido((adc_reading*10)/1023);
        vTaskDelay(100 / portTICK_PERIOD_MS);  /* 100 ms */
    }
}


void control_aforo_task()
{
	int estado = 0;
	int aux = 0;
    while(1){
        switch (estado) 
        {
            case 0:
                if(gpio_get_level(PIN_IR_1) == 0) //"1" en sensor_1 -> suma?
                {
                    ESP_LOGI(TAG,"+1?");
                    estado = 1;                    
                    ESP_LOGI(TAG, "estado = %d", estado);
                }
                else if (gpio_get_level(PIN_IR_2) == 0) //"1" en sensor_2 -> resta?
                {
                    ESP_LOGI(TAG,"-1?");
                    estado = 2;                    
                    ESP_LOGI(TAG, "estado = %d", estado);
                }
                
				vTaskDelay(100/ portTICK_PERIOD_MS);
                break;
                
            case 1:
                if (aux >= 50)
                {
					estado = 0;
					//ESP_LOGI(TAG, "RESET: estado = %d", estado);
					aux = 0;
					break;
				}
            	if(gpio_get_level(PIN_IR_2) == 0)
	            { //"1" en sensor_2 -> suma!
	                aforo = aforo + 1;
	                ESP_LOGI(TAG, "+1!!");
	                ESP_LOGI(TAG, "aforo = %d", aforo);
	                estado = 3;
	                aux = 0;		           	               
	                ESP_LOGI(TAG, "estado = %d", estado);
	            }
	            
                vTaskDelay(100 / portTICK_PERIOD_MS);		
                aux ++;
                break;
                
            case 2:
            	if (aux >= 50)
                {
					estado = 0;
					//ESP_LOGI(TAG, "RESET: estado = %d", estado);
					aux = 0;
					break;
				}
                if(gpio_get_level(PIN_IR_1) == 0)
                { //"1" en sensor_1 -> resta!
                    if(aforo > 0) aforo = aforo - 1;
                    ESP_LOGI(TAG,"-1!!");
                    ESP_LOGI(TAG, "aforo = %d", aforo);                    
                    estado = 3;
                    aux = 0;
                    ESP_LOGI(TAG, "estado = %d", estado);

                }
                vTaskDelay(100 / portTICK_PERIOD_MS);
                aux ++;
                break;
                
            case 3:
            	if(gpio_get_level(PIN_IR_1) == 1 && gpio_get_level(PIN_IR_2) == 1)
            	{
					estado = 0;
					ESP_LOGI(TAG, "estado = %d", estado);
				}
				vTaskDelay(100 / portTICK_PERIOD_MS);
				break;
				
            default:
                estado = 0;
                vTaskDelay(100 / portTICK_PERIOD_MS);
                break;	
        }
	}
}


void send_data_thingsboard()
{
    while(1)
    {
		esp_wifi_set_ps(WIFI_PS_NONE);
		vTaskDelay(100/portTICK_PERIOD_MS);
		char mensaje[100]="";
		ESP_LOGI(TAG, "Leyendo TELEGRAM");
		getTelegramMessage(mensaje, sizeof(mensaje));
        send_data(temperatura, humedad, presion, aforo, mensaje);    // aforo
        vTaskDelay(3000/portTICK_PERIOD_MS);
        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
        
    }

}


void app_main(void)
{
	sfotap_on();
    init_gpio_adc();    // pin del microfono
    init_gpios_barra_sonido();  // pines de la barra de leds
    oled_init();    // pines de la pantalla
    configure_IR();     // pines de los sensores IR;
    i2c_master_init();
    conf_Pir();
    
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);
    



    mqtt_init();

	/* tareas con prioridad 10 (un menor numero es menos prioridad) */
    xTaskCreate(actualizar_pantalla_task, "actualizar_pantalla_task", 2048, NULL, 6, NULL);    /* 5 s */
    xTaskCreate(mostrar_sonido_task, "mostrar_sonido_task", 2048, NULL, 5, NULL);      /* 100 ms */
    xTaskCreate(control_aforo_task, "control_aforo_task", 2048, NULL, 10, NULL);        /* 100 ms */
    
    /* sensor de movimiento y el BME280 */
    xTaskCreate(bme280_reader, "bme280_reader",  2048, NULL, 8, NULL);     /* 3 s */
	xTaskCreate(pir_Movement, "pir_Movement",  2048, NULL, 7, NULL);       /* 500 ms */
	xTaskCreate(send_data_thingsboard, "send_data_thingsboard",  4096, NULL, 9, NULL);       /* 5 s */

}
