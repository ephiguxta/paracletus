#include <stdio.h>
#include <stdlib.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "string.h"
#include "nmea_parser.h"
#include <time.h>
#include <json_generator.h>

// UART
static void nmea_parser_start_gps_uart(void);
static void uart_data_income(void *);

void gen_json(const gps_t *gps_data, const char date_time[32], char generated_json[128]);

void app_main(void)
{
	nmea_parser_start_gps_uart();
	xTaskCreate(uart_data_income, "uart_data_income", 4096, NULL, 10, NULL);
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

static void uart_data_income(void *arg) {
	size_t data_length = 0;

	uint8_t *data = (uint8_t *) malloc(128);

	char gps_sentence[128] = { 0 };
	char valid_date_time[32]= { 0 };

	char generated_json[128] = { 0 };

	while(1) {
		gps_t* gps_data = malloc(sizeof(gps_t));
		raw_sentence_data_t gps_raw_data = { 0 };

		gps_time_t gps_time = { 0 };
		gps_date_t gps_date = { 0 };

		uart_get_buffered_data_len(0, (size_t *) &data_length);

		// em média as linhas válidas possuem ~40 caracteres
		if(data_length >= 40) {
			uint8_t len = uart_read_bytes(0, (char *) data, 128, 200);
			uart_flush(0);


			if(len) {
				bool ret = get_gprmc((const char *) data, gps_sentence);

				if(ret == true) {
					fill_gps_raw_data(gps_sentence, &gps_raw_data);
					treat_coordinates_data(gps_raw_data, gps_data);

					gps_time = treat_time(gps_raw_data, gps_data);
					gps_date = treat_date(gps_raw_data, gps_data);

					fix_date_time(gps_time, gps_date, valid_date_time);

					gen_json(gps_data, valid_date_time, generated_json);
					printf("%s\n", generated_json);
				}

				memset(data, '\0', 128);
			}
		}

		vTaskDelay(50);
	}
}

void gen_json(const gps_t *gps_data, const char date_time[32], char generated_json[128]) {
	json_gen_str_t jstr;

	json_gen_str_start(&jstr, generated_json, 128, NULL, NULL);
	json_gen_start_object(&jstr);

	// TODO: esse dado deve ser pego do cartão sd
	json_gen_obj_set_string(&jstr, "placa", "ABC1234");

	json_gen_obj_set_float(&jstr, "latitude", gps_data->latitude);
	json_gen_obj_set_float(&jstr, "longitude", gps_data->longitude);
	json_gen_obj_set_string(&jstr, "data", date_time);

	json_gen_end_object(&jstr);
	json_gen_str_end(&jstr);

	return;
}
