static void file_date_time(uint16_t* date, uint16_t* time)
{
	*date = utc_date;
	*time = utc_time;
}


static void read_config(void)
{
	File config = SD.open("config.txt", O_READ);
	if (config)
	{
		char buffer[256];
		uint8_t mode = 0;
		int16_t pos = 0;
		int16_t size = config.read(buffer, sizeof(buffer) - 4);
		uint16_t *target_int = NULL;
		char *target_str = NULL;

		while (pos < size)
		{
			switch (mode)
			{
				case 0:
				{
					// looking for the start of a name
					if (isalnum(buffer[pos]))
					{
						memmove(buffer, buffer + pos, size - pos);
						size -= pos;
						pos = 0;
						mode = 1;
						continue;
					}
					else if (!isspace(buffer[pos]))
					{
						mode = 4;
					}
					break;
				}
				case 1:
				{
					// looking for the end of a name
					if (isspace(buffer[pos]) || buffer[pos] == '=')
					{
						buffer[pos] = 0;

						if (strcmp(buffer, "debug") == 0)
						{
							target_int = &debug;
						}
						else if (strcmp(buffer, "power_saving") == 0)
						{
							target_int = &power_saving;
						}
						else if (strcmp(buffer, "log_interval") == 0)
						{
							target_int = &log_interval;
						}
						else if (strcmp(buffer, "wait_for_fix") == 0)
						{
							target_int = &wait_for_fix;
						}
						else if (strcmp(buffer, "messages") == 0)
						{
							target_str = messages[1];
							messages[1][0] = 0;
						}

						if (target_int || target_str)
						{
							if (debug && logfile)
							{
								logfile.print(buffer);
								logfile.println(" =");
							}

							memmove(buffer, buffer + pos + 1, size - pos - 1);
							size -= pos + 1;
							pos = 0;
							mode = 2;
							continue;
						}
						else
						{
							mode = 4;
						}
					}
					else if (!isalnum(buffer[pos]) && buffer[pos] != '_')
					{
						mode = 4;
					}
					break;
				}
				case 2:
				{
					// looking for the start of a value
					if (isdigit(buffer[pos]) && target_int)
					{
						*target_int = atoi(buffer + pos);
						if (debug)
						{
							if (logfile)
							{
								logfile.println(*target_int);
							}
							else
							{
								logfile = SD.open("debug.log", O_WRITE | O_TRUNC | O_CREAT);
							}
						}
						target_int = NULL;
					}
					else if (isalnum(buffer[pos]) && target_str)
					{
						memmove(buffer, buffer + pos, size - pos);
						size -= pos;
						pos = 0;
						mode = 3;
						continue;
					}
					else if (buffer[pos] == '\r' || buffer[pos] == '\n')
					{
						mode = 0;
						target_int = NULL;
						target_str = NULL;
					}
					else if (!isspace(buffer[pos]) && buffer[pos] != '=')
					{
						mode = 4;
					}
					break;
				}
				case 3:
				{
					// looking for the end of a value
					if (isspace(buffer[pos]) || buffer[pos] == ',')
					{
						if (pos < sizeof(messages[0]))
						{
							memcpy(target_str, buffer, pos);
							target_str[pos] = 0;
							if (debug && logfile)
							{
								logfile.println(target_str);
							}
							target_str += sizeof(messages[0]);
							if ((uint32_t)target_str - (uint32_t)messages > sizeof(messages))
							{
								target_str = NULL;
							}
							min_messages++;
						}

						mode = 2;
					}
					else if (!isalnum(buffer[pos]))
					{
						mode = 4;
					}
				}
				case 4:
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
				if (mode == 0 || mode == 2 || mode == 4)
				{
					pos = 0;
					size = config.read(buffer, sizeof(buffer) - 4);
				}
				else
				{
					signal_error(ERROR_GPS_CONFIG_FILE);
				}
			}
		}

		if (size < 0)
		{
			signal_error(ERROR_SD_READ);
		}
	}
}
