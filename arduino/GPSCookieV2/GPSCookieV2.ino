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
#include <alloca.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include "ubx_messages.h"


#define PIN_GPS 2
#define PIN_LED_RED 3
#define PIN_LED_GREEN 4
#define PIN_VOLTAGE 8
#define PIN_SD 10

#define CONFIG_POWERSAVE_ATMEGA_POWERDOWN 4
#define CONFIG_POWERSAVE_GPS_OFF 3

enum
{
	ERROR_SD_INITIALIZE = 1,
	ERROR_SD_READ,
	ERROR_SD_WRITE,
	ERROR_GPS_CONFIG_FILE,
};


// Configuration settings
uint16_t debug = 0;
uint16_t power_saving = 0;
uint16_t log_interval = 1;
uint16_t wait_for_fix = 600;
char messages[11][4] = {"RMC", "GGA", "GSA", ""};

// Internal state
File logfile = File();
bool gpsready = false;
bool gpsinitialized = false;
bool gpsfix = false;
uint16_t utc_date = FAT_DEFAULT_DATE;
uint16_t utc_time = FAT_DEFAULT_TIME;
uint16_t power_saving_backup = 0;
uint16_t wait_for_fix_backup = 0;
uint16_t message_counter = 0;
uint16_t message_counter_previous = 0;
uint8_t min_messages = 1;
long timeout_start = 0;


void setup()
{
	// turn on voltage regulator
	pinMode(PIN_VOLTAGE, OUTPUT);
	digitalWrite(PIN_VOLTAGE, HIGH);

	// configure LEDs
	pinMode(PIN_LED_RED, OUTPUT);
	pinMode(PIN_LED_GREEN, OUTPUT);
	digitalWrite(PIN_LED_RED, HIGH);

	// disable unused peripherals to save power
	ADCSRA = 0;
	power_all_disable();
	power_timer0_enable();
	power_spi_enable();
	power_usart0_enable();

	// initialize SD card
	pinMode(PIN_SD, OUTPUT);
	SdFile::dateTimeCallback(file_date_time);
	if (!SD.begin(PIN_SD))
	{
		signal_error(ERROR_SD_INITIALIZE);
	}

	// read config file
	read_config();

	// initialize GPS module
	pinMode(PIN_GPS, OUTPUT);
	digitalWrite(PIN_GPS, HIGH);
	Serial.begin(9600);
	Serial.setTimeout(500);
}


void loop()
{
	if (!gpsready)
	{
		if (message_counter < 6)
		{
			// wait for GPS module to start up
			return;
		}
		else
		{
			gpsready = true;
		}
	}

	if (!gpsinitialized)
	{
		gps_initialize();
		return;
	}

	if ((int16_t)(message_counter - message_counter_previous) < min_messages && (signed long)(millis() - timeout_start) < 500)
	{
		// wait for more messages to arrive
		return;
	}

	if (power_saving >= CONFIG_POWERSAVE_GPS_OFF && !gpsfix && wait_for_fix)
	{
		// reduce powersaving mode until we have a fix
		power_saving_backup = power_saving;
		power_saving = CONFIG_POWERSAVE_GPS_OFF - 1;
		wait_for_fix_backup = wait_for_fix;
	}
	else if (power_saving_backup >= CONFIG_POWERSAVE_GPS_OFF)
	{
		if (!gpsfix && wait_for_fix)
		{
			// still no fix
			wait_for_fix--;
		}
		else
		{
			// got a fix or giving up
			wait_for_fix = wait_for_fix_backup;
			wait_for_fix_backup = 0;
			power_saving = power_saving_backup;
			power_saving_backup = 0;
		}
	}

	if (power_saving >= CONFIG_POWERSAVE_GPS_OFF)
	{
		// switch GPS to backup mode
		UBX_RXM_PMREQ config = {log_interval * 1000, 2};
		gps_send_ubx(0x02, 0x41, &config, sizeof(config));
		Serial.flush();
	}

	if (power_saving >= CONFIG_POWERSAVE_GPS_OFF + 1)
	{
		// switch off peripherals to save even more power
		digitalWrite(PIN_VOLTAGE, LOW);
		digitalWrite(PIN_GPS, LOW);
	}

	if (power_saving < CONFIG_POWERSAVE_ATMEGA_POWERDOWN)
	{
		// sleep until UART activity
		set_sleep_mode(SLEEP_MODE_IDLE);
		power_timer0_disable();
		power_spi_disable();
		digitalWrite(PIN_LED_RED, LOW);
		digitalWrite(PIN_LED_GREEN, LOW);

		sleep_mode();

		digitalWrite((gpsfix && logfile) ? PIN_LED_GREEN : PIN_LED_RED, HIGH);
		power_spi_enable();
		power_timer0_enable();
	}
	else
	{
		power_all_disable();
		digitalWrite(PIN_LED_RED, LOW);
		digitalWrite(PIN_LED_GREEN, LOW);

		uint16_t sleep_counter = log_interval;
		while (sleep_counter > 0)
		{
			if (sleep_counter >= 8)
			{
				wdt_enable(WDTO_8S);
				sleep_counter -= 8;
			}
			else if (sleep_counter >= 4)
			{
				wdt_enable(WDTO_4S);
				sleep_counter -= 4;
			}
			else if (sleep_counter >= 2)
			{
				wdt_enable(WDTO_2S);
				sleep_counter -= 2;
			}
			else
			{
				wdt_enable(WDTO_1S);
				sleep_counter -= 1;
			}

			WDTCSR |= (1 << WDIE);
			set_sleep_mode(SLEEP_MODE_PWR_DOWN);
			sleep_mode();
		}

		digitalWrite((gpsfix && logfile) ? PIN_LED_GREEN : PIN_LED_RED, HIGH);

		power_usart0_enable();
		power_spi_enable();
		power_timer0_enable();
	}

	message_counter_previous = message_counter;
	timeout_start = millis();

	if (power_saving >= CONFIG_POWERSAVE_GPS_OFF + 1)
	{
		// switch peripherals back on
		digitalWrite(PIN_VOLTAGE, HIGH);
		digitalWrite(PIN_GPS, HIGH);
	}

	if (power_saving >= CONFIG_POWERSAVE_GPS_OFF)
	{
		// GPS was off, so it has lost its fix
		gpsfix = false;
		// account for startup
		timeout_start += 500;
	}
}


ISR (WDT_vect)
{
	wdt_disable();
}
