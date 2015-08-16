typedef struct
{
	uint32_t clearMask;
	uint32_t saveMask;
	uint32_t loadMask;
	uint8_t deviceMask;
} UBX_CFG_CFG;

typedef struct
{
	uint8_t protocolID;
	uint8_t reserved0;
	uint16_t reserved1;
	uint8_t infMsgMask[6];
} UBX_CFG_INF;

typedef struct
{
	uint8_t msgClass;
	uint8_t msgID;
	uint8_t rate;
} UBX_CFG_MSG;

typedef struct
{
	uint8_t version;
	uint8_t reserved1;
	uint8_t reserved2;
	uint8_t reserved3;
	uint32_t flags;
	uint32_t updatePeriod;
	uint32_t searchPeriod;
	uint32_t gridOffset;
	uint16_t onTime;
	uint16_t minAcqTime;
	uint16_t reserved4;
	uint16_t reserved5;
	uint32_t reserved6;
	uint32_t reserved7;
	uint8_t reserved8;
	uint8_t reserved9;
	uint16_t reserved10;
	uint32_t reserved11;
} UBX_CFG_PM2;

typedef struct
{
	uint8_t reserved1;
	uint8_t lpMode;
} UBX_CFG_RXM;

typedef struct
{
	uint32_t duration;
	uint32_t flags;
} UBX_RXM_PMREQ;
