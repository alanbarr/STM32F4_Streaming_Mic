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

class RTPListener(object):

    def __init__(self,
                 local_ip,
                 local_port):

        self.local_ip   = local_ip
        self.local_port = int(local_port)
    
    def _loop(self, sock):
        while (True):
            try:
                data_bytes, (remote_ip, remote_port) = sock.recvfrom(1000)
                print("%u bytes from %s" % (len(data_bytes), remote_ip))
            except socket.error:
                time.sleep(1.0/1000)
                continue

    def start(self):
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((self.local_ip, self.local_port))
        print("Listening on %s:%u" % (self.local_ip, self.local_port))
        sock.setblocking(0)

        try:
            self._loop(sock)
        except KeyboardInterrupt:
            sock.close()

if __name__ == "__main__":
    
    parser = argparse.ArgumentParser(description=
            """
            A Simple RTP UDP Listener.
            """)
    parser.add_argument("--ip", "-i", 
                        nargs="?", 
                        help="Local IP Address to listen on.")
    parser.add_argument("--port", "-p",
                        nargs="?", 
                        help="Local Port Number to listen on.")
    cli_args = parser.parse_args()

    test = RTPListener(cli_args.ip,cli_args.port)
    test.start()

