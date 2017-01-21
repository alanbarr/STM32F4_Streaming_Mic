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

#include <string.h>
#include "ch.h"
#include "hal.h"
#include "debug.h"
#include "lwipthread.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "random.h"
#include "audio_control_server.h"
#include "audio_tx.h"
#include "config.h"

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
    AudioControlConfig audioControlConfig;
    struct lwipthread_opts opts;
    uint8_t optsMAC[6] = {CONFIG_NET_ETH_ADDR_0,
                          CONFIG_NET_ETH_ADDR_1,
                          CONFIG_NET_ETH_ADDR_2,
                          CONFIG_NET_ETH_ADDR_3,
                          CONFIG_NET_ETH_ADDR_4,
                          CONFIG_NET_ETH_ADDR_5};
    halInit();
    chSysInit();
    debugInit();

    chThdCreateStatic(waBinkingThread, 
                      sizeof(waBinkingThread),
                      LOWPRIO,
                      blinkingThread,
                      NULL);

    memset(&opts, 0, sizeof(opts));
    opts.macaddress = optsMAC;
    opts.address    = CONFIG_NET_IP_ADDR;
    opts.netmask    = CONFIG_NET_IP_NETMASK;
    opts.gateway    = CONFIG_NET_IP_GATEWAY;
    lwipInit(&opts);

    SC_ASSERT(randomInit());

    while (macPollLinkStatus(&ETHD1) == false)
    {
        chThdSleep(1);
    }

    memset(&audioControlConfig, 0, sizeof(audioControlConfig));
    audioControlConfig.localAudioSourcePort = CONFIG_AUDIO_SOURCE_PORT;
    audioControlConfig.localMgmtPort        = CONFIG_AUDIO_MGMT_PORT;
    SC_ASSERT(audioControlInit(&audioControlConfig));

    PRINT("Device MAC: %02x:%02x:%02x:%02x:%02x:%02x",
          optsMAC[0],
          optsMAC[1],
          optsMAC[2],
          optsMAC[3],
          optsMAC[4],
          optsMAC[5]);
    PRINT("Device IP:  %u.%u.%u.%u",
          ip4_addr1(&opts.address),
          ip4_addr2(&opts.address),
          ip4_addr3(&opts.address),
          ip4_addr4(&opts.address));
    PRINT("Mgmt Port:  %u", audioControlConfig.localMgmtPort);

    while (1)
    {
        chThdSleep(S2ST(1));
    }

    return 0;
}



