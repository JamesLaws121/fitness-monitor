

/***********************************************************
 * acc.c
 *
 * C.P. Moore, D. Beukenholdt, J. Laws
 *
 * Last modified: 18 May 2022
 **********************************************************/


#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>


#include "inc/hw_memmap.h"
#include "inc/hw_types.h"
#include "inc/hw_i2c.h"
#include "driverlib/pin_map.h"
#include "driverlib/systick.h"
#include "driverlib/sysctl.h"
#include "driverlib/gpio.h"
#include "driverlib/i2c.h"
#include "../OrbitOLED/OrbitOLEDInterface.h"
#include "utils/ustdlib.h"
#include "driverlib/interrupt.h"

#include "acc.h"
#include "i2c_driver.h"
#include "circBufT.h"




//Global variables

circBuf_t bufferZ;
circBuf_t bufferX;
circBuf_t bufferY;
circBuf_t magnitude_buffer;
uint8_t BUFF_SIZE = 6;


//======================================================
// initAccl: Initialises the ACC and associated buffers
//======================================================
void initAccl (void)
{
    char    toAccl[] = {0, 0};  // parameter, value

    /*
     * Enable I2C Peripheral
     */
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_I2C0);
    SysCtlPeripheralReset(SYSCTL_PERIPH_I2C0);

    /*
     * Set I2C GPIO pins
     */
    GPIOPinTypeI2C(I2CSDAPort, I2CSDA_PIN);
    GPIOPinTypeI2CSCL(I2CSCLPort, I2CSCL_PIN);
    GPIOPinConfigure(I2CSCL);
    GPIOPinConfigure(I2CSDA);

    /*
     * Setup I2C
     */
    I2CMasterInitExpClk(I2C0_BASE, SysCtlClockGet(), true);

    GPIOPinTypeGPIOInput(ACCL_INT2Port, ACCL_INT2);

    //Initialize ADXL345 Acceleromter

    // set +-2g, 13 bit resolution, active low interrupts
    toAccl[0] = ACCL_DATA_FORMAT;
    toAccl[1] = (ACCL_RANGE_2G | ACCL_FULL_RES);
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);

    toAccl[0] = ACCL_PWR_CTL;
    toAccl[1] = ACCL_MEASURE;
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);


    toAccl[0] = ACCL_BW_RATE;
    toAccl[1] = ACCL_RATE_100HZ;
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);

    toAccl[0] = ACCL_INT;
    toAccl[1] = 0x00;       // Disable interrupts from accelerometer.
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);

    toAccl[0] = ACCL_OFFSET_X;
    toAccl[1] = 0x00;
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);

    toAccl[0] = ACCL_OFFSET_Y;
    toAccl[1] = 0x00;
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);

    toAccl[0] = ACCL_OFFSET_Z;
    toAccl[1] = 0x00;
    I2CGenTransmit(toAccl, 1, WRITE, ACCL_ADDR);

    initCircBuf(&bufferY, BUFF_SIZE);
    initCircBuf(&bufferZ, BUFF_SIZE);
    initCircBuf(&bufferX, BUFF_SIZE);
    initCircBuf(&magnitude_buffer, MAGNITUDE_SAMPLES);

    // Obtain initial set of accelerometer data
    uint8_t i;
    for(i = 0; i < 20; i++){
       updateAccBuffers();
    }

    for(i = 0; i < MAGNITUDE_SAMPLES; i++){
        vector3_t accl = getAcclData();
        writeCircBuf(&magnitude_buffer, sqrt((accl.x*accl.x + accl.y*accl.y
                + accl.z*accl.z)));
    }
}

//======================================================
// getAcclData: Function to read accelerometer
//======================================================
vector3_t getAcclData (void)
{
    char    fromAccl[] = {0, 0, 0, 0, 0, 0, 0}; // starting address, placeholders for data to be read.
    vector3_t acceleration;
    uint8_t bytesToRead = 6;

    fromAccl[0] = ACCL_DATA_X0;
    I2CGenTransmit(fromAccl, bytesToRead, READ, ACCL_ADDR);

    acceleration.x = ((fromAccl[2] << 8) | fromAccl[1]); // Return 16-bit acceleration readings.
    acceleration.y = ((fromAccl[4] << 8) | fromAccl[3]);
    acceleration.z = ((fromAccl[6] << 8) | fromAccl[5]);

    return acceleration;
}



//=====================================================================
// Takes raw acceleration data and returns acceleration in
//  raw data, milli Gs, or metres per second per second.
//  unit: 0=raw, 1=Gs, 2=m/s/s
//=====================================================================
vector3_t convert(vector3_t accl_raw, uint8_t unit){
    vector3_t accl_out;

    switch (unit) {
        case 0:
            //Raw
            accl_out = accl_raw;
            break;
        case 1:
            // raw --> milli g
            accl_out.x = ((accl_raw.x * 100) / 256) * 10;
            accl_out.y = ((accl_raw.y * 100) / 256) * 10;
            accl_out.z = ((accl_raw.z * 100) / 256) * 10;
            break;
        case 2:
            // raw --> ms^-2
            accl_out.x = (accl_raw.x * 9.81) / 256;
            accl_out.y = (accl_raw.y * 9.81) / 256;
            accl_out.z = (accl_raw.z * 9.81) / 256;
            break;
        default:
            accl_out.x = 0;
            accl_out.y = 0;
            accl_out.z = 0;
        }
    return accl_out;
}



//====================================================================
// Returns a string representing the given unit
// 0=raw(no unit), 1=mGs, 2=m/s/s.
//====================================================================
char* getAcclUnitStr(int8_t unit_num){

    switch (unit_num){
        case 1:
            return "mG";
        case 2:
            return "m/s/s";
        default:
            return "";
    }
}



//====================================================================================
// getAverage: Returns the means of the acceleration data
//====================================================================================
vector3_t getAverage(){
    // Take a new running average of acceleration
    vector3_t currentAverage;

    currentAverage.x = averageData(BUFF_SIZE,&bufferX);
    currentAverage.y = averageData(BUFF_SIZE,&bufferY);
    currentAverage.z = averageData(BUFF_SIZE,&bufferZ);
    return currentAverage;
}

//====================================================================================
// averageData: Returns the mean of the data stored in the given buffer
//====================================================================================
int32_t averageData(uint8_t BUFF_SIZE,circBuf_t* buffer){
    int32_t sum = 0;
    int32_t temp;
    int32_t average;

    int i;
    for(i = 0; i < BUFF_SIZE;i++){
        temp = readCircBuf(buffer);
        sum += temp;
    }

    average = ((sum / BUFF_SIZE));
    return average;
}


//====================================================================================
// updateAccBuffers: places new data in buffers
//====================================================================================
void updateAccBuffers(){
    // Obtain accelerometer data and write to circular buffer
    vector3_t accl_data = getAcclData();

    writeCircBuf(&bufferX,abs(accl_data.x));
    writeCircBuf(&bufferY,abs(accl_data.y));
    writeCircBuf(&bufferZ,abs(accl_data.z));
}


//====================================================================================
// checkBump: detects spikes in acceleration data
//====================================================================================
bool checkBump(){
    static uint8_t step_cooldown = STEP_COOLDOWN;
    static uint16_t average_magnitude;
    static bool prev_over_threshold = false;
    static bool over_threshold = false;

    IntMasterDisable(); // disable interrupts while reading from buffer
    vector3_t currentAverage = getAverage(); // gets the average x,y,z data
    IntMasterEnable();

    // Current magnitude
    uint16_t magnitude = sqrt((currentAverage.x*currentAverage.x + currentAverage.y*currentAverage.y
            + currentAverage.z*currentAverage.z));

    // use a moving average as a baseline for detecting acceleration spikes
    writeCircBuf(&magnitude_buffer, magnitude);
    average_magnitude = averageData(MAGNITUDE_SAMPLES, &magnitude_buffer);

    //difference between current acceleration and moving average
    int16_t diffMagnitude = magnitude-average_magnitude;

    // check if the diffMagnitude is greater then the threshold
    prev_over_threshold = over_threshold;
    over_threshold = (diffMagnitude > PEAK_THRESHOLD);

    if(step_cooldown != 0) {step_cooldown--;}

    // Only register a step on the rising edge of the over_threshold variable
    if(over_threshold == 1 && prev_over_threshold == 0 && step_cooldown == 0){
        step_cooldown = STEP_COOLDOWN;
        return true;
    } else {
        return false;
    }
}

