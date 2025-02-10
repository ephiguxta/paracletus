#include <stdio.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"

#define RXD_PORT 5
#define TXD_PORT 4

typedef struct {
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
	uint16_t thousand;
} gps_time_t;

typedef struct {
	uint8_t year;
	uint8_t month;
	uint8_t day;
} gps_date_t;

typedef struct {
	float latitude;
	float longitude;
	gps_date_t date;
	gps_time_t time;
} gps_t;

static void nmea_parser_start_gps_uart(void);

void app_main(void)
{
	nmea_parser_start_gps_uart();
}

static void nmea_parser_start_gps_uart(void)
{
	uart_config_t uart_config = {
		.baud_rate = 9600,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};

	ESP_ERROR_CHECK(uart_driver_install(0, 256, 0, 0, NULL, 0));
	ESP_ERROR_CHECK(uart_param_config(0, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(0, TXD_PORT, RXD_PORT, -1, -1));
}
