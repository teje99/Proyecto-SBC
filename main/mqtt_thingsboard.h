#include "mqtt_client.h"

/* inicia el mqtt con thingsboard */
void mqtt_init(void);

/* envía los datos por mqtt a thingsboard */
void send_data(float temperatura, float humedad, float presion, int aforo, char *mensaje);
