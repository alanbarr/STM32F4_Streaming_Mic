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
#include <string.h>
#include <stdint.h>
#include "ch.h"
#include "project.h"
#include "lwipthread.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "random.h"

#define PAYLOAD_SAMPLES         365

void ipInit(void)
{
    struct lwipthread_opts opts;
    uint8_t optsMAC[6] = {STM32_ETHADDR_0,
                          STM32_ETHADDR_1,
                          STM32_ETHADDR_2,
                          STM32_ETHADDR_3,
                          STM32_ETHADDR_4,
                          STM32_ETHADDR_5};


    memset(&opts, 0, sizeof(opts));
    opts.macaddress = optsMAC;
    opts.address    = STM32_IPADDR;
    opts.netmask    = STM32_NETMASK;
    opts.gateway    = STM32_GATEWAY;

    lwipInit(&opts);
}

void ipTransmitRandomLoop(void)
{
    err_t lwipErr = ERR_OK;
    struct netconn *udpConn = NULL;
    struct netbuf *udpSendBuf = NULL;
    uint8_t *payload = NULL;
    ip_addr_t remoteIp;
    uint16_t remotePort;

    uint32_t index = 0;
    uint32_t random = 0;

    remoteIp.addr = REMOTE_IP;
    remotePort    = REMOTE_PORT;

    chThdSetPriority(LOWPRIO);

    if (NULL == (udpConn = netconn_new(NETCONN_UDP)))
    {
        PRINT_ERROR("New netconn failed",0);
    }
    
    if (ERR_OK != (lwipErr = netconn_bind(udpConn, IP_ADDR_ANY, STM32_PORT)))
    {
        PRINT_ERROR("LWIP ERROR: %s", lwip_strerr(lwipErr));
    }

    while(1)
    {
        if (NULL == (udpSendBuf = netbuf_new()))
        {
            PRINT_ERROR("Failed to get netbuf.", 0);
            while(1);
        }

        if (NULL == (payload = netbuf_alloc(udpSendBuf, 
                                            PAYLOAD_SAMPLES * sizeof(uint32_t))))
        {
            PRINT_ERROR("Failed to allocate netbuf.", 0);
            while(1);
        }
    
        for (index = 0; index < PAYLOAD_SAMPLES; index++)
        {
            if (0 != randomGet(&random))
            {
                PRINT_ERROR("Failed to get random number.", 0);
            }

            memcpy(payload, &random, sizeof(random));
            payload += sizeof(random);
        }
    
        if (ERR_OK != (lwipErr = netconn_sendto(udpConn, udpSendBuf,
                                                &remoteIp, remotePort)))
        {
            PRINT_ERROR("LWIP ERROR: %s", lwip_strerr(lwipErr));
        }

        netbuf_delete(udpSendBuf);

        chThdYield();
    }
}


