#include <stdint.h>

#define BLANCO	0xFFFF
#define NEGRO	0x0000

void oled_init(void);
void oled_clear(uint16_t color);
void write_line(uint8_t nLinea, char *texto, int valor);
