//***************************************************************************************
// MSP430 7-Segment with Timer Blink LED Demo - Timer A Software Toggle P1.0 & P1.6
//
// Adapted for use with GCC toolchains from TI's demo application 
//
// Description; Toggle P1.0 and P1.6 by xor'ing them inside of a software loop. 
// Since the clock is running at 1Mhz, an overflow counter will count to 8 and then toggle
// the LED. This way the LED toggles every 0.5s. 
// 1 instruction cylce: 1/1 MHz = 1 uSec
// ACLK = n/a, MCLK = SMCLK = default DCO 
//
// MSP430G2xxx
// -----------------
// / |    \| XIN|-
//   |     |    |
// --|RST   XOUT|-
//   |     P1.6 |-->LED
//   |      P1.0|-->LED
//	 |		P2.2|--> DIGIT-1 (left-most)SELECT
//   |		P2.3|--> 74HC595's CLK
//   |      P2.4|---> 74HC595's *OE (active low)
//   |      P2.5|---> 74HC595's serial data
//
//   DIGIT-x is actie high
//   pin A-G: active low
// Lutfi Shihab
// Dec 2012
// Built with msp430 GNU Toolchains
//***************************************************************************************
#include <msp430.h>

#if defined(__GNUC__)
#include <signal.h>
#include <isr_compat.h>
#else
/* TI's compilers */
#endif


#define LED_0 BIT0 
#define LED_1 BIT6
#define LED_OUT P1OUT
#define LED_DIR P1DIR

/* 7 segments */
#define SSEG_DIR	P2DIR
#define SSEG_OUT	P2OUT
#define SSEG_DIGIT1 BIT2
#define SSEG_CLK	BIT3
#define SSEG_OE		BIT4
#define SSEG_SER	BIT5

#define LED_ON(LED)     (LED_OUT |= (LED))
#define LED_OFF(LED)    (LED_OUT &= ~(LED))


#define BITS_CLEAR(PORT, PINS)		((PORT) &= ~(PINS))
#define BITS_SET(PORT, PINS)		((PORT) |= (PINS))
#define BITS_TOGGLE(PORT, PINS)		((PORT) ^= (PINS))

// Change WDT+ to interval timer mode, clock/8192 interval
#define WDT_INT_TIMER_MODE (WDTPW+WDTCNTCL+WDTTMSEL+WDTIS0)

//#define REVERSE_SSEG	1

/* Declarations for 7 segment display
 *    ==========
 *   ||   A    ||
 *   ||        ||
 *   ||F      B||
 *   ||   G    ||
 *    ==========
 *   ||        ||
 *   ||E      C||
 *   ||        ||
 *   ||   D    ||
 *    ==========   |=|dp
 * 
 */
// Bit positions for names (A is a low bit position)

#define SS_A 0x01 
#define SS_B 0x02
#define SS_C 0x04
#define SS_D 0x08
#define SS_E 0x10
#define SS_F 0x20
#define SS_G 0x40
#define SS_dp 0x80

// values for digits

#ifdef REVERSE_SSEG

#define SS_0 (SS_G)
#define SS_1 (SS_A + SS_D + SS_E + SS_F + SS_G)
#define SS_2 (SS_C + SS_F)
#define SS_3 (SS_E + SS_F)
#define SS_4 (SS_A + SS_D + SS_E)
#define SS_5 (SS_B + SS_E)
#define SS_6 (SS_B)
#define SS_7 (SS_D + SS_E + SS_F + SS_G)
#define SS_8 0
#define SS_9 (SS_E)

#else

#define SS_0 (SS_A + SS_B + SS_C + SS_D + SS_E + SS_F)
#define SS_1 (SS_B + SS_C)
#define SS_2 (SS_A + SS_B + SS_D + SS_E + SS_G)
#define SS_3 (SS_A + SS_B + SS_C + SS_D + SS_G)
#define SS_4 (SS_F + SS_B + SS_C + SS_G)
#define SS_5 (SS_A + SS_F + SS_G + SS_C + SS_D)
#define SS_6 (SS_A + SS_F + SS_C + SS_D + SS_E + SS_G)
#define SS_7 (SS_A + SS_B + SS_C)
#define SS_8 (SS_A + SS_B + SS_C + SS_D + SS_E + SS_F + SS_G)
#define SS_9 (SS_A + SS_B + SS_C + SS_D + SS_F + SS_G)

#endif

void sleep(unsigned num_millisecs);

unsigned int timerCount = 0;
const unsigned char decimal_code[]=  
	{SS_0, SS_1, SS_2, SS_3, SS_4, SS_5, SS_6, SS_7, SS_8, SS_9};

void DisplayTo7Seg(unsigned char b)
{
	unsigned char mask=0x80;    // variable bit mask
	unsigned char thisbit;      // current bit
	int i;

	// loop to shift in the data bits
	// note: ninth time is to clk last bit into the output register.
	//       for ninth time, mask=0 and we write an extra 0 bit
	// MSB first.
	BITS_SET(SSEG_OUT, SSEG_OE); // turn off output to the display
	for (i=0; i<9; ++i) {
		BITS_CLEAR(SSEG_OUT, SSEG_CLK); // drop the clock line so that data can change
		thisbit = (b & mask) ? SSEG_SER : 0; // compute current bit to send
		SSEG_OUT = (SSEG_OUT & ~SSEG_SER) | thisbit; // write the bit
		BITS_SET(SSEG_OUT, SSEG_CLK);   // raise the clock line to trigger read
		mask >>= 1;    // shift the mask to the right.
	} 
    BITS_CLEAR(SSEG_OUT, SSEG_OE); // turn on the display output
}


int main(void)
{
	WDTCTL = WDTPW + WDTHOLD;	// Stop watchdog timer
	BITS_SET(LED_DIR, (LED_0 | LED_1)); // Set P1.0 and P1.6 to output direction
	LED_OFF(LED_0);
	LED_ON(LED_1);			// set the LED_1 on

	CCTL0 = CCIE;				// capture/compare interrupt enable
	TACTL = TASSEL_2 + MC_2;	// Set the timer A to SMCLCK, Continuous up

	/* 7 segments */

	/* set all pins' direction to 74HC595 as outputs */
	BITS_CLEAR(SSEG_OUT, SSEG_SER);
	BITS_CLEAR(SSEG_OUT, SSEG_CLK);  // clocks low
	BITS_SET(SSEG_OUT, SSEG_OE);	// disable 7seg
	BITS_SET(SSEG_DIR, SSEG_CLK | SSEG_OE | SSEG_SER); 

	/* select the digit */
	BITS_SET(SSEG_OUT, SSEG_DIGIT1);

	while (1) {
	    int i; 
		unsigned char value; 
    
		for (i=9; i>=-1; --i) { 
			if (i>=0) { 
				value= decimal_code[i]; 
		    }
			else { 
				value=SS_dp; // turn on decimal point for i=-1
			}
		    DisplayTo7Seg(value);
			sleep(100);
			//DisplayTo7Seg(SS_2);
		}
		sleep(100);
	}

	// Clear the timer and enable timer interrupt
	__enable_interrupt();

	__bis_SR_register(LPM0_bits + GIE); // LPM0 with interrupts enabled
    
	/* at this point, CPU is put in sleep and is only awaken up to serve
     * timer interrupt, then goes back to sleep.
     * The interrupt is periodically called for 1 MHz/(2^16) = 15.25 times/sec
     * or about every 65 mSec.
     */
} 


unsigned MS_remaining;   // remaining milliseconds to sleep count

void sleep(unsigned num_millisecs) 
{
	// setup timer to count down milliseconds
	MS_remaining=num_millisecs;
	TACTL=TASSEL_2+ID_3; //  divisor=8, source=SMCLK, interrupt enable
	TACCTL0=CCIE; // enable TACCR0 interrupt
	TACCR0=1000; // 1 millisecond
	TACTL |= MC_1; // timer on in upmode
	_bis_SR_register(GIE+LPM0_bits); // interrupts on and CPU off
} 


// Timer A0 interrupt service routine

#ifdef __GNUC__
	#ifdef __MSP430G2553__
	ISR(TIMER0_A0, Timer_A )
	#else
	ISR( TIMERA0, Timer_A ) 
	#endif
#else
#pragma vector=TIMERA0_VECTOR
__interrupt void Timer_A (void)
#endif
{
    /* This interrupt is called for every 65 mSec.
     * To generate about 0.5 sec delay, we need to add another delay,
     * this time using variable that counts to 8, so 65 mSec * 8 = 0.52 secs */
	
	timerCount = (timerCount + 1) % 32;
	if(timerCount ==0)
	{
		BITS_TOGGLE(LED_OUT, LED_0 | LED_1);
	}
	if ((--MS_remaining)==0) {
		// here the final time has been reached
		TACTL=TACLR; // reset the timer
		//Turn on CPU (NOTE: GIE is set after sleep)
		__bic_SR_register_on_exit(LPM0_bits);
	}	
}
