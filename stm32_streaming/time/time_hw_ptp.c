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

#include <stdint.h>
#include <math.h>
#include "debug.h"
#include "hal.h"

/* PTP Register Calculations
 *
 * The following are calculated based on:
 *
 * SysClk     = 168 MHz
 * tick        = 20 ns
 *
 *  increment = tick * 2^31
 *            = (20.0 * 10**-9 * 2**31)
 *            = 42.94967296 
 *            = 43
 *
 *  addend    = 2^63 / (SysClk * increment)
 *            = (2.0 ** 63) / (168 * 10**6 * 43)
 *            = 1276768000.6720343
 *            = 1276768001
 */
#define TIME_HW_REG_INCREMENT   43
#define TIME_HW_REG_ADDEND      1276768001

#define TIME_HW_WAIT_FOR_OPERATION()    \
    while (ETH->PTPTSCR &               \
            (ETH_PTPTSCR_TSARU |        \
             ETH_PTPTSCR_TSSTU |        \
             ETH_PTPTSCR_TSSTI));

StatusCode timeHwPtpInit(void)
{
    /* Disable timestamp interrupt */
    ETH->MACIMR   |= ETH_MACIMR_TSTIM;

    /* Ethernet PTP Clock Enable */
    RCC->AHB1RSTR |= RCC_AHB1ENR_ETHMACPTPEN;

    /* Enable time stamping */
    ETH->PTPTSCR   = ETH_PTPTSCR_TSE;

    /* Set increment */
    ETH->PTPSSIR   = TIME_HW_REG_INCREMENT;

    /* Set addend */
    ETH->PTPTSAR   = TIME_HW_REG_ADDEND;

    /* We want Fine Updating - trigger addend update */
    ETH->PTPTSCR  |= ETH_PTPTSCR_TSFCU | ETH_PTPTSCR_TSARU;

    TIME_HW_WAIT_FOR_OPERATION();

    return STATUS_OK;
}

StatusCode timeHwPtpShutdown(void)
{
    /* Clear PTP Control Register - (disable time stamping) */
    ETH->PTPTSCR   = 0;

    /* Ethernet PTP Clock Disable */
    RCC->AHB1RSTR &= ~RCC_AHB1ENR_ETHMACPTPEN;

    return STATUS_OK;
}

StatusCode timeHwPtpSet(uint32_t seconds, uint32_t subseconds)
{
    ETH->PTPTSHUR = seconds;
    ETH->PTPTSLUR = subseconds>>1;
    ETH->PTPTSCR |= ETH_PTPTSCR_TSSTI;

    TIME_HW_WAIT_FOR_OPERATION();
    
    return STATUS_OK;
}

StatusCode timeHwPtpGet(uint32_t *seconds, uint32_t *subseconds)
{
    *seconds    = ETH->PTPTSHR;
    *subseconds = ETH->PTPTSHR;

    /* We don't expect a negative number */
    if (*subseconds & ETH_PTPTSLR_STPNS)
    {
        return STATUS_ERROR_HW;
    }

    return STATUS_OK;
}
