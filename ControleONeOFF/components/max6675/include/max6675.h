#ifndef MAX6675_H
#define MAX6675_H

void max6675_set(void);
float readMax6675(spi_device_handle_t spi);
extern spi_device_handle_t spi;

#endif
