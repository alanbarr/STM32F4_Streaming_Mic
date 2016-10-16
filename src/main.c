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
#include "ch.h"
#include "hal.h"
#include "debug.h"
#include "audio_tx.h"
#include "lwipthread.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "time_mgmt.h"
#include "random.h"

#define IP(A,B,C,D)             htonl(A<<24 | B<<16 | C<<8 | D)
#define LAN8720_IPADDR          IP(192, 168, 1, 60)
#define LAN8720_GATEWAY         IP(192, 168, 1, 254)
#define LAN8720_NETMASK         IP(255, 255, 255, 0)

#define LOCAL_PORT              50001

static THD_WORKING_AREA(waBinkingThread, 128);
static THD_FUNCTION(blinkingThread, arg) 
{
    (void)arg;
    chRegSetThreadName(__FUNCTION__);
    while (TRUE) 
    {
        BOARD_LED_GREEN_TOGGLE();
        chThdSleep(MS2ST(500));
    }
}

int main(void) 
{
    struct lwipthread_opts opts;
    uint8_t optsMAC[6] = {LAN8720_ETHADDR_0,
                          LAN8720_ETHADDR_1,
                          LAN8720_ETHADDR_2,
                          LAN8720_ETHADDR_3,
                          LAN8720_ETHADDR_4,
                          LAN8720_ETHADDR_5};
    halInit();
    chSysInit();
    debugInit();

    PRINT("Main is running",0);

    chThdCreateStatic(waBinkingThread, 
                      sizeof(waBinkingThread),
                      LOWPRIO,
                      blinkingThread,
                      NULL);

    memset(&opts, 0, sizeof(opts));
    opts.macaddress = optsMAC;
    opts.address    = LAN8720_IPADDR;
    opts.netmask    = LAN8720_NETMASK;
    opts.gateway    = LAN8720_GATEWAY;
    lwipInit(&opts);

    SC_ASSERT(randomInit());

    while(macPollLinkStatus(&ETHD1) == false)
    {
        chThdSleep(1);
    }

#if 1
    SC_ASSERT(timeInit());
    SC_ASSERT(timeNtpUpdate());

    audioTxInit();
    audioTxStart();
#endif

    while (1)
    {
        chThdSleep(S2ST(1));
    }
}



