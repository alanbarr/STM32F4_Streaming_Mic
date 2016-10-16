# STM32F4 Streaming Microphone

A much neglected side project involving the STM32F4Discovery (STM32F407 variant)
and the STM32F4DIS-EXT expansion board.

# Current Status 
Far from being any where near complete.
Currently more or less just a collection of different tests.

## Building and Running

1. Update submodules 

    git submodule update --init --recursive

2. Extract LWIP

To build this project, the LWIP source code should first be extracted from the
ChibiOS repository.

Assuming the current working directory is with this README.md, issue the
following commands to extract LWIP (assuming 7z is available on your system).

    cd ../../lib/ChibiOS/ext
    7z x lwip-1.4.1_patched.7z
    cd -

2. Build 

   cd src
   make

3. Program

   ./openocd -f board/stm32f4discovery.cfg -f interface/stlink-v2-1.cfg  -c "stm32f4x.cpu configure -rtos ChibiOS"

   cd src
   arm-none-eabi-gdb -x ../utils/gdb_openocd.cfg


## Serial

    minicom --device=/dev/ttyUSB0 --baudrate=38400
