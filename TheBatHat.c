																																		// Mega1284 version
// ECE 4760: Final Project
//
// ~~THE BAT HAT~~
//
// Jeff Buswell (jjb284@cornell.edu), Clifford Chou (cc697@cornell.edu), and Andrew Knauss (adk75@cornell.edu)
//  November 19, 2013
//
// Credits:
//  Some code borrowed from Bruce Land, 4760 examples

/*
       _==/          i     i          \==_
     /XX/            |\___/|            \XX\
   /XXXX\            |XXXXX|            /XXXX\
  |XXXXXX\_         _XXXXXXX_         _/XXXXXX|
 XXXXXXXXXXXxxxxxxxXXXXXXXXXXXxxxxxxxXXXXXXXXXXX
|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|
XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
|XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX|
 XXXXXX/^^^^"\XXXXXXXXXXXXXXXXXXXXX/^^^^^\XXXXXX
  |XXX|       \XXX/^^\XXXXX/^^\XXX/       |XXX|
    \XX\       \X/    \XXX/    \X/       /XX/
       "\       "      \X/      "      /" 

*/

#define F_CPU 16000000UL

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdio.h>

#include <avr/pgmspace.h>
#include <stdlib.h>
#include <string.h>
#include <util/delay.h>

// serial communication library
#include "uart.h"
#include "uart.c"

// UART file descriptor
// putchar and getchar are in uart.c
FILE uart_str = FDEV_SETUP_STREAM(uart_putchar, uart_getchar, _FDEV_SETUP_RW);

//define begin and end
#define begin {
#define end }

#define pulseDuration 8000
#define speedOfSound 0.000342 // m/us
#define theAnswerToLifeTheUniverseAndEverything 42
#define expectedMaxNominalPulseLength 2800 //microseconds
#define infinity 65535
#define maxDistance 70
#define motorMax 0
#define motorMin 235
#define motorOff 255
          
volatile unsigned int pulseCount;  //counter for pulse time
volatile unsigned long pulseTime, firstPulseLength, echoTime;
volatile unsigned int pulseOn; //the sound is being pulsed
volatile unsigned int distance; //the distance the sound travelled
volatile unsigned int risingEdge;
volatile unsigned int pulseNumber = 0;
         
//**********************************************************
//timer 0 compare ISR (every 10 us)
ISR (TIMER0_COMPA_vect)
begin
	pulseTime++;
end

//**********************************************************
// timer 1 capture ISR
//counts @ full rate, 16MHz
ISR (TIMER1_COMPA_vect)
begin
	if(pulseOn)
	begin
		OCR1A = 60000; //1/28th the time till the next pulse
		pulseOn = 0;
		PORTB=0; //turn off
		pulseCount = 0;
	end
	else
	begin
		pulseCount++;
		if (pulseCount >= 28)
		begin
			OCR1A = pulseDuration;
			pulseOn = 1;
			pulseNumber = 0;
			firstPulseLength = 0;
			risingEdge = 1;
			PORTB=(1<<PINB2); //turn on
		end
	end
end

// --- external interrupt ISR ------------------------
ISR (INT1_vect) 
begin
  if(risingEdge)
  begin
    if(pulseNumber == 0)
    begin
      pulseTime = 0;
      echoTime = 0;
    end
    else if(pulseNumber == 1)
    begin
	  //set pulse number to anything but 1 or 0
      pulseNumber = theAnswerToLifeTheUniverseAndEverything; //#yolo
      echoTime = pulseTime; //echo time in 10 us
    end
    risingEdge = 0;
  end
  else //falling edge
  begin
    if(pulseNumber == 0)
    begin
      firstPulseLength = pulseTime;
      pulseNumber = 1;
    end
    risingEdge = 1;
  end
end

//**********************************************************       
//Entry point and main loop
int main(void)
begin

  initialize();

  fprintf(stdout,"Initialization complete. \n\r");

  int lastEchoTime;
  
  while(1)
  begin 
    if(firstPulseLength*10 >= expectedMaxNominalPulseLength) 
    begin
      distance = 0; //near field
	  OCR2A = motorMax;
	  PORTD&=(~(1<<PIND2)); //turn on LED
      fprintf(stdout,"Distance: %i \n\r", distance);
    end
    else if(echoTime != 0)
    begin
      distance = (int) ((echoTime*10*speedOfSound*39)/2); //half round trip, in inches
	  OCR2A = (distance <= maxDistance ? ((int) (motorMax + (distance*(motorMin-motorMax)/maxDistance))) : motorMin);     
	  PORTD&=(~(1<<PIND2)); //turn on LED
	  fprintf(stdout,"Distance: %i \n\r", distance);
    end
    else 
	begin
		distance = infinity; //nothing in front of us
		OCR2A = motorOff;
	  	PORTD|=(1<<PIND2); //turn off LED
	end
  end
  
end  

//********************************************************** 
//Initialize Ports B and D, UART, ISRs
void initialize(void)
begin

  //set up timer 0
  TIMSK0= (1<<OCIE0A);  //turn on timer 0 cmp match ISR 
  OCR0A = 160;			//interrupt every 10 us
  TCCR0A= (1<<WGM01); 
  TCCR0B = 1;  

  //set up timer 1 
  TIMSK1= (1<<OCIE1A); 	//turn on timer 1 cmp match ISR 
  OCR1A = pulseDuration; //set the compare reg
  TCCR1A = 0;
  TCCR1B = 9; //no prescaler, turn on clear-on-match
  
  TCNT1 = 0; 

  // set up timer 2
  TCCR2B= (1<<CS22) ; //PRESCALER TO 256
  TCCR2A= (1<<COM2A0) | (1<<COM2A1) | (1<<WGM20) | (1<<WGM21) | (1<<COM2B0) | (1<<COM2B1) ;  
  OCR2A = 255;

  DDRB = (1<<PINB2);    // PORT b.2 is an output 
  PORTB=0;
  pulseOn = 1; 

  DDRD=0xC6;	// PORT D is an input except for UART on D.1, D.6, and D.7 (PWM on timer2), D.2 is the LED
  PORTD = 0xC0;

  //set up INT1 for any edge
  EIMSK = 1<<INT1 ; // turn on int1
  EICRA =  4;       // any edge triggers interrupt
      
  //init the UART -- uart_init() is in uart.c
  uart_init();
  stdout = stdin = stderr = &uart_str;
      
  //enable interrupts
  sei();
  
end
