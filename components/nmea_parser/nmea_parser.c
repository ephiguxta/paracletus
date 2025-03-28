#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include "nmea_parser.h"

uint8_t digit_pos(const char *buffer, char target, uint8_t *pos) {
	// função de suporte pra retornar a posição de um dígito
	// passado no segundo parâmetro
	//

	uint8_t sign_pos = 0;

	for(uint8_t i = *pos; i < 128; i++) {
		if(buffer[i] == target) {
			sign_pos = i;
			break;
		}

		*pos = i;
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

bool get_gprmc(const char *buffer, char *nmea_sentence) {
	char checksum[4] = { '\0' };
	uint8_t read_buffer_pos = 0;

	// procura pelo cifrão
	uint8_t dollar_sign_pos = 0;
	dollar_sign_pos = digit_pos(buffer, '$', &read_buffer_pos);

	// procura pelo asterístico
	uint8_t asterisk_pos = 0;
	asterisk_pos = digit_pos(buffer, '*', &read_buffer_pos);

	if(dollar_sign_pos > asterisk_pos) {
		return false;
	}

	if(valid_sentence_code(&buffer[dollar_sign_pos]) == true) {
		// caso for uma sentença completa
		if((asterisk_pos > 0) && (read_buffer_pos + 3 < 128)) {

			checksum[0] = buffer[read_buffer_pos + 2];
			checksum[1] = buffer[read_buffer_pos + 3];

			char complete_sentence[128];

			for(uint8_t i = dollar_sign_pos; i <= (asterisk_pos + 2); i++) {
				complete_sentence[i - dollar_sign_pos] = buffer[i];
			}

			complete_sentence[asterisk_pos + 3] = '\0';

			if(validate_checksum(complete_sentence, checksum) == true) {
				fill_buffer(buffer, nmea_sentence, dollar_sign_pos, asterisk_pos);
				return true;
			}
		}
	}

	return false;
}

bool validate_checksum(const char complete_sentence[128], const char checksum[4]) {
	uint8_t sentence_len = strnlen(complete_sentence, 128);

	uint16_t int_sentence_checksum = 0;
	char sentence_checksum[4];

	get_checksum(complete_sentence, sentence_checksum);

	uint16_t int_gen_checksum = 0;
	for(uint8_t i = 1; i < (sentence_len - 3); i++) {
		int_gen_checksum ^= complete_sentence[i];
	}

	int_sentence_checksum = strtol(sentence_checksum, NULL, 16);

	if(int_sentence_checksum == int_gen_checksum) {
		return true;
	}

	return false;
}

bool valid_sentence_code(const char *nmea_sentence) {
  char msg_tag[6] = { 0 };

  for (uint8_t i = 1; i <= 5; i++) {
    msg_tag[i - 1] = nmea_sentence[i];
  }

  if(strncmp("GPRMC", msg_tag, 6) == 0) {
	  return true;
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
	struct tm *tm_ptr;
	struct tm tm;

	if (gps_date.year == 0 || (gps_time.hour == 0 && gps_time.minute == 0)) {
		tm.tm_year = 70;
		tm.tm_mon = 0;
		tm.tm_mday = 1;

		tm.tm_hour = 0;
		tm.tm_min = 0;
		tm.tm_sec = 0;

		epoch = mktime(&tm);

	} else {
		tm.tm_year = gps_date.year - 1900;
		tm.tm_mon = gps_date.month - 1;
		tm.tm_mday =  gps_date.day;

		tm.tm_hour = gps_time.hour;
		tm.tm_min = gps_time.minute;
		tm.tm_sec = gps_time.second;

		epoch = mktime(&tm);

		// 3 horas = 10800 segundos
		// UTC-3
		epoch -= 10800;
	}

	tm_ptr = localtime(&epoch);
	strftime(fixed_data, 32, "%Y-%m-%d %H:%M:%S", tm_ptr);
}


