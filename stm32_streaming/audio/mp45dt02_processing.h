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

#ifndef __MP45DT02_PDM_H__
#define __MP45DT02_PDM_H__

/* Number of times interrupts are called when filling the buffer.
 * ChibiOS fires twice half full / full */
#define MP45DT02_INTERRUPTS_PER_BUFFER      2

/******************************************************************************/
/* Audio configuration */
/******************************************************************************/
/* Frequency at which the MP45DT02 is being (over) sampled. */
#define MP45DT02_RAW_FREQ_KHZ               1024

/* One raw sample provided to processing.*/
#define MP45DT02_RAW_SAMPLE_DURATION_MS     1

/******************************************************************************/
/* I2S Buffer - Contains 2 ms of sampled data. Emptied every 1ms */
/******************************************************************************/

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
/******************************************************************************/
/* Expanded Buffer - 1 ms worth of 1 bit samples */
/******************************************************************************/

/* Every bit in I2S signal needs to be expanded out into a word. */
/* Note: I2S Interrupts are always fired at half/full buffer point. Thus
 * processed buffers require to be 0.5 * bits in one I2S buffer */
#define MP45DT02_EXPANDED_BUFFER_SIZE       (MP45DT02_I2S_SAMPLE_SIZE_BITS)

/******************************************************************************/
/* Decimated Buffer - 1 ms worth of processed audio data */
/******************************************************************************/

/* Desired decimation factor */
#define MP45DT02_FIR_DECIMATION_FACTOR      64

/* Buffer size of the decimated sample */
#define MP45DT02_DECIMATED_BUFFER_SIZE      (MP45DT02_EXPANDED_BUFFER_SIZE / \
                                             MP45DT02_FIR_DECIMATION_FACTOR)

typedef void (*mp45dt02FullBufferCb) (float *data, uint16_t length);

typedef struct {
    /* Callback function to be notified when the processing buffer is full. */
    mp45dt02FullBufferCb fullbufferCb;
} mp45dt02Config;

void mp45dt02Init(mp45dt02Config *config);
void mp45dt02Shutdown(void);


#endif
