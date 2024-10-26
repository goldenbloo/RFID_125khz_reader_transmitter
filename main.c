
#define F_CPU 8000000UL
#define PWM_VALUE 32	

#define COIL 			PORTB3
#define SIGNAL_IN 		PORTD2
#define BUZZER 			PORTC0
#define COIL_VCC 		PORTB0
#define SAVE_LED 		PORTB1
#define SAVE_BUTTON 	PORTB4
#define MODE_BUTTON 	PORTB3
#define TRUE 1
#define FALSE 0
#define UP 1
#define DOWN 0
#define READ_MODE 0
#define TRANSMIT_MODE 1
#define BAUD 9600
#define BAUDRATE F_CPU / 16 / (BAUD - 1)
#define ONE_BIT_DELAY (1000000 / BAUDRATE)
#define T 64 					//T is half of bit length in manchester code
#define T_LOW		35
#define T_HIGH		95
#define TWO_T_LOW 	96
#define TWO_T_HIGH	160

#define read_eeprom_array(address,value_p,length) eeprom_read_block ((void *)value_p, (const void *)address, length)
#define write_eeprom_array(address,value_p,length) eeprom_write_block ((const void *)value_p, (void *)address, length)


#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

uint8_t counter = 0;
uint8_t countCopy;
uint8_t startBitCounter;
int8_t tagCount;
uint8_t timer0Counter = 0;
unsigned char lastBit = 0, checkNextEdge = 0, currentBit = 0,manchesterSync = FALSE, bitIsReady = FALSE;
volatile char  mode = READ_MODE, saveTagMode = FALSE, modeChangedFlag = FALSE;;
unsigned char dataBuffer[11]; // 10 nibbles and 1 column parity nibble
uint8_t tagId[5]; //= {0x09, 0x00, 0xB7,0xC2, 0x8D};
uint8_t existingTagId[5];


void hexAscii(void);
void init_PWM(void);
unsigned char readTagSerialNumber(void);

//===================================================================
// Reads the 32-bit RFID tag serial number and
// calculates the parity bits.
//===================================================================
unsigned char readTagSerialNumber(void)
{
	uint8_t i, k, ones, parity = TRUE, columnParity = 0;

	for (i = 0; i < 11; i++)
		dataBuffer[i] = 0;
	
	for (i = 0; i < 11; i++) /* Repeat 10 times the 5-bit lines of RFID data.*/
	{
		
		ones = 0; /* Clear the ones buffer. */
		for (k = 0; k < 4; k++)
		{
			dataBuffer[i] <<= 1;

			while (!bitIsReady && (mode == READ_MODE))
				asm("nop");
			
			dataBuffer[i] |= currentBit;
			ones += currentBit;
			bitIsReady = FALSE;			
		}
		// Get parity bit

		while (!bitIsReady && (mode == READ_MODE))
			asm("nop");
	
		ones += currentBit;
		bitIsReady = FALSE;		

		if ((ones % 2) && (i < 10)) // Exclude the 11th 4-bit array (column parity).
			parity = FALSE;			// If parity bit is odd, then mark it as FALSE.
	}

	for (i = 0; i < 11; i++)
		columnParity ^= dataBuffer[i];
	if (columnParity)
		parity = FALSE;

	return (parity); // Return the parity status (TRUE: if parity is OK, FALSE: is parity is not OK).
}

// Taking a 5 byte buffer and calculating even parity bits for rf transmittion to dataBuffer
void tagBytesToData()
{
	int8_t i, j;
	uint8_t temp, parity; // Declare parity here

	for (i = 0; i < 5; i++) // Copy a nibbles to buffer
	{		
		dataBuffer[i * 2 + 1] = tagId[i] & 0x0F;	// Copy nibble low
		dataBuffer[i * 2] = tagId[i] >> 4;	// Copy nibble high
	}
	// Calculate the parity of column bits before calc parity of nibbles
	// Stop bit should be 0
	for (i = 0; i < 10; i++)
		dataBuffer[10] ^= dataBuffer[i];
		dataBuffer[10] &= ~0x01;

	// Calculate the parity for each nibble and store it in the 5th bit.
	for (i = 0; i < 10; i++)
	{
		parity = 0;
		for (j = 0; j < 4; j++)
			parity ^= (dataBuffer[i] >> j) & 0x01;
		dataBuffer[i] <<= 1;
		dataBuffer[i] |= parity;
	}

	//Reverse bits for easy bitshift sending
	for (i = 0; i < 11; i++)
	{
		temp = dataBuffer[i];
		dataBuffer[i] = 0;
		for (j = 0; j < 5; j++) // Start from 4 for 5-bit reversal
		{			
			// dataBuffer[i] |= ((temp >> j) & 0x01) << (4 - j);
			 dataBuffer[i] <<= 1;
			 dataBuffer[i] |= (temp >> j) & 0x01; 
		}

	}
}


//Send initial 9 ones in manchester code
void sendManchesterNineOnes()
{
	uint8_t i, j;
	for (i = 0; i < 9; i++) // 9 ones loop
	{
		for (j = 0; j < 2; j++) // Half bit loops
		{
			while (timer0Counter < 8) // Wait for counter
				asm("nop");
			if (j == 0)	
				PORTB &= ~(1<<COIL); // Close transistor for high state
				
			else			 	
				
				PORTB |= (1<<COIL); // Open transistor for low state
			timer0Counter = 0;
		}

		
	}
}
// Send tag data in manchester code
void sendManchesterData()
{
	uint8_t i, bit;
	int8_t j;
	for (i = 0; i < 11; i++)
	{
		for (bit = 0; bit < 5; bit++)
		{
			for (j = 1; j >= 0; j--)
			{
				while (timer0Counter < 8)
					asm("nop");
				if ((j ^ ((dataBuffer[i] >> bit) & 0x01)) == 0)	
					PORTB &= ~(1<<COIL); // Clole MOSFET for high state
				else
					
					PORTB |= (1<<COIL); // Open MOSFET for low state
					
				timer0Counter = 0;
			}
		}
	}
}

// function to send data
void uartTransmit(unsigned char data)
{
	while (!(UCSRA & (1 << UDRE)))
		;		// wait while register is free
	UDR = data; // load data in the register
}

// function to receive data
unsigned char uartReceive(void)
{
	while (!(UCSRA) & (1 << RXC))
		;		// wait while data is being received
	return UDR; // return 8-bit data
}

void uartSendString(const char *str)
{
	while (*str)
	{
		uartTransmit(*str++);
	}
}
//===================================================================
// Convert a Hex number into ASCII and send it to serial port.
//===================================================================
void hexAscii(void)
{
	uint8_t i, temp;
	int8_t j;
	uartSendString("\r0x");	 
	for (i = 0; i < 10; i++) // Loop through 10 nibbles 
	{		
			temp = dataBuffer[i]; // Get each nibble of the current byte
			if (temp <= 9)
				uartTransmit(temp + '0'); // Convert nibble to ASCII numeral
			else
				uartTransmit(temp  + 'A' - 10); // Convert nibble to ASCII letter		
	}

	uartTransmit(0x0d); // Transmit carriage return
}


void syncErrorfunc()
{
	manchesterSync = FALSE;
	bitIsReady = FALSE;
	checkNextEdge = FALSE;


}

ISR(INT0_vect)
{
	countCopy = counter;
	counter = 0;
	sei();

	if ((countCopy > TWO_T_HIGH) || (countCopy < T_LOW))
	{
		syncErrorfunc();
		return;
	}


	// If Manchester sync is not established and counter is more than 55
	if (!manchesterSync)
	{
		if ((countCopy > TWO_T_LOW)&&(countCopy < TWO_T_HIGH))
		{
			// Establish Manchester sync and get the current state bit
			manchesterSync = TRUE;
			currentBit = ((PIND >> SIGNAL_IN) & 1);			
			// currentBit = lastBit;
			// bitIsReady = TRUE;
			// uartTransmit(currentBit + 0x30);
		}

	}
		// If Manchester sync is established and counter is more than 2T
	else 
	{
		lastBit = currentBit;
		if (countCopy > TWO_T_LOW)
			{
				if (checkNextEdge) syncErrorfunc();
				currentBit = !lastBit;
				bitIsReady = TRUE;
				
				// uartTransmit(currentBit + 0x30);
			}

			// If Manchester sync is established and counter is more than T
		else if (countCopy > T_LOW) 
		{
			// If next edge is not to be checked
			if (!checkNextEdge)
			{
				checkNextEdge = TRUE;
			}
			else
			{
				currentBit = lastBit;
				bitIsReady = TRUE;
				checkNextEdge = FALSE;
				// uartTransmit(currentBit + 0x30);
			}
		}
		else syncErrorfunc();

	}

}

ISR(INT1_vect)
{
	if (modeChangedFlag == FALSE)
	{
		if ((PIND & (1 << SAVE_BUTTON))>> SAVE_BUTTON == 0) // Save mode button is pressed
		{
			saveTagMode = (saveTagMode + 1) % 2;
			if (saveTagMode == 1)		
				PORTB |= 1 << SAVE_LED;
			else
				PORTB &= ~(1 << SAVE_LED);
			
		}
		else // Save mode button is not pressed and mode change button is pressed
		{
			// Change mode on button press
			mode = (mode + 1) % 2;
			if (mode == READ_MODE) 
			{
				// Start PWM and open a transistor for a coil
				PORTB |= (1 << COIL_VCC);
				TCCR2 |= (1 << WGM21 | 1 << COM20 | 1<<CS20 ); // Configure timer 2 for CTC mode. No prescaling
				// Stop Timer0
				// TCCR0 &= ~(1 << CS20);
				// Enable INT0 for data read
				GICR |= (1 << INT0);
			}
			if (mode == TRANSMIT_MODE) 
			{
				// Stop PWM and close a transistor for a coil
				PORTB &= ~(1 << COIL_VCC);
				TCCR2 &= ~(1 << WGM21 | 1 << COM20 | 1<<CS20 );
				// Start Timer0
				TCNT0 = 0;
				TCCR0 |= (1 << CS20);
				// Disable INT0
				GICR &= ~(1 << INT0);
				PORTB &= ~(1 << COIL);
			}
		}
		TCNT1 = 0;
		modeChangedFlag = TRUE;
	}
	

}


//==============================================================================
// Timer2 Compare interrupt with input state sense
//==============================================================================
ISR(TIMER2_COMP_vect)
{
	if (counter < 255)
		counter++;
}

// Timer for transmittion
ISR(TIMER0_OVF_vect)
{
	timer0Counter++;
	if (timer0Counter > 8)
	{
		timer0Counter = 0;
		// modeChangedFlag = FALSE;
	}
}

ISR(TIMER1_OVF_vect) 
{
	modeChangedFlag = FALSE;
}

//===============================================================================
// PWM function. Produces the 125kHz square wave.
//===============================================================================

void init_PWM(void)
{
	TCCR2 |= (1 << WGM21 | 1 << COM20); // Configure timer 2 for CTC mode
	TCCR2 |= (1<<CS20); // No prescaling
	TIMSK |= (1 << OCIE2); // Enable compare interrupt
	OCR2 = PWM_VALUE;	   // Set PWM value
}

void init_TIMER0(void)
{
	// TCCR0 |= (1 << CS20); // Configure timer prescaler to 256
	TIMSK |= 1 << TOIE0;  // Enable overflow interrupt
}

void uart_init(void)
{
	UBRRH = (BAUDRATE >> 8);										  // shift the register right by 8 bits
	UBRRL = BAUDRATE;												  // set baud rate
	UCSRB |= (1 << TXEN) | (1 << RXEN);								  // enable receiver and transmitter
	UCSRC = (1 << URSEL) | (0 << USBS) | (1 << UCSZ1) | (1 << UCSZ0); // 8bit data format
}

//============================================================================================
// Main function.
// Initializes PWM (Pulse Width Modulation for 125 kHz)
// Reads the 32-bit (4-byte) RFID tag serial number.
// If RFID serial number is read OK (parity bits are OK) then enable buzzer for 1 sec.
//============================================================================================
int main(void)
{
	uint8_t parityStatus = FALSE;	


	// Configure External Interrupts for INT0 and INT1 to trigger on any logical change
	MCUCR |= (0 << ISC01) | (1 << ISC00) | (1 << ISC11) | (0 << ISC10);
	
	// Enable External Interrupts INT0 and INT1
	GICR |= (1 << INT0) | (1 << INT1);
	
	// Set DDRB for output on COIL, COIL_VCC, and SAVE_LED pins
	DDRB |= (1 << COIL) | (1 << COIL_VCC) | (1 << SAVE_LED);
	
	// Configure Timer1 for normal operation with a prescaler of 64
	TCCR1B |= (1 << CS11) | (1 << CS10);
	
	// Enable Timer1 overflow interrupt
	TIMSK |= (1 << TOIE1);
	
	// Set MODE_BUTTON and SAVE_BUTTON pins as input and enable internal pull-up resistors
	DDRD &= ~((1 << MODE_BUTTON) | (1 << SAVE_BUTTON));
	PORTD |= (1 << MODE_BUTTON) | (1 << SAVE_BUTTON);

	init_PWM();
	init_TIMER0();

	uart_init();

	// Enable global interrupts
	sei();

	// Reset startBitCounter variable
	startBitCounter = 0;

	// Start PWM and open a MOSFET for a coil
	PORTB |= (1 << COIL_VCC);

	// Start Timer2 with no prescaler
	TCCR2 |= (1 << CS20);




	// eeprom_write_byte((uint8_t*)0,0);

	for (;;) // Loop for ever.
	{
		
		if ( mode == READ_MODE) // Wait here until you find a steady Logic ONE. This represents the 9 start bits of RFID tag.
		{
			while (startBitCounter < 9)
			{
				while (!bitIsReady && (mode == READ_MODE)) 
				{
					asm("nop");
				}
				if (currentBit)	
					startBitCounter++;
				else			
					startBitCounter = 0;

				bitIsReady = FALSE;

				if (mode == TRANSMIT_MODE) 
							goto exit_read;
			}	

			parityStatus = readTagSerialNumber(); // Read the RFID tag serial number and get the parity status.

			if (parityStatus == TRUE) // If parity is ok,
			{
				cli();
				hexAscii(); 
				uartSendString("ok\r");


				if (saveTagMode == 1)
				{
					for (uint8_t i = 0; i < 10; i++) 
					{
						tagId[i/2] <<= 4;
						tagId[i/2] |= dataBuffer[i] & 0x0F;
					}

					tagCount = eeprom_read_byte((uint8_t*)0); // Get tag count
					if (tagCount < 0) // Check if EEPROM byte is not negative
						tagCount = 0; // Blank state is often 0xFF
					uint8_t isUnique = TRUE;

					for (uint8_t j = 0; j < tagCount; j++) // Loop though tagCount
					{
						// Read EEPROM to existingTagId
						eeprom_read_block((void*)existingTagId, (void*)(1 + j * sizeof(tagId)), sizeof(tagId)); 
						// Compare tagID and existingTagId
						if (memcmp(tagId, existingTagId, sizeof(tagId)) == 0) 
						{
							isUnique = FALSE;
							break;
						}
					}
					if (isUnique) 
					{	
						// Ensure we don't overflow the tag count							
						if (tagCount < 90)
						{ 
							eeprom_write_block((void*)tagId, (void*)(1 + tagCount * sizeof(tagId)), sizeof(tagId));
							tagCount++;
							eeprom_write_byte((uint8_t*)0, tagCount); // Update the tag count
						}
					}
				}						
				PORTB ^= 1 << SAVE_LED;
				_delay_ms(500);
				PORTB ^= 1 << SAVE_LED;
				sei();
			}
			exit_read:
			startBitCounter = 0;
		}

		else if ( mode == TRANSMIT_MODE)
		{
			uint8_t i,j;
			tagCount = eeprom_read_byte((uint8_t*)0);

			for(i = 0; i < tagCount; i++)
			{
				timer0Counter = 0;
				eeprom_read_block((void*)tagId, (const void*)(1 + i * sizeof(tagId)), sizeof(tagId));			
				tagBytesToData();
				for(j = 0; j < 5; j++)
				{
					if (mode == READ_MODE) 
						goto exit_transmit;										
					sendManchesterNineOnes();
					sendManchesterData();				
				}
				_delay_ms(50);
			}
			exit_transmit:
		}
	}
}