/*------------------------------------------------------------------------------
  RPiGPIO_pig.c -- A simple MEX file for GPIO control on the RaspberryPi

  On Octave, compile with:

  mex -v -g -o RPiGPIOMex.mex RPiGPIO_pig.c -lpigpio -lrt -O3

  This program requires that the pigpio library is installed.
  See https://abyz.me.uk/rpi/pigpio for download and installation.
  Or simply 'sudo apt install pigpio' on RaspberryPi OS for a possibly
  recent enough version.

  ------------------------------------------------------------------------------

  Modified from RPiGPIOMex.c, which is Copyright (C) 2016-2023 Mario Kleiner
  Modifications by Steve Van Hooser, 2023 and Mario Kleiner, 2023.

  This program is licensed under the MIT license.

  A copy of the license can be found in the License.txt file inside the
  Psychtoolbox-3 top level folder.
------------------------------------------------------------------------------*/

/* Octave includes: */
#include "mex.h"

/* System includes */
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

/* pigpio library for RPi GPIO control includes */
#include <pigpio.h>

static bool firstTime = 1;
static volatile int isrDone = -1000;

void isrCallback(int gpio, int level, uint32_t tick)
{
    isrDone = level;
}

void exitfunc(void)
{
    gpioTerminate();
    firstTime = 1;
}

/* This is the main entry point from Octave: */
void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray*prhs[])
{
    int cmd, pin, arg;
    int rc;
    uint32_t version;
    double* out;

    // Get our name for output:
    const char* me = mexFunctionName();

    if (firstTime) {
        if (gpioInitialise() < 0)
            mexErrMsgTxt("Failed to initialize GPIO system with pigpio.");

        // Successfully connected. Register exit handler to close GPIO control
        // when mex file is flushed:
        mexAtExit(exitfunc);

        // Ready to rock:
        firstTime = 0;
    }

    // Special case: Called with one return argument and no input arguments. Return RPi board revision number:
    if (nrhs == 0 && nlhs == 1) {
        version = gpioHardwareRevision();
        plhs[0] = mxCreateDoubleMatrix(1, 1, mxREAL);
        *(mxGetPr(plhs[0])) = (double) version;
        return;
    }

    if (nrhs < 2) {
        mexPrintf("%s: A simple Octave MEX file for basic pigpio control of the RaspberryPi GPIO pins under GNU/Linux.\n\n", me);
        mexPrintf("(C) 2016-2023 Mario Kleiner, 2023 Steve Van Hooser -- Licensed to you under the MIT license.\n");
        mexPrintf("This file is part of Psychtoolbox-3 but should also work independently.\n\n");
        mexPrintf("Pin numbers are in Broadcom numbering scheme aka BCM_GPIO numbering.\n");
        mexPrintf("Mapping to physical connector pins can be found by typing 'pinout' on the RPi command line\n");
        mexPrintf("This mex file requires the pigpio library and applications available at http://abyz.me.uk/rpi/pigpio \n");
        mexPrintf("On RaspberryPi OS, the pigpio library can be easily installed via 'sudo apt install pigpio'.\n\n");
        mexPrintf("For testing purposes, pins 35 and 47 on a RaspberryPi 2B map to the red power and green status LEDs.\n\n");
        mexPrintf("The gpio command line utility allows to setup and export pins for use by a non-root user.\n\n");
        mexPrintf("\n");
        mexPrintf("Usage:\n\n");
        mexPrintf("revision = %s;\n", me);
        mexPrintf("- Return RaspberryPi board 'revision' number. Different revisions == different pinout.\n\n");
        mexPrintf("state = %s(0, pin);\n", me);
        mexPrintf("- Query 'state' of pin number 'pin': 1 = High, 0 = Low.\n\n");
        mexPrintf("%s(1, pin, level);\n", me);
        mexPrintf("- Set state of pin number 'pin' to logic level 'level': 1 = High, 0 = Low.\n\n");
        mexPrintf("%s(2, pin, level);\n", me);
        mexPrintf("- Set pulse-width modulation state of pin number 'pin' to level 'level': 0 - 1023.\n");
        mexPrintf("  Only available on GPIO logical pins 0-31.\n\n");
        mexPrintf("%s(3, pin, direction);\n", me);
        mexPrintf("- Set direction of pin number 'pin' to 'direction'. 1 = Output, 0 = Input.\n\n");
        mexPrintf("%s(4, pin, pullMode);\n", me);
        mexPrintf("- Set resistor mode of pin number 'pin' to 'pullMode'. -1 = Pull down, 1 = Pull up, 0 = None.\n");
        mexPrintf("  Pin must be configured as input for pullup/pulldown resistors to work.\n\n");
        mexPrintf("result = %s(5, pin, timeoutMsecs);\n", me);
        mexPrintf("- Wait for rising/falling edge on input pin number 'pin' with a timeout of 'timeoutMsecs': -1 = Infinite wait.\n");
        mexPrintf("  Return 'result' status code: -1 = error, 0 = timed out, 1 = trigger received.\n");
        mexPrintf("  Only available on GPIO logical pins 0-31. Pin must be configured as input.\n\n");

        return;
    }

    // First argument must be the command code:
    cmd = (int) mxGetScalar(prhs[0]);

    // Second argument is pin number:
    pin = (int) mxGetScalar(prhs[1]);

    if (nrhs > 2)
        arg = (int) mxGetScalar(prhs[2]);
    else
        arg = -1000;

    switch (cmd) {
        case 0: // Read input level from pin: 1 = High, 0 = Low
            rc = gpioRead(pin);
            plhs[0] = mxCreateDoubleMatrix(1, 1, mxREAL);
            *(mxGetPr(plhs[0])) = (double) rc;
            break;

        case 1: // Write a new level to output pin: 1 = High, 0 = Low
            if (arg < 0)
                mexErrMsgTxt("New logic level of output pin missing for output pin write.");

            gpioWrite(pin, arg);
            break;

        case 2: // Write a new pwm level to output pin: 0 to 1024 on RPi
            if (arg < 0)
                mexErrMsgTxt("New pwm level of output pin missing for output pin pulse-width modulation.");

            if (arg > 1024)
                mexErrMsgTxt("Invalid pwm level specified. Must be in range 0 - 1024.");

            // Set PWM range to 1024 for backwards compatibility with old mex file:
            if (gpioGetPWMrange(pin) != 1024)
               gpioSetPWMrange(pin, 1024);

            // Set new PWM dutycycle 'arg' between 0 - 1024 for pin:
            if (gpioPWM(pin, arg) < 0)
                mexErrMsgTxt("Failed to set new pwm level of output pin for output pin pulse-width modulation.");

            break;

        case 3: // Set pin i/o mode: 1 = out, 0 = in
            if (arg < 0)
                mexErrMsgTxt("New opmode for pin missing for pin mode configuration.");

            gpioSetMode(pin, arg ? PI_OUTPUT : PI_INPUT);
            break;

        case 4: // Set pullup/pulldowns: 1 = out, 0 = in
            if (arg < -1)
                mexErrMsgTxt("New pullup/down for pin missing for pin resistor configuration.");

            gpioSetPullUpDown(pin, (arg==0) ? PI_PUD_OFF : ((arg>0) ? PI_PUD_UP : PI_PUD_DOWN));
            break;

        case 5: // Wait for rising or falling edge on given input pin via interrupt.
            if (arg < -1)
                mexErrMsgTxt("Timeout value in milliseconds missing.");

            plhs[0] = mxCreateDoubleMatrix(1, 1, mxREAL);

            // set isrCallback to be called after trigger reception or timeout:
            isrDone = -1000;
            if (gpioSetISRFunc(pin, EITHER_EDGE, arg, isrCallback)) {
                // Failed:
                *(mxGetPr(plhs[0])) = (double) -1;
            }

            // Busy wait for isrCallback to signal being called:
            while (isrDone == -1000);

            // Reset to off:
            gpioSetISRFunc(pin, EITHER_EDGE, arg, NULL);

            // Timeout or trigger received?
            rc = (isrDone == 2) ? 0 : 1;
            *(mxGetPr(plhs[0])) = (double) rc;
            break;

        default:
            mexErrMsgTxt("Unknown command code provided!");
    }

    // Done.
    return;
}
