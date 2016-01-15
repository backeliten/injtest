	//
	///*wait to make a nice startup*/
	//for(counter = 0; counter < 15; counter++)
	//{
	//	_delay_ms(50);
	//}

////
	///*Setup ADC*/ /*Want to use ADC3*/
	//ADCSRA = _BV(ADEN) | _BV(ADPS2)| _BV(ADPS1) | _BV(ADPS0);
	//ADMUX  |= (_BV(MUX1) | _BV(MUX0));		//Setup ADC3 in MUX
	//
	//
	///*Setup TIMER1*/	/*We want to use OC1A as output*/
	//TCCR1A = 0x02;	//Fast PWM mode, ICR1 maxvalue
	//TCCR1B = 0x18;	//Stop timer set Fast PWM mode
	//TCCR1C = 0x00; //No Force output
	//
	//TCNT1H = 0x00;	//Setup counter register to zero
	//TCNT1L = 0x00; 	
	//
	//OCR1AH = 0x04;	//Set Output Compare register (0x7FFF = 32767 -> 50% dutycycle)
	//OCR1AL = 0xFF;
	//
	//ICR1H = 0xFF;	//Set maxvalue on timer
	//ICR1L = 0XFF;
	//
	//TIMSK1 = 0x03;	//Enable overflow interrupt and compare interrupt (OCIE1A)
	//
	////TCCR1B |= (_BV(CS12) | _BV(CS10));		//Clock prescaler = 1024
	//TCCR1B = 0x19;		//Clock prescaler = 0
	//
	///*Setup TIMER0*/
	//TCCR0A = 0x00;	//Normal timer;
	//TCCR0B = 0x00;	//Stop timer;
	//
	//TCNT0 = 0x00;	//Setup counter register to zero
	//
	//TIMSK0 = 0x01;	//Enable overflow interrupt
	//
	//TCCR0B = 0x04;	//Stop timer, prescaler = 256;
	//
	//sei();			//Enables Global Interrupt
	//
	//lcd_clrscr();
	//
	//while(1)
	//{
	//	if(clockflag == 1)
	//	{
	//		/*Make a ADC read*/
	//		ADMUX = 0x03;								//Setup ADC3 in MUX
	//		
	//		_delay_ms(10);
	//		
	//		ADCSRA |= _BV(ADSC);
	//		_delay_ms(1);
	//		adcval = ADCW;
	//		
	//		_delay_ms(1);
	//		
	//		ADMUX = 0x04; 								//Setup ADC4 in MUX
	//		
	//		_delay_ms(10);
	//		
	//		ADCSRA |= _BV(ADSC);
	//		_delay_ms(1);
	//		adcval2 = ADCW;
	//		
	//		/*Set limits*/
	//		
	//		if(adcval == 1023)		//dutycycle
	//		{
	//			adcval = 1022;
	//		}
	//		
	//		if(adcval < 20)
	//		{
	//			adcval = 20;
	//		}
	//		
	//		if(adcval2 == 1023)		//Speed
	//		{
	//			adcval2 = 1022;
	//		}
	//		
	//		if(adcval2 < 200)
	//		{
	//			adcval2 = 150;
	//		}
	//		
	//		/*A hole lot of calculation*/
	//		conversion = (float)adcval;
	//		procent = (unsigned char)((conversion/1023.0)*100.0);
	//		
	//		updatecompare = 64*(adcval2+1);
	//		
	//		dutycycle = (double)updatecompare*((double)procent/100);
	//		
	//		updatecompare = (unsigned short)dutycycle;
	//		
	//		duty = updatecompare;				//Dutycycle
	//		
	//		updatecompare = 64*(adcval2+1);	
	//		
	//		maxcount = updatecompare;			//Speed
	//		
	//		frequency = (unsigned char)(((1022.0 - (double)adcval2)*K) + 15.0);
	//		
	//		/*Lets print on display*/
	//		sprintf(text, "Freq:%3u Hz", frequency);
	//		lcd_gotoxy(0, 0);
	//		lcd_puts(text);
	//		
	//		sprintf(text, "Duty:%3u %%", procent);
	//		lcd_gotoxy(0, 1);
	//		lcd_puts(text);
	//		
	//		clockflag = 0;
	//		}

////
	//}
	return 0;
}
//
//ISR(TIMER1_OVF_vect)
//{
//	PORTB = 0x00;
//	ICR1 = maxcount;
//	OCR1A = duty;
//}
//
//ISR(TIMER0_OVF_vect)
//{
//	globalclock++;
//	clockflag = 1;
//}
//
//ISR(TIMER1_COMPA_vect)
//{
//	PORTB = 0x02;
//}