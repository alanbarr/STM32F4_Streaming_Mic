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
         
import threading
import socket
import struct

RTP_HEADER_LEN = 12
UINT16_LEN = 2

class AudioReceiver(object):

    def __init__(self, sink_ip, sink_port, queues):

        self._rx_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._rx_sock.bind((sink_ip, sink_port))
        self._queues = queues

    @staticmethod
    def _get_rtp_header(rx_data):
        rtp_header_str = "!ccHII"
        (b1, b2, sequence, timestamp, ssrc) = struct.unpack(rtp_header_str,
                                                            rx_data[:RTP_HEADER_LEN])
        return (b1, b2, sequence, timestamp, ssrc)

    @staticmethod
    def _get_rtp_payload(rx_data):

        len_uint16s = int(len(rx_data[RTP_HEADER_LEN:]) / UINT16_LEN)

        conversion_str = "!"  + "h" * len_uint16s

        payload_ints = struct.unpack(conversion_str, rx_data[RTP_HEADER_LEN:])
    
        payload_bytes = b""
        for sample in payload_ints:
            payload_bytes += struct.pack("h", sample)

        return (payload_ints, payload_bytes)
    
    def _socket_receiver(self):

        while self._should_stop.is_set() == False:
            parsed = {}
            (data_bytes, (src_ip, src_port)) = self._rx_sock.recvfrom(65535)
            (parsed["ints"], parsed["bytes"]) = self._get_rtp_payload(data_bytes)

            for q in self._queues:
                q.put(parsed["bytes"])

    def run(self):
        self._should_stop = threading.Event()
        self._rx_thread = threading.Thread(target=self._socket_receiver)
        self._rx_thread.start()

    def close(self):
        self._should_stop.set()
        self._rx_thread.join()
        self._rx_sock.close()


