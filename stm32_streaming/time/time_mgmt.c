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
#include "sntp.h"
#include "debug.h"
#include "time_hw_ptp.h"

StatusCode timeInit(void)
{
    SC_ASSERT(timeHwPtpInit());

    return STATUS_OK;
}

StatusCode timeShutdown(void)
{
    SC_ASSERT(timeHwPtpShutdown());

    return STATUS_OK;
}

StatusCode timeNtpUpdate(void)
{
    uint32_t seconds = 0;
    uint32_t fraction = 0;

    SC_ASSERT_MSG(sntpGetTime(&seconds, &fraction), "Get Time Failed", 0)

    /* Convert NTP unsiged fraction to PTP signed fraction */
    SC_ASSERT(timeHwPtpSet(seconds, fraction));

    return STATUS_OK;
}

StatusCode timeNtpGet(uint32_t *seconds, uint32_t *fraction)
{
    SC_ASSERT(timeHwPtpGet(seconds, fraction));

    *fraction <<= 1;

    return STATUS_OK;
}
