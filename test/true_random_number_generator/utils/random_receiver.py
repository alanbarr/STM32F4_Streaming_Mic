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
import argparse
import sys
import signal

exit_signalled = False

class UdpLogger(object):

    def __init__(self,
                 socket = None,
                 max_bytes = None,
                 write_to_file = None):

        self.duration        = 0
        self.socket          = socket
        self.rx_packets      = 0
        self.rx_bytes        = 0

        if (write_to_file != None):
            self.out_file = open(write_to_file, "wb")
        else:
            self.out_file = open(sys.stdout.fileno(), "wb")

        self.target_bytes = int(max_bytes)

    def limited_loop(self): 

        while self.rx_bytes < self.target_bytes:

            try:
                data_bytes, (src_ip, src_port) = sock.recvfrom(65535)
            except socket.error:
                continue

            self.out_file.write(data_bytes[0:self.target_bytes - self.rx_bytes])

            self.rx_packets += 1
            self.rx_bytes   += len(data_bytes)

    def infinate_loop(self):

        while exit_signalled == False:

            try:
                data_bytes, (src_ip, src_port) = sock.recvfrom(65535)
            except socket.error:
                continue

            try:
                self.out_file.write(data_bytes)
            except BrokenPipeError:
                return

            self.rx_packets += 1
            self.rx_bytes   += len(data_bytes)

                
    def run(self):
        started = time.time()

        if self.target_bytes == 0:
            self.infinate_loop()
        else:
            self.limited_loop()

        ended = time.time()
        self.duration = ended - started

    def get_duration(self):
        return self.duration

    def get_rx_bytes(self):
        return self.rx_bytes

    def get_rx_packets(self):
        return self.rx_packets


################################################################################
def print_stats(udp_class):
    sys.stderr.write("\n############################\n")
    sys.stderr.write("Duration (s): " + str(udp_class.get_duration()) + "\n")
    sys.stderr.write("Packets Rx: " + str(udp_class.get_rx_packets()) + "\n")
    sys.stderr.write("Bytes Rx (B): " + str(udp_class.get_rx_bytes()) + "\n")

################################################################################

def sig_int_handler(signal, frame):
    global exit_signalled
    sys.stderr.write('Exiting...')
    exit_signalled = True

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description=
            """
            Logs a stream of UDP packets either to stdout or a file.
            """)

    parser.add_argument("--local-ip", 
                        nargs=1, 
                        help="Local IP Address to listen on.")

    parser.add_argument("--local-port",
                        nargs=1, 
                        help="Local Port Number to listen on.")

    parser.add_argument("--file",
                        nargs="?", 
                        help="The file to log to. If not specified, stdout is used.")

    parser.add_argument("--max-bytes",
                        nargs="?", 
                        help="Specifies the maximum number of bytes to write." +
                             "If not set, will loop infinitely.",
                        default=0)


    cli_args = parser.parse_args()

################################################################################

    signal.signal(signal.SIGINT, sig_int_handler)

################################################################################

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((cli_args.local_ip[0], int(cli_args.local_port[0])))
    
    logger = UdpLogger(socket        = sock,
                       max_bytes     = cli_args.max_bytes,
                       write_to_file = cli_args.file)
    logger.run()

    print_stats(logger)
    
    sock.close()

################################################################################
