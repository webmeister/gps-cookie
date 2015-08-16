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


void serialEvent(void)
{
	while (Serial.available())
	{
		union
		{
			struct
			{
				uint8_t sync1;
				uint8_t sync2;
				uint8_t cls;
				uint8_t id;
				uint16_t length;
				char data[194];
			} ubx;
			char nmea[200];
			uint8_t raw[200];
		} buffer;
		uint8_t length;

		if (Serial.peek() == 0xb5)
		{
			// receive UBX message
			length = Serial.readBytes(buffer.raw, 6);

			if (length != 6 || buffer.ubx.length > sizeof(buffer) - 8)
			{
				// no valid header received or message too large for buffer
				continue;
			}

			length += Serial.readBytes(buffer.ubx.data, buffer.ubx.length + 2);

			if (length != buffer.ubx.length + 8)
			{
				// incomplete message
				continue;
			}

			for (uint8_t i = length - 3; i >= 2; i--)
			{
				buffer.raw[length - 1] -= buffer.raw[length - 2];
				buffer.raw[length - 2] -= buffer.raw[i];
			}

			if (buffer.raw[length - 1] != 0 || buffer.raw[length - 2] != 0)
			{
				// invalid checksum
				continue;
			}

			message_counter++;

			if (debug >= 3 && logfile)
			{
				char msg[50];

				if (buffer.ubx.length)
				{
					snprintf(msg, sizeof(msg), "<- UBX %#04x %#04x with %u bytes payload: ", buffer.ubx.cls, buffer.ubx.id, buffer.ubx.length);
					logfile.print(msg);

					for (uint8_t i = 0; i < buffer.ubx.length; i++)
					{
						sprintf(msg, "%02x", buffer.ubx.data[i]);
						logfile.write(msg, 2);
					}

					logfile.println();
				}
				else
				{
					snprintf(msg, sizeof(msg), "<- UBX %#04x %#04x", buffer.ubx.cls, buffer.ubx.id);
					logfile.println(msg);
				}
			}

			if (buffer.ubx.cls == 0x04)
			{
				// INF message
				if (debug && logfile)
				{
					logfile.println(buffer.ubx.data);
				}
			}
		}
		else if (Serial.peek() == '$')
		{
			// receive NMEA message
			length = Serial.readBytesUntil('\n', buffer.nmea, sizeof(buffer.nmea) - 2);
			buffer.nmea[length++] = '\n';
			memset(buffer.nmea + length, 0, sizeof(buffer.nmea) - length);

			// verify checksum
			if (buffer.nmea[length - 5] != '*' || xor_buffer(buffer.nmea + 1, length - 6) != parse_hex(buffer.nmea + length - 4))
			{
				continue;
			}

			message_counter++;

			// parse interesting messages
			if (memcmp(buffer.nmea, "$GPRMC", 6) == 0)
			{
				char *status = find_field(buffer.nmea, 2);
				gpsfix = (status && *status == 'A');

				if (gpsfix)
				{
					char *time = find_field(buffer.nmea, 1);
					char *date = find_field(buffer.nmea, 9);

					if (date && time)
					{
						utc_date = FAT_DATE(2000 + parse_digits(date + 4), parse_digits(date + 2), parse_digits(date));
						utc_time = FAT_TIME(parse_digits(time), parse_digits(time + 2), parse_digits(time + 4));

						if (!logfile || strcmp(logfile.name(), "debug.log") == 0)
						{
							if (logfile)
							{
								logfile.close();
							}

							char filename[] = "20YYMMDD.log";
							memcpy(filename + 2, date + 4, 2);
							memcpy(filename + 4, date + 2, 2);
							memcpy(filename + 6, date, 2);
							logfile = SD.open(filename, O_WRITE | O_APPEND | O_CREAT);
						}
					}
				}
			}

			// write message to log file
			if ((gpsfix || debug) && logfile && logfile.write(buffer.nmea, length) != length)
			{
				signal_error(ERROR_SD_WRITE);
			}
		}
		else
		{
			// consume unknown garbage
			Serial.read();
		}
	}

	if (logfile)
	{
		logfile.flush();
	}
}
