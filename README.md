# Xilinx Platform Cable USB II (DLC9LP) driver

### This is a simple tool for those, who would prefer an open-source alternative to Xilinx iMPACT for FPGA programming

## Usage
- You need a Linux machine, libusb-1.0, libftdi1, make, and a recent version of GCC to compile the project
- Plug the cable into a USB port and type `./xpc init` to load the firmware
- Type `./xpc identify` to ensure that the FPGA board is detected properly
- `./xpc reset` can be used to reset the FPGA board
- To program the FPGA using a bit file, simply type `./xpc load path/to/bitfile.bit`
