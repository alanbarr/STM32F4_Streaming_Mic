# LWIP UDP Echo Test

This is a very crude test to prove LWIP is working.

This test creates a simple UDP echo server on the board - any UDP packet
received by the server will be echoed back to the sender.

The IP configuration of the server can be configured at the top of src/lan8720.c

The Python script, utils/udp_max_throughput_tester.py, can be used to provide a
rough UDP throughput calculation against the echo server.
Pass in --help for details on the required arguments.

## Extracting LWIP

To build this project, the LWIP source code should first be extracted from the
ChibiOS repository.

Assuming the current working directory is with this README.md, issue the
following commands to extract LWIP (assuming 7z is available on your system).

    cd ../../lib/ChibiOS/ext
    7z x lwip-1.4.1_patched.7z
    cd -

