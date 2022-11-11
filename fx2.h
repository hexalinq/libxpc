#pragma once
#include <linux/usb/ch9.h>
#include <libusb-1.0/libusb.h>

#define CYPRESS_FX2_RW_INTERNAL 0xA0
#define CYPRESS_FX2_CPUCS_ADDRESS 0xE600
#define CYPRESS_FX2_CPUCS_RESET 0
#define CYPRESS_FX2_CPUCS_HALT 1

static void _CypressFX2WriteRAM(struct libusb_device_handle* hDevice, uint16_t iAddress, unsigned char* pData, uint16_t iLength) {
	if(libusb_control_transfer(hDevice, USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE, CYPRESS_FX2_RW_INTERNAL, iAddress, 0, pData, iLength, 4000) != iLength) crash();
}
