/**************************************
 * ADC.c
 *
 * P.J. Bones   UCECE, D. Beukenholdt, J. Laws
 *
 * Last modified: 18 May 2022
 **************************************/

#include <stdint.h>
#include <stdbool.h>
#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "driverlib/adc.h"
#include "driverlib/gpio.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/debug.h"
#include "utils/ustdlib.h"
#include "circBufT.h"

//*****************************************************************************
// Constants
//*****************************************************************************
#define BUF_SIZE 5


//*****************************************************************************
// Global variables
//*****************************************************************************
static circBuf_t g_adcBuffer;		// Buffer of size BUF_SIZE integers (sample values)


//*****************************************************************************
// The handler for the ADC conversion complete interrupt.
// Writes to the circular buffer.
//*****************************************************************************
void ADCIntHandler(void)
{
	uint32_t ulValue;
	
	//
	// Get the single sample from ADC0.  ADC_BASE is defined in
	// inc/hw_memmap.h
	ADCSequenceDataGet(ADC0_BASE, 3, &ulValue);
	//
	// Place it in the circular buffer (advancing write index)
	writeCircBuf (&g_adcBuffer, ulValue);



	//
	// Clean up, clearing the interrupt
	ADCIntClear(ADC0_BASE, 3);                          
}


//*****************************************************************************
// Initializes the ADC peripheral
//*****************************************************************************
void  initADC(void)
{
    initCircBuf (&g_adcBuffer, BUF_SIZE);

    // The ADC0 peripheral must be enabled for configuration and use.
    SysCtlPeripheralEnable(SYSCTL_PERIPH_ADC0);
    
    // Enable sample sequence 3 with a processor signal trigger.  Sequence 3
    // will do a single sample when the processor sends a signal to start the
    // conversion.
    ADCSequenceConfigure(ADC0_BASE, 3, ADC_TRIGGER_PROCESSOR, 0);
  
    //
    // Configure step 0 on sequence 3.  Sample channel 0 (ADC_CTL_CH0) in
    // single-ended mode (default) and configure the interrupt flag
    // (ADC_CTL_IE) to be set when the sample is done.  Tell the ADC logic
    // that this is the last conversion on sequence 3 (ADC_CTL_END).  Sequence
    // 3 has only one programmable step.  Sequence 1 and 2 have 4 steps, and
    // sequence 0 has 8 programmable steps.  Since we are only doing a single
    // conversion using sequence 3 we will only configure step 0.  For more
    // on the ADC sequences and steps, refer to the LM3S1968 datasheet.
    ADCSequenceStepConfigure(ADC0_BASE, 3, 0, ADC_CTL_CH0 | ADC_CTL_IE |
                             ADC_CTL_END);    
                             
    //
    // Since sample sequence 3 is now configured, it must be enabled.
    ADCSequenceEnable(ADC0_BASE, 3);
  
    //
    // Register the interrupt handler
    ADCIntRegister (ADC0_BASE, 3, ADCIntHandler);
  
    //
    // Enable interrupts for ADC0 sequence 3 (clears any outstanding interrupts)
    ADCIntEnable(ADC0_BASE, 3);
}


//*****************************************************************************
// Returns the mean value in the ADCs circular buffer
//*****************************************************************************
uint16_t getADCMean(){
    uint16_t sum = 0;
    uint8_t i;
    for (i = 0; i < BUF_SIZE; i++)
        sum = sum + readCircBuf(&g_adcBuffer);
        // Calculate and return the rounded mean of the buffer contents

    return ((2 * sum + BUF_SIZE) / 2 / BUF_SIZE);
}







