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

import time
import queue
import argparse

from receiver import AudioReceiver
from debug_logger import AudioDebugLogger
from debug_generator import AudioDebugGenerator
from playback import AudioPlayback
from stm32 import Stm32AudioSource


def stream(sink_ip,
           sink_port,
           device_ip=None,
           device_port=None,
           run_time=5,
           save=False):

    sampling_freq = 16000

    audio_queue = queue.Queue()
    queues = [audio_queue]

    if save:
        debug_queue = queue.Queue()
        queues += [debug_queue]

    receiver = AudioReceiver(sink_ip=sink_ip,
                             sink_port=sink_port,
                             queues=queues)
    receiver.run()

    if device_ip:
        audio_source = Stm32AudioSource(stm32_ip=device_ip,
                                        stm32_port=device_port,
                                        sink_ip=sink_ip,
                                        sink_port=sink_port)
    else:
        print("device ip not specified - generating local audio")
        audio_source = AudioDebugGenerator(sink_ip=sink_ip,
                                           sink_port=sink_port,
                                           sampling_freq=sampling_freq,
                                           samples_per_message=320)

    audio_source.start()

    playback = AudioPlayback(queue=audio_queue,
                             sampling_freq=sampling_freq)
    playback.start()

    while run_time:
        time.sleep(1)
        run_time -= 1

    playback.stop()
    receiver.close()
    audio_source.stop()

    if save:
        audio_logger = AudioDebugLogger(queue=debug_queue,
                                        output_base_name="samples",
                                        sampling_freq=sampling_freq)
        audio_logger.run()
        audio_logger.save_plot()
        audio_logger.save_wave_file()


if __name__ == "__main__":

    parser = argparse.ArgumentParser(description="""
                Starts, receives and plays an audio stream from a STM32 running
                the companion software. """)

    parser.add_argument("--device-ip",
                        nargs="?",
                        type=str,
                        help="STM32 IP Address."
                             "If this is not provided a test/debug signal is "
                             "generated.")

    parser.add_argument("--device-port",
                        nargs="?",
                        type=int,
                        help="STM32 management channel port number.")

    parser.add_argument("--local-ip",
                        nargs="?",
                        type=str,
                        help="Local IP address to use.")

    parser.add_argument("--local-port",
                        nargs="?",
                        type=int,
                        default=54321,
                        help="Local UDP port number to receive audio data on.")

    parser.add_argument("--save-samples",
                        action="store_true",
                        help="Save recorded audio and graph. "
                             "Be mindful of the running time when using this "
                             "option.")

    parser.add_argument("--run-time",
                        nargs="?",
                        type=int,
                        default=5,
                        help="Run for specified number of seconds. "
                             "Actual run time will be slightly longer than this.")

    cli_args = parser.parse_args()

    print(cli_args)

    stream(sink_ip=cli_args.local_ip,
           sink_port=cli_args.local_port,
           device_ip=cli_args.device_ip,
           device_port=cli_args.device_port,
           save=cli_args.save_samples,
           run_time=cli_args.run_time)
