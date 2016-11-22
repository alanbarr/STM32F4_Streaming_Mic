# STM32F4 Random Number Test

Streams numbers from the STM32F407's True Random Number Generator over UDP.

IP configuration of the STM32F4 and the remote target is found in `./src/project.h`.

The `./utils/random_receiver.py` file can be used to capture the stream and
provide the data to a 3rd party test program, such as:

* [Dieharder](https://www.phy.duke.edu/~rgb/General/dieharder.php)
* [PractRand](http://pracrand.sourceforge.net/)
* [NIST Statistical Test Suite](http://csrc.nist.gov/groups/ST/toolkit/rng/documentation_software.html)


# Hardware Used:

* STM32F407G-DISC1 - STM32F407 Discovery Board 
* STM32F4DIS-EXT   - STM32F4 Discovery Base Board (Used for Ethernet Phy)

