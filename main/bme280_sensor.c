#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "sdkconfig.h"

#include "bme280.h"
#include "bme280_sensor.h"


/* sensor de humedad i2c, sensor de movimiento y luz led */
#define SDA_PIN GPIO_NUM_22
#define SCL_PIN GPIO_NUM_21
#define PIR_INPUT 2
#define LED_OUTPUT 4

#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1


#define TAG_BME280 "BME280"

// SemaphoreHandle_t Semaphore_1, Semaphore_2 = NULL;
float temperatura, humedad, presion;

/* inicia el bus i2c */
void i2c_master_init()
{
	i2c_config_t i2c_config = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = SDA_PIN,
		.scl_io_num = SCL_PIN,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 1000000
	};
	i2c_param_config(I2C_NUM_0, &i2c_config);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);        /* revisar */
}


/* inicia el pin del sensor de movimiento */
void conf_Pir(){
	gpio_config_t io_conf = {
		.pin_bit_mask = (1ULL << PIR_INPUT),    
		.mode = GPIO_MODE_INPUT,
		.pull_up_en = GPIO_PULLUP_DISABLE,
		.pull_down_en = GPIO_PULLUP_ENABLE,
		.intr_type = GPIO_INTR_DISABLE
	};
	gpio_config(&io_conf);

	/* pin de salida del led rojo*/
    gpio_reset_pin(LED_OUTPUT);
    gpio_set_direction(LED_OUTPUT, GPIO_MODE_OUTPUT);
}


/* funciones para interactuar con el sensor BME */
s8 BME280_I2C_bus_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
	s32 iError = BME280_INIT_VALUE;

	esp_err_t espRc;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, reg_addr, true);
	i2c_master_write(cmd, reg_data, cnt, true);
	i2c_master_stop(cmd);

	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	if (espRc == ESP_OK) {
		iError = SUCCESS;
	} else {
		iError = -20;
	}
	i2c_cmd_link_delete(cmd);

	return (s8)iError;
}

s8 BME280_I2C_bus_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
	
	s32 iError = BME280_INIT_VALUE;
	esp_err_t espRc;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, reg_addr, true);

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);

	if (cnt > 1) {
		i2c_master_read(cmd, reg_data, cnt-1, I2C_MASTER_ACK);
	}
	i2c_master_read_byte(cmd, reg_data+cnt-1, I2C_MASTER_NACK);
	i2c_master_stop(cmd);

	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	if (espRc == ESP_OK) {
		iError = SUCCESS;
	} else {
		iError = -20;
	}

	i2c_cmd_link_delete(cmd);

	return (s8)iError;
}


void BME280_delay_msek(u32 msek)
{
	vTaskDelay(msek/portTICK_PERIOD_MS);
}


/* realiza la recogida de datos del sensor BME */
void bme280_reader(void *ignore)
{
	// xSemaphoreTake(Semaphore_1, portMAX_DELAY);
	struct bme280_t bme280 = {
		.bus_write = BME280_I2C_bus_write,
		.bus_read = BME280_I2C_bus_read,
		.dev_addr = BME280_I2C_ADDRESS1,
		.delay_msec = BME280_delay_msek
	};

	s32 com_rslt;
	s32 v_uncomp_pressure_s32;
	s32 v_uncomp_temperature_s32;
	s32 v_uncomp_humidity_s32;

	com_rslt = bme280_init(&bme280);

	com_rslt += bme280_set_oversamp_pressure(BME280_OVERSAMP_16X);
	com_rslt += bme280_set_oversamp_temperature(BME280_OVERSAMP_2X);
	com_rslt += bme280_set_oversamp_humidity(BME280_OVERSAMP_1X);

	com_rslt += bme280_set_standby_durn(BME280_STANDBY_TIME_1_MS);
	com_rslt += bme280_set_filter(BME280_FILTER_COEFF_16);

	com_rslt += bme280_set_power_mode(BME280_NORMAL_MODE);
	if (com_rslt == SUCCESS) {
		while(true) {
			
			com_rslt = bme280_read_uncomp_pressure_temperature_humidity(
				&v_uncomp_pressure_s32, &v_uncomp_temperature_s32, &v_uncomp_humidity_s32);

			if (com_rslt == SUCCESS) {
				temperatura = bme280_compensate_temperature_double(v_uncomp_temperature_s32);
				humedad = bme280_compensate_humidity_double(v_uncomp_humidity_s32);
				presion = bme280_compensate_pressure_double(v_uncomp_pressure_s32)/100;
				
			} else {
				ESP_LOGE(TAG_BME280, "measure error. code: %d", com_rslt);
			}
			// xSemaphoreGive(Semaphore_1);
			vTaskDelay(3000 / portTICK_PERIOD_MS);
		}
	} else {
		ESP_LOGE(TAG_BME280, "init or setting error. code: %d", com_rslt);
	}
	//vTaskDelete(NULL);
}


/* función para el sensor de movimiento */
void pir_Movement(){
	// xSemaphoreTake(Semaphore_2, portMAX_DELAY);
	
    // conf_Pir();      // esto no debe ir aquí 
    // gpio_reset_pin(LED_OUTPUT);
    // gpio_set_direction(LED_OUTPUT, GPIO_MODE_OUTPUT);
	
    
    int PIR_State = 0;  /* pirState*/
	int val = 0;
	while(true){
		val=gpio_get_level(PIR_INPUT);
		if (val == 1){
			if (PIR_State == 0) {
				PIR_State = 1;
			}
			else{
				gpio_set_level(LED_OUTPUT, 1);
			}
		} else {
    		if (PIR_State == 1){
      			PIR_State = 0;
    		}
    		else{
				gpio_set_level(LED_OUTPUT, 0);
			}
    		   // xSemaphoreGive(Semaphore_2);
    	}
		
    	vTaskDelay(500 / portTICK_PERIOD_MS);             // valorar
    }
}




