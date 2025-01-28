#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_wifi_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include <esp_http_client.h>
#include <string.h>
#include "cJSON.h"







#define TAG "CDA"
#define MIN(i, j) (((i) < (j)) ? (i) : (j))

extern const char server_cert_pem_start[] asm("_binary_telegram_cert_pem_start");
extern const char server_cert_pem_end[] asm("_binary_telegram_cert_pem_end");
static uint32_t lastUpdateId = 0;

wifi_config_t wifi_init_softap();
httpd_handle_t start_webserver(void);
// Variables para almacenar el SSID y la contraseÃ±a ingresados por el usuario
char wifi_ssid[32] = {0};
char wifi_password[64] = {0};
int conectar =0;
int configurado=0;





void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(TAG, "Conectado a Wi-Fi");
    }
}

void url_decode(char *src, char *dest) {
    char *p = src;
    char code[3] = {0}; // Buffer para los cÃ³digos hexadecimales
    while (*p) {
        if (*p == '%') {
            strncpy(code, p + 1, 2); // Extraer los dos caracteres hexadecimales
            *dest++ = (char)strtol(code, NULL, 16); // Convertir a un carÃ¡cter
            p += 3; // Avanzar 3 caracteres
        } else if (*p == '+') {
            *dest++ = ' '; // Convertir '+' a espacio
            p++;
        } else {
            *dest++ = *p++; // Copiar carÃ¡cter tal cual
        }
    }
    *dest = '\0'; // Terminar la cadena
}

wifi_config_t wifi_init_sta(void) {
	
      // Obtener la info de la AP a la que estÃ¡ conectado
    esp_netif_create_default_wifi_sta();
		wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = ""
        	},
    	};
    return wifi_config;
    }
 void conect_sta(wifi_config_t config){
	wifi_ap_record_t ap_info;
	ESP_LOGI(TAG, "Conectandose a Wifi: %s con contrasena %s desde formulario", wifi_ssid, wifi_password);
    strncpy((char*)config.sta.ssid, wifi_ssid, sizeof(config.sta.ssid) - 1);   // Copiar el SSID a la estructura
	strncpy((char*)config.sta.password, wifi_password, sizeof(config.sta.password) - 1);  // Copiar la contraseÃ±a

	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_set_config(WIFI_IF_STA, &config);
    esp_wifi_start();
    esp_wifi_connect();
   	vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_wifi_sta_get_ap_info(&ap_info);
	if (ap_info.rssi != 0) {
        configurado=2;
        ESP_LOGI(TAG, "EXITO al conectarse potencia de la senal: %i", ap_info.rssi);
    }else{
		ESP_LOGE(TAG, "FALLO al conectarse");
		configurado=0;
		conectar=0;
	}

}

void conect_sta_credentials(wifi_config_t config, char* ssid, char* password){
	wifi_ap_record_t ap_info;
	ESP_LOGI(TAG, "Conectandose a Wifi: %s con contrasena %s desde NVS", ssid, password);
    strncpy((char*)config.sta.ssid, ssid, sizeof(config.sta.ssid) - 1);   // Copiar el SSID a la estructura
	strncpy((char*)config.sta.password, password, sizeof(config.sta.password) - 1);  // Copiar la contraseÃ±a

	esp_wifi_set_mode(WIFI_MODE_STA);
	esp_wifi_set_config(WIFI_IF_STA, &config);
    esp_wifi_start();
    esp_wifi_connect();
   	vTaskDelay(5000 / portTICK_PERIOD_MS);
    esp_wifi_sta_get_ap_info(&ap_info);
	if (ap_info.rssi != 0) {
        configurado=3;
        ESP_LOGI(TAG, "EXITO al conectarse potencia de la senal: %i", ap_info.rssi);
    }else{
		configurado=0;
		conectar=0;
		ESP_LOGE(TAG, "FALLO al conectarse");
	}

}


// Manejador del formulario (para recibir SSID y contraseÃ±a)
esp_err_t handle_form_post(httpd_req_t *req) {
    // Buffer para recibir los datos del formulario
    char buf[100];
    char aux[64] = {0};
    int ret, len = req->content_len;
	int remaining = req->content_len;
    
    while (remaining > 0) {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                httpd_resp_send_408(req);
            }
            return ESP_FAIL;
        }
        remaining -= ret;
        buf[len]='\0';
        // Parsear los datos recibidos (asumiendo que vienen como URL-encoded)
        sscanf(buf, "ssid=%31[^&]&password=%63s", wifi_ssid, aux);
	}
		
        
    
	url_decode(aux, wifi_password);
    ESP_LOGI(TAG, "SSID recibido: %s", wifi_ssid);
    ESP_LOGI(TAG, "Password recibido: %s", wifi_password);
    
     if (wifi_ssid[0] != '\0' && wifi_password[0] != '\0') {
        conectar = 1;  // Cambiar la variable global conectar solo cuando se recibieron los datos correctamente
        ESP_LOGI(TAG, "Conectar a la red Wi-Fi.");
	}
    // Responder con una confirmaciÃ³n al usuario
    const char* resp_str = "Datos recibidos, conectando al Wi-Fi...";
    httpd_resp_send(req, resp_str, strlen(resp_str));
	
    // Intentar conectar a la red Wi-Fi con los datos recibidos
    
	
    return ESP_OK;
}

// PÃ¡gina HTML con el formulario para que el usuario ingrese el SSID y la contraseÃ±a
esp_err_t handle_root_get(httpd_req_t *req) {
    const char* html_form = "<!DOCTYPE html><html><body>"
                            "<h2>ConfiguraciÃ³n Wi-Fi</h2>"
                            "<form action=\"/connect\" method=\"POST\">"
                            "SSID: <input type=\"text\" name=\"ssid\"><br>"
                            "Password: <input type=\"password\" name=\"password\"><br>"
                            "<input type=\"submit\" value=\"Conectar\">"
                            "</form></body></html>";

    httpd_resp_send(req, html_form, strlen(html_form));
    return ESP_OK;
}

// Iniciar el servidor web
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        // Registrar manejador para la pÃ¡gina principal (GET)
        httpd_uri_t uri_get = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = handle_root_get,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_get);

        // Registrar manejador para recibir el formulario (POST)
        httpd_uri_t uri_post = {
            .uri = "/connect",
            .method = HTTP_POST,
            .handler = handle_form_post,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_post);
    }
    
  
    return server;
}


// Configurar el ESP32 como Access Point
wifi_config_t wifi_init_softap(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "wifi_grM02",
            .ssid_len = strlen("wifi_grM02"),
            .channel = 1,
            .password = "12345678",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    return wifi_config;
}
    /*if (strlen((char*)(wifi_config.ap.password)) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }*/
void conect_ap(wifi_config_t config){
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &config);
    esp_wifi_start();

    ESP_LOGI(TAG, "ESP32 configurado como Access Point con SSID: ESP32_AP");
    
    configurado =1;
    

}

void getTelegramMessage(char *buffer, size_t buffer_size) {
	char *output_buffer= malloc(4096);   // Buffer to store response of http request
	int content_length = 0;
    char url[256];
    snprintf(url, sizeof(url), "https://api.telegram.org/bot"TU TOKEN DE TELEGRAM"/getUpdates?offset=%u", (unsigned int)(lastUpdateId+1));

    esp_http_client_config_t config = {
        .url = url,
		.cert_pem = server_cert_pem_start,
		.method = HTTP_METHOD_GET,
		.timeout_ms = 10000,
		.event_handler = NULL,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        }else{
            content_length = esp_http_client_fetch_headers(client);
            if (content_length < 0) {
                ESP_LOGE(TAG, "HTTP client fetch headers failed");
            } else {
                esp_http_client_read_response(client, output_buffer, 4096);
            }
        }
        esp_http_client_close(client);


        // Parsear el JSON
        cJSON *root = cJSON_Parse(output_buffer);
        if (root == NULL) {
            ESP_LOGE(TAG, "Error al parsear el JSON");
            free(output_buffer);
            esp_http_client_cleanup(client);
        }
            
        // Extraer la información del mensaje (ejemplo simplificado)
        cJSON *result = cJSON_GetObjectItem(root, "result");
        if (cJSON_IsArray(result)) {
            cJSON *last_update = cJSON_GetArrayItem(result, cJSON_GetArraySize(result) - 1);
            if (last_update!=NULL){
                cJSON *message = cJSON_GetObjectItem(last_update, "message");
                if (message != NULL) {
                    cJSON *text = cJSON_GetObjectItem(message, "text");                      
                    if (text != NULL) {
                        strncpy(buffer, text->valuestring, buffer_size - 1);
                        ESP_LOGI(TAG, "Mensaje recibido: %s", text->valuestring);
                        // Actualizar lastUpdateId
                        lastUpdateId = cJSON_GetObjectItem(last_update, "update_id")->valueint;
                    }
                }
            }else{
                ESP_LOGI(TAG, "No hay mensaje nuevo");
            }
        }
        cJSON_Delete(root);
        free(output_buffer);
        
    } else{
        ESP_LOGE(TAG, "Error en la solicitud HTTP: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// Conectar a la red Wi-Fi con los datos recibidos


// FunciÃ³n principal

void sfotap_on(void) {
	
	esp_err_t err = nvs_flash_init();
    
	wifi_config_t config_ap=wifi_init_softap();
	wifi_config_t config_sta=wifi_init_sta();
	
	nvs_handle_t nvs_handle;
	esp_err_t res =ESP_OK;
    err = nvs_open("Wifi", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
         ESP_LOGE(TAG, "Error al abrir NVS: %s", esp_err_to_name(err));
    }
   	
    nvs_iterator_t it = NULL;
    nvs_entry_find_in_handle(nvs_handle, NVS_TYPE_STR, &it);
    if (it == NULL) {
        ESP_LOGI(TAG, "No se encontraron credenciales WiFi almacenadas");
        res=ESP_ERR_NVS_NOT_FOUND;
    }
	
    while (res == ESP_OK&&configurado==0) {
		
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
		
        char value[128]; // Para leer el valor asociado a la clave
        size_t value_len = sizeof(value);
        
        

        // Intentar leer el valor de la clave
        err = nvs_get_str(nvs_handle, info.key, value, &value_len);
        if (err == ESP_OK) {
			ESP_LOGI(TAG, "Clave: '%s' Contrasena: %s", info.key, value);
            conect_sta_credentials(config_sta ,info.key, value);
        } else {
            ESP_LOGE(TAG, "Error al leer clave '%s': %s", info.key, esp_err_to_name(err));
        }

  
        
        res = nvs_entry_next(&it);
    }
	nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Fin de la lista de credenciales WiFi");
	nvs_release_iterator(it);
    
    
    while(1){
		vTaskDelay(10 / portTICK_PERIOD_MS);
		if(conectar==0&&configurado==0){
			conect_ap(config_ap);  // Iniciar el Access Point
			start_webserver();
			
		}
		else{
			if(conectar==1&&configurado<2){
				conect_sta(config_sta);
			}
		}
		if(configurado==2){
			nvs_open("Wifi", NVS_READWRITE,&nvs_handle);
			nvs_set_str(nvs_handle, wifi_ssid, wifi_password);
			ESP_LOGI(TAG, "Escribiendo Credenciales");
			nvs_close(nvs_handle);
			configurado=3;
		}
		if(configurado==3){
			ESP_LOGI(TAG, "Desea borrar la lista de credenciales? Y/N");
			vTaskDelay(100 / portTICK_PERIOD_MS);

            // Leer la entrada del usuario desde el monitor serie
            char eleccion = 0;
            while (eleccion != 'Y' && eleccion != 'y' && eleccion != 'N' && eleccion != 'n') {
                // Usamos la funciÃ³n 'scanf' del monitor serie
                if (scanf("%c", &eleccion) > 0) {
                    // Validar la entrada
                    if (eleccion == 'Y' || eleccion == 'y') {
                        ESP_LOGI(TAG, "Borrando credenciales...");
                        nvs_flash_erase();  // Borrar NVS
                        ESP_LOGI(TAG, "Credenciales borradas.");
                        configurado=0;
                        conectar=0;
                        nvs_flash_init();
                    } else if (eleccion == 'N' || eleccion == 'n') {
                        ESP_LOGI(TAG, "No se borraron las credenciales.");
                        configurado=4;
                    } else {
                        ESP_LOGI(TAG, "Entrada invlida, ingrese Y o N.");
                    }
                }
                // Esperar brevemente para evitar que el bucle consuma todos los recursos
                vTaskDelay(10 / portTICK_PERIOD_MS);
            }
            
		}
		if(configurado==4){
			return;
			}
	}
}



