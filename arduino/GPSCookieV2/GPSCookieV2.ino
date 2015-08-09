/*
 * GPS Cookie Firmware v2
 * Copyright (C) 2015  Alexander Steffen

 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <SD.h>
#include <SPI.h>
#include <SoftwareSerial.h>
#include <avr/power.h>
#include <avr/sleep.h>

#define PIN_GPS 2
#define PIN_LED_RED 3
#define PIN_LED_GREEN 4
#define PIN_SD 10


enum
{
	ERROR_SD_INITIALIZE = 1,
	ERROR_SD_READ,
	ERROR_SD_WRITE,
	ERROR_GPS_STARTUP_COMMAND,
};


SoftwareSerial GPS(0, 1);
File logfile = File();
bool gpsfix = false;
uint16_t utc_date = FAT_DEFAULT_DATE;
uint16_t utc_time = FAT_DEFAULT_TIME;


static void signal_error(uint8_t error)
{
	while (true)
	{
		digitalWrite(PIN_LED_GREEN, LOW);
		delay(500);

		for (uint8_t i = 0; i < error; i++)
		{
			digitalWrite(PIN_LED_RED, HIGH);
			delay(500);
			digitalWrite(PIN_LED_RED, LOW);
			delay(500);
		}

		digitalWrite(PIN_LED_GREEN, HIGH);
		delay(500);
	}
}

static char *find_field(char *buffer, uint8_t index)
{
	while (index)
	{
		if (*(buffer++) == ',')
		{
			index--;
		}
	}

	return (*buffer != ',') ? buffer : NULL;
}

static inline uint8_t parse_digits(char *buffer)
{
	return (buffer[0] - '0') * 10 + (buffer[1] - '0');
}

static inline uint8_t parse_hex(char *buffer)
{
	return (buffer[0] - (isdigit(buffer[0]) ? '0' : '7')) * 16 + (buffer[1] - (isdigit(buffer[1]) ? '0' : '7'));
}

static char xor_buffer(char *buffer, uint8_t length)
{
	char result = 0;

	while (length--)
	{
		result ^= *(buffer++);
	}

	return result;
}

static void file_date_time(uint16_t* date, uint16_t* time)
{
	*date = utc_date;
	*time = utc_time;
}

void setup()
{
	// configure LEDs
	pinMode(PIN_LED_RED, OUTPUT);
	pinMode(PIN_LED_GREEN, OUTPUT);
	digitalWrite(PIN_LED_GREEN, HIGH);

	// disable unused peripherals to save power
	ADCSRA = 0;
	power_all_disable();

	// enable necessary peripherals
	power_timer0_enable();
	power_spi_enable();
	power_usart0_enable();
	pinMode(PIN_SD, OUTPUT);
	pinMode(PIN_GPS, OUTPUT);
	digitalWrite(PIN_GPS, HIGH);

#ifdef DEBUG
	Serial.begin(9600);
	Serial.println("GPS Cookie v2");
#endif

	// initialize SD card
	SdFile::dateTimeCallback(file_date_time);
	if (!SD.begin(PIN_SD))
	{
		signal_error(ERROR_SD_INITIALIZE);
	}

	// initialize GPS module
	GPS.begin(9600);
	File startup = SD.open("startup.txt", O_READ);
	if (startup)
	{
		char buffer[256];
		uint8_t mode = 0;
		int16_t pos = 0;
		int16_t size = startup.read(buffer, sizeof(buffer) - 4);

		while (pos < size)
		{
			switch (mode)
			{
				case 0:
				{
					// looking for the start of a command
					if (buffer[pos] == '$')
					{
						memmove(buffer, buffer + pos, size - pos);
						size -= pos;
						pos = 0;
						mode = 1;
						continue;
					}
					else if (!isspace(buffer[pos]))
					{
						mode = 2;
					}
					break;
				}
				case 1:
				{
					// looking for the end of a command
					if (isspace(buffer[pos]))
					{
						if (pos < 3)
						{
							// not enough data -> skip garbage until next line
							mode = 2;
							continue;
						}

						if (buffer[pos - 3] != '*')
						{
							// add space for missing checksum
							memmove(buffer + pos + 3, buffer + pos, size - pos - 1);
							buffer[pos] = '*';
							pos += 3;
							size += 3;
						}

						// recalculate checksum
						sprintf(buffer + pos - 2, "%02X", xor_buffer(buffer + 1, pos - 4));
						buffer[pos] = 0;
#ifdef DEBUG
						Serial.println(buffer);
#endif
						GPS.println(buffer);
						memmove(buffer, buffer + pos + 1, size - pos - 1);
						size -= pos + 1;
						pos = 0;
						mode = 0;
						continue;
					}
					break;
				}
				case 2:
				{
					// consuming garbage until the end of the line
					if (buffer[pos] == '\r' || buffer[pos] == '\n')
					{
						mode = 0;
					}
					break;
				}
			}

			pos++;

			if (pos == size)
			{
				if (mode != 1)
				{
					pos = 0;
					size = startup.read(buffer, sizeof(buffer) - 4);
				}
				else
				{
					signal_error(ERROR_GPS_STARTUP_COMMAND);
				}
			}
		}

		if (size < 0)
		{
			signal_error(ERROR_SD_READ);
		}
	}

	digitalWrite(PIN_LED_GREEN, LOW);
}

void loop()
{
	// sleep until UART activity
	set_sleep_mode(SLEEP_MODE_IDLE);
	power_timer0_disable();
	power_spi_disable();
	sleep_enable();
	sleep_mode();
	sleep_disable();
	power_spi_enable();
	power_timer0_enable();

	digitalWrite((gpsfix && logfile) ? PIN_LED_GREEN : PIN_LED_RED, HIGH);

	while (GPS.available() && GPS.read() == '$')
	{
		char buffer[128];
		uint8_t index = 0;

		// read entire message into buffer
		buffer[index] = '$';
		do
		{
			if (index < sizeof(buffer) - 1)
			{
				index++;
			}

			while (!GPS.available()) ;

			buffer[index] = GPS.read();
		}
		while (buffer[index] != '\n');

		// verify checksum
		if (buffer[index - 4] != '*' || xor_buffer(buffer + 1, index - 5) != parse_hex(buffer + index - 3))
		{
			continue;
		}

		// parse interesting messages
		if (strstr(buffer, "$GPRMC"))
		{
			char *status = find_field(buffer, 2);
			gpsfix = (status && *status == 'A');

			if (gpsfix)
			{
				char *time = find_field(buffer, 1);
				char *date = find_field(buffer, 9);

				if (date && time)
				{
					utc_date = FAT_DATE(2000 + parse_digits(date + 4), parse_digits(date + 2), parse_digits(date));
					utc_time = FAT_TIME(parse_digits(time), parse_digits(time + 2), parse_digits(time + 4));

					if (!logfile)
					{
						char filename[] = "20YYMMDD.log";
						memcpy(filename + 2, date + 4, 2);
						memcpy(filename + 4, date + 2, 2);
						memcpy(filename + 6, date, 2);

#ifdef DEBUG
						Serial.println(filename);
#endif

						logfile = SD.open(filename, O_WRITE | O_APPEND | O_CREAT);
					}
				}
			}
		}

		// write message to log file
		if (gpsfix && logfile && logfile.write(buffer, index + 1) != index + 1)
		{
			signal_error(ERROR_SD_WRITE);
		}
	}

	if (logfile)
	{
		logfile.flush();
	}

	digitalWrite(PIN_LED_RED, LOW);
	digitalWrite(PIN_LED_GREEN, LOW);
}

