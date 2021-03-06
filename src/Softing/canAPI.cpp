/*======================*/
/*       Includes       */
/*======================*/
//system headers
#include <stdio.h>
#include <errno.h>
#ifndef _WIN32
#include <inttypes.h>
#include <pthread.h>
#include <syslog.h>
#include <unistd.h>
#endif
#include <windows.h>
#include <malloc.h>
#include <assert.h>
//project headers
extern "C" {
#include "Softing/Can_def.h"
#include "Softing/CANL2.h"
}
#include "canDef.h"
#include "canAPI.h"

CANAPI_BEGIN


#define CH_COUNT			(int)2 // number of CAN channels

static CAN_HANDLE hCAN[CH_COUNT] = {-1, -1}; // CAN channel handles

const char* szCanDevType[] = {
	"",
	"CANcard2",
	"CAN-ACx-PCI",
	"CAN-ACx-PCI/DN",
	"CAN-ACx-104",
	"CANusb",
	"CAN-PROx-PCIe", 
	"CAN-PROx-PC104+",
	"CANpro USB", 
	"EDICcard2",
};

// gets the device name from the device type
char *getDeviceType(int u32DeviceType)
{
  static char u8Type[100];

  switch(u32DeviceType)
  {
  case D_CANCARD2:
    strcpy(u8Type,"CANcard2");
    break;

  case D_CANACPCI:
    strcpy(u8Type,"CAN-AC PCI");
    break;

  case D_CANACPCIDN:
    strcpy(u8Type,"CAN-AC PCI/DN");
    break;

  case D_CANAC104:
    strcpy(u8Type,"CAN-AC PC/104");
    break;

  case D_CANUSB:
    strcpy(u8Type,"CANusb");
    break;

  case D_EDICCARD2:
    strcpy(u8Type,"EDICcard2");
    break;

  case D_CANPROXPCIE:
    strcpy(u8Type,"CANpro PCI Express");
    break;

  case D_CANPROX104:
    strcpy(u8Type,"CANpro PC/104plus");
    break;

  case D_CANECO104:
    strcpy(u8Type,"CAN-ECOx-104");
    break;

  case D_CANFANUPC8:
    strcpy(u8Type,"PC8 onboard CAN");
    break;

  case D_CANPROUSB:
    strcpy(u8Type,"CANpro USB");
    break;

  default:
    strcpy(u8Type,"UNKNOWN");
    break;

  }
  return u8Type;
}

int canWrite(CAN_HANDLE handle,
			 unsigned long id, 
			 void * msg,
			 unsigned int dlc,
			 int mode)
{
	if (handle < 0)
		return -1;

	int ret = CANL2_send_data(handle, id, mode, dlc, (unsigned char*)msg);
	if (ret)
	{
		printf("CAN write error\n");
		return ret;
	}

	return 0;
}

int command_can_open(int ch)
{
	assert(ch >= 1 && ch <= CH_COUNT);

	int ret = 0;
	
	////////////////////////////////////////////////////////////////////////
	// Init Channel
	char ch_name[256];
	sprintf_s(ch_name, 256, "CAN-ACx-PCI_%d", ch);
	printf("Open CAN channel: %s...\n", ch_name);

	ret = INIL2_initialize_channel(&hCAN[ch-1], ch_name);
	if (ret)
	{
		printf("\tError: CAN open\n");
		return ret;
	}

	///////////////////////////////////////////////////////////////////////
	// Reset Chip
//	ret = CANL2_reset_chip(hCAN[ch-1]);
//	if (ret)
//	{
//		printf("\tError: CAN reset chip\n");
//		INIL2_close_channel(hCAN[ch-1]);
//		hCAN[ch-1] = 0;
//		return ret;
//	}

	///////////////////////////////////////////////////////////////////////
	// Init Chip
//	ret = CANL2_initialize_chip(hCAN[ch-1], 1, 1, 4, 3, 0);
//	if (ret)
//	{
//		printf("\tError: CAN set baud rate\n");
//		INIL2_close_channel(hCAN[ch-1]);
//		hCAN[ch-1] = 0;
//		return ret;
//	}
	
	///////////////////////////////////////////////////////////////////////
	// Set Out Control
//	ret = CANL2_set_output_control(hCAN[ch-1], -1);

	///////////////////////////////////////////////////////////////////////
	// Enable FIFO
	L2CONFIG L2Config;
	L2Config.fBaudrate = 1000.0;
	L2Config.bEnableAck = false;
	L2Config.bEnableErrorframe = false;
	L2Config.s32AccCodeStd = GET_FROM_SCIM;
	L2Config.s32AccMaskStd = GET_FROM_SCIM;
	L2Config.s32AccCodeXtd = GET_FROM_SCIM;
	L2Config.s32AccMaskXtd = GET_FROM_SCIM;
	L2Config.s32OutputCtrl = GET_FROM_SCIM;
	L2Config.s32Prescaler = GET_FROM_SCIM;
	L2Config.s32Sam = GET_FROM_SCIM;
	L2Config.s32Sjw = GET_FROM_SCIM;
	L2Config.s32Tseg1 = GET_FROM_SCIM;
	L2Config.s32Tseg2 = GET_FROM_SCIM;
	L2Config.hEvent = (void*)-1;
	ret = CANL2_initialize_fifo_mode(hCAN[ch-1], &L2Config);
	if (ret)
	{
		printf("\tError: CAN set fifo mode\n");
		INIL2_close_channel(hCAN[ch-1]);
		hCAN[ch-1] = 0;
		return ret;
	}

	///////////////////////////////////////////////////////////////////////
	// Set Acceptance (Filter)
	ret = CANL2_set_acceptance(
		hCAN[ch-1], 
		((unsigned long)ID_DEVICE_MAIN <<3), 
		0x038, 
		0, 0x1fffffff);
	if (ret)
	{
		printf("\tError: CAN set acceptance\n");
		INIL2_close_channel(hCAN[ch-1]);
		hCAN[ch-1] = 0;
		return ret;
	}

	return 0;
}

int command_can_open_ex(int ch, int type, int index)
{
	assert(ch >= 1 && ch <= CH_COUNT);
	assert(type >= 1 && type < 9/*eCanDevType_COUNT*/);
	assert(index >= 1 && index <= 8);

	int ret = 0;
	PCHDSNAPSHOT pBuffer = NULL;
	unsigned long u32NeededBufferSize, u32NumOfChannels, u32ProvidedBufferSize, channelIndex;
	int sw_version, fw_version, hw_version, license, chip_type;

	////////////////////////////////////////////////////////////////////////
	// Set buffer size
	u32ProvidedBufferSize = 0;

	// call the function without a valid buffer size first to get the needed buffersize in "u32NeededBufferSize"
	ret = CANL2_get_all_CAN_channels(0, &u32NeededBufferSize, &u32NumOfChannels, NULL);

	if(!u32NumOfChannels)
	{
		printf("you have no Softing CAN interface card plugged in your Computer!\n");
		printf("plug a interface card first and start this program again after this.\n");
		return -1;
	}
	if(ret)
	{
		printf("The driver reported a problem: Error Code %x\n", ret);
		return -1;
	}

	pBuffer = (PCHDSNAPSHOT)malloc(u32NeededBufferSize);
	u32ProvidedBufferSize = u32NeededBufferSize;

	ret = CANL2_get_all_CAN_channels(u32ProvidedBufferSize, &u32NeededBufferSize, &u32NumOfChannels, pBuffer);

	if(ret)
	{
		printf("The driver reported a problem: Error Code %x\n", ret);
		return -1;
	}

	printf("You have %u Softing CAN channels in your system\n\n", u32NumOfChannels);

	printf("\tname\t\t serialnumber\t type\t\t chan.\t    open\n");
	printf("------------------------------------------------------------------------\n");
	printf("\n");

	for (channelIndex=0; channelIndex<u32NumOfChannels; channelIndex++)
	{
		PCHDSNAPSHOT pCh = &pBuffer[channelIndex];

		printf("% 17s\t %09u  % 18s\t %2u\t % 5s\n",
			pCh->ChannelName,
			pCh->u32Serial,
			getDeviceType(pCh->u32DeviceType),
			pCh->u32PhysCh,
			(pCh->bIsOpen) ? "yes" : "no");
	}

	////////////////////////////////////////////////////////////////////////
	// Init Channel
	char ch_name[256];
	sprintf_s(ch_name, 256, "%s_%d", szCanDevType[type], index);
	printf("Open CAN channel[%d]: %s...\n", ch, ch_name);
//hCAN[0] = -1;
	ret = INIL2_initialize_channel(&hCAN[ch-1], ch_name);
	if (ret)
	{
		switch (ret) {
			case -536215551: printf("  Internal Error.\n"); break;
			case -536215550: printf("  General Error.\n"); break;
			case -536215546: printf("  Illegal driver call.\n"); break;
			case -536215542: printf("  Driver not loaded / not installed, or device is not plugged.\n"); break;
			case -536215541: printf("  Out of memory.\n"); break;
			case -536215531: printf("  An error occurred while hooking the interrupt service routine.\n"); break;
			case -536215523: printf("  Device not found.\n"); break;
			case -536215522: printf("  Can not get a free address region for DPRAM from system.\n"); break;
			case -536215521: printf("  Error while accessing hardware.\n"); break;
			case -536215519: printf("  Can not access the DPRAM memory.\n"); break;
			case -536215516: printf("  Interrupt does not work/Interrupt test failed!\n"); break;
			case -536215514: printf("  Device is already open.\n"); break;
			case -536215512: printf("  An incompatible firmware is running on that device. (CANalyzer/CANopen/DeviceNet firmware)\n"); break;
			case -536215511: printf("  Channel can not be accessed, because it is not open.\n"); break;
			case -536215500: printf("  Error while calling a Windows function.\n"); break;
			case -1002:      printf("  Too many open channels.\n"); break;
			case -1003:      printf("  Wrong DLL or driver version.\n"); break;
			case -1004:      printf("  Error while loading the firmware. (This may be a DPRAM access error)\n"); break;
			case -1:         printf("  Function not successful.\n"); break;
		}

		printf("\tError: CAN open\n");
		return ret;
	}

	///////////////////////////////////////////////////////////////////////
	// Reset Chip
//	ret = CANL2_reset_chip(hCAN[ch-1]);
//	if (ret)
//	{
//		printf("\tError: CAN reset chip\n");
//		INIL2_close_channel(hCAN[ch-1]);
//		hCAN[ch-1] = 0;
//		return ret;
//	}

	///////////////////////////////////////////////////////////////////////
	// Init Chip
//	ret = CANL2_initialize_chip(hCAN[ch-1], 1, 1, 4, 3, 0);
//	if (ret)
//	{
//		printf("\tError: CAN set baud rate\n");
//		INIL2_close_channel(hCAN[ch-1]);
//		hCAN[ch-1] = 0;
//		return ret;
//	}
	
	///////////////////////////////////////////////////////////////////////
	// Set Out Control
//	ret = CANL2_set_output_control(hCAN[ch-1], -1);
	
	///////////////////////////////////////////////////////////////////////
	// Enable FIFO
	L2CONFIG L2Config;
	L2Config.fBaudrate = 1000.0;
	L2Config.bEnableAck = false;
	L2Config.bEnableErrorframe = false;
	L2Config.s32AccCodeStd = GET_FROM_SCIM;
	L2Config.s32AccMaskStd = GET_FROM_SCIM;
	L2Config.s32AccCodeXtd = GET_FROM_SCIM;
	L2Config.s32AccMaskXtd = GET_FROM_SCIM;
	L2Config.s32OutputCtrl = GET_FROM_SCIM;
	L2Config.s32Prescaler = GET_FROM_SCIM;
	L2Config.s32Sam = GET_FROM_SCIM;
	L2Config.s32Sjw = GET_FROM_SCIM;
	L2Config.s32Tseg1 = GET_FROM_SCIM;
	L2Config.s32Tseg2 = GET_FROM_SCIM;
	L2Config.hEvent = (void*)-1;
	ret = CANL2_initialize_fifo_mode(hCAN[ch-1], &L2Config);
	if (ret)
	{
		printf("\tError: CAN set fifo mode\n");
		INIL2_close_channel(hCAN[ch-1]);
		hCAN[ch-1] = 0;
		return ret;
	}

	///////////////////////////////////////////////////////////////////////
	// Print driver version info
	ret = CANL2_get_version(hCAN[ch-1], &sw_version, &fw_version, &hw_version, &license, &chip_type);
	if (ret)
	{
		printf("Error %u in CANL2_get_version()\n",ret);
	}
	else
	{
		printf("\n VERSION INFO: \n\n");
		printf("    - Software version: %u.%02u\n", sw_version/100, sw_version%100);
		printf("    - Firmware version: %u.%02u\n", fw_version/100, fw_version%100);
		printf("    - Hardware version: %x.%02x\n", hw_version/0x100, hw_version%0x100);
		printf("    - CAN chip        : %s\n", (chip_type==1000)? "SJA1000": (chip_type==161) ? "Infineon XC161" : "Infineon XE164");
	}

	///////////////////////////////////////////////////////////////////////
	// Set Acceptance (Filter)
	ret = CANL2_set_acceptance(
		hCAN[ch-1], 
		((unsigned long)ID_DEVICE_MAIN <<3), 
		0x038, 
		0, 0x1fffffff);
	if (ret)
	{
		printf("\tError: CAN set acceptance\n");
		INIL2_close_channel(hCAN[ch-1]);
		hCAN[ch-1] = 0;
		return ret;
	}

	return 0;
}

int command_can_reset(int ch)
{
	return -1;
}

int command_can_close(int ch)
{
	INIL2_close_channel(hCAN[ch-1]);
	hCAN[ch-1] = 0;
	return 0;
}

int command_can_query_id(int ch)
{
	long Txid;
	unsigned char data[8];

	Txid = ((unsigned long)ID_CMD_QUERY_ID<<6) | ((unsigned long)ID_COMMON <<3) | ((unsigned long)ID_DEVICE_MAIN);
	canWrite(hCAN[ch-1], Txid, data, 0, STD);

	return 0;
}

int command_can_sys_init(int ch, int period_msec)
{
	long Txid;
	unsigned char data[8];

	Txid = ((unsigned long)ID_CMD_SET_PERIOD<<6) | ((unsigned long)ID_COMMON <<3) | ((unsigned long)ID_DEVICE_MAIN);
	data[0] = (unsigned char)period_msec;
	canWrite(hCAN[ch-1], Txid, data, 1, STD);

	Sleep(10);

	Txid = ((unsigned long)ID_CMD_SET_MODE_TASK<<6) | ((unsigned long)ID_COMMON <<3) | ((unsigned long)ID_DEVICE_MAIN);
	canWrite(hCAN[ch-1], Txid, data, 0, STD);

	Sleep(10);

	Txid = ((unsigned long)ID_CMD_QUERY_STATE_DATA<<6) | ((unsigned long)ID_COMMON <<3) | ((unsigned long)ID_DEVICE_MAIN);
	canWrite(hCAN[ch-1], Txid, data, 0, STD);

	return 0;
}

int command_can_start(int ch)
{
	long Txid;
	unsigned char data[8];

	Txid = ((unsigned long)ID_CMD_QUERY_STATE_DATA<<6) | ((unsigned long)ID_COMMON <<3) | ((unsigned long)ID_DEVICE_MAIN);
	canWrite(hCAN[ch-1], Txid, data, 0, STD);

	Sleep(10);

	Txid = ((unsigned long)ID_CMD_SET_SYSTEM_ON<<6) | ((unsigned long)ID_COMMON <<3) | ((unsigned long)ID_DEVICE_MAIN);
	canWrite(hCAN[ch-1], Txid, data, 0, STD);

	return 0;
}

int command_can_stop(int ch)
{
	long Txid;
	unsigned char data[8];

	Txid = ((unsigned long)ID_CMD_SET_SYSTEM_OFF<<6) | ((unsigned long)ID_COMMON <<3) | ((unsigned long)ID_DEVICE_MAIN);
	canWrite(hCAN[ch-1], Txid, data, 0, STD);

	return 0;
}

int command_can_AHRS_set(int ch, unsigned char rate, unsigned char mask)
{
	long Txid;
	unsigned char data[8];

	Txid = ((unsigned long)ID_CMD_AHRS_SET<<6) | ((unsigned long)ID_COMMON <<3) | ((unsigned long)ID_DEVICE_MAIN);
	data[0] = (unsigned char)rate;
	data[1] = (unsigned char)mask;
	canWrite(hCAN[ch-1], Txid, data, 2, STD);

	return 0;
}

int write_current(int ch, int findex, short* pwm)
{
	long Txid;
	unsigned char data[8];

	if (findex >= 0 && findex < 4)
	{
		data[0] = (unsigned char)( (pwm[0] >> 8) & 0x00ff);
		data[1] = (unsigned char)(pwm[0] & 0x00ff);

		data[2] = (unsigned char)( (pwm[1] >> 8) & 0x00ff);
		data[3] = (unsigned char)(pwm[1] & 0x00ff);

		data[4] = (unsigned char)( (pwm[2] >> 8) & 0x00ff);
		data[5] = (unsigned char)(pwm[2] & 0x00ff);

		data[6] = (unsigned char)( (pwm[3] >> 8) & 0x00ff);
		data[7] = (unsigned char)(pwm[3] & 0x00ff);

		Txid = ((unsigned long)(ID_CMD_SET_TORQUE_1 + findex)<<6) | ((unsigned long)ID_COMMON <<3) | ((unsigned long)ID_DEVICE_MAIN);
		canWrite(hCAN[ch-1], Txid, data, 8, STD);
	}
	else
		return -1;
	
	return 0;
}

int get_message(int ch, char* cmd, char* src, char* des, int* len, unsigned char* data, int blocking)
{
	int ret;
	can_msg msg;
	PARAM_STRUCT param;
	
	ret = CANL2_read_ac(hCAN[ch-1], &param);

	switch (ret)
	{
	case CANL2_RA_XTD_DATAFRAME :
		msg.msg_id = param.Ident;
		msg.STD_EXT = EXT;
		msg.data_length = param.DataLength;
		
		msg.data[0] = param.RCV_data[0];
		msg.data[1] = param.RCV_data[1];
		msg.data[2] = param.RCV_data[2];
		msg.data[3] = param.RCV_data[3];
		msg.data[4] = param.RCV_data[4];
		msg.data[5] = param.RCV_data[5];
		msg.data[6] = param.RCV_data[6];
		msg.data[7] = param.RCV_data[7];

		break;

	case CANL2_RA_DATAFRAME:

		msg.msg_id = param.Ident;
		msg.STD_EXT = STD;
		msg.data_length = param.DataLength;
		
		msg.data[0] = param.RCV_data[0];
		msg.data[1] = param.RCV_data[1];
		msg.data[2] = param.RCV_data[2];
		msg.data[3] = param.RCV_data[3];
		msg.data[4] = param.RCV_data[4];
		msg.data[5] = param.RCV_data[5];
		msg.data[6] = param.RCV_data[6];
		msg.data[7] = param.RCV_data[7];

		break;

	case CANL2_RA_NO_DATA:
		return -1;

	default:
		return -1;
	}

	*cmd = (char)( (msg.msg_id >> 6) & 0x1f );
	*des = (char)( (msg.msg_id >> 3) & 0x07 );
	*src = (char)( msg.msg_id & 0x07 );
	*len = (int)( msg.data_length );
	for(int nd=0; nd<(*len); nd++) data[nd] = msg.data[nd];

	return 0;
}



CANAPI_END
