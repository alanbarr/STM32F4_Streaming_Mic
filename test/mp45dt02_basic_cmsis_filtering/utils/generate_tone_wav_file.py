#! /usr/bin/env python2
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
import wave
import os
import struct
import operator

################################################################################
# Docs
# Creates a wave file of specified sinusoidal waves.
################################################################################

################################################################################
# Configuration
################################################################################
# The list of frequencies to be superimposed in the output wave form
freqs               = [500, 2000, 4000, 7000, 10000, 11000]
# Output Directory
dir_files           = "./output/test/audio/"
# Sampling frequency of output wave file
sampling_freq       = 32000
# Playback duration of output wave file
sampling_duration   = 10
# Output file name
wav_file = dir_files + "tones.wave"

################################################################################

if not os.path.exists(dir_files):
    os.makedirs(dir_files)

sampling_number = sampling_duration * sampling_freq

################################################################################
t = np.arange(sampling_number) / float(sampling_freq)

signal = [0] * sampling_number

for freq in freqs:
    tone = np.cos(t*2*np.pi*freq)
    signal = map(operator.add, signal, tone)

wav = wave.open(wav_file, "w")
wav.setparams((1,                   # nchannels
               2,                   # sampwidth
               sampling_freq,       # framerate 
               0,                   # nframes
               "NONE",              # comptype
               "not compressed"     # compname
               ))

scaling = 32760 / np.max(signal)
packed = []
for sample in signal:
    sample = sample * scaling
    packed.append(struct.pack("h", int(sample)))

wav.writeframes("".join(packed))
wav.close()

print "Generated %s" %wav_file


