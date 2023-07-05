#pragma once
#include <libusb-1.0/libusb.h>

#define CYPRESS_FX2_RW_INTERNAL 0xA0
#define CYPRESS_FX2_CPUCS_ADDRESS 0xE600
#define CYPRESS_FX2_CPUCS_RESET 0
#define CYPRESS_FX2_CPUCS_HALT 1

#define XPC_DLC9LP_VENDOR 0x03FD
#define XPC_DLC9LP_PRODUCT_BOOT 0x000F
#define XPC_DLC9LP_PRODUCT_FW 0x0008
#define XPC_DLC9LP_FIRMWARE_VERSION 0x0404

void XPC_CypressFX2WriteRAM(struct libusb_device_handle* hDevice, uint16_t iAddress, unsigned char* pData, uint16_t iLength);
void XPC_DLC9LP_UploadFirmware(struct libusb_device_handle* hDevice);
