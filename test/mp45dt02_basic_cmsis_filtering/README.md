You can find a related write-up of PDM at:
www.theunterminatedstring.com/probing-pdm/

# I2S Setup:

The I2S driver is configured to sample the MP45DT02 at 1024 kHz. This 1-bit data
stream is ultimately used as the input to a FIR filter with decimation factor of
64. The result of filtering is a 16 kHz PCM output stream.

To achieve this the following values were used:

## I2S Settings in src/mp45dt02_pdm.c:

    I2SDIV = 42
    I2SODD = 0
    
## I2S Settings in src/mcuconf.h:

    HSE Clock = 8 MHz
    PLLM      = 8
    PLLI2SN   = 258
    PLLI2SR   = 3

## STM32 pinout:

    MP45DT02 CLK = PB10
    MP45DT02 PDM = PC3

# Operation Notes: 

1. Configured to capture 1 ms of 1024 kHz data between I2S interrupts.
   The I2S buffer needs to hold a total of 2 ms worth of data due to interrupts
   occuring at the buffer half full point.
   The buffer therefore needs to hold a total of 2048 1-bit samples (spread over
   128 uint16_t's).

2. When an I2S interrupt occurs, each I2S sampled bit is "expanded" - moved from
   existing as a bitfield in the I2S data to it's own float. This requires 1024
   floats to hold 1 ms of data.

3. The array of floats are filtered and decimated by a factor of 64 to produce a
   1 ms sample consisting of 16 floats.

4. These are added to the output buffer which stores 1.6 s of audio (25,600
   floats).

5. A 1.6 second audio sample will be taken at first run, and with each
   subsequent press of the user button on the STM32F4DISCOVERY.

6. When run with the GDB script (see below) the audio sample will be extracted
   via GDB logs allowing further examination.

# Utils

## utils/fir_design.py

Computes CMSIS compatible FIR filter coefficients.
The following configuration variables in the file are of note:

* `sampling_f` - Sampling frequency
* `cutoff_f` - Number of taps
* `taps_n` - Cutoff frequency

The output wave file can be found, relative to where the script was run:

* `./output/design/plots/fir.png` - FIR response
* `./output/design/files/*` - Generated C source and header file for CMSIS

## utils/generate_tone_wave_file.py

Generates a simple wave audio file, with a configurable number of
superimposed tones.

Tones can be changed by modifying the list `freqs`.

The output wave file can be found, relative to where the script was run,
`./output/test/audio/tones.wave`.

## utils/gdb_openocd.cfg

This script can be sourced with `arm-none-eabi-gdb -x utils/gdb_openocd.cfg`.

It contains GDB commands to be used to run and extract data for testing
purposes.

The commands set a breakpoint in the code, which will be hit after the
audio sample has been recorded. After being hit, the sample will be read from
the MCU.

This will be logged to a file `./gdb_output.txt`, location relative to where GDB
was started.

## utils/signal_inspection_of_gdb_log.py

This will process the GDB log file containing the audio samples, preforming an
FFT on the signal and additionally saving the signal to a wave audio file.

It requires one argument, which is the GDB log file to be parsed.

The output from the script will be saved relative to where the script is run:

* `./output/inspection/audio/recorded.wave` - wave PCM audio of waveform
* `./output/inspection/plots/1_signal.png` - the waveform
* `./output/inspection/plots/2_signal_fft.png` - FFT of waveform



