#include <stdio.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h> 
#include "lcd.h"

#define F_CPU 2000000UL
#include <util/delay.h>
#define T_DIV 8

#define I_TICK	4
#define P_TICK 	4

#include "temptable.h"

#define K 0.103211

#include <avr/io.h>

#define  MAXTEMP 60

void calctimer(unsigned short wait, unsigned short injtime);
unsigned char temp_calc(unsigned char raw);
unsigned char bat_calc(unsigned char raw);

//TODO:

//Implement downcounting		
//Make update in epos mode better. Add start/stop switch. 

// Check for overrun of AD		--Done
	
//Timers for Background light 	--Not needed, maybe in the future, but HW is prepared.
//Timers for Contrast.			--Not needed, maybe in the future, but HW is prepared.

// Calibrate output to match the real values, we will use 2Mhz CPU speed. 16Mhz not needed.

/*

	ADC = 150 -> 105Hz
	ADC = 1022 -> 15.5Hz (ungefär 15Hz)
	
*/

/*
unsigned short ad_freq;
unsigned short ad_dc;

unsigned short ad_vbatt;
unsigned short ad_temp;
*/

static unsigned short ad[4];

volatile unsigned short adcraw = 0;

unsigned char ad_count = 0;

unsigned char counter = 0;	//Counter for FOR-loops

unsigned char mode = 0;
unsigned char oldmode = 1;

unsigned char oldvalue = 0;
unsigned char value = 0;

unsigned char down = 0;
unsigned char up = 0;

unsigned char enter = 0;
unsigned char back = 0;

unsigned char toggle = 0;

unsigned char cursor = 0;

unsigned char temp = 0;

/*
This is used for enter new values in mode 1 and mode 2
*/

unsigned char m2pos[14] = {0,11,12,13,14,15,19,20,21,26,27,29,30,5};
unsigned char m2value[14] = {0,0,0,1,0,0,1,0,0,0,4,3,1,0};

unsigned long m2count = 0;
unsigned short m2rest = 0;
unsigned char m2high = 0;
unsigned char m2low = 0;

unsigned char m1pos[9] = {0,10,11,12,26,27,29,30,5};
unsigned char m1value[9] = {0,1,0,0,1,0,0,0,0};

unsigned short m1rest = 0;
unsigned char m1high = 0;
unsigned char m1low = 0;

unsigned char m0freq = 127;
unsigned char m0dchigh = 23;
unsigned char m0dclow = 34;

unsigned char m3freq = 100;
unsigned char m3dchigh = 50;
unsigned char m3dclow = 00;

unsigned char epos = 0;

unsigned char diagnostic = 0;

volatile unsigned short downcount= 100;
volatile unsigned char downcount_mode = 0;

char text[30]; //Array holding text for LCD printout
char text2[2]; //Array holding text for LCD printout

/*variables updated in ISR*/
volatile unsigned short maxcount;
volatile unsigned short duty;
//
///*Display values*/
unsigned char frequency = 0;

float conversion;
unsigned char procent = 0;
unsigned char dutycycle = 0;
unsigned long updatecompare = 0;

ISR(ADC_vect)
{
	//adcraw = ADC;
	
	ad[ad_count] = (ADC>>2);
	ad_count++;
	if(ad_count > 3)
	{
		ad_count = 0;
	}
	
	ADMUX = ad_count+4;			//Start from ADC4 
	ADCSRA |= (1<<ADSC);
}

ISR(TIMER0_OVF_vect)
{
	
	TCCR0B = 0x00;		//Stop the timer									
	TCNT0 = 0x00;		//Reset the timer
	OCR0A = 0x00;		//Compare register to zero at start

}

ISR(TIMER1_OVF_vect)
{
	if(downcount_mode)
	{
		downcount--;
		if(downcount == 0)
		{
			//OCR1A = 0x0100;
			//OCR1B = 0x0100;
			//TCNT1 = 0x0000;
			
			TCCR1A = 0;
			TCCR1C = 0;
			TCCR1B = 0;			//Stop timer
			
			//TCCR1B &= ~(1<<CS11);
			//TCCR1B &= ~(0<<CS10);
			//TIMSK0 = 0;			//Disable interrupt
			downcount_mode = 0;
			//PORTB = PORTB | (1<<1) | (1<<2);
			PORTB = 0xFF;
		}
	}
}

/*
	Function: calctimer
	wait in ms 
	injtime in ms/100, -> 401 = 4.01ms

*/
void calctimer(unsigned short wait, unsigned short injtime)
{
	unsigned long int_l;
	unsigned long int_p;
	
	int_l = ((unsigned long)injtime*10)/4;
	int_p = ((unsigned long)wait*1000)/4;
	
	ICR1 = int_p;
	
	OCR1B = int_l;
	OCR1A = int_l;
}



unsigned char temp_calc(unsigned char raw)
{
	return pgm_read_word(&temptable[raw])-40;
}

unsigned char bat_calc(unsigned char raw)
{
	// 12,7 volt = 127 = 2,5volt in
	// 25,5 volt = 255
	
	//Since we have to large resistors, we need to take care of the input resitance in AD converter.
	//But for now we approx that to be around 3volt.
	
	unsigned char temp;
	
	temp = raw - 30; //30 = 3.0 volt.
	
	return temp;
}


int main(void)
{
	/*Init function*/

	DDRB = 0xFF;		//Set all PORTB to output		Need to be set to output to work with OCR1x function
	PORTB = 0xFF;		//Set all to on port to 1
	
	DDRC=0x00;		//All on PORTC to be input
	PORTC = 0xFF;	//Set pullup on all pins
	
	DDRD=0xFF;		//Set all PORTD to output
	//	DDRE=0xFF;		//Set port B to output
	
	//Setup counter0 for debounce of buttons	
	TCCR0B = 0x00;		//Stop the timer
	TCNT0 = 0x00;		//Set to zero at startup
	OCR0A = 0x00;		//Compare register to zero at start
	
	TIMSK0 = (1 << TOIE0);		//Set overflow interrupt
	
	//Enable ADC convert
	ADCSRA = ((1<<ADEN) | (1<<ADIE) | (0<< ADPS2) | (1<<ADPS1)| (1<<ADPS0));		//Enable ADC, Enable Interrupt, Enable div factor 128. Since we are not that time critical.
	ADMUX = 0x04; 		//Channel ADC4 is used for vbatt measure
	
	//Lets enable inj-timer function
	
	//Set upper limit
	ICR1 = 0xFFFF;
	
	//Set compare level
	OCR1A = 0x3FFF;
	OCR1B = 0x3FFF;	

	//Set counter to zero
	TCNT1 = 0x0000;
	
	TCCR1A =((1<<COM1A1) | (1<<COM1A0) | (1<<COM1B1) | (1<<COM1B0) | (1<<WGM11) | (0<<WGM10));		//Enable OCR1A and OCR1B to Set high on OCRx, fast PWM mode, ICR1 = upper level, OCR1A, OCR1B toggle
	TCCR1C = ((1<<FOC1A) | (1<<FOC1B));
	TCCR1B =((1<<WGM13) | (1<<WGM12) | (1<<CS11) | (0<<CS10));		//Start timer
	
	TIMSK1 = ((1<<TOIE1));
	
	sei(); //Enable Global interrupt
	
	ADCSRA |= (1<<ADSC);	//Start conversion;
	
	/*Need to set contrast output to zero*/
	
	PORTD = 0x40;		//Set to zero
	DDRD = 0xFF;		//All output
	
	/*Init display*/
	lcd_init(LCD_DISP_ON);
	lcd_clrscr();
	
	/* put string to display (line 1) with linefeed */
    lcd_puts("INJ-test by JB");
	lcd_gotoxy(0, 1);
	lcd_puts("Ver 2.0");
	lcd_clrscr();
	/*
	while(1)
	{
		if((PINC & 0x08)==0)
		{
			lcd_gotoxy(0,0);
			lcd_puts("1");
			TCCR0B = 0x05;
		}
		else
		{
			lcd_gotoxy(0,0);
			lcd_puts(" ");
		}
		
		if((PINC & 0x04)==0)
		{
			lcd_gotoxy(1,0);
			lcd_puts("2");
			TCCR0B = 0x05;
		}
		else
		{
			lcd_gotoxy(1,0);
			lcd_puts(" ");
		}
		
		if((PINC & 0x02)==0)
		{
			lcd_gotoxy(2,0);
			lcd_puts("3");
			TCCR0B = 0x05;
		}
		else
		{
			lcd_gotoxy(2,0);
			lcd_puts(" ");
		}
		
		if((PINC & 0x01)==0)
		{
			lcd_gotoxy(3,0);
			lcd_puts("4");
			TCCR0B = 0x05;
		}
		else
		{
			lcd_gotoxy(3,0);
			lcd_puts(" ");
			
		}
		
		if(TCCR0B == 0)
		{
			lcd_gotoxy(0,1);
			lcd_puts("D");
		}
		else
		{
			lcd_gotoxy(0,1);
			lcd_puts(" ");
		}

		sprintf(text, "%u", TCNT0);		//lowpart
		lcd_gotoxy(4, 1);
		lcd_puts(text);

		
	}
*/
		while(1)
		{	
			//downcount--;
			if(((PINC&0x01) == 0) & ((PINC&0x02) == 0))	//Enter temp/voltage mode
			{
				if(TCCR0B == 0)
				{
					lcd_clrscr();
					diagnostic = 1;
				}
				TCCR0B = 0x05;
			}
			if((PINC & 0x08)==0)	//we pressed the first button ENTER
			{
				if(TCCR0B == 0)
				{
					enter = 1;
					TCCR0B = 0x05;
				}
			}
				
			if((PINC& 0x04)==0)	//Second button pushed	BACK
			{
				if(TCCR0B == 0)
				{
					back = 1;
					TCCR0B = 0x05;
				}
			}
				
			if((PINC& 0x02)==0)	//Third button pressed DOWN
			{
				if(TCCR0B == 0)
				{
					if(epos > 0)
					{
						down = 1;
					}
					else
					{
						mode++;
					}
					if(mode > 3)
					{
						mode = 0;
					}
					TCCR0B = 0x05;
				}
			}
				
			if((PINC& 0x01)==0)	//Foruth button pressed UP
			{
				if(TCCR0B == 0)
				{
					if(epos > 0)
					{
						up = 1;
					}
					else
					{
						mode--;
					}
					
					if(mode == 255)
					{
						mode = 3;
					}
					TCCR0B = 0x05;
				}
			}
			if(up)
			{
				value++;
				if(value > 9)
				{
					value = 0;
				}
				up = 0;
			}
			
			if(down)
			{
				value--;
				if(value == 255)
				{
					value = 9;
				}
				down = 0;
			}
			
			if(diagnostic == 0)
			{
				if(epos == 0)
				{
					lcd_command(LCD_DISP_ON);
				}
				else
				{
					if(mode == 1)
					{
						if(oldvalue != value)
						{
							lcd_command(LCD_DISP_ON_CURSOR);
							if(m1pos[epos] > 16)
							{
								lcd_gotoxy(((m1pos[epos]-16-1)),1);
								lcd_putc(0x30+value);
								lcd_gotoxy(((m1pos[epos]-16-1)),1);
							}
							else
							{
								lcd_gotoxy((m1pos[epos]-1),0);
								lcd_putc(0x30+value);
								lcd_gotoxy((m1pos[epos]-1),0);
							}
						}
					}
					if(mode == 2)
					{
						if(oldvalue != value)
						{
							lcd_command(LCD_DISP_ON_CURSOR);
							if(m2pos[epos] > 16)
							{
								lcd_gotoxy(((m2pos[epos]-16-1)),1);
								lcd_putc(0x30+value);
								lcd_gotoxy(((m2pos[epos]-16-1)),1);
							}
							else
							{
								lcd_gotoxy((m2pos[epos]-1),0);
								lcd_putc(0x30+value);
								lcd_gotoxy((m2pos[epos]-1),0);
							}
						}
					}
					
					oldvalue = value;
				}
				
				if(enter)
				{
					if(mode == 1)
					{
						m1value[epos] = value;
						epos++;
						if(epos > 7)		//Should be 8, but on/off mode removed
						{
							epos = 0;
						}
						value = m1value[epos];
						lcd_command(LCD_DISP_ON_CURSOR);
						
						m1rest = (m1value[1]*100+m1value[2]*10+m1value[3]);
						m1high = (10*m1value[4]+m1value[5]);
						m1low = (10*m1value[6]+m1value[7]);
						
						if(m1rest > 200)
						{
							m1rest = 200;
						}
						if(m1rest < m1high)
						{
							m1rest = m1high +1;
						}
						if(mode == 1)
						{
							calctimer(m1rest, (m1high*100 + m1low));
						}
						
						if(m2pos[epos] > 16)
						{
							lcd_gotoxy(((m1pos[epos]-16-1)),1);
							lcd_putc(0x30+value);
							lcd_gotoxy(((m1pos[epos]-16-1)),1);
						}
						else
						{
							lcd_gotoxy((m1pos[epos]-1),0);
							lcd_putc(0x30+value);
							lcd_gotoxy((m1pos[epos]-1),0);
						}
						TCCR1A = 0x00;
						TCCR1C = 0x00;
						TCCR1B = 0x00;	// Clear timer, when we updating the values
					}
					if(mode == 2)
					{
						m2value[epos] = value;
						epos++;
						TCCR1A = 0x00;
						TCCR1C = 0x00;
						TCCR1B = 0x00;	// Clear timer, when we updating the values
						
						if(epos > 12)	//When enter all the values, enable interrupt and timer do make countdown to work
						{
							epos = 0;
							TCCR1A =((1<<COM1A1) | (1<<COM1A0) | (1<<COM1B1) | (1<<COM1B0) | (1<<WGM11) | (0<<WGM10));		//Enable OCR1A and OCR1B to Set high on OCRx, fast PWM mode, ICR1 = upper level, OCR1A, OCR1B toggle
							TCCR1C = ((1<<FOC1A) | (1<<FOC1B));
							TCCR1B =((1<<WGM13) | (1<<WGM12) | (1<<CS11) | (0<<CS10));		//Start timer	
							downcount_mode = 1;
							downcount = m2count;
							calctimer(m2rest, (m2high*100 + m2low));
						}
						value = m2value[epos];
						lcd_command(LCD_DISP_ON_CURSOR);
						temp = value+0x30;
						
						m2count = (m2value[1]*10000+m2value[2]*1000+m2value[3]*100+m2value[4]*10+m2value[5]);
						m2rest = (m2value[6]*100+m2value[7]*10+m2value[8]);
						m2high =(10*m2value[9]+m2value[10]);
						m2low = (10*m2value[11]+m2value[12]);
						if(m2rest > 200)
						{
							m2rest = 200;
						}
						if(m2rest < m2high)
						{
							m2rest = m2high + 1;
						}
						if(m2count > 90000)
						{
							m2count = 90000;
						}		
						
						if(m2pos[epos] > 16)
						{
							lcd_gotoxy(((m2pos[epos]-16-1)),1);
							lcd_putc(temp);
							lcd_gotoxy(((m2pos[epos]-16-1)),1);
						}
						else
						{
							lcd_gotoxy((m2pos[epos]-1),0);
							lcd_putc(temp);
							lcd_gotoxy((m2pos[epos]-1),0);
						}	

						
					}
					if(mode == 3)
					{
						if(m3dchigh == 100)
						{
							m3dchigh = 0;
						}
						if(m3dchigh == 75)
						{
							m3dchigh = 100;
						}
						if(m3dchigh == 50)
						{
							m3dchigh = 75;
						}
						if(m3dchigh == 25)
						{
							m3dchigh = 50;
						}
						if(m3dchigh == 0)
						{
							m3dchigh = 25;
						}
						
						calctimer(10, m3dchigh*10 );
						
						sprintf(text, "DC:%3u%%", m3dchigh);
						lcd_gotoxy(4, 1);
						lcd_puts(text);
					}
						enter = 0;
				}
				else
				{
					if(mode == 0)
					{
						TCCR1A =((1<<COM1A1) | (1<<COM1A0) | (1<<COM1B1) | (1<<COM1B0) | (1<<WGM11) | (0<<WGM10));		//Enable OCR1A and OCR1B to Set high on OCRx, fast PWM mode, ICR1 = upper level, OCR1A, OCR1B toggle
						TCCR1C = ((1<<FOC1A) | (1<<FOC1B));
						TCCR1B =((1<<WGM13) | (1<<WGM12) | (1<<CS11) | (0<<CS10));		//Start timer
					}
					if(mode == 1)
					{
						TCCR1A =((1<<COM1A1) | (1<<COM1A0) | (1<<COM1B1) | (1<<COM1B0) | (1<<WGM11) | (0<<WGM10));		//Enable OCR1A and OCR1B to Set high on OCRx, fast PWM mode, ICR1 = upper level, OCR1A, OCR1B toggle
						TCCR1C = ((1<<FOC1A) | (1<<FOC1B));
						TCCR1B =((1<<WGM13) | (1<<WGM12) | (1<<CS11) | (0<<CS10));		//Start timer
						calctimer(m1rest, (m1high*100 + m1low));
					}
					if(mode == 2)
					{
						if(epos == 0)			//
						{
							sprintf(text, "C:%5u", downcount);		//Print curren counter value
							lcd_gotoxy(8, 0);
							lcd_puts(text);
						}	
						else
						{
							//downcount = m2count;
							//calctimer(m2rest, (m2high*100 + m2low));	
							//downcount_mode = 1;
						}	
					}
					if(mode == 3)
					{
						calctimer(10, m3dchigh*10 );
					}
				
				}
				
				if(back)
				{
					epos = 0;
					lcd_command(LCD_DISP_ON);
					back=0;
					diagnostic = 0;
				}
				
				if(epos == 0)
				{
					if(mode == 1)
					{
						m1rest = (m1value[1]*100+m1value[2]*10+m1value[3]);
						m1high = (10*m1value[4]+m1value[5]);
						m1low = (10*m1value[6]+m1value[7]);
						
						if(m1rest > 200)
						{
							m1rest = 200;
						}
						if(m1rest < m1high)
						{
							m1rest = m1high +1;
						}
						if(mode == 1)
						{
							calctimer(m1rest, (m1high*100 + m1low));
						}
					}
					if(mode == 2)
					{
						m2count = (m2value[1]*10000+m2value[2]*1000+m2value[3]*100+m2value[4]*10+m2value[5]);
						m2rest = (m2value[6]*100+m2value[7]*10+m2value[8]);
						m2high =(10*m2value[9]+m2value[10]);
						m2low = (10*m2value[11]+m2value[12]);
						if(m2rest > 200)
						{
							m2rest = 200;
						}
						if(m2rest < m2high)
						{
							m2rest = m2high + 1;
						}
						if(m2count > 90000)
						{
							m2count = 90000;
						}
					}
				}
			
				if(mode == 0)
				{
					
					//double temp
					/*
					temp = (double)256/(double)ad[3];
					temp2 = (double)256/(double)ad[2];
					
					dutycycle = (temp * 100);
					*/
				
					dutycycle = (unsigned char)((((double)ad[3])/(double)255)*100);
					frequency = (unsigned char)((((double)ad[2])/(double)255)*100);
					
					if(frequency < 3)
					{
						frequency = 3;
					}
					if(dutycycle < 1)
					{
						dutycycle = 1;
					}
					sprintf(text, "Period:%3ums", frequency);
					lcd_gotoxy(4, 0);
					lcd_puts(text);
					sprintf(text, "Duty:%3u%%", dutycycle);
					lcd_gotoxy(4, 1);
					lcd_puts(text);
					
					//To calc real dutycycle
					dutycycle = (unsigned char)((double)frequency * ((double)dutycycle/(double)100));
					
					calctimer(frequency, dutycycle*100);
				}
				if(oldmode != mode)
				{
				lcd_clrscr();
				switch(mode)
				{
					case(0):
						lcd_puts("M0:");
						TCCR1A =((1<<COM1A1) | (1<<COM1A0) | (1<<COM1B1) | (1<<COM1B0) | (1<<WGM11) | (0<<WGM10));		//Enable OCR1A and OCR1B to Set high on OCRx, fast PWM mode, ICR1 = upper level, OCR1A, OCR1B toggle
						TCCR1C = ((1<<FOC1A) | (1<<FOC1B));
						TCCR1B =((1<<WGM13) | (1<<WGM12) | (1<<CS11) | (0<<CS10));		//Start timer
					break;
					case(1):
						lcd_puts("M1:");
						lcd_gotoxy(4, 0);
						lcd_puts("S");
						sprintf(text, "Tr:%3ums",m1rest);		//highpart
						lcd_gotoxy(6, 0);
						lcd_puts(text);
						sprintf(text, "To:%2u.", m1high);		//highpart
						lcd_gotoxy(6, 1);
						lcd_puts(text);
						sprintf(text, "%2ums", m1low);		//lowpart
						lcd_gotoxy(12, 1);
						lcd_puts(text);
						TCCR1A =((1<<COM1A1) | (1<<COM1A0) | (1<<COM1B1) | (1<<COM1B0) | (1<<WGM11) | (0<<WGM10));		//Enable OCR1A and OCR1B to Set high on OCRx, fast PWM mode, ICR1 = upper level, OCR1A, OCR1B toggle
						TCCR1C = ((1<<FOC1A) | (1<<FOC1B));
						TCCR1B =((1<<WGM13) | (1<<WGM12) | (1<<CS11) | (0<<CS10));		//Start timer
					break;
					case(2):
						lcd_puts("M2:");
						lcd_gotoxy(4, 0);
						lcd_puts("S");
						sprintf(text, "C:%5u", m2count);		//highpart
						lcd_gotoxy(8, 0);
						lcd_puts(text);
						sprintf(text, "Tr%3ums", m2rest);		
						lcd_gotoxy(0, 1);
						lcd_puts(text);
						sprintf(text, "To%2u.", m2high);		//highpart
						lcd_gotoxy(7, 1);
						lcd_puts(text);
						sprintf(text, "%2ums", m2low);		//lowpart
						lcd_gotoxy(12, 1);
						lcd_puts(text);
					break;
					case(3):
						lcd_puts("M3:");
						lcd_gotoxy(4, 0);
						lcd_puts("Condition");
						sprintf(text, "DC:%2u%%", m3dchigh);		//lowpart
						lcd_gotoxy(4, 1);
						lcd_puts(text);
						TCCR1A =((1<<COM1A1) | (1<<COM1A0) | (1<<COM1B1) | (1<<COM1B0) | (1<<WGM11) | (0<<WGM10));		//Enable OCR1A and OCR1B to Set high on OCRx, fast PWM mode, ICR1 = upper level, OCR1A, OCR1B toggle
						TCCR1C = ((1<<FOC1A) | (1<<FOC1B));
						TCCR1B =((1<<WGM13) | (1<<WGM12) | (1<<CS11) | (0<<CS10));		//Start timer
					break;
					case(4):
						
					break;
				}
				oldmode = mode;
			}
			
			
			}
			else
			{
				sprintf(text, "Temp:%3u C", temp_calc(ad[1]));		//lowpart
				lcd_gotoxy(4, 0);
				lcd_puts(text);
				
				sprintf(text, "Battery:%3u C",bat_calc(ad[0]));		//lowpart
				lcd_gotoxy(4, 1);
				lcd_puts(text);
				
				if(back)
				{
					back = 0;
					diagnostic = 0;
					lcd_clrscr();
				}
			}
			//Overtemp watchguard
			/*
			if(temp_calc(ad[0]) > MAXTEMP)
			{
				lcd_clrscr();
			}
			
			while(temp_calc(ad[0]) > MAXTEMP)
			{
				lcd_gotoxy(0, 0);
				lcd_puts("System overheated");
			}
			*/
		};
}