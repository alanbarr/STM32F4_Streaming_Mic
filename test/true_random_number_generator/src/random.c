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
#include "hal.h"
#include "random.h"
#include "project.h"

static struct {

    struct {
        mutex_t mutex;
    } sync;

#if RANDOM_STATS
    struct {
        uint32_t hwDataReady;
        uint32_t hwSeedErrors;
        uint32_t hwClockErrors;
    } stats;
#endif

} rngMgmt;

int randomInit(void)
{
    chMtxObjectInit(&rngMgmt.sync.mutex);
    
    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
    RNG->CR      |= RNG_CR_RNGEN;

    return 0;
}

int randomShutdown(void)
{
    RNG->CR      &= ~(RNG_CR_RNGEN | RNG_CR_IE);
    RCC->AHB2ENR &= ~RCC_AHB2ENR_RNGEN;

    return 0;
}

int randomGet(uint32_t *random)
{
    uint32_t remainingTries = 200;

    chMtxLock(&rngMgmt.sync.mutex);
    while (remainingTries--)
    {
        if (RNG->SR & RNG_SR_CEIS)
        {
#if RANDOM_STATS
            rngMgmt.stats.hwClockErrors++;
#endif
            RNG->SR &= ~RNG_SR_CEIS;
            chMtxUnlock(&rngMgmt.sync.mutex);
            return -2;
        }

        if (RNG->SR & RNG_SR_SEIS)
        {
#if RANDOM_STATS
            rngMgmt.stats.hwSeedErrors++;
#endif
            RNG->SR &= ~RNG_SR_SEIS;

            /* Reinitialise the Random Number Generator and hopefully everything
             * will be OK next time around */
            RNG->CR &= ~RNG_CR_RNGEN;
            RNG->CR |= RNG_CR_RNGEN;
            chMtxUnlock(&rngMgmt.sync.mutex);
            return -3;
        }

        if (RNG->SR & RNG_SR_DRDY)
        {
#if RANDOM_STATS
            rngMgmt.stats.hwDataReady++;
#endif
            *random = RNG->DR;
            chMtxUnlock(&rngMgmt.sync.mutex);
            return 0;
        }

        chThdYield();
    }

    chMtxUnlock(&rngMgmt.sync.mutex);

    return -1;
}


