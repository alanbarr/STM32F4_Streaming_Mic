# STM32F4 Streaming Microphone

This project runs on the STM32F4Discovery (STM32F407 variant) in combination
with the STM32F4DIS-EXT expansion board.

The on-board MP45DT02 MEMS microphone is sampled and filtered to produce a 16
kHz PCM audio signal. This is streamed over UDP via the LAN8720 Ethernet Phy on
the expansion board. 

A simple Python script is provided to receive and play the audio stream.

This project was a learning exercise and should be considered as little more
than a proof of concept. If it is of interest to you, may also
want to check out some relevant [blog posts](
https://www.theunterminatedstring.com/tag/project_stm32_streaming_mic/)
written along the way.

# Building and Running

## 1. Update submodules 

    git submodule update --init --recursive

## 2. Extract LWIP

The LWIP source code should be extracted from the ChibiOS repository.

Assuming the current working directory the top level of the repo, issue the
following commands to extract LWIP. This assumes 7z is available on your system.

    cd lib/ChibiOS/ext
    7z x lwip-1.4.1_patched.7z
    cd -

## 3. Configure as Required

Modify `stm32_streaming/config.h` with the desired static IP configuration.

## 4. Build 

    cd stm32_streaming
    make
    cd -

## 5. Program

(Assuming you have an [OpenOCD](openocd.org) binary).

    ./openocd -f board/stm32f4discovery.cfg -f interface/stlink-v2-1.cfg  -c "stm32f4x.cpu configure -rtos ChibiOS"

    cd stm32_streaming
    arm-none-eabi-gdb -x ../utils/gdb_openocd.cfg

## 6. Streaming

In a separate terminal run the Python script to trigger and receive the audio
stream. The available command line options can be printed by passing in
`--help`.

    cd python_playback
    ./main.py --help

An example of invoking this script is:

    ./main.py --local-ip 192.168.1.154 --local-port 41234 --device-ip 192.168.1.60 --device-port 20000  --run-time 2

## 7. Serial Output / Logging

There is some minor debug output printed from the STM32 using ChibiOS SD1 driver
over GPIOB pins 6 and 7.

    minicom --device=/dev/ttyUSB0 --baudrate=38400

# Debian Misc Commands

- `amixer set Master 25%` Change volume
- `eog <file>` Open png image
- `aplay <file>` Play audio file

# STM32 Architecture Overview

## stm32_streaming/audio/audio_control_server.c

This module is responsible for setting up an LWIP TCP server, bound to TCP port
20000. This server accepts two ASCII commands:

- `start <ip> <port>` where ip, port is the target address for the UDP audio
stream. These are both provided in hexidecimal format, with leading zeros
present as necessary.

- `stop` stop a running audio stream

## stm32_streaming/audio/mp45dt02_processing.c

This file handles (over) sampling the MP45DT02 MEMS microphone as well as
running a CMSIS FIR filter over the data. The FIR coefficients can be found in
`stm32_streaming/audio/autogen_fir_coeffs.c` which are generated using
`utils/fir_design.py`

##  stm32_streaming/audio/audio_tx.c

This module tracks when 20 ms of audio has been collected from the MP45DT02.
Once a full payload has been stored, it requests `rtp/rtp.c` add a RTP header to
the payload and then transmits the data over UDP to the address previously
specified by the user. 

## Dependencies

The STM32 binary requires the following libraries: 

- [ChibiOS](http://www.chibios.org/dokuwiki/doku.php) for RTOS + HAL
- [LWIP](https://savannah.nongnu.org/projects/lwip/) for IP/UDP/TCP
- [CMSIS](https://developer.arm.com/embedded/cmsis) for FIR DSP

These should be met by following the steps listed above to build the project.

# Python Playback Software

In `python_playback` there are a handful of Python scripts which will request,
recieve and play the audio stream. There are also some additional debug modules.
See the run steps above for an example of running `main.py` with arguments.

## python_playback/stm32.py

Connects to the STM32 over TCP and requests that the STM32 start/stop
streaming audio.

## python_playback/receiver.py

Spawns a thread to receive the UDP packets from the STM32 and extract the audio
payload from them. These frames are added to a queue for consumption by a
different thread.

## python_playback/playback.py

Responsible for initially buffering audio packets popped from the queue and then
playing the stream via PyAudio.
The amount of buffering, chosen through trial and error, was what would
reliability work on my system. This approach was also used when picking the
default frame size passed to PyAudio of 1600 samples (10% sampling rate).

## Dependencies

Core functionality requires `pyaudio`, with some of the debug utilities
requiring `matplotlib` and `numpy`. The following commands should hopefully
obtain everything you need on a Debian system.

    apt-get install python3-pyaudio
    apt-get install python3-matplotlib python3-numpy

# Issues / Limitations

## RTP

This project borrows the RTP header to stream the audio data. There is no actual
RTP implementation.
This wasn't always the case as I had hoped to let 
[VLC](https://www.videolan.org/vlc/index.en-GB.html) take care of playback for
me.
I made some headway implementing what I considered the minimum required
functionality from the follow specifications:

* Real-Time Streaming Protocol (RTSP) ([RFC2326](https://tools.ietf.org/html/rfc2326)) 
* Real-Time Transport Protocol (RTP) [RFC3550](https://tools.ietf.org/html/rfc3550)
    * Real-Time Transport Protocol [RTP](https://tools.ietf.org/html/rfc3550#page-13)
    * Real-Time Control Protocol [RTCP](https://tools.ietf.org/html/rfc3550#page-19)
* Session Description Protocol (SDP) [RFC4566](https://tools.ietf.org/html/rfc4566)

However I eventually gave up on this approach due to a mixture of getting fed
up, and VLC appearing to expect non-standard RTSP headers.

## Python PyAudio Playback

I hoped for a low latency audio stream but ran into some issues with
PyAudio/PortAudio when trying to play 20 ms audio frames. The callback from
PortAudio was trying to consume data a lot faster than it was being provided.
Buffering a few packets alone didn't appear to alleviate PortAudio's hunger, and
neither did bumping the frame/chunk size. So both approaches were taken.

Note that this may be something to do with my specific hardware or the
VM I was attempting to play it back through. Debugging audio playback wasn't
something I particularly wanted to do - which is why I originally planned on
using VLC to avoid it altogether.

Finally, there is no Python code in place to handle lost / out of order
frames. The RTP sequence number in the packets is completely ignored.

