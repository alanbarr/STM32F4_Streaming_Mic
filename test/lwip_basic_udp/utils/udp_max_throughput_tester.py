#! /usr/bin/env python3
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

import socket
import time
import threading
import os
import argparse

class UdpTest(object):

    def __init__(self,
                 socket,
                 remote_udp_ip,
                 remote_udp_port,
                 payload_length       = 128,
                 duration             = 10):

        self.tx_messages            = []
        self.rx_messages            = []
        self.payload_length         = int(payload_length)
        self.duration               = int(duration)
        self.actual_duration        = 0
        self.socket                 = socket
        self.remote                 = (remote_udp_ip, int(remote_udp_port))
        self.socket.setblocking(0)

    def _loop(self): 
        end_time = time.time() + self.duration
        while time.time() < end_time:
            # Send
            try:
                random = os.urandom(self.payload_length)
                self.socket.sendto(random, self.remote)
                self.tx_messages.append(random)
            except socket.error():
                pass

            # Receive
            try:
                data_bytes, (src_ip, src_port) = sock.recvfrom(self.payload_length)
                self.rx_messages.append(data_bytes)
            except socket.error:
                pass

        # Grab anything still being sent in reply to us
        stop_rx_time = time.time() + 0.2
        while time.time() < stop_rx_time:
            try:
                data_bytes, (src_ip, src_port) = sock.recvfrom(self.payload_length)
                self.rx_messages.append(data_bytes)
            except socket.error:
                pass

    def run(self):
        started = time.time()
        self._loop()
        ended = time.time()
        self.actual_duration = ended - started

    def number_messages_tx(self):
        return len(self.tx_messages)

    def number_messages_rx(self):
        return len(self.rx_messages)

    def valid_messages_rx(self):
        return len(set(self.tx_messages) & set(self.rx_messages))

    def number_messages_lost(self):
        return len(self.tx_messages) - self.valid_messages_rx()

    def valid_througput_bps(self):
        valid_bytes = self.valid_messages_rx() * self.payload_length
        valid_bits  = valid_bytes * 8.0
        return valid_bits / self.actual_duration

    def valid_througput_kbps(self):
        return self.valid_througput_bps()/1000

    def valid_througput_mbps(self):
        return self.valid_througput_kbps()/1000



################################################################################
def print_stats(udp_class):
    print("############################")
    print("Tx Messages: "           + str(udp_class.number_messages_tx()))
    print("Rx Messages: "           + str(udp_class.number_messages_rx()))
    print("Rx Messages (Valid): "   + str(udp_class.number_messages_rx()))
    print("Lost/Corrupt Messages: " + str(udp_class.number_messages_lost()))
    print("UDP Throughput (b/s): "  + str(udp_class.valid_througput_bps()))
    print("UDP Throughput (mb/s): "  + str(udp_class.valid_througput_mbps()))

################################################################################


if __name__ == "__main__":

    parser = argparse.ArgumentParser(description=
            """
            Attempts to send as many UDP messages as possible to the remote host.
            UDP message payload will be of specified length, filled with random data.
            """)
    parser.add_argument("--local-ip", 
                        nargs="?", 
                        help="Local IP Address to listen on.")

    parser.add_argument("--local-port",
                        nargs="?", 
                        help="Local Port Number to listen on.")

    parser.add_argument("--remote-ip",
                        nargs="?", 
                        help="Remote IP Address to communicate to.")

    parser.add_argument("--remote-port",
                        nargs="?", 
                        help="Remote Port Number to communicate to.")

    parser.add_argument("--udp-payload-length",
                        nargs="?", 
                        help="THe number of random bytes to insert into UDP payload.")


    cli_args = parser.parse_args()

################################################################################

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((cli_args.local_ip, int(cli_args.local_port)))
    
    test_thd = UdpTest(socket          = sock,
                       remote_udp_ip   = cli_args.remote_ip,
                       remote_udp_port = cli_args.remote_port,
                       payload_length  = cli_args.udp_payload_length)
    test_thd.run()
    print_stats(test_thd)
    
    sock.close()

################################################################################

