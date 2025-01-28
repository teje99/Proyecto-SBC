idf_component_register(
	SRCS "control_de_aforo.c"
    	"pmod_oled_rgb.c"
    	"bme280_sensor.c"
    	#"mqtt_thingsboard.c"
    INCLUDE_DIRS "."
	EMBED_TXTFILES "certificado/telegram_cert.pem")
