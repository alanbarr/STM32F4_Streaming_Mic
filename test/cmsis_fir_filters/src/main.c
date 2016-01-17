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
#include "chprintf.h"
#include "autogen_fir_coeff.h"
#include "arm_math.h"

#define SERIAL_PRINT_DRIVER SD1

#define PRINT(FMT, ...)                                                     \
        debugSerialPrint("(%s:%d) " FMT "\n\r", __FILE__, __LINE__, __VA_ARGS__)  

static mutex_t serialPrintMtx;

static union 
{
    arm_fir_instance_f32 f;
    arm_fir_instance_q15 q15;
    arm_fir_instance_q31 q31;
} instance; 

static union
{
    float f[TEST_FIR_DATA_TYPE_TAP_LEN];
    q15_t q15[TEST_FIR_DATA_TYPE_TAP_LEN];
    q31_t q31[TEST_FIR_DATA_TYPE_TAP_LEN];
} coeffs;

static union
{
    float f[TEST_FIR_DATA_TYPE_SIGNAL_LEN];
    q15_t q15[TEST_FIR_DATA_TYPE_SIGNAL_LEN];
    q31_t q31[TEST_FIR_DATA_TYPE_SIGNAL_LEN];
} input;

static union 
{
    float f[TEST_FIR_DATA_TYPE_TAP_LEN + TEST_FIR_DATA_TYPE_BLOCK_LEN - 1];
    q15_t q15[TEST_FIR_DATA_TYPE_TAP_LEN + TEST_FIR_DATA_TYPE_BLOCK_LEN - 1];
    q31_t q31[TEST_FIR_DATA_TYPE_TAP_LEN + TEST_FIR_DATA_TYPE_BLOCK_LEN - 1];
} state;

static union
{
    float f[TEST_FIR_DATA_TYPE_SIGNAL_LEN];
    q15_t q15[TEST_FIR_DATA_TYPE_SIGNAL_LEN];
    q31_t q31[TEST_FIR_DATA_TYPE_SIGNAL_LEN];
} tempOutput;

static struct {

    float f[TEST_FIR_DATA_TYPE_SIGNAL_LEN];
    float q15[TEST_FIR_DATA_TYPE_SIGNAL_LEN];
    float q15Fast[TEST_FIR_DATA_TYPE_SIGNAL_LEN];
    float q31[TEST_FIR_DATA_TYPE_SIGNAL_LEN];
    float q31Fast[TEST_FIR_DATA_TYPE_SIGNAL_LEN];
} convertedOutput;

static struct 
{
    time_measurement_t fFull;
    time_measurement_t fBlock;

    time_measurement_t q15Full;
    time_measurement_t q15Block;

    time_measurement_t q15FastFull;
    time_measurement_t q15FastBlock;

    time_measurement_t q31Full;
    time_measurement_t q31Block;

    time_measurement_t q31FastFull;
    time_measurement_t q31FastBlock;
} time;

void debugInit(void)
{
    /* Configure Debug Serial */
    sdStart(&SERIAL_PRINT_DRIVER, NULL);
    palSetPadMode(GPIOB, 6, PAL_MODE_ALTERNATE(7));
    palSetPadMode(GPIOB, 7, PAL_MODE_ALTERNATE(7));
    chMtxObjectInit(&serialPrintMtx);
}

void debugShutdown(void)
{
    sdStop(&SERIAL_PRINT_DRIVER);
}

void debugSerialPrint(const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    chMtxLock(&serialPrintMtx);
    chvprintf((BaseSequentialStream*)&SERIAL_PRINT_DRIVER, fmt, ap);
    chMtxUnlock(&serialPrintMtx);
    va_end(ap);
}

static void testFloat(void)
{
    uint32_t i = 0;

    memset(&instance, 0, sizeof(instance));
    memset(&coeffs,   0, sizeof(coeffs));
    memset(&input,    0, sizeof(input));
    memset(&state,    0, sizeof(state));
    memset(&tempOutput,   0, sizeof(tempOutput));
    memset(&time.fBlock, 0, sizeof(time.fBlock));
    memset(&time.fFull, 0, sizeof(time.fFull));

    memcpy(coeffs.f, armFirCoeffFloat, sizeof(coeffs.f));
    memcpy(input.f, testSignalFloat, sizeof(input.f));

    arm_fir_init_f32(&instance.f,
                     TEST_FIR_DATA_TYPE_TAP_LEN,
                     coeffs.f,
                     state.f,
                     TEST_FIR_DATA_TYPE_BLOCK_LEN);

    chTMObjectInit(&time.fBlock);
    chTMObjectInit(&time.fFull);

    chTMStartMeasurementX(&time.fFull);

    for(i = 0;
        i < TEST_FIR_DATA_TYPE_SIGNAL_LEN;
        i += TEST_FIR_DATA_TYPE_BLOCK_LEN)
    {
        chTMStartMeasurementX(&time.fBlock);
        arm_fir_f32(&instance.f, &input.f[i], &tempOutput.f[i], TEST_FIR_DATA_TYPE_BLOCK_LEN);
        chTMStopMeasurementX(&time.fBlock);
    }

    memcpy(convertedOutput.f, tempOutput.f, sizeof(convertedOutput.f));

    chTMStopMeasurementX(&time.fFull);
}

static void testQ15(void)
{
    uint32_t i = 0;

    memset(&instance, 0, sizeof(instance));
    memset(&coeffs,   0, sizeof(coeffs));
    memset(&input,    0, sizeof(input));
    memset(&state,    0, sizeof(state));
    memset(&tempOutput,   0, sizeof(tempOutput));
    memset(&time.q15Block, 0, sizeof(time.q15Block));
    memset(&time.q15Full, 0, sizeof(time.q15Full));

    arm_float_to_q15(armFirCoeffFloat, coeffs.q15, TEST_FIR_DATA_TYPE_TAP_LEN);
    arm_float_to_q15(testSignalFloat, input.q15, TEST_FIR_DATA_TYPE_SIGNAL_LEN);

    if (ARM_MATH_SUCCESS != arm_fir_init_q15(&instance.q15,
                                TEST_FIR_DATA_TYPE_TAP_LEN,
                                coeffs.q15,
                                state.q15,
                                TEST_FIR_DATA_TYPE_BLOCK_LEN))
    {
        while(1);
    }

    chTMObjectInit(&time.q15Block);
    chTMObjectInit(&time.q15Full);

    chTMStartMeasurementX(&time.q15Full);

    for(i = 0;
        i < TEST_FIR_DATA_TYPE_SIGNAL_LEN;
        i += TEST_FIR_DATA_TYPE_BLOCK_LEN)
    {
        chTMStartMeasurementX(&time.q15Block);
        arm_fir_q15(&instance.q15, &input.q15[i], &tempOutput.q15[i], TEST_FIR_DATA_TYPE_BLOCK_LEN);
        chTMStopMeasurementX(&time.q15Block);
    }

    chTMStopMeasurementX(&time.q15Full);

    arm_q15_to_float(tempOutput.q15, convertedOutput.q15, TEST_FIR_DATA_TYPE_SIGNAL_LEN);
}


static void testQ15Fast(void)
{
    uint32_t i = 0;

    memset(&instance, 0, sizeof(instance));
    memset(&coeffs,   0, sizeof(coeffs));
    memset(&input,    0, sizeof(input));
    memset(&state,    0, sizeof(state));
    memset(&tempOutput,   0, sizeof(tempOutput));
    memset(&time.q15FastBlock, 0, sizeof(time.q15FastBlock));
    memset(&time.q15FastFull, 0, sizeof(time.q15FastFull));

    arm_float_to_q15(armFirCoeffFloat, coeffs.q15, TEST_FIR_DATA_TYPE_TAP_LEN);
    arm_float_to_q15(testSignalFloat, input.q15, TEST_FIR_DATA_TYPE_SIGNAL_LEN);

    if (ARM_MATH_SUCCESS != arm_fir_init_q15(&instance.q15,
                     TEST_FIR_DATA_TYPE_TAP_LEN,
                     coeffs.q15,
                     state.q15,
                     TEST_FIR_DATA_TYPE_BLOCK_LEN))
    {
        while(1);
    }

    chTMObjectInit(&time.q15FastBlock);
    chTMObjectInit(&time.q15FastFull);

    chTMStartMeasurementX(&time.q15FastFull);

    for(i = 0;
        i < TEST_FIR_DATA_TYPE_SIGNAL_LEN;
        i += TEST_FIR_DATA_TYPE_BLOCK_LEN)
    {
        chTMStartMeasurementX(&time.q15FastBlock);
        arm_fir_fast_q15(&instance.q15, &input.q15[i], &tempOutput.q15[i], TEST_FIR_DATA_TYPE_BLOCK_LEN);
        chTMStopMeasurementX(&time.q15FastBlock);
    }

    chTMStopMeasurementX(&time.q15FastFull);

    arm_q15_to_float(tempOutput.q15, convertedOutput.q15Fast, TEST_FIR_DATA_TYPE_SIGNAL_LEN);
}


static void testQ31(void)
{
    uint32_t i = 0;

    memset(&instance, 0, sizeof(instance));
    memset(&coeffs,   0, sizeof(coeffs));
    memset(&input,    0, sizeof(input));
    memset(&state,    0, sizeof(state));
    memset(&tempOutput,   0, sizeof(tempOutput));
    memset(&time.q31Full, 0, sizeof(time.q31Full));
    memset(&time.q31Block, 0, sizeof(time.q31Block));

    arm_float_to_q31(armFirCoeffFloat, coeffs.q31, TEST_FIR_DATA_TYPE_TAP_LEN);
    arm_float_to_q31(testSignalFloat, input.q31, TEST_FIR_DATA_TYPE_SIGNAL_LEN);

    arm_fir_init_q31(&instance.q31,
                     TEST_FIR_DATA_TYPE_TAP_LEN,
                     coeffs.q31,
                     state.q31,
                     TEST_FIR_DATA_TYPE_BLOCK_LEN);

    chTMObjectInit(&time.q31Full);
    chTMObjectInit(&time.q31Block);

    chTMStartMeasurementX(&time.q31Full);

    for(i = 0;
        i < TEST_FIR_DATA_TYPE_SIGNAL_LEN;
        i += TEST_FIR_DATA_TYPE_BLOCK_LEN)
    {
        chTMStartMeasurementX(&time.q31Block);
        arm_fir_q31(&instance.q31, &input.q31[i], &tempOutput.q31[i], TEST_FIR_DATA_TYPE_BLOCK_LEN);
        chTMStopMeasurementX(&time.q31Block);
    }

    chTMStopMeasurementX(&time.q31Full);

    arm_q31_to_float(tempOutput.q31, convertedOutput.q31, TEST_FIR_DATA_TYPE_SIGNAL_LEN);
}

static void testQ31Fast(void)
{
    uint32_t i = 0;

    memset(&instance, 0, sizeof(instance));
    memset(&coeffs,   0, sizeof(coeffs));
    memset(&input,    0, sizeof(input));
    memset(&state,    0, sizeof(state));
    memset(&tempOutput,   0, sizeof(tempOutput));
    memset(&time.q31FastFull, 0, sizeof(time.q31FastFull));
    memset(&time.q31FastBlock, 0, sizeof(time.q31FastBlock));

    arm_float_to_q31(armFirCoeffFloat, coeffs.q31, TEST_FIR_DATA_TYPE_TAP_LEN);
    arm_float_to_q31(testSignalFloat, input.q31, TEST_FIR_DATA_TYPE_SIGNAL_LEN);

    arm_fir_init_q31(&instance.q31,
                     TEST_FIR_DATA_TYPE_TAP_LEN,
                     coeffs.q31,
                     state.q31,
                     TEST_FIR_DATA_TYPE_BLOCK_LEN);

    chTMObjectInit(&time.q31FastFull);
    chTMObjectInit(&time.q31FastBlock);

    chTMStartMeasurementX(&time.q31FastFull);

    for(i = 0;
        i < TEST_FIR_DATA_TYPE_SIGNAL_LEN;
        i += TEST_FIR_DATA_TYPE_BLOCK_LEN)
    {
        chTMStartMeasurementX(&time.q31FastBlock);
        arm_fir_fast_q31(&instance.q31, &input.q31[i], &tempOutput.q31[i], TEST_FIR_DATA_TYPE_BLOCK_LEN);
        chTMStopMeasurementX(&time.q31FastBlock);
    }

    chTMStopMeasurementX(&time.q31FastFull);

    arm_q31_to_float(tempOutput.q31, convertedOutput.q31Fast, TEST_FIR_DATA_TYPE_SIGNAL_LEN);
}


void testFirDataTypes(void)
{
    memset(&convertedOutput, 0, sizeof(convertedOutput));

    testQ15Fast();
    testQ15();
    testQ31Fast();
    testQ31();
    testFloat();

    PRINT("= Time Results =", 0);
    PRINT("Fast Q15  : %u", time.q15FastBlock.worst);
    PRINT("Q15       : %u", time.q15Block.worst);
    PRINT("Q31       : %u", time.q31Block.worst);
    PRINT("Fast Q31  : %u", time.q31FastBlock.worst);
    PRINT("Float     : %u", time.fBlock.worst);
}

int main(void) 
{
    halInit();
    chSysInit();
    debugInit();

    PRINT("Running.",0);

    testFirDataTypes();

    while(1)
    {
        palTogglePad(GPIOD, GPIOD_LED4);
        chThdSleepMilliseconds(500);
    }
}
