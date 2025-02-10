#include <stdio.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "string.h"

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

// UART
static void nmea_parser_start_gps_uart(void);
static void uart_data_income(void *);

// GPS
esp_err_t get_nmea_sentence(const char *buffer, char *nmea_sentence);

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

	while(1) {
		uart_get_buffered_data_len(0, (size_t *) &data_length);

		// em média as linhas válidas possuem ~40 caracteres
		if(data_length >= 40) {
			uint8_t len = uart_read_bytes(0, (char *) data, data_length, 100);

			if(len) {
				esp_err_t ret = get_nmea_sentence((const char *) data, gps_sentence);
				if(ret == ESP_OK) {
					printf("(%s)\n", gps_sentence);
				}

				memset(data, '\0', 128);
			}
		}

		vTaskDelay(50);
	}
}

uint8_t digit_pos(const char *buffer, char target) {
	// função de suporte pra retornar a posição de um dígito
	// passado no segundo parâmetro
	//

	uint8_t sign_pos = 0;

	for(uint8_t i = 0; i < 128; i++) {
		if(buffer[i] == target) {
			sign_pos = i;
			break;
		}
	}

	return sign_pos;
}

void fill_buffer(const char *buffer, char *nmea_sentence, uint8_t begin, uint8_t end) {
	for(uint8_t i = begin; i <= (end + 2); i++) {
		nmea_sentence[i - begin] = buffer[i];
	}

	// pula os dois dígitos de checksum e insere '\0'
	nmea_sentence[end + 3] = '\0';
}

esp_err_t get_nmea_sentence(const char *buffer, char *nmea_sentence) {

	// procura pelo cifrão
	uint8_t dollar_sign_pos = 0;
	dollar_sign_pos = digit_pos(buffer, '$');

	// procura pelo asterístico
	uint8_t asterisk_pos = 0;
	asterisk_pos = digit_pos(buffer, '*');

	// se na sentença não tiver dados ou delimitador de início/fim, ignora a linha
	if(asterisk_pos <= dollar_sign_pos) {
		return ESP_FAIL;
	}

	fill_buffer(buffer, nmea_sentence, dollar_sign_pos, asterisk_pos);

	return ESP_OK;
}
