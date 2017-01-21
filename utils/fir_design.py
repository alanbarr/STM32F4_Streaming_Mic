#! /usr/bin/env python3
################################################################################
# Copyright (c) 2017, Alan Barr
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
import time

################################################################################
##### Configuration ##### 
################################################################################

dir_text  = "./output/design/files/"
dir_plots = "./output/design/plots/"

file_c_coeff  = "autogen_fir_coeffs"

# Sampling frequency, Hz
sampling_f = 1.024 * 10**6
# Cutoff frequency, Hz
cutoff_f   = 6000.0
# Number of taps/ coefficients for FIR filter. (Order + 1)
taps_n     = 256
# Number of samples to be processed at once (used for generated C file.)

################################################################################
##### Setup #####
################################################################################

nyquist_f  = sampling_f / 2.0

if dir_plots and not os.path.exists(dir_plots):
    os.makedirs(dir_plots)

header_string  = "/*\n"
header_string += "This is an auto-generated file.\n\n"
header_string += "Generated on: %s\n\n" %(time.strftime("%Y-%m-%d %H:%M"))
header_string += "Design Parameters:\n"
header_string += "    Sampling Frequency: %s Hz\n" %(str(sampling_f))
header_string += "    Cutoff Frequency: %s Hz\n" %(str(cutoff_f))
header_string += "    Taps: %s\n" %(str(taps_n))
header_string += "*/\n\n"
################################################################################
##### Create FIR #####
################################################################################

fir_coeff = signal.firwin(taps_n, cutoff=cutoff_f, nyq = nyquist_f)

w, h = signal.freqz(fir_coeff)

unnormalised_f = w * nyquist_f/np.pi
unnormalised_f = unnormalised_f[0:20]

fig, ax1 = plot.subplots()
plot.title('FIR Response')

ax1.plot(unnormalised_f, 20 * np.log10(abs(h))[0:20], 'b')
ax1.set_ylabel('Amplitude [dB]', color='b')
for tl in ax1.get_yticklabels():
     tl.set_color('b')
     
ax2 = ax1.twinx()
ax2.plot(unnormalised_f, np.unwrap(np.angle(h))[0:20], 'g')
ax2.set_ylabel('Angle [radians]', color='g')
for tl in ax2.get_yticklabels():
     tl.set_color('g')
     
ax1.set_xlabel('Frequency [Hz]')

plot.grid()
plot.axis('tight')
plot.savefig(dir_plots + "fir.png")
plot.close()

################################################################################
##### Create FIR C Files #####
################################################################################

fir_coeff_reversed = fir_coeff[::-1]

if not os.path.exists(dir_text):
    os.makedirs(dir_text)
    
# Header File
h_file_handle = open(dir_text + file_c_coeff + ".h","w")

h_file_handle.write(header_string)
h_file_handle.write("#ifndef __%s__\n"   %file_c_coeff.upper())
h_file_handle.write("#define __%s__\n\n" %file_c_coeff.upper())

h_file_handle.write("#include \"arm_math.h\"\n\n")

h_file_handle.write("#define FIR_COEFFS_LEN    %s\n" %taps_n)

h_file_handle.write("extern float32_t firCoeffs[FIR_COEFFS_LEN];\n")

h_file_handle.write("\n#endif\n")
h_file_handle.close()

# C File
file_handle = open(dir_text + file_c_coeff + ".c","w")
file_handle.write(header_string)

file_handle.write("#include \"" + file_c_coeff + ".h\"\n\n")

# Float Coeffs
file_handle.write("float32_t firCoeffs[FIR_COEFFS_LEN] = {\n")
for f in fir_coeff_reversed:
    file_handle.write("    %.23f,\n" %f)
file_handle.write("};\n\n")

file_handle.close()


