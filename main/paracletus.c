#include <stdio.h>
#include <stdlib.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "string.h"
#include "time.h"

#define RXD_PORT 20
#define TXD_PORT 21

typedef struct {
	uint8_t hour;
	uint8_t minute;
	uint8_t second;
} gps_time_t;

typedef struct {
	uint16_t year;
	uint8_t month;
	uint8_t day;
} gps_date_t;

typedef struct {
	float latitude;
	float longitude;
} gps_t;

typedef struct {
	char time_stamp[16];
	char validity[4];
	char latitude[16];
	char lat_direction[4];
	char longitude[16];
	char lng_direction[4];
	char date_stamp[16];
	char checksum[4];
} raw_sentence_data_t;

// UART
static void nmea_parser_start_gps_uart(void);
static void uart_data_income(void *);

// GPS
bool get_nmea_sentence(const char *buffer, char *nmea_sentence);
bool valid_sentence_code(const char *nmea_sentence);
void fill_gps_raw_data(const char *buffer, raw_sentence_data_t *gps_raw_data);

void treat_coordinates_data(raw_sentence_data_t gps_raw_data, gps_t *gps_data);
gps_time_t treat_time(raw_sentence_data_t gps_raw_data, gps_t *gps_data);
gps_date_t treat_date(raw_sentence_data_t gps_raw_data, gps_t *gps_data);
void fix_date_time(gps_time_t gps_time, gps_date_t gps_date, char *fixed_data);

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

	while(1) {
		gps_t* gps_data = malloc(sizeof(gps_t));
		raw_sentence_data_t gps_raw_data = { 0 };

		gps_time_t gps_time = { 0 };
		gps_date_t gps_date = { 0 };

		uart_get_buffered_data_len(0, (size_t *) &data_length);

		// em média as linhas válidas possuem ~40 caracteres
		if(data_length >= 40) {
			uint8_t len = uart_read_bytes(0, (char *) data, data_length, 100);

			if(len) {
				bool ret = get_nmea_sentence((const char *) data, gps_sentence);
				if(ret == true) {
					fill_gps_raw_data(gps_sentence, &gps_raw_data);

					treat_coordinates_data(gps_raw_data, gps_data);

					gps_time = treat_time(gps_raw_data, gps_data);
					gps_date = treat_date(gps_raw_data, gps_data);

					fix_date_time(gps_time, gps_date, valid_date_time);

					printf("(%s)\n", valid_date_time);
					printf("latitude: %f\n", gps_data->latitude);
					printf("longitude: %f\n", gps_data->longitude);
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

bool get_nmea_sentence(const char *buffer, char *nmea_sentence) {

	// procura pelo cifrão
	uint8_t dollar_sign_pos = 0;
	dollar_sign_pos = digit_pos(buffer, '$');

	// procura pelo asterístico
	uint8_t asterisk_pos = 0;
	asterisk_pos = digit_pos(buffer, '*');

	// se na sentença não tiver dados ou delimitador de início/fim, ignora a linha
	if(asterisk_pos <= dollar_sign_pos) {
		return false;
	}

	fill_buffer(buffer, nmea_sentence, dollar_sign_pos, asterisk_pos);

	if(valid_sentence_code(nmea_sentence) == true) {
		return true;
	}

	return false;
}

bool valid_sentence_code(const char *nmea_sentence) {
  char msg_tag[6] = { 0 };

  for (uint8_t i = 1; i <= 5; i++) {
    msg_tag[i - 1] = nmea_sentence[i];
  }

  const char valid_tags[6][6] = {
    "GNGGA", "GPGGA", "GNRMC", "GPRMC", "GPGLL", "GPZDA"
  };

  for (uint8_t i = 0; i < 4; i++) {
    if (strncmp(valid_tags[i], msg_tag, 6) == 0) {
		 return true;
    }
  }

  return false;
}

uint8_t get_data_in_pos(const char *buffer, uint8_t pos, char *data) {
	uint8_t comma_count = 0;

	// percorre até chegar no primeiro endereço do campo requerido
	uint8_t i = 0;
	if(pos > 0) {
		for(i = 0; i < 128; i++) {
			if(comma_count == pos) {
				break;
			}

			if(buffer[i] == ',') {
				comma_count++;
			}
		}
	}

	uint8_t bytes_read = 0;

	for(uint8_t j = i; j < 128; j++) {
		// TODO: se refatorar a lógica dessa condicional, não vai
		// ser necessário a função get_checksum();
		if(buffer[j] == '\0' || buffer[j] == '*') {
			break;
		}

		if(buffer[j] != ',') {
			data[j - i] = buffer[j];
			bytes_read++;
			continue;
		}

		break;
	}

	return bytes_read;
}

void get_checksum(const char *buffer, char *checksum) {
	uint8_t len = strnlen(buffer, 128);

	if(len == 128) {
		return;
	}

	if(len > 0) {
		checksum[0] = buffer[len - 2];
		checksum[1] = buffer[len - 1];
		checksum[2] = '\0';
	}
}

uint8_t get_dot_position(const char coord[16]) {
	uint8_t dot_position = 0;

	for(uint8_t i = 0; i < 16; i++) {
		if(coord[i] == '.') {
			dot_position = i;
		}
	}

	return dot_position;
}

void treat_coordinates_data(raw_sentence_data_t gps_raw_data, gps_t *gps_data) {

	uint8_t lat_dot_position = 0;
	uint8_t lng_dot_position = 0;
	uint8_t coord_len = 0;

	char degrees[4] = { 0 };
	float decimal_degrees = 0;

	char minutes[16] = { 0 };
	float decimal_minutes = 0;

	lat_dot_position = get_dot_position((const char *) gps_raw_data.latitude);
	coord_len = strnlen(gps_raw_data.latitude, 16);

	for(uint8_t i = 0; i < (lat_dot_position - 2); i++) {
		degrees[i] = gps_raw_data.latitude[i];
	}

	uint8_t j = 0;
	for(uint8_t i = (lat_dot_position - 2); i < coord_len; i++) {
		minutes[j] = gps_raw_data.latitude[i];
		j += 1;
	}

	decimal_degrees = strtod(degrees, NULL);
	decimal_minutes = strtod(minutes, NULL);

	gps_data->latitude = decimal_degrees + (decimal_minutes / 60);

	if(strncmp(gps_raw_data.lat_direction, "S", 4) == 0) {
		gps_data->latitude *= -1;
	}

	memset(degrees, '\0', 4);
	memset(minutes, '\0', 16);
	decimal_degrees = 0;
	decimal_minutes = 0;

	lng_dot_position = get_dot_position((const char *) gps_raw_data.longitude);
	coord_len = strnlen(gps_raw_data.longitude, 16);

	for(uint8_t i = 0; i < (lng_dot_position - 2); i++) {
		degrees[i] = gps_raw_data.longitude[i];
	}

	j = 0;
	for(uint8_t i = (lng_dot_position - 2); i < coord_len; i++) {
		minutes[j] = gps_raw_data.longitude[i];
		j += 1;
	}

	decimal_degrees = strtod(degrees, NULL);
	decimal_minutes = strtod(minutes, NULL);

	gps_data->longitude= decimal_degrees + (decimal_minutes / 60);

	if(strncmp(gps_raw_data.lng_direction, "W", 4) == 0) {
		gps_data->longitude *= -1;
	}

}

void fill_gps_raw_data(const char *buffer, raw_sentence_data_t *gps_raw_data) {
	char tag_id[8] = { 0 };

	get_data_in_pos(buffer, 0, tag_id);

	if(strncmp(tag_id, "$GPRMC", 8) == 0) {
		// TODO: refatorar, colocar essa seção em um loop
		get_data_in_pos(buffer, 1, gps_raw_data->time_stamp);
		get_data_in_pos(buffer, 2, gps_raw_data->validity);

		get_data_in_pos(buffer, 3, gps_raw_data->latitude);

		get_data_in_pos(buffer, 4, gps_raw_data->lat_direction);
		get_data_in_pos(buffer, 5, gps_raw_data->longitude);
		get_data_in_pos(buffer, 6, gps_raw_data->lng_direction);

		get_data_in_pos(buffer, 9, gps_raw_data->date_stamp);
		get_checksum(buffer, gps_raw_data->checksum);
	}

	return;
}

gps_time_t treat_time(raw_sentence_data_t gps_raw_data, gps_t *gps_data) {
	gps_time_t time = { 0 };
	char buffer[4] = { 0 };

	if (strnlen(gps_raw_data.time_stamp, 16) == 9) {
		buffer[0] = gps_raw_data.time_stamp[0];
		buffer[1] = gps_raw_data.time_stamp[1];

		time.hour = atoi(buffer);

		buffer[0] = gps_raw_data.time_stamp[2];
		buffer[1] = gps_raw_data.time_stamp[3];

		time.minute = atoi(buffer);

		buffer[0] = gps_raw_data.time_stamp[4];
		buffer[1] = gps_raw_data.time_stamp[5];

		time.second = atoi(buffer);
	}

	return time;
}

gps_date_t treat_date(raw_sentence_data_t gps_raw_data, gps_t *gps_data) {
	gps_date_t date = { 0 };
	char buffer[4] = { 0 };

	if (strnlen(gps_raw_data.date_stamp, 16) == 6) {
		buffer[0] = gps_raw_data.date_stamp[0];
		buffer[1] = gps_raw_data.date_stamp[1];

		date.day = atoi(buffer);

		buffer[0] = gps_raw_data.date_stamp[2];
		buffer[1] = gps_raw_data.date_stamp[3];

		date.month = atoi(buffer);

		buffer[0] = gps_raw_data.date_stamp[4];
		buffer[1] = gps_raw_data.date_stamp[5];

		date.year = atoi(buffer);
		date.year += 2000;
	}

	return date;
}

void fix_date_time(gps_time_t gps_time, gps_date_t gps_date, char *fixed_data) {
	time_t epoch;
	struct tm new_tm;
	struct tm *tm_ptr;

	struct tm tm = {
		.tm_year = gps_date.year - 1900,
		.tm_mon = gps_date.month - 1,
		.tm_mday =  gps_date.day,

		.tm_hour = gps_time.hour,
		.tm_min = gps_time.minute,
		.tm_sec = gps_time.second
	};

	epoch = mktime(&tm);
	// 3 horas = 10800 segundos
	// UTC-3
	epoch -= 10800;

	tm_ptr = localtime(&epoch);
	strftime(fixed_data, 32, "%Y-%m-%d %H:%M:%S", tm_ptr);
}
