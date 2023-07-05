#pragma once
#include "platform.h"

#define FPGA_INSTRUCTION_INTEST 0b000111
#define FPGA_INSTRUCTION_EXTEST 0b001111
#define FPGA_INSTRUCTION_BYPASS 0b111111
#define FPGA_INSTRUCTION_HIGHZ 0b001011
#define FPGA_INSTRUCTION_JPROGRAM 0b001011
#define FPGA_INSTRUCTION_JSTART 0b001100
#define FPGA_INSTRUCTION_JSHUTDOWN 0b001101
#define FPGA_INSTRUCTION_SAMPLE 0b000001
#define FPGA_INSTRUCTION_JRDBK 0b000100
#define FPGA_INSTRUCTION_JCONFIG 0b000101
#define FPGA_INSTRUCTION_IDCODE 0b001001
#define FPGA_INSTRUCTION_USER1 0b000010
#define FPGA_INSTRUCTION_USER2 0b000001
#define FPGA_INSTRUCTION_USER3 0b100010
#define FPGA_INSTRUCTION_USER4 0b100001
#define FPGA_INSTRUCTION_USERCODE 0b001000

#define XPC_BULK_MAX 32768 // max bits per a single USB bulk transfer
#define XPC_00A6_MAX_BITS 37667
#define XPC_00A6_CHUNKSIZE (XPC_00A6_MAX_BITS / 4) // 16-bit words; each encodes 4 bits
#define XPC_TDO_BUFSIZE 1024 // max pending TDO bytes waiting to be read

extern uint8_t g_bFTDI;

struct libusb_device_handle* XPC_Connect(uint16_t iVendor, uint16_t iProduct);
int XPC_Initialize(uint16_t iVendor, uint16_t iProduct);
int XPC_InitializeFTDI(uint16_t iVendor, uint16_t iProduct);
void XPC_Disconnect(void);

void JTAG_Initialize(void);
size_t JTAG_OutputSize(void);
void JTAG_Flush(uchar* pTDO);
void JTAG_Commit(uchar* pTDO);
void JTAG_BulkTransfer(const uchar* pTDI, const uchar* pTMS, uchar* pTDO, size_t iBits);
void JTAG_Enqueue(uint8_t bTMS, uint8_t bTDI, uint8_t bReadTDO);
void JTAG_WriteInstructionRegister(uint8_t iValue);
void JTAG_WriteInstructionRegisterBits(uint8_t iValue);
uint8_t JTAG_Transfer(uint8_t bTMS, uint8_t bTDI);
static inline void JTAG_TestLogicReset(void) { FOR_RANGE(iBit, 5) JTAG_Enqueue(1, 0, 0); }
void JTAG_WriteDataRegister(uint8_t* dBits, size_t iBits);
void JTAG_ReadDataRegister(uint8_t* dBits, size_t iBits);
