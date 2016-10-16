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
#include "autogen_fir_coeffs.h"
#include "debug.h"
#include "mp45dt02_processing.h"

#if 0
#include "lwipthread.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/err.h"
#include "rtp.h"
#endif


/******************************************************************************/
/* Hardware configuration */
/******************************************************************************/

/* ChibiOS I2S driver in use */
#define MP45DT02_I2S_DRIVER                 I2SD2

/* STM32F4 configuration for the I2S configuration register */
#define I2SCFG_MODE_MASTER_RECEIVE          (SPI_I2SCFGR_I2SCFG_0 | SPI_I2SCFGR_I2SCFG_1)
#define I2SCFG_STD_I2S                      (0)
#define I2SCFG_STD_LSB_JUSTIFIED            (SPI_I2SCFGR_I2SSTD_1)
#define I2SCFG_STD_MSB_JUSTIFIED            (SPI_I2SCFGR_I2SSTD_0)
#define I2SCFG_STD_PCM                      (SPI_I2SCFGR_I2SSTD_0 | SPI_I2SCFGR_I2SSTD_1)
#define I2SCFG_CKPOL_STEADY_LOW             (0)
#define I2SCFG_CKPOL_STEADY_HIGH            (SPI_I2SCFGR_CKPOL)

/* STM32F4 configuration for I2S prescalar register */
#define MP45DT02_I2SDIV                     42
#define MP45DT02_I2SODD                     0
#define I2SPR_I2SODD_SHIFT                  8

/* Debugging - check for buffer overflows */
#define MEMORY_GUARD                        0xDEADBEEF

static struct {
    uint32_t offset;
    uint32_t number;
    uint16_t buffer[MP45DT02_I2S_BUFFER_SIZE_2B];
    uint32_t guard;
} mp45dt02I2sData;

static struct {
    arm_fir_decimate_instance_f32 decimateInstance;
    float32_t state[FIR_COEFFS_LEN + MP45DT02_EXPANDED_BUFFER_SIZE - 1];
    uint32_t guard;
} cmsisDsp;

static thread_t *pMp45dt02ProcessingThd;
static THD_WORKING_AREA(mp45dt02ProcessingThdWA, 1024);
static semaphore_t mp45dt02ProcessingSem;

static I2SConfig mp45dt02I2SConfig;

static float32_t mp45dt02ExpandedBuffer[MP45DT02_EXPANDED_BUFFER_SIZE];
static uint16_t mp45dt02DecimatedBuffer[MP45DT02_DECIMATED_BUFFER_SIZE];

/* 
 * outBuffer: Array of floats, where each element is derived from an input in
 *            inBuffer.
 *            It must be of length MP45DT02_EXPANDED_BUFFER_SIZE
 * inBuffer: Array of I2S data that is to be expanded to a more useful datatype
 *           by this function.
 *           It must be of length MP45DT02_I2S_SAMPLE_SIZE_2B
 */
static void expand(float32_t *outBuffer,
                   const uint16_t *inBuffer)
{
    uint32_t bitIndex = 0;
    uint16_t modifiedCurrentWord = 0;

    memset(outBuffer, 0, sizeof(MP45DT02_EXPANDED_BUFFER_SIZE));

    /* Move each bit from each uint16_t word to an element of output array. */
    for(bitIndex=0;
        bitIndex < MP45DT02_I2S_SAMPLE_SIZE_BITS;
        bitIndex++)
    {
        if (bitIndex % 16 == 0)
        {
            modifiedCurrentWord = inBuffer[bitIndex/MP45DT02_I2S_WORD_SIZE_BITS];
        }

        if (modifiedCurrentWord & 0x8000)
        {
            outBuffer[bitIndex] = INT16_MAX;
        }
        else 
        {
            outBuffer[bitIndex] = INT16_MIN;
        }

        modifiedCurrentWord = modifiedCurrentWord << 1;
    }
}

static THD_FUNCTION(mp45dt02ProcessingThd, arg)
{
    (void)arg;
    uint32_t index = 0;
    uint16_t *sample = NULL;

    chRegSetThreadName(__FUNCTION__);

    while (chThdShouldTerminateX() == false)
    {
        chSemWait(&mp45dt02ProcessingSem);
        
        if (chThdShouldTerminateX() == true)
        {
            break;
        }

        if (mp45dt02I2sData.number != MP45DT02_I2S_SAMPLE_SIZE_2B)
        {
            DEBUG_CRITICAL("Unexpected number of samples provided. %d not %d.",
                           mp45dt02I2sData.number,
                           MP45DT02_I2S_SAMPLE_SIZE_2B);
        }

        /**********************************************************************/ 
        /* Convert I2S data to a useful format                                */
        /**********************************************************************/ 

        expand(mp45dt02ExpandedBuffer,
               &mp45dt02I2sData.buffer[mp45dt02I2sData.offset]);

        /**********************************************************************/ 
        /* Filtering                                                          */
        /**********************************************************************/ 
        
        /* AB TODO we don't want floats... */
#if 0
        arm_fir_decimate_f32(&cmsisDsp.decimateInstance,
                             mp45dt02ExpandedBuffer,
                             mp45dt02DecimatedBuffer,
                             MP45DT02_EXPANDED_BUFFER_SIZE);
#endif
        /**********************************************************************/ 
        /* Notify of new data                                                 */
        /**********************************************************************/ 
        audioTxHandleFullMp45dt02Buffer(mp45dt02DecimatedBuffer,
                                        sizeof(mp45dt02DecimatedBuffer));

        if (mp45dt02I2sData.guard != MEMORY_GUARD)
        {
            DEBUG_CRITICAL("Overflow detected.",0);
        }
        if (cmsisDsp.guard != MEMORY_GUARD)
        {
            DEBUG_CRITICAL("Overflow detected.",0);
        }
    }
}

/* (*i2scallback_t) */
static void mp45dt02Cb(I2SDriver *i2sp, size_t offset, size_t number)
{
    (void)i2sp;

    chSysLockFromISR();
    mp45dt02I2sData.offset = offset;
    mp45dt02I2sData.number = number;
    chSemSignalI(&mp45dt02ProcessingSem);
    chSysUnlockFromISR();
}

static void dspInit(void)
{
    arm_status armStatus;

    memset(&cmsisDsp, 0, sizeof(cmsisDsp));
    cmsisDsp.guard = MEMORY_GUARD;

    if (ARM_MATH_SUCCESS != (armStatus = arm_fir_decimate_init_f32(
                                            &cmsisDsp.decimateInstance,
                                            FIR_COEFFS_LEN,
                                            MP45DT02_FIR_DECIMATION_FACTOR,
                                            firCoeffs,
                                            cmsisDsp.state,
                                            MP45DT02_EXPANDED_BUFFER_SIZE)))
    {
        DEBUG_CRITICAL("arm_fir_decimate_init_f32 failed with %d", armStatus);
    }
}

void mp45dt02Init(void)
{
    PRINT("Initialising mp45dt02.\n\r"
          "mp45dt02I2sData.buffer size: %u words %u bytes\n\r"
          "mp45dt02ExpandedBuffer size: %u words %u bytes\n\r"
          "MP45DT02_DECIMATED_BUFFER_SIZE: %u",
          MP45DT02_I2S_BUFFER_SIZE_2B, sizeof(mp45dt02I2sData.buffer),
          MP45DT02_EXPANDED_BUFFER_SIZE, sizeof(mp45dt02ExpandedBuffer),
          MP45DT02_DECIMATED_BUFFER_SIZE);

    chSemObjectInit(&mp45dt02ProcessingSem, 0);

    pMp45dt02ProcessingThd = chThdCreateStatic(mp45dt02ProcessingThdWA,
                                               sizeof(mp45dt02ProcessingThdWA),
                                               NORMALPRIO,
                                               mp45dt02ProcessingThd, NULL);

    dspInit();

    memset(&mp45dt02I2SConfig, 0, sizeof(mp45dt02I2SConfig));

    memset(&mp45dt02I2sData, 0, sizeof(mp45dt02I2sData));
    mp45dt02I2sData.guard = MEMORY_GUARD;

    mp45dt02I2SConfig.tx_buffer = NULL;
    mp45dt02I2SConfig.rx_buffer = mp45dt02I2sData.buffer;
    mp45dt02I2SConfig.size      = MP45DT02_I2S_BUFFER_SIZE_2B;
    mp45dt02I2SConfig.end_cb    = mp45dt02Cb;

    mp45dt02I2SConfig.i2scfgr   = I2SCFG_MODE_MASTER_RECEIVE    |
                                  I2SCFG_STD_MSB_JUSTIFIED      |
                                  I2SCFG_CKPOL_STEADY_HIGH;

    mp45dt02I2SConfig.i2spr     = (SPI_I2SPR_I2SDIV & MP45DT02_I2SDIV) |
                                  (SPI_I2SPR_ODD & (MP45DT02_I2SODD << I2SPR_I2SODD_SHIFT));

    i2sStart(&MP45DT02_I2S_DRIVER, &mp45dt02I2SConfig);
    i2sStartExchange(&MP45DT02_I2S_DRIVER);
}

void mp45dt02Shutdown(void)
{
    i2sStopExchange(&MP45DT02_I2S_DRIVER);
    i2sStop(&MP45DT02_I2S_DRIVER);

    chThdTerminate(pMp45dt02ProcessingThd);
    chThdWait(pMp45dt02ProcessingThd);
    chSemReset(&mp45dt02ProcessingSem, 1);
    pMp45dt02ProcessingThd = NULL;
}

