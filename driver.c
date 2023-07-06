#include "platform.h"
#include "driver.h"
#include "dlc9lp.h"
#include <ftdi.h>

struct libusb_device_handle* g_hDevice = NULL;
struct ftdi_context g_tFTDI;
uint8_t g_bFTDI = FALSE;
uint8_t g_dJTAGFrames[XPC_00A6_CHUNKSIZE * 2];
//uint16_t g_dJTAGFrames[XPC_00A6_CHUNKSIZE];
size_t g_iFrameBits = 0;
size_t g_iTDOBits = 0;
size_t g_iTDORead = 0;
uchar* g_pTDOBuffer = NULL;

static void XPC_Disable(struct libusb_device_handle* hDevice) {
	if(libusb_control_transfer(hDevice, 0x40, 0xB0, 0x0010, 0, NULL, 0, 1000)) crash();
}

static void XPC_Enable(struct libusb_device_handle* hDevice) {
	if(libusb_control_transfer(hDevice, 0x40, 0xB0, 0x0018, 0, NULL, 0, 1000)) crash();
}

static void XPC_SetClock(struct libusb_device_handle* hDevice, int iPrescaler) {
	// Hz = 786432000000 >> iPrescaler
	// 0x12 = 3 MHz; 0x11 = 6 MHz; 0x10 = 12 MHz; ...
	if(libusb_control_transfer(hDevice, 0x40, 0xB0, 0x0028, iPrescaler, NULL, 0, 1000)) crash();
}

// TODO libusb_control_transfer(hDevice, 0xC0, 0xB0, 0x0050, 0x0102, pBuffer, 2, 1000) -> 04 00 [Type: 0x0004]
// TODO libusb_control_transfer(hDevice, 0xC0, 0xB0, 0x0042, 0x0000, pBuffer, 8, 1000) -> 01 6C 06 2B 1A 00 00 DB [ESN option: 00001A2B066C01]
// TODO libusb_control_transfer(hDevice, 0x40, 0xB0, 0x0052, 0, NULL, 0, 1000)
// TODO libusb_control_transfer(hDevice, 0x40, 0xB0, 0x0052, 1, NULL, 0, 1000)

// CONTROL_IN 0xC0 0xB0 0x0050 0x0001 2 > 12 00 [PLD version: 0x0012]
static void XPC_ReadCPLDVersion(struct libusb_device_handle* hDevice, uint16_t* pVersion) {
	if(libusb_control_transfer(hDevice, 0xC0, 0xB0, 0x0050, 0x0001, (uchar*)pVersion, 2, 1000) != 2) crash();
}

static void XPC_ReadFirmwareVersion(struct libusb_device_handle* hDevice, uint16_t* pVersion) {
	if(libusb_control_transfer(hDevice, 0xC0, 0xB0, 0x0050, 0x0000, (uchar*)pVersion, 2, 1000) != 2) crash();
}

static void XPC_JTAGTransfer(struct libusb_device_handle* hDevice, uint8_t* pFrames, size_t iFrameBits, uint8_t* pTDO, size_t iTDOBits) {
	if(libusb_control_transfer(hDevice, 0x40, 0xB0, 0x00A6, iFrameBits, NULL, 0, 1000)) crash();

	size_t iCursor = 0;
	size_t iRemaining = 2 * ((iFrameBits + 3) / 4);
	while(iRemaining) {
		size_t iSize = MIN(XPC_BULK_MAX / 2, iRemaining);
		int iTransferred;
		if(libusb_bulk_transfer(hDevice, 0x02, pFrames + iCursor, iSize, &iTransferred, 1000) || iTransferred != (ssize_t)iSize) crash();
		iRemaining -= iSize;
		iCursor += iSize;
	}

	if(iTDOBits) {
		size_t iTDOBytes = 2 * ((iTDOBits + 15) / 16);
		int iTransferred;
		if(libusb_bulk_transfer(hDevice, 0x06 | LIBUSB_ENDPOINT_IN, pTDO, iTDOBytes, &iTransferred, 1000) || iTransferred != (ssize_t)iTDOBytes) crash();
	}
}

static struct libusb_device_handle* _OpenDevice(struct libusb_device** pDevices, uint16_t iVendor, uint16_t iProduct) {
	for(; *pDevices; ++pDevices) {
		struct libusb_device_descriptor tDescriptor;
		if(libusb_get_device_descriptor(*pDevices, &tDescriptor)) continue;
		if(tDescriptor.idVendor == iVendor && tDescriptor.idProduct == iProduct) {
			struct libusb_device_handle* hDevice;
			if(libusb_open(*pDevices, &hDevice)) return NULL;
			if(libusb_reset_device(hDevice)) goto error;

			int iConfiguration;
			if(libusb_get_configuration(hDevice, &iConfiguration)) goto error;
			if(libusb_set_configuration(hDevice, iConfiguration)) goto error;

			if(libusb_claim_interface(hDevice, 0)) goto error;
			return hDevice;

			error:
			libusb_close(hDevice);
			return NULL;
		}
	}

	return NULL;
}

struct libusb_device_handle* XPC_Connect(uint16_t iVendor, uint16_t iProduct) {
	struct libusb_device** pDevices;
	if(libusb_get_device_list(NULL, &pDevices) <= 0) crash();
	struct libusb_device_handle* hDevice = _OpenDevice(pDevices, iVendor, iProduct);
	libusb_free_device_list(pDevices, 1);
	return hDevice;
}

int XPC_Initialize(uint16_t iVendor, uint16_t iProduct) {
	g_hDevice = XPC_Connect(iVendor, iProduct);
	if(!g_hDevice) return 1;

	XPC_SetClock(g_hDevice, 0x10);
	if(libusb_control_transfer(g_hDevice, 0x40, 0xB0, 0x0030, 0x0008, NULL, 0, 1000)) crash();

	uint16_t iFirmwareVersion;
	XPC_ReadFirmwareVersion(g_hDevice, &iFirmwareVersion);
	if(iFirmwareVersion != XPC_DLC9LP_FIRMWARE_VERSION) crash("Firmware version mismatch");

	uint16_t iCPLDVersion;
	XPC_ReadCPLDVersion(g_hDevice, &iCPLDVersion);
	fprintf(stderr, "%02X %02X [PLD version: 0x%04X]\n", iCPLDVersion & 0xFF, iCPLDVersion >> 8 & 0xFF, (unsigned)iCPLDVersion);

	XPC_Disable(g_hDevice);
	XPC_SetClock(g_hDevice, 0x11);
	XPC_Enable(g_hDevice);
	XPC_JTAGTransfer(g_hDevice, (uint8_t[]){ 0, 0 }, 2, NULL, 0);
	XPC_SetClock(g_hDevice, 0x10);
	JTAG_Initialize();
	return 0;
}

int XPC_InitializeFTDI(uint16_t iVendor, uint16_t iProduct) {
	g_bFTDI = TRUE;
	if(ftdi_init(&g_tFTDI)) crash();
	if(ftdi_usb_open_desc(&g_tFTDI, iVendor, iProduct, 0, 0)) {
		ftdi_deinit(&g_tFTDI);
		return 1;
	}

	ftdi_usb_reset(&g_tFTDI);
	ftdi_set_interface(&g_tFTDI, INTERFACE_A);
	ftdi_set_latency_timer(&g_tFTDI, 1);
	ftdi_set_bitmode(&g_tFTDI, 0xfb, BITMODE_MPSSE);

	if(ftdi_write_data(&g_tFTDI, (uint8_t[]){ SET_BITS_LOW, 0x08, 0x0B, SET_BITS_HIGH, 0x00, 0x00, TCK_DIVISOR, 0x00, 0x00, LOOPBACK_END }, 10) != 10) {
		fprintf(stderr, "Config error\n");
		ftdi_deinit(&g_tFTDI);
		return 1;
	}

	if(ftdi_write_data(&g_tFTDI, (uint8_t[]){ GET_BITS_LOW, SEND_IMMEDIATE }, 2) != 2) {
		fprintf(stderr, "Write error\n");
		ftdi_deinit(&g_tFTDI);
		return 1;
	}

	uint8_t xStatus;
	ftdi_read_data(&g_tFTDI, &xStatus, 1);
	if((xStatus & 0x10) == 0x00) {
		fprintf(stderr, "The cable doesn't seem to be connected to an FPGA\n");
		ftdi_deinit(&g_tFTDI);
		return 1;
	}

	JTAG_Initialize();
	return 0;
}

static void _Enqueue(bool bTDI, bool bTMS, bool bPadding, bool bReadTDO) {
	int iFrameBit = g_iFrameBits & 3;
	int iFrameByte = (g_iFrameBits - iFrameBit) / (8 / 4); // 8 bits per byte, 4 bits per cycle

	//if(iFrameBit == 0) g_dJTAGFrames[iFrameWord] = 0;
	if(iFrameBit == 0) {
		g_dJTAGFrames[iFrameByte] = 0;
		g_dJTAGFrames[iFrameByte + 1] = 0;
	}

	if(!bPadding) {
		g_dJTAGFrames[iFrameByte] |= (((uint16_t)bTMS << 4) | (uint16_t)bTDI) << iFrameBit;
		if(!bReadTDO) g_dJTAGFrames[iFrameByte + 1] |= 1 << iFrameBit;
		else {
			g_dJTAGFrames[iFrameByte + 1] |= 0b10001 << iFrameBit;
			g_iTDOBits++;
		}
	}

	g_iFrameBits++;
}

void JTAG_Initialize(void) {
	g_iFrameBits = 0;
	g_iTDOBits = 0;
	g_iTDORead = 0;
}

size_t JTAG_OutputSize(void) {
	return (g_iTDOBits + 7) / 8;
}

void JTAG_Flush(uchar* pTDO) {
	if(!g_iFrameBits) return;
	if(g_iTDOBits && !pTDO) crash();

	if(g_bFTDI) {
		if(pTDO) crash();
		return;
	}

	g_pTDOBuffer = pTDO;
	if((g_iFrameBits & 3) == 0) _Enqueue(0, 0, TRUE, FALSE);

	XPC_JTAGTransfer(g_hDevice, g_dJTAGFrames, g_iFrameBits, g_dJTAGFrames, g_iTDOBits);
	g_iFrameBits = 0;

	for(size_t iCursor = 0; g_iTDOBits; iCursor += 2) {
		uint16_t xFrame = (g_dJTAGFrames[iCursor + 1] << 8) | g_dJTAGFrames[iCursor];
		uint16_t xBit = (g_iTDOBits >= 16) ? 1 : (1 << (16 - g_iTDOBits));
		while(g_iTDOBits) {
			uint16_t bTDO = !!(xFrame & xBit);
			if((g_iTDORead & 7) == 0) g_pTDOBuffer[g_iTDORead >> 3] = bTDO;
			else g_pTDOBuffer[g_iTDORead >> 3] |= bTDO << (g_iTDORead & 7);
			++g_iTDORead;
			--g_iTDOBits;
			if(xBit == (1 << 15)) break;
			xBit <<= 1;
		}
	}
}

void JTAG_Commit(uchar* pTDO) {
	JTAG_Flush(pTDO);
	g_iTDORead = 0;
}

void JTAG_Enqueue(uint8_t bTMS, uint8_t bTDI, uint8_t bReadTDO) {
	if(!g_bFTDI) _Enqueue(bTDI & 1, bTMS & 1, FALSE, bReadTDO);
	else {
		if(bReadTDO) crash("Not implemented");
		if(bReadTDO) ++g_iTDOBits;
		if(ftdi_write_data(&g_tFTDI, (uint8_t[]){
			MPSSE_WRITE_TMS | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG,
			0,
			(bTMS ? 1 : 0) | ((bTDI & 0x01) << 7),
		}, 3) != 3) crash();
	}
}

void JTAG_BulkTransfer(const uchar* pTDI, const uchar* pTMS, uchar* pTDO, size_t iBits) {
	JTAG_Initialize();
	FOR_RANGE(iBit, iBits) {
		JTAG_Enqueue(pTMS[iBit >> 3] >> (iBit & 7), pTDI[iBit >> 3] >> (iBit & 7), pTDO ? TRUE : FALSE);
		if(g_iFrameBits == (4 * XPC_00A6_CHUNKSIZE - 1) || g_iTDOBits == (8 * XPC_TDO_BUFSIZE - 1)) JTAG_Flush(pTDO);
	}

	JTAG_Commit(pTDO);
}

void XPC_Disconnect(void) {
	if(g_hDevice) {
		libusb_close(g_hDevice);
		g_hDevice = NULL;
	}
}

uint8_t JTAG_Transfer(uint8_t bTMS, uint8_t bTDI) {
	uchar dTMS[] = { bTMS & 1 };
	uchar dTDI[] = { bTDI & 1 };
	JTAG_BulkTransfer(dTDI, dTMS, dTDI, 1);
	return dTDI[0]; // TDO
}

uint8_t JTAG_TransferByte(uint8_t xTMS, uint8_t xTDI) {
	uchar dBuffer[1];
	JTAG_BulkTransfer(&xTDI, &xTMS, dBuffer, 8);
	return dBuffer[0]; // TDO
}

void JTAG_WriteInstructionRegisterBits(uint8_t iValue) {
	if(g_bFTDI) {
		if(ftdi_write_data(&g_tFTDI, (uint8_t[]){ MPSSE_DO_WRITE | MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG, 4, iValue }, 3) != 3) crash();
		JTAG_Enqueue(1, (iValue >> 5), FALSE);

	} else {
		JTAG_Enqueue(0, (iValue >> 0), FALSE);
		JTAG_Enqueue(0, (iValue >> 1), FALSE);
		JTAG_Enqueue(0, (iValue >> 2), FALSE);
		JTAG_Enqueue(0, (iValue >> 3), FALSE);
		JTAG_Enqueue(0, (iValue >> 4), FALSE);
		JTAG_Enqueue(1, (iValue >> 5), FALSE);
	}
}

void JTAG_WriteInstructionRegister(uint8_t iValue) {
	JTAG_Enqueue(1, 0, 0);
	JTAG_Enqueue(1, 0, 0);
	JTAG_Enqueue(0, 0, 0);
	JTAG_Enqueue(0, 0, 0);
	JTAG_WriteInstructionRegisterBits(iValue);
	JTAG_Enqueue(1, 0, 0);
	JTAG_Enqueue(0, 0, 0);
}

void JTAG_ReadWriteDataRegisterBitsFTDI(uint8_t* dTDO, uint8_t* dTDI, uint8_t iBits) {
	if(!iBits || iBits > 8) crash();

	if(ftdi_write_data(&g_tFTDI, (uint8_t[]){
		MPSSE_LSB | MPSSE_BITMODE | MPSSE_WRITE_NEG | (dTDI ? MPSSE_DO_WRITE : 0) | (dTDO ? MPSSE_DO_READ : 0),
		iBits - 1,
		dTDI ? *dTDI : 0,
	}, 3) != 3) crash();

	if(dTDO && ftdi_read_data(&g_tFTDI, dTDO, 1) != 1) crash();
}

#define FTDI_CHUNKSIZE 65532
void JTAG_ReadWriteDataRegisterFTDI(uint8_t* dTDO, uint8_t* dTDI, uint32_t iBits) {
	uint8_t dBuffer[3 + FTDI_CHUNKSIZE];
	dBuffer[0] = MPSSE_LSB | MPSSE_WRITE_NEG;
	if(dTDI && dTDO) crash();
	if(dTDI) dBuffer[0] |= MPSSE_DO_WRITE;
	else if(dTDO) dBuffer[0] |= MPSSE_DO_READ;
	else crash();

	size_t iBytes = iBits / 8;
	size_t iRemainingBits = iBits % 8;

	for(size_t iByte = 0; iByte < iBytes;) {
		size_t iSize = MIN(FTDI_CHUNKSIZE, iBytes - iByte);

		dBuffer[1] = (iSize - 1) & 0xFF;
		dBuffer[2] = ((iSize - 1) >> 8) & 0xFF;

		if(dTDI) memcpy(dBuffer + 3, (dTDI + iByte), iSize);
		if(ftdi_write_data(&g_tFTDI, dBuffer, iSize + 3) != (ssize_t)iSize + 3) crash();
		if(dTDO) ftdi_read_data(&g_tFTDI, (dTDO + iByte), iSize);

		iByte += iSize;
	}

	if(!iRemainingBits) JTAG_Enqueue(1, 0, FALSE);
	else {
		JTAG_ReadWriteDataRegisterBitsFTDI(dTDO, dTDI ? dTDI + iBytes : NULL, iRemainingBits - 1);
		JTAG_Enqueue(1, (dTDI[iBytes] >> (iRemainingBits - 1)), FALSE);
	}
}

void JTAG_WriteDataRegister(uint8_t* dBits, size_t iBits) {
	JTAG_Enqueue(1, 0, 0);
	JTAG_Enqueue(0, 0, 0);
	JTAG_Enqueue(0, 0, 0);
	JTAG_Commit(NULL);

	if(g_bFTDI) JTAG_ReadWriteDataRegisterFTDI(NULL, dBits, iBits);
	else {
		size_t iBytes = iBits / 8;
		size_t iRemainingBits = iBits % 8;
		if(!iRemainingBits) {
			--iBytes;
			iRemainingBits += 8;
		}

		for(size_t iByte = 0; iByte < iBytes;) {
			size_t iSize = MIN(XPC_00A6_CHUNKSIZE * 2 - 1, iBytes - iByte);
			printf("Writing %lu / %lu\n", iByte, iBytes);

			uchar dZero[iSize];
			memset(dZero, 0, iSize);
			JTAG_BulkTransfer(dBits ? dBits + iByte : dZero, dZero, NULL, iSize * 8);
			iByte += iSize;
		}

		FOR_RANGE(iBit, iRemainingBits) {
			if(iBit == iRemainingBits - 1) JTAG_Enqueue(1, dBits[iBytes] >> iBit, FALSE);
			else JTAG_Enqueue(0, dBits[iBytes] >> iBit, FALSE);
		}
	}

	JTAG_Enqueue(1, 0, 0);
	JTAG_Enqueue(0, 0, 0);
}

void JTAG_ReadDataRegister(uint8_t* dBits, size_t iBits) {
	JTAG_Enqueue(1, 0, 0);
	JTAG_Enqueue(0, 0, 0);
	JTAG_Enqueue(0, 0, 0);
	JTAG_Commit(NULL);

	if(g_bFTDI) JTAG_ReadWriteDataRegisterFTDI(dBits, NULL, iBits);
	else {
		FOR_RANGE(iBit, iBits) {
			if(iBit == iBits - 1) JTAG_Enqueue(1, 0, TRUE);
			else JTAG_Enqueue(0, 0, TRUE);
		}

		JTAG_Commit(dBits);
	}

	JTAG_Enqueue(1, 0, 0);
	JTAG_Enqueue(0, 0, 0);
}
