/*******************************************************************************
* Copyright (c) 2016, Alan Barr
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
#include "ch.h"
#include "project.h"
#include "lwipthread.h"
#include <string.h>
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/err.h"

#define IP(A,B,C,D)             htonl(A<<24 | B<<16 | C<<8 | D)
#define LAN8720_IPADDR          IP(192, 168, 1, 60)
#define LAN8720_GATEWAY         IP(192, 168, 1, 1)
#define LAN8720_NETMASK         IP(255, 255, 255, 0)

#define LAN8720_ETHADDR_0       0xC2
#define LAN8720_ETHADDR_1       0xAF
#define LAN8720_ETHADDR_2       0x51
#define LAN8720_ETHADDR_3       0x03
#define LAN8720_ETHADDR_4       0xCF
#define LAN8720_ETHADDR_5       0x46

#define LOCAL_PORT              50001

void lan8720Init(void)
{
    struct lwipthread_opts opts;
    uint8_t optsMAC[6] = {LAN8720_ETHADDR_0,
                          LAN8720_ETHADDR_1,
                          LAN8720_ETHADDR_2,
                          LAN8720_ETHADDR_3,
                          LAN8720_ETHADDR_4,
                          LAN8720_ETHADDR_5};


    memset(&opts, 0, sizeof(opts));
    opts.macaddress = optsMAC;
    opts.address    = LAN8720_IPADDR;
    opts.netmask    = LAN8720_NETMASK;
    opts.gateway    = LAN8720_GATEWAY;

    lwipInit(&opts);
}

void lan8720Shutdown(void)
{


}


void lan8720TestUDP(void)
{
    err_t lwipErr = ERR_OK;
    struct netconn *udpConn = NULL;
    struct netbuf *udpSendBuf = NULL;
    struct netbuf *udpRecvBuf = NULL;
    uint8_t *payload = NULL;
    ip_addr_t remoteIp;
    uint16_t remotePort = 0; 
    uint32_t rxDataLength = 0;

    if (NULL == (udpConn = netconn_new(NETCONN_UDP)))
    {
        while(1);
    }
    
    if (ERR_OK != (lwipErr = netconn_bind(udpConn, IP_ADDR_ANY, LOCAL_PORT)))
    {
        PRINT("LWIP ERROR: %s", lwip_strerr(lwipErr));
        while(1);
    }

    while(1)
    {
        if (ERR_OK != (lwipErr = netconn_recv(udpConn, &udpRecvBuf)))
        {
            if (ERR_IS_FATAL(lwipErr))
            {
                while(1);
            }

            chThdSleep(MS2ST(2));
            continue;
        }

        remoteIp     = *netbuf_fromaddr(udpRecvBuf);
        remotePort   = netbuf_fromport(udpRecvBuf);
        rxDataLength = netbuf_len(udpRecvBuf);

        udpSendBuf = netbuf_new();

        if (NULL == (payload = netbuf_alloc(udpSendBuf, rxDataLength)))
        {
            while(1);
        }

        netbuf_copy(udpRecvBuf, payload, rxDataLength);
    
        if (ERR_OK != (lwipErr = netconn_sendto(udpConn, udpSendBuf,
                                                &remoteIp, remotePort)))
        {
            PRINT("LWIP ERROR: %s", lwip_strerr(lwipErr));
            while(1);
        }

        netbuf_delete(udpSendBuf);
        netbuf_delete(udpRecvBuf);
    }
}


