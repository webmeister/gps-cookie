uint8_t initializing = 0;


static void gps_send_nmea(char *msg)
{
	uint8_t length = strlen(msg);
	char *buffer = (char*)alloca(length + 4);
	memcpy(buffer, msg, length);

	if (buffer[length - 3] != '*')
	{
		buffer[length] = '*';
		length += 3;
	}

	sprintf(buffer + length - 2, "%02X", xor_buffer(buffer + 1, length - 4));
	buffer[length] = 0;

	Serial.println(buffer);

	if (debug >= 2 && logfile)
	{
		logfile.println(buffer);
	}
}


static void gps_send_ubx(uint8_t cls, uint8_t id, const void *data, uint8_t length)
{
	uint8_t *buffer = (uint8_t*)alloca(length + 8);

	buffer[0] = 0xb5;
	buffer[1] = 0x62;
	buffer[2] = cls;
	buffer[3] = id;
	buffer[4] = length;
	buffer[5] = 0;
	memcpy(buffer + 6, data, length);
	buffer[length + 6] = 0;
	buffer[length + 7] = 0;

	for (uint8_t i = 2; i < length + 6; i++)
	{
		buffer[length + 6] += buffer[i];
		buffer[length + 7] += buffer[length + 6];
	}

	Serial.write(buffer, length + 8);

	if (debug >= 3 && logfile)
	{
		char msg[50];

		if (length)
		{
			snprintf(msg, sizeof(msg), "-> UBX %#04x %#04x with %u bytes payload: ", cls, id, length);
			logfile.print(msg);

			for (uint8_t i = 0; i < length; i++)
			{
				sprintf(msg, "%02x", ((uint8_t*)data)[i]);
				logfile.write(msg, 2);
			}

			logfile.println();
		}
		else
		{
			snprintf(msg, sizeof(msg), "-> UBX %#04x %#04x", cls, id);
			logfile.println(msg);
		}
	}
}


static void gps_set_power_mode(uint8_t mode)
{
	UBX_CFG_RXM config = {8, mode};
	gps_send_ubx(0x06, 0x11, &config, sizeof(config));
}


static void gps_poll_config(void)
{
	if (debug >= 3)
	{
		// CFG-NAV5, CFG-NAVX5, CFG-PM2, CFG-RXM, CFG-SBAS
		const uint8_t msgids[] = {0x24, 0x23, 0x3b, 0x11, 0x16};
		for (uint8_t i = 0; i < sizeof(msgids) / sizeof(msgids[0]); i++)
		{
			if (Serial.available())
			{
				serialEvent();
			}

			gps_send_ubx(0x06, msgids[i], NULL, 0);
		}

		while (!Serial.available()) ;
	}
}


static void gps_request_messages(uint16_t interval)
{
	for (uint8_t i = 0; i < sizeof(messages) / sizeof(messages[0]); i++)
	{
		if (messages[i][0] == 0)
			break;

		if (Serial.available())
		{
			serialEvent();
		}

		char msg[32];
		snprintf(msg, sizeof(msg), "$PUBX,40,%s,0,%u,0,0,0,0", messages[i], interval);
		gps_send_nmea(msg);
	}

	Serial.flush();
}


static void gps_initialize(void)
{
	// set initial configuration step by step
	switch (initializing++)
	{
		case 0:
		{
			// disable I2C interface
			gps_send_nmea("$PUBX,41,0,0,0,0,0");
			Serial.flush();
			break;
		}
		case 1:
		{
			// disable default messages for all interfaces
			gps_send_nmea("$PUBX,40,GGA,0,0,0,0,0,0");
			gps_send_nmea("$PUBX,40,GLL,0,0,0,0,0,0");
			gps_send_nmea("$PUBX,40,GSA,0,0,0,0,0,0");
			gps_send_nmea("$PUBX,40,GSV,0,0,0,0,0,0");
			gps_send_nmea("$PUBX,40,RMC,0,0,0,0,0,0");
			gps_send_nmea("$PUBX,40,VTG,0,0,0,0,0,0");

			// disable further TXT messages
			const UBX_CFG_INF config = {1, 0, 0, {0, 3, 0, 0, 0, 0}};
			gps_send_ubx(0x06, 0x02, &config, sizeof(config));

			Serial.flush();
			break;
		}
		case 2:
		{
			gps_poll_config();
			break;
		}
		case 3:
		{
			// enable requested messages
			gps_request_messages(power_saving >= CONFIG_POWERSAVE_GPS_OFF ? 1 : log_interval);
			break;
		}
		case 4:
		{
			UBX_CFG_PM2 config = {
				1, // version
				0, // reserved1
				0, // reserved1
				0, // reserved1
				0x00021000, // flags
				1000, // updatePeriod
				log_interval * 1000, // searchPeriod
				0, // gridOffset
				2, // onTime
				wait_for_fix, // minAcqTime
				0, // reserved4
				0, // reserved5
				0, // reserved6
				0, // reserved7
				0, // reserved8
				0, // reserved9
				0, // reserved10
				0, // reserved11
			};
			gps_send_ubx(0x06, 0x3b, &config, sizeof(config));

			if (power_saving == 2)
			{
				// Power Save Mode
				gps_set_power_mode(1);
			}
			else if (power_saving >= 1)
			{
				// Eco Mode
				gps_set_power_mode(4);
			}
			else
			{
				// Maximum Performance Mode
				gps_set_power_mode(0);
			}
			while (!Serial.available()) ;
			break;
		}
		case 5:
		{
			// save settings to battery backed RAM
			const UBX_CFG_CFG config = {0, 0x1f, 0, 1};
			gps_send_ubx(0x06, 0x09, &config, sizeof(config));
			while (!Serial.available()) ;
			break;
		}
		case 6:
		{
			gps_poll_config();
			break;
		}
		default:
		{
			gpsinitialized = true;
			message_counter_previous = message_counter;
			timeout_start = millis();
		}
	}
}
