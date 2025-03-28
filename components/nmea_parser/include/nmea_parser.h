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

bool get_gprmc(const char *buffer, char *nmea_sentence);
bool valid_sentence_code(const char *nmea_sentence);
void fill_gps_raw_data(const char *buffer, raw_sentence_data_t *gps_raw_data);
bool validate_checksum(const char complete_sentence[128], const char checksum[4]);
void get_checksum(const char *buffer, char *checksum);

void treat_coordinates_data(raw_sentence_data_t gps_raw_data, gps_t *gps_data);
gps_time_t treat_time(raw_sentence_data_t gps_raw_data, gps_t *gps_data);
gps_date_t treat_date(raw_sentence_data_t gps_raw_data, gps_t *gps_data);
void fix_date_time(gps_time_t gps_time, gps_date_t gps_date, char *fixed_data);


