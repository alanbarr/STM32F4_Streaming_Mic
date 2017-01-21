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

import socket
import operator
import struct
import threading
import time
import numpy


################################################################################
# Configuration
################################################################################
# The list of frequencies to be superimposed in the output wave form
freqs = [500, 2000, 4000, 7000, 10000, 11000]


class AudioDebugGenerator(object):

    def __init__(self,
                 sink_ip,
                 sink_port,
                 sampling_freq,
                 samples_per_message):

        self.bytes_per_msg = samples_per_message * 2

        self.between_packet_sleep = 1.0 / (sampling_freq/samples_per_message)

        self.sink_ip = sink_ip
        self.sink_port = sink_port
        self.tx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

        self.should_stop = threading.Event()

        self._init_signal(sampling_freq=sampling_freq)

    def _init_signal(self, sampling_freq):
        audio_signal = [0] * sampling_freq

        t = numpy.arange(sampling_freq) / float(sampling_freq)

        for freq in freqs:
            tone = numpy.cos(t*2*numpy.pi*freq)
            audio_signal = list(map(operator.add, audio_signal, tone))

        scaling = 32760 / numpy.max(audio_signal)

        self.signal = b""
        for sample in audio_signal:
            sample = sample * scaling
            self.signal += struct.pack("h", int(sample))

    def _stream_data(self):
        while self.should_stop.is_set() is False:
            message = "FakeRTPHeadr".encode("ASCII") + \
                       self.signal[0:self.bytes_per_msg]
            self.tx_sock.sendto(message, (self.sink_ip, self.sink_port))
            time.sleep(self.between_packet_sleep)

    def start(self):
        self.should_stop.clear()
        self.tx_thread = threading.Thread(target=self._stream_data)
        self.tx_thread.start()
        print("started tx thread")

    def stop(self):
        self.should_stop.set()
        self.tx_thread.join()
