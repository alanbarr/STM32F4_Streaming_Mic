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

import queue
import pyaudio
import struct
import time
import math

class AudioPlayback(object):

    def __init__(self, queue, sampling_freq, frame_count=None, is_blocking=False):

        self.frame_count = frame_count if frame_count is not None else sampling_freq/10
        self.frame_count = int(self.frame_count)

        self.is_blocking = is_blocking
        self.sampling_freq = sampling_freq

        self._queue = queue

        self.last_time = time.time()

    def start(self):
        
        if self.is_blocking:
            cb = None
        else:
            cb = self._streaming_callback

        # Empty audio frame to insert when no data
        self._empty_frame = self.frame_count * struct.pack("h", 0)

        self.pyaudio = pyaudio.PyAudio()

        self._fresh_buffering()

        self.stream = self.pyaudio.open(format=pyaudio.paInt16,
                                        channels=1,
                                        rate=self.sampling_freq,
                                        output=True,
                                        stream_callback=cb,
                                        frames_per_buffer=self.frame_count)

    def _fresh_buffering(self):

        message = self._queue.get()
        self._leftover_bytes = message

        min_queue_len = float(len(self._empty_frame)) / len(self._leftover_bytes)
        min_queue_len = math.ceil(min_queue_len)
        min_queue_len *= 8

        while (self._queue.qsize() < min_queue_len):
            time.sleep(0.010)

    def _streaming_callback(self, in_data, frame_count, time_info, status):

        self.last_time = time.time()

        new_frame = b""
        missing_bytes = frame_count * 2

        if len(self._leftover_bytes):
            new_frame += self._leftover_bytes
            missing_bytes -= len(self._leftover_bytes)
            self._leftover_bytes = b""

        while missing_bytes > 0:

            if self._queue.empty():
                new_frame += self._empty_frame[0:missing_bytes]
                missing_bytes = 0
                print("using empty")
            else:
                message = self._queue.get()
                new_frame += message
                missing_bytes -= len(message)

        # Save any excess for next time around
        if (missing_bytes < 0):
            self._leftover_bytes = new_frame[missing_bytes::1]

        return (new_frame, pyaudio.paContinue)

    def write(self, timeout):
        end_time = time.time() + timeout

        while time.time() < end_time:
            self.last_time = time.time()

            message = self._queue.get()

            before_write = time.time()
            self.stream.write(message)

    def stop(self):
        self.stream.stop_stream()
        self.stream.close()
        self.pyaudio.terminate()


