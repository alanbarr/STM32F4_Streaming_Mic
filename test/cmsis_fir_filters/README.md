# CMSIS FIR Filtering Function Comparison

The files here to produce a crude comparison of CMSIS DSP FIR filtering
functions. The steps required to run this test are listed below.

You can see a write up of this test at: 
[www.theunterminatedstring.com/cmsis-arm-fir-comparison](http://www.theunterminatedstring.com/cmsis-arm-fir-comparison).

Note: The comparisons made by these scripts are very rough, and are intended to
provide a ballpark figure. For instance, there are some accuracy issues with how
floating point numbers are being pulled from GDB.

To generate the FIR filter coefficients and test signal the following
dependancies will need to be met:
* Python (version 2)
* NumPy
* SciPy
* matplotlib. 

You hopefully should be able to obtain these with ease via your package manager
(GNU/Linux assumed). Alternatively, [Sage Math](http://www.sagemath.org/),
provides them, packaged up along with other related tools. 

## Create FIR Filter

The file `utils/design.py`, when executed, produces:
* FIR filter coefficients
* a test signal
* the expected filtered signal
* some plots of the FIR filter response and signal.

The output of this script can be found in `output/design/`. This directory
structure will be created relative to where this file was executed.

The auto-generated C files, found in `output/design/files` need to be moved to
`src/`.

The generated plots and be viewed in `output/design/plots`, if so desired.

## Make & Execute on MCU

With the output of design.py, all the files required to run the test against a
STM32F4DISCOVERY (STM32F407) should now be present.
Optimised and unoptimised performance of the CMSIS filter functions was of
interest to me so to reproduce this, this step will need to be repeated. The
variable `USE_OPT` in `src/Makefile` should be altered as appropriate between
runs, and the log file name changes as appropriate. The names I used can be seen
sequentially in the command list below (obviously the last call of `set logging
file` will set the log file to be used).

Once the program has been built and executed to completion on the MCU, the
GDB commands listed below will retrieve the required data for comparison.

### GDB Commands

    set height 0
    set print elements 0
    set print array on
    set logging file gdb_data_out_unopt.txt
    set logging file gdb_data_out_opt.txt
    set logging overwrite on
    set logging on
    print convertedOutput
    print time
    set logging off


## Comparison

The script `utils/compare.py` can now be run to perform the comparison
of the expected filtered signal as calculated by Python, and the various
filtered signals calculated by CMSIS on the MCU. This script requires several
arguments, which are described in the file itself.

The output of this script is directed towards `output/comparison/`, relative to
where it was executed.

