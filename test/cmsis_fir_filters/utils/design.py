################################################################################
# Copyright (c) 2016, Alan Barr
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
################################################################################

import numpy as np
import os
from scipy import signal as signal
from scipy.fftpack import fft
import matplotlib.pyplot as plot

################################################################################
##### Configuration ##### 
################################################################################

np.random.seed(42)

dir_text  = "./output/design/files/"
dir_plots = "./output/design/plots/"

file_c_coeff  = "autogen_fir_coeff"

# Sampling frequency, Hz
sampling_f = 16000.0
# Cutoff frequency, Hz
cutoff_f   = 6000.0
# Number of taps/ coefficients for FIR filter. (Order + 1)
taps_n     = 128 
# The number of samples to put into a test signal.
samples_n  = int(sampling_f / 10) # 100 ms worth of samples
# Number of samples to be processed at once (used for generated C file.)
block_n    = 64

################################################################################
##### Setup #####
################################################################################

nyquist_f  = sampling_f / 2.0

if dir_plots and not os.path.exists(dir_plots):
    os.makedirs(dir_plots)

################################################################################
##### Create signal, scaled between -1 and 1 #####
################################################################################

t = np.arange(samples_n) / sampling_f

x =      1.0 * np.cos(t*2*np.pi*500)    + \
         1.0 * np.cos(t*2*np.pi*3000)   + \
         1.0 * np.cos(t*2*np.pi*6060)   + \
         1.0 * np.cos(t*2*np.pi*7000)

x += np.random.normal(0, 0.30, samples_n)

x = x / max(abs(x))

plot.figure()
plot.plot(x)
plot.grid()
plot.title("Test Signal")
plot.xlabel("Sample Number")
plot.ylabel("Amplitude")
plot.savefig(dir_plots + "1_x_signal.png")
plot.close()

################################################################################
##### FFT the created signal #####
################################################################################

fft_x = fft(x, sampling_f)

plot.figure()
plot.plot(np.abs(fft_x[0:len(fft_x)/2]))
plot.grid()
plot.title("FFT of Test Signal")
plot.xlabel("Frequency [Hz]")
plot.ylabel("Power")
plot.savefig(dir_plots + "2_x_signal_fft.png")
plot.close()

################################################################################
##### Create FIR #####
################################################################################

fir_coeff = signal.firwin(taps_n, cutoff=cutoff_f, nyq = nyquist_f)

w, h = signal.freqz(fir_coeff)

unnormalised_f = w * nyquist_f/np.pi

fig, ax1 = plot.subplots()
plot.title('FIR Response')

ax1.plot(unnormalised_f, 20 * np.log10(abs(h)), 'b')
ax1.set_ylabel('Amplitude [dB]', color='b')
for tl in ax1.get_yticklabels():
     tl.set_color('b')
     
ax2 = ax1.twinx()
ax2.plot(unnormalised_f, np.unwrap(np.angle(h)), 'g')
ax2.set_ylabel('Angle [radians]', color='g')
for tl in ax2.get_yticklabels():
     tl.set_color('g')
     
ax1.set_xlabel('Frequency [Hz]')

plot.grid()
plot.axis('tight')
plot.savefig(dir_plots + "3_fir.png")
plot.close()

################################################################################
##### Create FIR C Files #####
################################################################################

fir_coeff_reversed = fir_coeff[::-1]

if not os.path.exists(dir_text):
    os.makedirs(dir_text)
    
# Header File
h_file_handle = open(dir_text + file_c_coeff + ".h","w")

h_file_handle.write("/* This is an auto-generated file. */\n\n")
h_file_handle.write("#ifndef __%s__\n"   %file_c_coeff.upper())
h_file_handle.write("#define __%s__\n\n" %file_c_coeff.upper())

h_file_handle.write("#define TEST_FIR_DATA_TYPE_BLOCK_LEN  %s\n" %block_n)
h_file_handle.write("#define TEST_FIR_DATA_TYPE_TAP_LEN    %s\n" %taps_n)
h_file_handle.write("#define TEST_FIR_DATA_TYPE_SIGNAL_LEN %s\n\n" %samples_n)

h_file_handle.write("extern float testSignalFloat[%s];\n"  %samples_n)
h_file_handle.write("extern float armFirCoeffFloat[%s];\n" %taps_n)

h_file_handle.write("\n#endif\n")
h_file_handle.close()

# C File
file_handle = open(dir_text + file_c_coeff + ".c","w")
file_handle.write("/* This is an auto-generated file. */\n\n")
file_handle.write("#include \"arm_math.h\"\n\n")

# Signal
file_handle.write("float testSignalFloat[" + str(samples_n) + "] = {\n")
for s in x:
    file_handle.write("    %.23f,\n" %s)
file_handle.write("};\n\n")

# Float Coeffs
file_handle.write("float armFirCoeffFloat[" + str(taps_n) + "] = {\n")
for f in fir_coeff_reversed:
    file_handle.write("    %.23f,\n" %f)
file_handle.write("};\n\n")

file_handle.close()

################################################################################
##### FIR the Created Signal ##### 
################################################################################

x_filtered = signal.lfilter(fir_coeff, [1], x)

f_expected_signal = open(dir_text + "expected_signal.txt","w")

for s in x_filtered:
    f_expected_signal.write("%.23f\n" %s)

f_expected_signal.close()

fft_x_filtered = fft(x_filtered, sampling_f)

plot.plot(abs(fft_x_filtered[0:len(fft_x_filtered)/2]))
plot.grid()
plot.title("FFT of Filtered Test Signal")
plot.xlabel("Frequency [Hz]")
plot.ylabel("Power")
plot.savefig(dir_plots + "4_x_signal_filtered_fft.png")
plot.close()


