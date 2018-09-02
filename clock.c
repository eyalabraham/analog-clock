/* clock.c
 *
 * This program is the stepper motor driver program for a 3D printer analog clock.
 * The stepper motor is driven at an accurate rate, and when engaged with 3D printer gears it moves the clock.
 * The stepper motor is a simple unipolar 28BJY-48 5v motor with a ULN2003 driver.
 * The controller is an ATtiny85 that is clocked by an accurate TTL Oscillator at 9.8304MHz.
 * At this clock rate, a simple chain of clock divisions by the MCU yields the necessary and accurate motor turn rate for the clock.
 * A pair of pushbuttons are monitored and when activated allow fast forward or backward adjustment of the clock time.
 *
 * More on this project on this web page [https://sites.google.com/site/eyalabraham/3d-printed-analog-clock]
 *
 *  +-----------+
 *  |           |
 *  | 9.8304MHz |
 *  |   TTL OSC |
 *  |           |
 *  +-----+-----+
 *        |
 *  +-----+-----+              +-------+    +---------+    +----------+
 *  |           |              |       |    |         |    |          |
 *  | ATtiny85  +--< PB0,1 >---+ Logic +----+ ULN2003 +----+ 28BJY-48 |
 *  |           |              |       |    |         |    |          |
 *  +-----+-----+              +-------+    +---------+    +----------+
 *        |
 *    < PB2,4 >
 *        |
 *  +-----+-----+
 *  | adjust:   |
 *  | Fast-FWD  |
 *  | Fast-REV  |
 *  +-----------+
 *
 * Port B bit assignment
 *
 *  b5 b4 b3 b2 b1 b0
 *  |  |  |  |  |  |
 *  |  |  |  |  |  +--- 'o' Stepper logic bit-0
 *  |  |  |  |  +------ 'o' Stepper logic bit-1
 *  |  |  |  +--------- 'i' Fast Forward pushbutton
 *  |  |  +------------ 'i' CLKI clock input from oscillator
 *  |  +--------------- 'i' Fast Reverse pushbutton
 *  +------------------ 'i' ^Reset
 *
 * note: all references to data sheet are for ATtiny85 Rev. 2586Q–AVR–08/2013
 * 
 */

#include    <stdint.h>
#include    <stdlib.h>
#include    <string.h>
#include    <stdio.h>
#include    <math.h>

#include    <avr/pgmspace.h>
#include    <avr/io.h>
#include    <avr/interrupt.h>
#include    <avr/wdt.h>

// IO ports B initialization
#define     PB_DDR_INIT     0b00000011  // port data direction
#define     PB_PUP_INIT     0b00010100  // port input pin pull-up
#define     PB_INIT         0x00    // port initial values

#define     BUTTON_FAST_FWD 0b00000100
#define     BUTTON_FAST_REV 0b00010000

// Timer1 initialization
#define     TCCR1_INIT      0x89    // CK/256
#define     GTCCR_INIT      0x00
#define     OCR1C_INIT      125
#define     TIMSK_INIT      0x40    // Interrupt on OCR1A compare

// Timer1 frequency constants
// With 9.8304Mhz external clock use pre-scaler 256 to get 38,400Hz
// Use OCR1A to divide by 125 that will generate interrupt about every 3.2552mSec
// Interrupt routing will count 3 and change motor state at the 102.4 Hz
#define     RATE_FAST       0       // 2 = 102.4Hz, 1 = 153.6Hz, 0 = 307.2Hz
#define     RATE_NORMAL     2

#define     DIR_FWD         1
#define     DIR_REV         2

#define     MAX_DIR_STEPS   2048

/****************************************************************************
  special function prototypes
****************************************************************************/
// This function is called upon a HARDWARE RESET:
void reset(void) __attribute__((naked)) __attribute__((section(".init3")));

/****************************************************************************
  Globals
****************************************************************************/
volatile uint16_t   timerTicks = 0;   // clock timer ticks, increment at PID_FREQ [Hz]
volatile int        clock_direction = DIR_FWD;
volatile uint16_t   clock_rate = RATE_NORMAL;

/* ----------------------------------------------------------------------------
 * ioinit()
 *
 *  Initialize IO interfaces
 *  Timer and data rates calculated based on external oscillator 9.8304MHz
 *
 */
void ioinit(void)
{
    // Reconfigure system clock scaler to 8MHz
    CLKPR = 0x80;   // change clock scaler (sec 6.5.2 p.32)
    CLKPR = 0x00;

    // Initialize Timer1 to provide a periodic interrupt
    TCNT1 = 0;
    TCCR1 = TCCR1_INIT;
    GTCCR = GTCCR_INIT;
    OCR1C = OCR1C_INIT;
    TIMSK = TIMSK_INIT;

    // initialize general IO PB and PD pins for output
    // - PB0, PB1: output, no pull-up, right and left motor fwd/rev control
    // -
    DDRB  = PB_DDR_INIT;            // PB pin directions
    PORTB = PB_INIT | PB_PUP_INIT;  // initial value and pull-up setting
}

/* ----------------------------------------------------------------------------
 * reset()
 *
 *  Clear SREG_I on hardware reset.
 *  source: http://electronics.stackexchange.com/questions/117288/watchdog-timer-issue-avr-atmega324pa
 */
void reset(void)
{
     cli();
    // Note that for newer devices (any AVR that has the option to also
    // generate WDT interrupts), the watchdog timer remains active even
    // after a system reset (except a power-on condition), using the fastest
    // prescaler value (approximately 15 ms). It is therefore required
    // to turn off the watchdog early during program startup.
    MCUSR = 0; // clear reset flags
    wdt_disable();
}

/* ----------------------------------------------------------------------------
 * this ISR will trigger when Timer-1 compare reaches the time interval
 * the ISR will maintain a 20bit counter to drive the motor TTL logic
 * that in turn will provide stepper motor 4-bit sequence
 *
 */
ISR(TIMER1_COMPA_vect)
{
    static uint8_t two_bit_counter = 0;
    uint8_t temp_port;

    timerTicks++;   // increment timer ticks
    if ( timerTicks > clock_rate )
    {
        timerTicks = 0;

        // Output counter bits
        temp_port = PORTB & 0b11111100;
        temp_port |= two_bit_counter & 0b00000011;
        PORTB = temp_port;

        // Increment the counter, no need to worry about overflow
        if ( clock_direction == DIR_FWD )
            two_bit_counter--;
        else
            two_bit_counter++;
    }
}

/* ----------------------------------------------------------------------------
 * main() control functions
 *
 */
int main(void)
{
    // initialize IO devices
    ioinit();

    // enable interrupts
    sei();

    // loop forever and sample pushbuttons and change clock mode as required
    while ( 1 )
    {
        if ( (PINB & BUTTON_FAST_FWD) == 0 )
        {
            clock_rate = RATE_FAST;
            clock_direction = DIR_FWD;
        }
        else if ( (PINB & BUTTON_FAST_REV) == 0 )
        {
            clock_rate = RATE_FAST;
            clock_direction = DIR_REV;
        }
        else
        {
            clock_rate = RATE_NORMAL;
            clock_direction = DIR_FWD;
        }
    } /* endless while loop */

    return 0;
}
