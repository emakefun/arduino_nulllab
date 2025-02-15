/*
  main.cpp - Main loop for Arduino sketches
  Copyright (c) 2005-2013 Arduino Team.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include <avr/wdt.h>
#include <Arduino.h>
#include <wiring_private.h>

#define OSC_DELAY()	do {\
	_NOP(); _NOP(); _NOP(); _NOP(); _NOP(); _NOP(); _NOP(); _NOP(); _NOP(); _NOP();\
	_NOP(); _NOP(); _NOP(); _NOP(); _NOP(); _NOP(); _NOP(); _NOP(); _NOP(); _NOP();\
} while(0);

// Declared weak in Arduino.h to allow user redefinitions.
int atexit(void (* /*func*/ )()) { return 0; }
uint8_t clock_set = 0;
// Weak empty variant initialization function.
// May be redefined by variant files.
void initVariant() __attribute__((weak));
void initVariant() { }

void setupUSB() __attribute__((weak));
void setupUSB() { }

#if defined(__LGT8FX8E__) || defined(__LGT8FX8P__)
void __patch_wdt(void) \
	     __attribute__((naked)) \
	     __attribute__((section(".init3")));
void __patch_wdt(void)
{
	MCUSR = 0;
	wdt_disable();
}
#endif

//#pragma __attribute__(always_inline)
void unlockWrite(volatile uint8_t *p, uint8_t val)
{
	uint8_t _o_sreg = SREG;
	volatile uint8_t *cp = p; 

	if(p == &PMX1)
		cp = &PMX0;
	cli();
	*cp = 0x80;
	*p = val;
	SREG = _o_sreg;
}

void atomicWriteWord(volatile uint8_t *p, uint16_t val)
{
	uint8_t _o_sreg = SREG;

	cli();
	*(p + 1) = (uint8_t)(val >> 8);
	nop(); nop(); nop();
	*p = (uint8_t)val;
	SREG = _o_sreg;
}

void sysClock(uint8_t mode)
{
	if(mode == INT_OSC_32M) {
		// switch to internal crystal
		GPIOR0 = PMCR & 0x9f;
		PMCR = 0x80;
		PMCR = GPIOR0;

		// disable external crystal
		GPIOR0 = PMCR & 0xf3;
		PMCR = 0x80;
		PMCR = GPIOR0;

	}  else if(mode == INT_OSC_32K) {
	    // switch to internal 32K crystal
		GPIOR0 = (PMCR & 0x9f) | 0x40;
		PMCR = 0x80;
		PMCR = GPIOR0;

		// disable external crystal
		GPIOR0 = (PMCR & 0xf2) | 0x02;
		PMCR = 0x80;
		PMCR = GPIOR0;
	} else if(mode == EXT_OSC_32K) {
        // enable external 32K OSC crystal
        GPIOR0 = (PMCR & 0xf0) | 0x08;
        PMCR = 0x80;
        PMCR = GPIOR0;
        
        // waiting for crystal stable
        OSC_DELAY();
    
        // switch to external crystal
        GPIOR0 = (PMCR & 0x9f) | 0x60;
        PMCR = 0x80;
        PMCR = GPIOR0;
   } else {

	// set to right prescale first
	CLKPR = 0x80;
	CLKPR = 0x01;

	asm volatile ("nop");
	asm volatile ("nop");

	// enable external 400~32MHz OSC crystal
	GPIOR0 = PMX2 | 0x04;
	PMX2 = 0x80;
	PMX2 = GPIOR0;

	GPIOR0 = (PMCR & 0xf3) | 0x04;
	PMCR = 0x80;
	PMCR = GPIOR0;

	// waiting for crystal stable
	OSC_DELAY();

	// switch to external 400~32MHz crystal
	PMCR = 0x80;
	PMCR = 0xb7;
	OSC_DELAY();

	// disable internal 32MHz crystal
	PMCR = 0x80;
	PMCR = 0xb6;
	OSC_DELAY();
    }
	clock_set = 1;
}

void sysClockPrescale(uint8_t divn)
{
	GPIOR0 = 0x80 | (divn & 0xf);
	CLKPR = 0x80;
	CLKPR = GPIOR0;
}

void sysClockOutput(uint8_t enable)
{
	if (enable)
		CLKPR |= 0x20;  // output cup fre to PB0
	else 
		CLKPR &= ~(0x20);
	//CLKPR |= 0x40;  // output cup fre to PE5
}

#if 0
#if defined(__LGT8FX8P__) || defined(__LGT8FX8E__)
// Log(HSP v3.7): PWM working mode
// Function:
//	wmode: pwm working mode
//		- PWM_MODE_NORMAL: normal single output
//		- PWM_MODE_COMPM0: complementary dual output 
//		- PWM_MODE_COMPM1: complementary dual output (inverted)
//		- PWM_MODE_COMPM2: complementary dual output 
//		- PWM_MODE_COMPM3: complementary dual output (inverted)
//	fmode: pwm frequency settings
//		- PWM_FREQ_SLOW: slow range
//		- PWM_FREQ_NORMAL: normal range
//		- PWM_FREQ_FAST: fast range 
//		- PWM_FREQ_BOOST: boost target frequency by x4
//	dband: dead band settings
//		- only valid for complementary working mode 
// note:
//		- Timer 2 is used for system tick, so don't touch!!
//static uint8_t tmr1_boost_en = 0;
//static uint8_t tmr3_boost_en = 0;

void pwmMode(uint8_t pin, uint8_t wmode, uint8_t fmode, uint8_t dband)
{
	volatile uint8_t *pTCCRX = 0;

	uint8_t timer = digitalPinToTimer(pin) & 0xf0;

	if(timer == TIMER0) { // TIMER0
		pTCCRX = &TCCR0B;
		if(wmode == PWM_MODE_NORMAL) {
			cbi(TCCR0B, DTEN0);
			cbi(TCCR0A, COM0B0);
		} else {
			sbi(TCCR0B, DTEN0);
			TCCR0A = (TCCR0A & ~_BV(COM0B0)) | (wmode & 0x10);
			DTR0 = ((dband & 0xf) << 4) | (dband & 0xf);
		}

		if((fmode & PWM_FREQ_BOOST) == PWM_FREQ_BOOST) {
			// enable frequency boost (x4) mode
			sbi(TCKCSR, F2XEN);
			delayMicroseconds(10);
			sbi(TCKCSR, TC2XS0);					
		} else if(bit_is_set(TCKCSR, TC2XS0)) {
			cbi(TCKCSR, TC2XS0);
			delayMicroseconds(10);
			cbi(TCKCSR, F2XEN);				
		}
	} else if(timer == TIMER1) { // TIMER1
		pTCCRX = &TCCR1B;
		if(wmode == PWM_MODE_NORMAL) {
			cbi(TCCR1C, DTEN1);
			cbi(TCCR1A, COM1B0);
		} else {
			sbi(TCCR1C, DTEN1);
			TCCR1A = (TCCR1A & ~_BV(COM1B0)) | (wmode & 0x10);
			DTR1L = dband;
			DTR1H = dband;
		}
		if((fmode & PWM_FREQ_BOOST) == PWM_FREQ_BOOST) {
			sbi(TCKCSR, F2XEN);
			delayMicroseconds(10);
			sbi(TCKCSR, TC2XS1);
		} else if(bit_is_set(TCKCSR, TC2XS1)) {
			cbi(TCKCSR, TC2XS1);
			delayMicroseconds(10);
			cbi(TCKCSR, F2XEN);
		}		
	} else if(timer == TIMER3) { // TIMER3
		pTCCRX = &TCCR3B;
		if(wmode == PWM_MODE_NORMAL) {
			cbi(TCCR3C, DTEN3);
			cbi(TCCR3A, COM3B0);
		} else {
			sbi(TCCR3C, DTEN3);
			TCCR3A = (TCCR3A & ~_BV(COM3B0)) | (wmode & 0x10);
			DTR3A = dband;
			DTR3B = dband;
		}
	}

	if(pTCCRX == 0) return;

	if((fmode & 0x7f) == PWM_FREQ_SLOW) {
		*pTCCRX = (*pTCCRX & 0xf8) | PWM_FREQ_SLOW;	// prescale = 1024 (slowest mode)
	} else if((fmode & 0x7f) == PWM_FREQ_FAST) {
		*pTCCRX = (*pTCCRX & 0xf8) | PWM_FREQ_FAST; // prescale = 1 (fastest mode)
	} else if ((fmode & 0x7f) == PWM_FREQ_NORMAL) {
		*pTCCRX = (*pTCCRX & 0xf8) | PWM_FREQ_NORMAL;	// prescale = 64 (default)
	}
}

// Log(HSP v3.7): enhanced PWM settings
// Function:
//	- set PWM frequency (unit: Hz), return maximum duty cycle 
// Note: 
//	- only PWM Timer1/Timer3 support frequency update
uint16_t pwmFrequency(uint8_t pin, uint32_t fhz)
{
	uint16_t icrx = 0;
	uint8_t csxs = 0;
	uint8_t boost = 0;
	volatile uint8_t *pICRX = 0;

	uint8_t timer = digitalPinToTimer(pin) & 0xf0;

	// Note for TIMER0 
	// ============================================================================
	// timer 0 working in FPWM mode which TOP is fixed to 0xFF
	// so we can change its prescale to set frequency range (fast/normal/slow)
	// fast mode:	16000000/(1*256) = 62.5K, support boost up to 62.5x4 = 250KHz
	// normal mode:	16000000/(64*256) = 976Hz, support boost up to 3.9KHz
	// slow mode:	16000000/(1024*256) = 61Hz, support boost up to 244Hz
	// ============================================================================

	if(timer == TIMER1) { // TIMER1
		pICRX = &ICR1L;
		csxs = TCCR1B & 0x7;
		boost = bit_is_set(TCKCSR, TC2XF1);
	} else if(timer == TIMER3) { // TIMER3
		pICRX = &ICR3L;
		csxs = TCCR3B & 0x7;
	}

	if(pICRX == 0) return 0xff;

	// DO NOT try to merge the two cases, compiler will try to 
	// optimize the divider if either of oprands is constant value
	if(boost == 0) {
		if(csxs == PWM_FREQ_FAST) { // fast mode
			icrx = (uint16_t) ((F_CPU >> 1) / fhz);
		} else if(csxs == PWM_FREQ_NORMAL) { // normal mode
			icrx = (uint16_t) ((F_CPU >> 7) / fhz);
		} else if(csxs == PWM_FREQ_SLOW) { // slow mode
			icrx = (uint16_t) ((F_CPU >> 11) / fhz);
		}
	} else {
		if(csxs == PWM_FREQ_FAST) { // fast mode
			icrx = (uint16_t) ((64000000UL >> 1) / fhz);
		} else if(csxs == PWM_FREQ_NORMAL) { // normal mode
			icrx = (uint16_t) ((64000000UL >> 7) / fhz);
		} else if(csxs == PWM_FREQ_SLOW) { // slow mode
			icrx = (uint16_t) ((64000000UL >> 11) / fhz);
		}	
	}
	
	atomicWriteWord(pICRX, icrx);

	return icrx;
}

// Log(HSP v3.7):
// Function:
//	- return frequency (in Hz) by give PWM resolution (bits width of duty)
// Note: 
//	- timer0/2 works in FPWM mode, pwm frequency is fixed by given mode
//	- timer1/3 works in PCPWM mode, means frequency reduced by a half
uint32_t pwmResolution(uint8_t pin, uint8_t resBits)
{
	uint8_t csxs = 0;
	uint8_t boost = 0;
	uint32_t freq = 0x0UL;

	uint8_t timer = digitalPinToTimer(pin) & 0xf0;

	if(timer != TIMER1 && timer != TIMER3)
		return 0x0UL;

	if(timer == TIMER1) { // TIMER1
		csxs = TCCR1B & 0x7;
		boost = bit_is_set(TCKCSR, TC2XF1);
	} else if(timer == TIMER3) { // TIMER3
		csxs = TCCR3B & 0x7;
	}	
	
	if(boost != 0) {
		if(csxs == PWM_FREQ_FAST) {
			freq = (64000000UL >> 1) / (1 << resBits);
		} else if(csxs == PWM_FREQ_SLOW) {
			freq = (64000000UL >> 11) / (1 << resBits);
		} else { // PWM_FREQ_NORMAL
			freq = (64000000UL >> 7) / (1 << resBits);
		}
	} else {
		if(csxs == PWM_FREQ_FAST) {
			freq = (F_CPU >> 1) / (1 << resBits);
		} else if(csxs == PWM_FREQ_SLOW) {
			freq = (F_CPU >> 11) / (1 << resBits);
		} else { // PWM_FREQ_NORMAL
			freq = (F_CPU >> 7) / (1 << resBits);
		}
	}

	// update pwm frequency
	pwmFrequency(pin, freq);

	return freq;
}
#endif

#endif

void lgt8fx8x_init()
{
#if defined(__LGT8FX8E__)
// store ivref calibration 
	GPIOR1 = VCAL1;
	GPIOR2 = VCAL2;

// enable 1KB E2PROM 
	ECCR = 0x80;
	ECCR = 0x40;

// clock source settings
	if((VDTCR & 0x0C) == 0x0C) {
		// switch to external crystal
		sysClock(EXT_OSC_32M);
	} else {
		CLKPR = 0x80;
		CLKPR = 0x01;
	}
#else

#if defined(__LGT8F_SSOP20__)
        GPIOR0 = PMXCR | 0x07;
        PMXCR = 0x80;
        PMXCR = GPIOR0;
#endif

	// enable 32KRC for WDT
	 GPIOR0 = PMCR | 0x10;
	 PMCR = 0x80;
	 PMCR = GPIOR0;

	 // clock scalar to 16MHz
	 //CLKPR = 0x80;
	 //CLKPR = 0x01;
#endif
}

void lgt8fx8x_clk_src()
{
// select clock source
#if defined(CLOCK_SOURCE)
	sysClock(CLOCK_SOURCE);
#endif

// select clock prescaler
#if defined(F_CPU)
    CLKPR = 0x80;
    #if F_CPU == 32000000L
        #if CLOCK_SOURCE == INT_OSC_32M || CLOCK_SOURCE == EXT_OSC_32M
            CLKPR = SYSCLK_DIV_0;
        #else
            #error "Clock Source Must 32MHz"
        #endif
    #elif F_CPU == 24000000L
		#if CLOCK_SOURCE == EXT_OSC_24M
            CLKPR = SYSCLK_DIV_0;
        #else
            #error "Clock Source Must 24MHz"
        #endif
    #elif F_CPU == 16000000L
        #if CLOCK_SOURCE == INT_OSC_32M || CLOCK_SOURCE == EXT_OSC_32M
            CLKPR = SYSCLK_DIV_2;
        #elif CLOCK_SOURCE == EXT_OSC_16M
            CLKPR = SYSCLK_DIV_0;
        #else
            #error "Clock Source Must 16,32MHZ"
        #endif
    #elif F_CPU == 12000000L
        #if CLOCK_SOURCE == EXT_OSC_24M
            CLKPR = SYSCLK_DIV_2;
        #elif CLOCK_SOURCE == EXT_OSC_12M
            CLKPR = SYSCLK_DIV_0;
        #else
            #error "Clock Source Must 12,24MHZ"
        #endif
    #elif F_CPU == 8000000L
        #if CLOCK_SOURCE == INT_OSC_32M || CLOCK_SOURCE == EXT_OSC_32M
            CLKPR = SYSCLK_DIV_4;
        #elif CLOCK_SOURCE == EXT_OSC_16M
            CLKPR = SYSCLK_DIV_2;
        #elif CLOCK_SOURCE == EXT_OSC_8M
            CLKPR = SYSCLK_DIV_0;        
        #else
            #error "Clock Source Must 8,16,32MHz"
        #endif
    #elif F_CPU == 4000000L
        #if CLOCK_SOURCE == INT_OSC_32M || CLOCK_SOURCE == EXT_OSC_32M
            CLKPR = SYSCLK_DIV_8;
        #elif CLOCK_SOURCE == EXT_OSC_16M
            CLKPR = SYSCLK_DIV_4;
        #elif CLOCK_SOURCE == EXT_OSC_8M
            CLKPR = SYSCLK_DIV_2;
        #elif CLOCK_SOURCE == EXT_OSC_4M
            CLKPR = SYSCLK_DIV_0;
        #else
            #error "Clock Source Must 4,8,16,32MHZ"
        #endif
    #elif F_CPU == 2000000L
        #if CLOCK_SOURCE == INT_OSC_32M || CLOCK_SOURCE == EXT_OSC_32M
            CLKPR = SYSCLK_DIV_16;
        #elif CLOCK_SOURCE == EXT_OSC_16M
            CLKPR = SYSCLK_DIV_8;
        #elif CLOCK_SOURCE == EXT_OSC_8M
            CLKPR = SYSCLK_DIV_4;
        #elif CLOCK_SOURCE == EXT_OSC_4M
            CLKPR = SYSCLK_DIV_2;
        #elif CLOCK_SOURCE == EXT_OSC_2M
            CLKPR = SYSCLK_DIV_0;
        #else
            #error "Clock Source Must 2,8,16,32MHZ"
        #endif

    #elif F_CPU == 1000000L
        #if CLOCK_SOURCE == INT_OSC_32M || CLOCK_SOURCE == EXT_OSC_32M
            CLKPR = SYSCLK_DIV_32;
        #elif CLOCK_SOURCE == EXT_OSC_16M
            CLKPR = SYSCLK_DIV_16;
        #elif CLOCK_SOURCE == EXT_OSC_8M
            CLKPR = SYSCLK_DIV_8;
        #elif CLOCK_SOURCE == EXT_OSC_4M
            CLKPR = SYSCLK_DIV_4;
        #elif CLOCK_SOURCE == EXT_OSC_2M
            CLKPR = SYSCLK_DIV_2;
        #elif CLOCK_SOURCE == EXT_OSC_1M
            CLKPR = SYSCLK_DIV_0;
        #else
            #error "Clock Source Must 1,2,4,8,16,32MHZ"
        #endif
    #elif F_CPU == 400000L
        #if CLOCK_SOURCE == EXT_OSC_400K
            CLKPR = SYSCLK_DIV_0;
        #else
            #error "Clock Source Must 400KHz" 
        #endif
    #elif F_CPU == 32000L
        #if CLOCK_SOURCE == INT_OSC_32K || CLOCK_SOURCE == EXT_OSC_32K
            CLKPR = SYSCLK_DIV_0;
        #else
            #error "Clock Source Must 32KHz"
        #endif
    #endif
	// CLKPR |= 0x20;  // output cup fre to PB0
#endif
}


int main(void)
{

#if defined(__LGT8F__)

	lgt8fx8x_init();

#if defined(CLOCK_SOURCE)
	if(clock_set == 0)
		lgt8fx8x_clk_src();
#endif

#endif	

	init();

	initVariant();

#if defined(USBCON)
	USBDevice.attach();
#endif
	
	setup();
    
	for (;;) {
		loop();
		if (serialEventRun) serialEventRun();
	}
        
	return 0;
}
