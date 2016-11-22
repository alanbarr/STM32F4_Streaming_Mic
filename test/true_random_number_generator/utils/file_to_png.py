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

import argparse
import os
import sys
from PIL import Image

def colour_image(input_file_name, width, height):

    binary = get_input_data(input_file_name,
                            width * height * 3)

    i = Image.new("RGB", (width, height), "white")
    pix = i.load()
    
    index = 0
    for w in range(width):
        for h in range(height):
            pix[w, h] = (binary[index],
                         binary[index+1],
                         binary[index+2])
            index += 3
    
    return i

def bw_image(input_file_name, width, height):

    binary = get_input_data(input_file_name,
                            width * height)

    i = Image.new("1", (width, height), "white")
    pix = i.load()
    
    index = 0
    for w in range(width):
        for h in range(height):
            pix[w, h] = binary[index] & 1
            index += 1
    
    return i

def get_input_data(input_file, number_bytes):

    try:
        file_stats = os.stat(input_file)
    except FileNotFoundError:
        print("Input file does not exist.", file=sys.stderr)
        exit(1)

    if file_stats.st_size < number_bytes:
        print("Input file too small to generate desired output.", file=sys.stderr)
        exit(1)

    file_handle = open(input_file, "rb")
    data = file_handle.read(number_bytes)
    file_handle.close()

    return data

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description=
            """
            Reads a file and presents its binary information as an image
            file.

            When using the monochrome output option, only the lowest bit of each
            byte is used to represt a pixel.""")

    parser.add_argument("--input", 
                        nargs=1, 
                        help="Input file.")

    parser.add_argument("--output",
                        nargs=1, 
                        help="Output image.")

    parser.add_argument("--type",
                        nargs=1, 
                        help="Colour or monochrome output format. "
                             "Defaults to \"colour\".",
                        default=["colour"],
                        choices=("colour", "monochrome"))

    parser.add_argument("--height",
                        nargs=1, 
                        help="Output image height in pixels.",
                        default=[1080],
                        type=int)

    parser.add_argument("--width",
                        nargs=1, 
                        help="Output image width in pixels.",
                        default=[1920],
                        type=int)

    cli_args = parser.parse_args()

################################################################################

    if cli_args.type[0] == "colour":
        image = colour_image(cli_args.input[0],
                             cli_args.width[0],
                             cli_args.height[0])
    else:
        image = bw_image(cli_args.input[0],
                         cli_args.width[0],
                         cli_args.height[0])

    image.save(cli_args.output[0], "PNG")

