/* Name: main.c
 * Project: Datalogger based on AVR USB driver
 * Original Author: Christian Starkjohann
 * Edited by Ryan Owens (SparkFun Electronics)
 * Reedited by Alexander K
 * Creation Date: 2006-04-23
 * Edited 2009-06-30
 * Reedited 2012-06-16
 * Tabsize: 4
 * Copyright: (c) 2006 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: Proprietary, free under certain conditions. See Documentation.
 * This Revision: $Id: main.c 537 2008-02-28 21:13:01Z cs $
 */

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "usbdrv/usbdrv.h"
#include "temptable.h"

/*
Pin assignment:
PB3 = analog input (ADC3)
PB4 = analog input (ADC2) - Can alternately be used as an LED output.
PB1 = LED output

PB0, PB2 = USB data lines
*/

// compile with -DNOADC2TEMP to log raw ADC value
#ifndef  NOADC2TEMP
#define  ADC2TEMP
#endif

#define WHITE_LED  1
#define YELLOW_LED 3

#define UTIL_BIN4(x)        (uchar)((0##x & 01000)/64 + (0##x & 0100)/16 + (0##x & 010)/4 + (0##x & 1))
#define UTIL_BIN8(hi, lo)   (uchar)(UTIL_BIN4(hi) * 16 + UTIL_BIN4(lo))

#define sbi(var, mask)   ((var) |= (uint8_t)(1 << mask))
#define cbi(var, mask)   ((var) &= (uint8_t)~(1 << mask))

#ifndef NULL
#define NULL    ((void *)0)
#endif

#ifndef uint
#define uint    unsigned int
#endif
/* ------------------------------------------------------------------------- */

// USB HID report descriptor for boot protocol keyboard
// see HID1_11.pdf appendix B section 1
// USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH is defined in usbconfig
PROGMEM char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
    0x05, 0x01,                    // USAGE_PAGE (Generic Desktop)
    0x09, 0x06,                    // USAGE (Keyboard)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
    0x19, 0xe0,                    //   USAGE_MINIMUM (Keyboard LeftControl)(224)
    0x29, 0xe7,                    //   USAGE_MAXIMUM (Keyboard Right GUI)(231)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x01,                    //   LOGICAL_MAXIMUM (1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs) ; Modifier byte
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x81, 0x03,                    //   INPUT (Cnst,Var,Abs) ; Reserved byte
    0x95, 0x05,                    //   REPORT_COUNT (5)
    0x75, 0x01,                    //   REPORT_SIZE (1)
    0x05, 0x08,                    //   USAGE_PAGE (LEDs)
    0x19, 0x01,                    //   USAGE_MINIMUM (Num Lock)
    0x29, 0x05,                    //   USAGE_MAXIMUM (Kana)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs) ; LED report
    0x95, 0x01,                    //   REPORT_COUNT (1)
    0x75, 0x03,                    //   REPORT_SIZE (3)
    0x91, 0x03,                    //   OUTPUT (Cnst,Var,Abs) ; LED report padding
    0x95, 0x06,                    //   REPORT_COUNT (6)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x25, 0x65,                    //   LOGICAL_MAXIMUM (101)
    0x05, 0x07,                    //   USAGE_PAGE (Keyboard)(Key Codes)
    0x19, 0x00,                    //   USAGE_MINIMUM (Reserved (no event indicated))(0)
    0x29, 0x65,                    //   USAGE_MAXIMUM (Keyboard Application)(101)
    0x81, 0x00,                    //   INPUT (Data,Ary,Abs)
    0xc0                           // END_COLLECTION
};

// data structure for boot protocol keyboard report
// see HID1_11.pdf appendix B section 1
typedef struct {
   uint8_t modifier;
   uint8_t reserved;
   uint8_t keycode[6];
} keyboard_report_t;

typedef struct {
  int quot;
  int rem;
} Fraction;

/* global variables */
static keyboard_report_t keyboard_report;
#define keyboard_report_reset() keyboard_report.modifier=0;keyboard_report.reserved=0;keyboard_report.keycode[0]=0;keyboard_report.keycode[1]=0;keyboard_report.keycode[2]=0;keyboard_report.keycode[3]=0;keyboard_report.keycode[4]=0;keyboard_report.keycode[5]=0;
static uint8_t idle_rate = 500 / 4; // see HID1_11.pdf sect 7.2.4
static uint8_t protocol_version = 0; // see HID1_11.pdf sect 7.2.6
static uint8_t LED_state = 0; // see HID1_11.pdf appendix B section 1
static uint8_t blink_count = 0; // keep track of how many times caps lock have toggled

static uchar    adcPending = 0;
static uchar    valueBuffer[16];
static uchar    *nextDigit=0;
static uchar    seconds = 0;
/* ------------------------------------------------------------------------- */

/* Keyboard usage values, see usb.org's HID-usage-tables document, chapter
 * 10 Keyboard/Keypad Page for more codes.
 */
#define MOD_CONTROL_LEFT    (1<<0)
#define MOD_SHIFT_LEFT      (1<<1)
#define MOD_ALT_LEFT        (1<<2)
#define MOD_GUI_LEFT        (1<<3)
#define MOD_CONTROL_RIGHT   (1<<4)
#define MOD_SHIFT_RIGHT     (1<<5)
#define MOD_ALT_RIGHT       (1<<6)
#define MOD_GUI_RIGHT       (1<<7)

#define KEY_1       30
#define KEY_2       31
#define KEY_3       32
#define KEY_4       33
#define KEY_5       34
#define KEY_6       35
#define KEY_7       36
#define KEY_8       37
#define KEY_9       38
#define KEY_0       39
#define KEY_RETURN  40
#define KEY_TAB     43
#define KEY_POINT   55
#define KEY_COMMA   54
#define KEY_MINUS   45
#define KEY_A        4
#define KEY_B        5
#define KEY_C        6
#define KEY_D        7
#define KEY_E        8
#define KEY_F        9



/* ------------------------------------------------------------------------- */
static void convertADCToTemp(unsigned value, Fraction* fract)
{
  uchar i;
  
  for (i=0;i<tempTableSize; ++i)
  {
    uint adcValue = (pgm_read_word(&tempTable[i].adcVal));
    if (value<adcValue)
      break;
  }
  
  if (i==0)
  { // value is below the first table entry
    uint adcVal = (pgm_read_word(&tempTable[0].adcVal));
    char temp =   (pgm_read_byte(&tempTable[0].temp));
    char step =   (pgm_read_byte(&tempTable[0].step));
    fract->quot = temp - ((adcVal - value) / step);
    fract->rem = (10*((adcVal - value) % step))/step;
  }
  else 
  {
    uint adcVal = (pgm_read_word(&tempTable[--i].adcVal));
    char temp =   (pgm_read_byte(&tempTable[i].temp));
    char step =   (pgm_read_byte(&tempTable[i].step));
    fract->quot = temp + ((value-adcVal) / step);
    fract->rem = (10*((value-adcVal) % step))/step;
  }
}

static void writeDigitToBuffer(int value)
{
  uchar digit, negative = 0;

  if (value<0)
  {
    negative = 1;
    value = -value;
  }

  do
  {
    digit = value % 10;
    value /= 10;
    *--nextDigit = 0;
    if(digit == 0)
      *--nextDigit = KEY_0;
    else
      *--nextDigit = KEY_1 - 1 + digit;
  } while(value != 0);
  if (negative)
  {
    *--nextDigit = 0;
    *--nextDigit = KEY_MINUS;
  }
}

/* ------------------------------------------------------------------------- */
static void convertADC(unsigned value)
{
  nextDigit = &valueBuffer[sizeof(valueBuffer)];  //The Buffer is constructed 'backwards,' so point to the end of the Value Buffer before adding the values.
  *--nextDigit = 0xFF;
  *--nextDigit = 0;
  *--nextDigit = KEY_RETURN;
#ifdef ADC2TEMP
  Fraction fract;
  convertADCToTemp(value, &fract);
  writeDigitToBuffer(fract.rem);
  *--nextDigit = 0;
  *--nextDigit = KEY_POINT;
  writeDigitToBuffer(fract.quot);
#else
  writeDigitToBuffer(value);
#endif
  // two zeros below are added to fix strange linux specific
  // problem that the first key is received twice
  *--nextDigit = 0;
  *--nextDigit = 0;
}

/* ------------------------------------------------------------------------- */
static inline void startADC()
{
  adcPending = 1;

  /* logic 1 (5v) on pb3 */
  DDRB |= _BV(YELLOW_LED);
  sbi(PORTB, YELLOW_LED);

  ADCSRA |= (1 << ADSC);  /* start next conversion */
}

static char adcPoll(void)
{
  if(adcPending && !(ADCSRA & (1 << ADSC)))
  {
    adcPending = 0;
    cbi(PORTB, YELLOW_LED); // logic 0 on pb3 
    return 1;
  }

  return 0;
}

/* ------------------------------------------------------------------------- */
static void timerPoll(void)
{
  static uchar timerCnt;

  if(TIFR & (1 << TOV1))
  {  //This flag is triggered at 60 hz.
    TIFR = (1 << TOV1); /* clear overflow */
    if(++timerCnt > 61)
    {
      timerCnt = 0;
      ++seconds;
    }
  }
}

/* ------------------------------------------------------------------------- */
static inline void timerInit(void)
{
  TCCR1 = 0x0b;           /* select clock: 16.5M/1k -> overflow rate = 16.5M/256k = 62.94 Hz */
}

/* ------------------------------------------------------------------------- */
static inline void adcInit(void)
{
  ADMUX = UTIL_BIN8(1001, 0010);  /* vref = 2.56V, measure ADC2 (PB4) */
  ADCSRA = UTIL_BIN8(1000, 0111); /* enable ADC, not free running, interrupt disable, rate = 1/128 */
}



/* ------------------------------------------------------------------------- */
/* ------------------------ interface to USB driver ------------------------ */
/* ------------------------------------------------------------------------- */
usbMsgLen_t usbFunctionSetup(uint8_t data[8])
{
   // see HID1_11.pdf sect 7.2 and http://vusb.wikidot.com/driver-api
   usbRequest_t *rq = (void *)data;

   if ((rq->bmRequestType & USBRQ_TYPE_MASK) != USBRQ_TYPE_CLASS)
      return 0; // ignore request if it's not a class specific request

   // see HID1_11.pdf sect 7.2
   switch (rq->bRequest)
   {
      case USBRQ_HID_GET_IDLE:
         usbMsgPtr = &idle_rate; // send data starting from this byte
         return 1; // send 1 byte
      case USBRQ_HID_SET_IDLE:
         idle_rate = rq->wValue.bytes[1]; // read in idle rate
         return 0; // send nothing
      case USBRQ_HID_GET_PROTOCOL:
         usbMsgPtr = &protocol_version; // send data starting from this byte
         return 1; // send 1 byte
      case USBRQ_HID_SET_PROTOCOL:
         protocol_version = rq->wValue.bytes[1];
         return 0; // send nothing
      case USBRQ_HID_GET_REPORT:
         usbMsgPtr = (uchar*)&keyboard_report; // send the report data
         return sizeof(keyboard_report);
      case USBRQ_HID_SET_REPORT:
         if (rq->wLength.word == 1) // check data is available
         {
            // 1 byte, we don't check report type (it can only be output or feature)
            // we never implemented "feature" reports so it can't be feature
            // so assume "output" reports
            // this means set LED status
            // since it's the only one in the descriptor
            return USB_NO_MSG; // send nothing but call usbFunctionWrite
         }
         else // no data or do not understand data, ignore
         {
            return 0; // send nothing
         }
      default: // do not understand data, ignore
         return 0; // send nothing
   }
}

usbMsgLen_t usbFunctionWrite(uint8_t * data, uchar len)
{
   uint8_t mask = data[0] ^ LED_state;
   if ( bit_is_set(mask, 2) ) // only if scroll lock is pressed 2 times
   {
     if (!blink_count)
       seconds = 0;
     // increment count when LED has toggled
     if (seconds > 1)
       blink_count = 0;
     
     ++blink_count;
   }
   
   LED_state = data[0];
  
   if (blink_count>=2)
   {
     startADC(); // start ADC only if two presses within 1 sec
     blink_count = 0;
   }

   return 1; // 1 byte read
}

/* ------------------------------------------------------------------------- */
/* ------------------------ Oscillator Calibration ------------------------- */
/* ------------------------------------------------------------------------- */

/* Calibrate the RC oscillator to 8.25 MHz. The core clock of 16.5 MHz is
 * derived from the 66 MHz peripheral clock by dividing. Our timing reference
 * is the Start Of Frame signal (a single SE0 bit) available immediately after
 * a USB RESET. We first do a binary search for the OSCCAL value and then
 * optimize this value with a neighboorhod search.
 * This algorithm may also be used to calibrate the RC oscillator directly to
 * 12 MHz (no PLL involved, can therefore be used on almost ALL AVRs), but this
 * is wide outside the spec for the OSCCAL value and the required precision for
 * the 12 MHz clock! Use the RC oscillator calibrated to 12 MHz for
 * experimental purposes only!
 */
static void calibrateOscillator(void)
{
  uchar       step = 128;
  uchar       trialValue = 0, optimumValue;
  int         x, optimumDev, targetValue = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);

  /* do a binary search: */
  do{
      OSCCAL = trialValue + step;
      x = usbMeasureFrameLength();    /* proportional to current real frequency */
      if(x < targetValue)             /* frequency still too low */
          trialValue += step;
      step >>= 1;
  }while(step > 0);
  /* We have a precision of +/- 1 for optimum OSCCAL here */
  /* now do a neighborhood search for optimum value */
  optimumValue = trialValue;
  optimumDev = x; /* this is certainly far away from optimum */
  for(OSCCAL = trialValue - 1; OSCCAL <= trialValue + 1; OSCCAL++){
      x = usbMeasureFrameLength() - targetValue;
      if(x < 0)
          x = -x;
      if(x < optimumDev){
          optimumDev = x;
          optimumValue = OSCCAL;
      }
  }
  OSCCAL = optimumValue;
}
/*
Note: This calibration algorithm may try OSCCAL values of up to 192 even if
the optimum value is far below 192. It may therefore exceed the allowed clock
frequency of the CPU in low voltage designs!
You may replace this search algorithm with any other algorithm you like if
you have additional constraints such as a maximum CPU clock.
For version 5.x RC oscillators (those with a split range of 2x128 steps, e.g.
ATTiny25, ATTiny45, ATTiny85), it may be useful to search for the optimum in
both regions.
*/
void usbEventResetReady(void)
{
  calibrateOscillator();
  eeprom_write_byte(0, OSCCAL);   /* store the calibrated value in EEPROM */
}

void buildReport(uchar keyVal, uchar modifier)
{
  keyboard_report.modifier = modifier;
  keyboard_report.keycode[0] = keyVal;
}

/* ------------------------------------------------------------------------- */
/* --------------------------------- main ---------------------------------- */
/* ------------------------------------------------------------------------- */

int main(void)
{
  uchar i, calibrationValue;

  calibrationValue = eeprom_read_byte(0); /* calibration value from last time */
  if(calibrationValue != 0xff)
    OSCCAL = calibrationValue;
 
  
  //Production Test Routine - Turn on both LEDs and an LED on the SparkFun Pogo Test Bed.
  DDRB |= _BV(WHITE_LED) | _BV(YELLOW_LED) | _BV(4);   /* output for LED */
  sbi(PORTB, WHITE_LED);
  sbi(PORTB, YELLOW_LED);  
  for(i=0;i<20;i++){  /* 300 ms disconnect */
    _delay_ms(15);    /* 15ms is the maximum value for _delay_ms with 16.5Mhz clock */
  }
  cbi(PORTB, WHITE_LED);
  cbi(PORTB, YELLOW_LED);
  DDRB &= ~_BV(4);
  
  //Initialize the USB Connection with the host computer.
  usbDeviceDisconnect();
    for(i=0;i<20;i++) { /* 300 ms disconnect */
        _delay_ms(15);
    }
  usbDeviceConnect();
  
  wdt_enable(WDTO_1S);
  
  keyboard_report_reset();
  timerInit();  //Create a timer that will trigger a flag at a ~60hz rate 
  adcInit();    //Setup the ADC conversions
  usbInit();    //Initialize USB comm.
  sei();        // enable interrupts
  
  for(;;)
  { /* main event loop */
    wdt_reset();
    usbPoll();  //Check to see if it's time to send a USB packet
    if (usbInterruptIsReady() && nextDigit != NULL)
    { /* we can send another key */
      buildReport(*nextDigit, 0);
      usbSetInterrupt((uchar*)&keyboard_report, sizeof(keyboard_report)); // send
      if (0xFF == *++nextDigit)
        nextDigit = NULL;
    }
    timerPoll();
    if (adcPoll())
    {
      unsigned adcVal = ADC;
      convertADC(adcVal);
    }
  }
  return 0;
}

