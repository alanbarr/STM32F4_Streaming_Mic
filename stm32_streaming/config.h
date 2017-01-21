/*******************************************************************************
* Copyright (c) 2017, Alan Barr
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice, this
*   list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above copyright notice,
*   this list of conditions and the following disclaimer in the documentation
*   and/or other materials provided with the distribution.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/
#ifndef __CONFIG_H__
#define __CONFIG_H__

#define IP(A,B,C,D)                 htonl(A<<24 | B<<16 | C<<8 | D)

/******************************************************************************/
/* Board Networking Configuration */
/******************************************************************************/

/* The IP address of the board */
#define CONFIG_NET_IP_ADDR          IP(192, 168, 1, 60)

/* The available IP gateway of the board */
#define CONFIG_NET_IP_GATEWAY       IP(192, 168, 1, 254)

/* The subnet the board is attached to */
#define CONFIG_NET_IP_NETMASK       IP(255, 255, 255, 0)

/* MAC address of the board */
#define CONFIG_NET_ETH_ADDR_0       0xC2
#define CONFIG_NET_ETH_ADDR_1       0xAF
#define CONFIG_NET_ETH_ADDR_2       0x51
#define CONFIG_NET_ETH_ADDR_3       0x03
#define CONFIG_NET_ETH_ADDR_4       0xCF
#define CONFIG_NET_ETH_ADDR_5       0x46

/******************************************************************************/
/* Audio Streaming Configuration */
/******************************************************************************/

/* TCP port number to accept management connections on */
#define CONFIG_AUDIO_MGMT_PORT      20000

/* UDP port number which will be the source of the audio stream */
#define CONFIG_AUDIO_SOURCE_PORT    40000


#endif /* Header Guard */
