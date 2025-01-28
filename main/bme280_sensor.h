extern float temperatura, humedad, presion;

void i2c_master_init();
void conf_Pir();

void bme280_reader(void *ignore);    /* void ignore*/
void pir_Movement();

