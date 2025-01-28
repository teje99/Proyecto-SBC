#include "esp_log.h"
#include "cJSON.h"
#include "mqtt_thingsboard.h"


static const char *TAG = "mqtt";
esp_mqtt_client_handle_t client;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}



void mqtt_init(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
       .broker.address.uri = "mqtt://demo.thingsboard.io/",
       // .event_handle = mqtt_event_handler,
       .broker.address.port = 1883,
       .credentials.username = "Token Thingsboard Device",
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_start(client);

}


// envia el valor de la luminosidad por mqtt.
void send_data(float temperatura, float humedad, float presion, int aforo, char *mensaje){	// sonido
    //ESP_LOGI(TAG, "La luminosidad es de %f!", luminosidad);
    
    cJSON *atributos = cJSON_CreateObject();
    cJSON_AddNumberToObject(atributos, "temperatura", temperatura);
    cJSON_AddNumberToObject(atributos, "humedad", humedad);
    cJSON_AddNumberToObject(atributos, "presion", presion);
    cJSON_AddNumberToObject(atributos, "aforo", aforo);
    
    char *post_attributes = cJSON_PrintUnformatted(atributos);
    esp_mqtt_client_publish(client, "v1/devices/me/attributes", post_attributes, 0, 1, 0);
    cJSON_Delete(atributos);
    free(post_attributes);
    cJSON *telemetria = cJSON_CreateObject();
    if (strcmp(mensaje, "")!=0){
    	if(mensaje[0]=='/'){
        	memmove(mensaje, mensaje + 1, strlen(mensaje));
        	cJSON_AddStringToObject(telemetria, "comando", mensaje);
        }
    }
    cJSON_AddNumberToObject(telemetria, "temperatura", temperatura);
    cJSON_AddNumberToObject(telemetria, "humedad", humedad);
    cJSON_AddNumberToObject(telemetria, "presion", presion);
    cJSON_AddNumberToObject(telemetria, "aforo", aforo);
    
    char *post_data = cJSON_PrintUnformatted(telemetria);
    esp_mqtt_client_publish(client, "v1/devices/me/telemetry", post_data, 0, 1, 0);
    cJSON_Delete(telemetria);
    
    // Free is intentional, it's client responsibility to free the result of cJSON_Print
    free(post_data);
    

}
