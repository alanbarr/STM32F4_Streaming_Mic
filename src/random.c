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

/* Random numbers are generated after 40 cycles of the PLL48CLK - so the pool
 * doesn't have to be particularly big */
#define RANDOM_QUEUE_SIZE   4

#define RNG_IRQ_VECTOR      Vector180
#define RNG_IRQ_POSITION    80
#define RNG_IRQ_PRIORITY    15

#define RANDOM_STATS        1

static struct {

    struct {
        uint32_t q[RANDOM_QUEUE_SIZE];
        uint32_t size;
        uint32_t *nextRead;
        uint32_t *nextWrite;
        mutex_t mutex;
        condition_variable_t conditionEmpty;
    } queue;

#if RANDOM_STATS
    struct {
        uint32_t hwDataReady;
        uint32_t hwSeedErrors;
        uint32_t hwClockErrors;
    } stats;
#endif

} rngMgmt;

/* Hash and Rng Global Interrupt */
CH_IRQ_HANDLER(RNG_IRQ_VECTOR)
{
    CH_IRQ_PROLOGUE();

    uint32_t random = 0;

    chSysLockFromISR();

    if (RNG->SR & RNG_SR_DRDY)
    {
#if RANDOM_STATS
        rngMgmt.stats.hwDataReady++;
#endif
        random = RNG->DR;
    }

    if (RNG->SR & RNG_SR_CEIS)
    {
#if RANDOM_STATS
        rngMgmt.stats.hwClockErrors++;
#endif
        RNG->SR &= ~RNG_SR_CEIS;
    }

    if (RNG->SR & RNG_SR_SEIS)
    {
#if RANDOM_STATS
        rngMgmt.stats.hwSeedErrors++;
#endif

        RNG->SR &= ~RNG_SR_SEIS;

        /* Reinitialise the Random Number Generator and hopefully everything 
         * will be OK. */
        RNG->CR &= ~RNG_CR_RNGEN;
        RNG->CR |= RNG_CR_RNGEN;
    }

    else
    /* Only add the random number to the queue if not currently in error */
    {
        if (rngMgmt.queue.size < RANDOM_QUEUE_SIZE)
        {
            *rngMgmt.queue.nextWrite = random;
            rngMgmt.queue.size++;

            if (++rngMgmt.queue.nextWrite == &rngMgmt.queue.q[RANDOM_QUEUE_SIZE])
            {
                rngMgmt.queue.nextWrite = &rngMgmt.queue.q[0];
            }
     
            chCondSignalI(&rngMgmt.queue.conditionEmpty);
        }

        if (rngMgmt.queue.size == RANDOM_QUEUE_SIZE)
        {
            /* If we have a full queue, stop interrupts for the time being. Note
             * by the time we get here there is likely already one more
             * interrupt ready to fire. */
            RNG->CR &= ~RNG_CR_IE;
        }
    }

    chSysUnlockFromISR();

    CH_IRQ_EPILOGUE();
}

StatusCode randomInit(void)
{
    chMtxObjectInit(&rngMgmt.queue.mutex);
    chCondObjectInit(&rngMgmt.queue.conditionEmpty);
    
    rngMgmt.queue.size = 0;
    rngMgmt.queue.nextWrite = &rngMgmt.queue.q[0];
    rngMgmt.queue.nextRead  = &rngMgmt.queue.q[0];

    nvicEnableVector(RNG_IRQ_POSITION, RNG_IRQ_PRIORITY);

    RCC->AHB2ENR |= RCC_AHB2ENR_RNGEN;
    RNG->CR      |= RNG_CR_RNGEN | RNG_CR_IE;

    return STATUS_OK;
}

StatusCode randomShutdown(void)
{
    RNG->CR      &= ~(RNG_CR_RNGEN | RNG_CR_IE);
    RCC->AHB2ENR &= ~RCC_AHB2ENR_RNGEN;

    return STATUS_OK;
}

StatusCode randomGet(uint32_t *random)
{
    chSysLock();
    chMtxLockS(&rngMgmt.queue.mutex);
    
    while (rngMgmt.queue.size == 0)
    {
        if (MSG_OK != chCondWaitTimeoutS(&rngMgmt.queue.conditionEmpty,
                                         ST2MS(25)))
        {
            chMtxUnlockS(&rngMgmt.queue.mutex);
            chSysUnlock();
            return STATUS_ERROR_TIMEOUT;
        }
    }
 
    *random = *rngMgmt.queue.nextRead;
    rngMgmt.queue.size--;

    if (++rngMgmt.queue.nextRead == &rngMgmt.queue.q[RANDOM_QUEUE_SIZE])
    {
        rngMgmt.queue.nextRead = &rngMgmt.queue.q[0];
    }

    RNG->CR |= RNG_CR_IE;

    chMtxUnlockS(&rngMgmt.queue.mutex);
    chSysUnlock();
 
    return STATUS_OK;
}


