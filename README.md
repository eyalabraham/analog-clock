# 3D printed Analog Clock with stepper motor
This program is the stepper motor driver of the analog clock.
The stepper motor is paced at an accurate rate, and when engaged with 3D printer gears it moves the clock.
The stepper motor is a simple unipolar 28BJY-48 5v motor with a ULN2003 driver.
The controller is an ATtiny85 that is clocked by an accurate TTL Oscillator at 9.8304MHz.
At this clock rate, a simple chain of clock divisions by the MCU yields the necessary and accurate motor turn rate for the clock.
A pair of pushbuttons are monitored, and, when pressed, allow fast forward or backward adjustment of the clock time.

More on this project on this web page [https://sites.google.com/site/eyalabraham/3d-printed-analog-clock]

```
 +-----------+
 |           |
 | 9.8304MHz |
 |   TTL OSC |
 |           |
 +-----+-----+
       |
 +-----+----+             +-----------+  +---------+  +----------+
 |          |             |           |  |         |  |          |
 | ATtiny85 +--< PB0,1 >--+ Sequencer +--+ ULN2003 +--+ 28BJY-48 |
 |          |             | Logic     |  |         |  |          |
 +-----+----+             +-----------+  +---------+  +----------+
       |
   < PB2,4 >
       |
 +-----+----+
 | adjust:  |
 | Fast-FWD |
 | Fast-REV |
 | Run      |
 +----------+
```
### Port B bit assignment
```
 *  b5 b4 b3 b2 b1 b0
 *  |  |  |  |  |  |
 *  |  |  |  |  |  +--- 'o' Stepper logic bit-0
 *  |  |  |  |  +------ 'o' Stepper logic bit-1
 *  |  |  |  +--------- 'i' Fast Forward pushbutton
 *  |  |  +------------ 'i' CLKI clock input from oscillator
 *  |  +--------------- 'i' Fast Reverse pushbutton
 *  +------------------ 'i' ^Reset
```
## Picking accurate clock rate for stepper motor on ATtiny85
The clock is driven by a stepper motor. In order to achieve clock accuracy, it is necessary to drive the stepper at the correct rate.
The stepper motor used in this project is a 28BYJ-48 model. It completes a full 360 degree rotation every 2,048 steps.
Given that the motor-pinion to seconds gear has a 3:1 ratio, the stepper needs to run at a step rate of 102.4Hz.
The calculation is as follows (3 revolutions x 2048 steps per revolution) / 60 sec = 102.4 steps per sec
### 9.8304Mhz external clock
With a 9.8304Mhz external clock use pre-scaler 256 to get 38,400Hz
Then use OCR1C to divide by 125, which will generate interrupt about every 3.2552mSec (307.2Hz)
The interrupt routing will count 3 and change motor state at the 102.4 Hz
### Accuracy test and adjustments
To test accuracy, I synchronized clock time with real time and ran for 12 hours.
After 12 hours I compared real time with clock time: the difference was 5min 40sec behind, 340[sec] out of 43,200[sec]
This means that the crystal oscillator has a small deviation from the specified rate of 9.8304MHz.
The difference is approximately 0.7870%, and with a clock deviser of 124 in OCR1C, instead of 125, the clock will yield a closer match to the desired motor rate of 102.4Hz.
The new expected error after changing OCR1C to 124 is approximately 0.0131%, which is about 5.6452 seconds every 12 hours.
Retest result measurements show that the clock is now less than 2 sec fast in 24 hours, which does not match the calculated accuracy.
## Sequencer logic
Refer to electronics schematics for diagram.
These are external TTL logic components connected to create the correct motor coil sequence.
Since the ATtiny85 has very few IO pins, and i did not want to disable the RESET pin, I needed a way to use fewer IO pins.
Therefor the four motor coil positions are driven by two bit, and using a 74139 + 7400 I create the coil sequences.
I use the other two AVR pins as input bits to sense the clock adjustment pushbuttons: Fast Forward and Fast Reverse.
## CAD and 3D printer-ready file
See GrabCAD at [https://grabcad.com/library/electromechanical-clock-1]
## Electronic schematic
See [https://github.com/eyalabraham/schematics] under analog clock or see web page
## Project files
- **clock.c** - main module for clock driver

