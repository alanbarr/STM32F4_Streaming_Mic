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

/* 
 * Please consult the readme located at ../README.md for more information.
 */

#include <stdint.h>
#include <string.h>
#include "ch.h"
#include "hal.h"
#include "autogen_fir_coeffs.h"
#include "debug.h"

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

/* STM32F4 pinout to MP45DT02 */
#define MP45DT02_CLK_PORT                   GPIOB
#define MP45DT02_CLK_PAD                    10
#define MP45DT02_PDM_PORT                   GPIOC
#define MP45DT02_PDM_PAD                    3

/* STM32F4 configuration for I2S prescalar register */
#define MP45DT02_I2SDIV                     42
#define MP45DT02_I2SODD                     0
#define I2SPR_I2SODD_SHIFT                  8

/* Frequency at which the MP45DT02 is being (over) sampled. */
#define MP45DT02_RAW_FREQ_KHZ               1024

/* One raw sample provided to processing.*/
#define MP45DT02_RAW_SAMPLE_DURATION_MS     1

/* The number of bits in I2S word */
#define MP45DT02_I2S_WORD_SIZE_BITS         16

/* The number of PDM samples (bits) to process in one interrupt */
#define MP45DT02_I2S_SAMPLE_SIZE_BITS       (MP45DT02_RAW_FREQ_KHZ * \
                                                MP45DT02_RAW_SAMPLE_DURATION_MS)
/* The length, in uint16_t's of the i2s buffer that will be filled per
 * interrupt. */
#define MP45DT02_I2S_SAMPLE_SIZE_2B         (MP45DT02_I2S_SAMPLE_SIZE_BITS / \
                                                MP45DT02_I2S_WORD_SIZE_BITS)
/* The total required I2S buffer length. */
#define MP45DT02_I2S_BUFFER_SIZE_2B         (MP45DT02_I2S_SAMPLE_SIZE_2B * \
                                                MP45DT02_INTERRUPTS_PER_BUFFER)

/* Number of times interrupts are called when filling the buffer.
 * ChibiOS fires twice half full / full */
#define MP45DT02_INTERRUPTS_PER_BUFFER      2

/* Every bit in I2S signal needs to be expanded out into a word. */
/* Note: I2S Interrupts are always fired at half/full buffer point. Thus
 * processed buffers require to be 0.5 * bits in one I2S buffer */
#define MP45DT02_EXPANDED_BUFFER_SIZE       (MP45DT02_I2S_SAMPLE_SIZE_BITS)

/* Desired decimation factor */
#define MP45DT02_FIR_DECIMATION_FACTOR      64

/* Buffer size of the decimated sample */
#define MP45DT02_DECIMATED_BUFFER_SIZE      (MP45DT02_EXPANDED_BUFFER_SIZE / \
                                                MP45DT02_FIR_DECIMATION_FACTOR)

/* Number of samples to collect */
#define MP45DT02_OUTPUT_BUFFER_DURATION_MS  1600

/* Size of the buffer holding output data */
#define MP45DT02_OUTPUT_BUFFER_SIZE         (MP45DT02_DECIMATED_BUFFER_SIZE    * \
                                             MP45DT02_OUTPUT_BUFFER_DURATION_MS/ \
                                             MP45DT02_RAW_SAMPLE_DURATION_MS)

/* Debugging - check for buffer overflows */
#define MEMORY_GUARD                        0xDEADBEEF

static thread_t * pMp45dt02ProcessingThd;
static THD_WORKING_AREA(mp45dt02ProcessingThdWA, 256);
static semaphore_t mp45dt02ProcessingSem;

static I2SConfig mp45dt02I2SConfig;

static struct {
    uint32_t offset;
    uint32_t number;
    uint16_t buffer[MP45DT02_I2S_BUFFER_SIZE_2B];
    uint32_t guard;
} mp45dt02I2sData;

static float32_t mp45dt02ExpandedBuffer[MP45DT02_EXPANDED_BUFFER_SIZE];

static struct {
    arm_fir_decimate_instance_f32 decimateInstance;
    float32_t state[FIR_COEFFS_LEN + MP45DT02_EXPANDED_BUFFER_SIZE - 1];
    uint32_t guard;
} cmsisDsp;

static struct {
    uint32_t count;
    float32_t buffer[MP45DT02_OUTPUT_BUFFER_SIZE];
    uint32_t guard;
} output;

static struct {
    time_measurement_t decimate;
    time_measurement_t expansion;
    time_measurement_t callback;
    time_measurement_t totalProcessing;
    time_measurement_t oneSecond;
} debugTimings;

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

    chRegSetThreadName("mp45dt02ProcessingThd");

    while (1)
    {
        chSemWait(&mp45dt02ProcessingSem);

        if (chThdShouldTerminateX())
        {
            break;
        }

        if (mp45dt02I2sData.number != MP45DT02_I2S_SAMPLE_SIZE_2B)
        {
            PRINT_ERROR("Unexpected number of samples provided. %d not %d.",
                        mp45dt02I2sData.number,
                        MP45DT02_I2S_SAMPLE_SIZE_2B);
        }

        chTMStartMeasurementX(&debugTimings.totalProcessing);

        /**********************************************************************/ 
        /* Getting I2S data to a useful format                                */
        /**********************************************************************/ 
        chTMStartMeasurementX(&debugTimings.expansion);

        expand(mp45dt02ExpandedBuffer,
               &mp45dt02I2sData.buffer[mp45dt02I2sData.offset]);

        chTMStopMeasurementX(&debugTimings.expansion);

        /**********************************************************************/ 
        /* Filtering */
        /**********************************************************************/ 
        chTMStartMeasurementX(&debugTimings.decimate);

        arm_fir_decimate_f32(&cmsisDsp.decimateInstance,
                             mp45dt02ExpandedBuffer,
                             &output.buffer[output.count * MP45DT02_DECIMATED_BUFFER_SIZE],
                             MP45DT02_EXPANDED_BUFFER_SIZE);

        chTMStopMeasurementX(&debugTimings.decimate);

        output.count++;

        chTMStopMeasurementX(&debugTimings.totalProcessing);

        /**********************************************************************/ 
        /* Pause Condition */
        /**********************************************************************/ 
        if (output.count == MP45DT02_OUTPUT_BUFFER_DURATION_MS / 
                            MP45DT02_RAW_SAMPLE_DURATION_MS)
        {

            i2sStopExchange(&MP45DT02_I2S_DRIVER);
            LED_ORANGE_CLEAR(); /* Breakpoint */

            while (1)
            {
                LED_BLUE_TOGGLE();

                chTMStartMeasurementX(&debugTimings.oneSecond);
                chThdSleepMilliseconds(1000);
                chTMStopMeasurementX(&debugTimings.oneSecond);

                if (palReadPad(GPIOA, GPIOA_BUTTON))
                {
                    chThdSleepMilliseconds(10);
                    while (palReadPad(GPIOA, GPIOA_BUTTON));
                    chThdSleepMilliseconds(200);

                    LED_BLUE_CLEAR();
                    memset(output.buffer, 0, sizeof(output.buffer));
                    output.count = 0;
                    i2sStartExchange(&MP45DT02_I2S_DRIVER);
                    LED_ORANGE_SET();
                    break;
                }
            }
        }

        if (mp45dt02I2sData.guard != MEMORY_GUARD)
        {
            PRINT_ERROR("Overflow detected.",0);
        }
        if (cmsisDsp.guard != MEMORY_GUARD)
        {
            PRINT_ERROR("Overflow detected.",0);
        }
        if (output.guard != MEMORY_GUARD)
        {
            PRINT_ERROR("Overflow detected.",0);
        }
    }
}

/* (*i2scallback_t) */
static void mp45dt02Cb(I2SDriver *i2sp, size_t offset, size_t number)
{
    (void)i2sp;

    chTMStartMeasurementX(&debugTimings.callback);

    chSysLockFromISR();
    mp45dt02I2sData.offset = offset;
    mp45dt02I2sData.number = number;
    chSemSignalI(&mp45dt02ProcessingSem);
    chSysUnlockFromISR();

    chTMStopMeasurementX(&debugTimings.callback);
}

void dspInit(void)
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
        PRINT_ERROR("arm_fir_decimate_init_f32 failed with %d", armStatus);
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

    chTMObjectInit(&debugTimings.decimate);
    chTMObjectInit(&debugTimings.expansion);
    chTMObjectInit(&debugTimings.callback);
    chTMObjectInit(&debugTimings.totalProcessing);
    chTMObjectInit(&debugTimings.oneSecond);

    memset(&mp45dt02I2SConfig, 0, sizeof(mp45dt02I2SConfig));

    memset(&mp45dt02I2sData, 0, sizeof(mp45dt02I2sData));
    mp45dt02I2sData.guard = MEMORY_GUARD;

    output.guard = MEMORY_GUARD;

    palSetPadMode(MP45DT02_PDM_PORT, MP45DT02_PDM_PAD,
                  PAL_MODE_ALTERNATE(5)     |
                  PAL_STM32_OTYPE_PUSHPULL  |
                  PAL_STM32_OSPEED_HIGHEST);

    palSetPadMode(MP45DT02_CLK_PORT, MP45DT02_CLK_PAD,
                  PAL_MODE_ALTERNATE(5)     |
                  PAL_STM32_OTYPE_PUSHPULL  |
                  PAL_STM32_OSPEED_HIGHEST);

#if 0
    /* Debug: Output clock to pin. */
    RCC->CFGR = (RCC->CFGR & ~(0x1F<<27)) |
                1<<30 |
                5<<27;

    /* output PLLI2S to PC9 MCO2*/
    palSetPadMode(GPIOC, 9,
                  PAL_MODE_ALTERNATE(0) |
                  PAL_STM32_OTYPE_PUSHPULL |
                  PAL_STM32_OSPEED_HIGHEST);
#endif

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

