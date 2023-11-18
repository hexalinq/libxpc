#include "platform.h"
#include "bitfile.h"
#include "driver.h"
#include "dlc9lp.h"

static void _Reset(void) {
	JTAG_RunTestIdle();
	JTAG_WriteInstructionRegister(FPGA_INSTRUCTION_JPROGRAM);
	JTAG_RunTestIdle();
}

static void _Init(void) {
	if(XPC_Initialize(XPC_DLC9LP_VENDOR, XPC_DLC9LP_PRODUCT_FW)) {
		if(XPC_InitializeFTDI(0x0403, 0x6014)) {
			if(XPC_InitializeFTDI(0x0403, 0x6010)) {
				crash("Platform cable initialization failure");
			}
		}
	}
}

int main(int argc, char** argv) {
	double fStart = timef();
	if(libusb_init(NULL)) crash();

	if(argc == 2 && !strcmp(argv[1], "identify")) {
		_Init();
		printf("The cable and the FPGA both seem to be connected properly.\n");
		printf("Reading FPGA serial number...\n");
		JTAG_RunTestIdle();
		JTAG_Commit(NULL);
		uint8_t dTDOBuffer[4];
		JTAG_ReadDataRegister(dTDOBuffer, sizeof(dTDOBuffer) * 8);
		JTAG_Commit(NULL);

		printf("%02X %02X %02X %02X\n", dTDOBuffer[3], dTDOBuffer[2], dTDOBuffer[1], dTDOBuffer[0]);

	} else if(argc == 2 && !strcmp(argv[1], "reset")) {
		_Init();
		_Reset();
		JTAG_Commit(NULL);

	} else if(argc == 3 && !strcmp(argv[1], "load")) {
		_Init();

		FILE* hFile;
		if(!strcmp(argv[2], "-")) hFile = stdin;
		else {
			hFile = fopen(argv[2], "r");
			if(!hFile) crash("Failed to open the bitstream file");
		}

		struct BitFile_Sections* pSections;
		if(BitFile_Read(hFile, &pSections)) crash("Corrupted bitstream");

		printf("Bitstream description: %s\n", pSections->sDescription);
		printf("Bitstream part: %s\n", pSections->sPart);
		printf("Bitstream timestamp: %s %s\n", pSections->sTimestampDate, pSections->sTimestampTime);
		printf("Bitstream length: %lu\n", pSections->iBitStreamLength);

		FOR_RANGE(iByte, pSections->iBitStreamLength) {
			uint8_t iValue = pSections->dBitStream[iByte];
			uint8_t iReversed = 0;
			FOR_RANGE(iBit, 8) if(iValue & (1 << iBit)) iReversed |= 1 << (7 - iBit);
			pSections->dBitStream[iByte] = iReversed;
		}

		_Reset();
		JTAG_WriteInstructionRegister(FPGA_INSTRUCTION_JCONFIG);
		JTAG_Commit(NULL);
		printf("Initialization took %.2f second(s)\n", timef() - fStart);

		fStart = timef();
		JTAG_WriteDataRegister(pSections->dBitStream, pSections->iBitStreamLength * 8);
		JTAG_Commit(NULL);
		printf("Programming took %.2f second(s)\n", timef() - fStart);

		JTAG_WriteInstructionRegister(FPGA_INSTRUCTION_JSTART);
		FOR_RANGE(i, 16) JTAG_Enqueue(0, 0, 0);

		JTAG_RunTestIdle();
		JTAG_Commit(NULL);

		BitFile_Free(pSections);
		fclose(hFile);

	} else if(argc == 2 && !strcmp(argv[1], "init")) {
		struct libusb_device_handle* hDevice = XPC_Connect(XPC_DLC9LP_VENDOR, XPC_DLC9LP_PRODUCT_BOOT);
		if(!hDevice) crash("Platform cable not found or already initialized");
		XPC_CypressFX2WriteRAM(hDevice, CYPRESS_FX2_CPUCS_ADDRESS, (uint8_t[]){ CYPRESS_FX2_CPUCS_HALT }, 1);
		XPC_DLC9LP_UploadFirmware(hDevice);
		XPC_CypressFX2WriteRAM(hDevice, CYPRESS_FX2_CPUCS_ADDRESS, (uint8_t[]){ CYPRESS_FX2_CPUCS_RESET }, 1);

	} else {
		fprintf(stderr, "Usage: %s identify\n", argv[0]);
		fprintf(stderr, "       %s reset\n", argv[0]);
		fprintf(stderr, "       %s init\n", argv[0]);
		fprintf(stderr, "       %s load <bitfile.bit>\n", argv[0]);
		return 1;
	}

	XPC_Disconnect();
	libusb_exit(NULL);
	return 0;
}
