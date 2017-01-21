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

import matplotlib.pyplot as plot
import wave
import struct

class AudioDebugLogger(object):

    def __init__(self, queue, sampling_freq, output_base_name):

        self._queue = queue
        self._sampling_freq = sampling_freq
        self._filebase = output_base_name

    def run(self):
        self._captured_bytes = b""
        self._captured_ints  = []

        while self._queue.qsize():
            parsed = self._queue.get()
            self._captured_bytes += parsed

    def save_plot(self):

        # to length of int16s
        samples = int(len(self._captured_bytes) / 2)

        conversion_str = "h" * samples

        ints = struct.unpack(conversion_str, self._captured_bytes)

        plot.plot(ints)
        plot.grid()
        plot.savefig(self._filebase + ".png")
        plot.close()

    def save_wave_file(self):

        wav = wave.open(self._filebase + ".wav", "w")

        wav.setparams((1,                   # nchannels
                       2,                   # sampwidth
                       self._sampling_freq, # framerate
                       0,                   # nframes
                       "NONE",              # comptype
                       "not compressed"     # compname
                       ))

        wav.writeframes(self._captured_bytes)
        wav.close()



