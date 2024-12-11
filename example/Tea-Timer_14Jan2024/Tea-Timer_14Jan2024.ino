/*
 * Tea Timer with Power Down Sleep
 * January 13, 2024
 * 
 * Copyright () 2024 by David G. Sparks
 * _wdtOff code copyright by Microchip, Inc.
 * All Rights Reserved
 * 
 * Caveat lector: this code reflects a triumph of function
 * over form, meaning that it stands the way it happened
 * to have come together at the moment it began to work 
 * as I intended. It is not as well-organized as I like
 * nor does it contain all of the comments I wish to include.
 * It probably contains errors. 
 * The code is provided here for illustration purposes 
 * to accompany an article on long battery life.
 * No claim is made regarding any other use for it.
 * 
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA  02111-1307  USA
 * 
 * Test apparatus is powered by 4 AA NiMH batteries.
 * The battery maker claims 2300 mAH of capacity.
 * Voltage fully charged is 1.4 per cell x 4 = 5.6 volts.
 * Remark: my ATmega328 chip appears to tolerate 5.6 volts.
 * Battery maker projeccts voltage to stabilize at 1.2 per cell
 * for a long plateau then decline to near 1 volt at about 80%
 * depletion. The ATmega328 operates well on 4 volts.
 * I use 80% depletion to estimate battery life.
 * 
 * In deep sleep the test circuit appears to draw 0.130 mA.
 * Running with the display on draws 25 mA more or less.
 * Estimate current per day and its average rate.
 * On-time: 25 mA for 7 minutes = 25 x 7 =                175
 * Sleep-time: 0.13 mA x ((24 x 60) - 7) = 0.13 x 1433 =  430
 *                                                       ----
 *                                                        605
 * Divide by (24 x 60) minutes in a day                / 1440
 *                                                   --------
 *                                                   0.420 mA
 *                                                
 * Possible battery life = 2300 mAH * .8 /.420 = 4,379 hours
 * 27 weeks on a charge of four Energizer NiMH AA batteries.
 * 
 * Interestingly, it continues to run after 48 weeks.             
 * Measured battery voltage at 12/10/2024 was 5.0 volts.
 */



#define DATA A0 // Arduino A0 = PC0 = DIP pin 23
#define CLK  A1 // Arduino A1 = PC1 = DIP pin 24
#define DELAY 1 // at 8MHz should be at least 5 uSec

#define cmdWriteDataIncrementAddress  0b01000000
#define cmdWriteDataToFixedAddress    0b01000100
#define cmdSetCharAddress_0           0b11000000
#define cmdSetCharAddress_1           0b11000001
#define cmdSetCharAddress_2           0b11000010
#define cmdSetCharAddress_3           0b11000011

/*
 * Brightness level defauls to 0 = 1/16th duty cycle.
 * Add 1 - 7 to the following for brighter displaly. 
 */
#define cmdDisplayControlOFF          0b10000000
#define cmdDisplayControlON           0b10001000

/* when desired, add turnDotsON to number code below */
#define turnDotsON                    0b10000000
/* the number codes */
unsigned char numchar[] =
{
  0b00111111, // 0
  0b00000110, // 1
  0b01011011, // 2
  0b01001111, // 3 
  0b01100110, // 4
  0b01101101, // 5
  0b01111101, // 6
  0b00000111, // 7
  0b01111111, // 8
  0b01101111  // 9
};

void startTM ();
void stopTM ();
bool transmitByte (unsigned char);
void writeByteToAddress (uint8_t, unsigned char);
void writeDataBuf ();
void __WDT_off ();


unsigned char dataBuf[] = {0xff, 0xff, 0xff, 0xff};  
volatile int16_t teaTime = -1;

void setup() {

  /* let's try to minimize power consumption */
  /* disable and turn off power to unused peripherals */
  ADCSRA = 0;       // disable ADC before turning its power offu
  PRR = 0b11001111; // TWI, TIM2, --, --, TIM1, SPI, USART, ADC
  ACSR |= (1<<ACD); // Analog Comparator Disable
  // Internal Bandgap Reference turns off when ADC and AC off
  // BOD is off in the fuses

  __WDT_off();  // turn off the WDT
  
  /* set the TM1637 IO pins to output */
  pinMode(CLK, OUTPUT);
  pinMode(DATA, OUTPUT);

  /* let's add a MOSFET to switch the TM1637 */
  pinMode(A5, OUTPUT);                            // PC5 = pin 28
  digitalWrite(A5, HIGH);                         // energize the gate
  // initialize Port D pins to input
  DDRD = 0;;     // input
  PORTD = 0;     // tri-state
  // enable pins D5, D6 and D7 to interrupt
  PCIFR = (1<<PCIF2);                                   // Clear PORTD interrupt flag
  PCMSK2 = (1<<PCINT21) | (1<<PCINT22) | (1<<PCINT23);  // Pins PD5-PD7 = Pins 11-13
  PCICR |= (1<<PCIE2);                                  // Enable PORTD PC interrupts
  sei();                                                // Enable interrupts globally       
  
  /* calibrate the internal oscillator */
  EEARH = 0;
  EEARL = 0;                  // select EEPROM address 0
  while (EECR & (1<<EEPE)) { ; /* wait for EEPE to go low */}
  EECR = (1<<EERE);           // enable read EEPROM
  OSCCAL = EEDR;              // stored calibration byte into OSCCAL

  /* initialize TM1637 */
  startTM();
  transmitByte (0x80);        // display off
  stopTM();
  writeDataBuf ();
  startTM();
  transmitByte (0x88 + 3);        // display on duty cycle 10/16
  stopTM();

  dataBuf[0] = 0;

  delay(1000);
//  startTM();
//  transmitByte (0x80);        // display off
//  stopTM();
  
}

void loop() {

    
  while ( teaTime >= 0 )
  {
    dataBuf[1] = numchar[(teaTime / 100)] + 0x80; // 0x80 enables the dots
    dataBuf[2] = numchar[(teaTime % 100) / 10];
    dataBuf[3] = numchar[(teaTime % 10)];
    writeDataBuf ();
    startTM();
    transmitByte (0x88 + 3);        // display on duty cycle 10/16
    stopTM();   
    teaTime -= 1;
    if ( (teaTime % 100) / 59 ) teaTime -= 40;
    delay(teaTime >= 0 ? 499 : 2500);
  }

  // =====================
  /* darken the display */
  // =====================
  startTM();
  transmitByte (0x80);        // display off
  stopTM();
  digitalWrite(A5, LOW);      // de-energize the mosfet
  
  /* put the processor into power-down sleep */
  SMCR = 0b101;
  asm volatile ("sleep \n\t");

  /* the next instruction will run only after an interrupt */
  if (teaTime > 0) digitalWrite(A5, HIGH);  // energize the mosfet
  
}

//==================================================

void startTM ()
{
  digitalWrite(CLK, HIGH);
  digitalWrite(DATA, HIGH);
  delayMicroseconds(DELAY);

  digitalWrite(DATA, LOW);
  digitalWrite(CLK, LOW);
  delayMicroseconds(DELAY);
}

void stopTM ()
{
  digitalWrite(CLK, LOW);
  digitalWrite(DATA, LOW);
  delayMicroseconds(DELAY);

  digitalWrite(CLK, HIGH);
  digitalWrite(DATA, HIGH);
  delayMicroseconds(DELAY);

}

bool transmitByte (unsigned char cat)
{
  for(uint8_t i = 0; i < 8; i++) 
  { 
    digitalWrite(CLK, LOW); 
    delayMicroseconds(DELAY); 
    digitalWrite(DATA, (cat & (1 << i)) >> i); 
    delayMicroseconds(DELAY); 
    digitalWrite(CLK, HIGH); 
    delayMicroseconds(DELAY); 
    } 
  
    // wait for ACK 
    digitalWrite(CLK,LOW); 
    delayMicroseconds(DELAY); 
    
    pinMode(DATA,INPUT); 
  
    digitalWrite(CLK,HIGH); 
    delayMicroseconds(5); 
  
    bool ack = digitalRead(DATA) == 0; 
     
    pinMode(DATA,OUTPUT); 
 
    return ack;   
}

void writeByteToAddress (uint8_t position, unsigned char cat)
{
  startTM();
  transmitByte(0x44);
  stopTM();

  startTM();
  transmitByte(0xC0 + (position & 0x03));
  transmitByte(cat);
  stopTM();

}

void writeDataBuf ()
{
  startTM();
  transmitByte(0x40);
  stopTM();

  startTM();
  transmitByte(0xC0);
  for (int i = 0; i<sizeof(dataBuf); i++)
  {
    transmitByte(dataBuf[i]);
  }
  stopTM();
}

ISR(PCINT2_vect)
{
  /* disable sleep mode */
  SMCR = 0;
  
  // the interrupt flag is cleared automatically

  // determine which pin was pressed
  uint8_t pins = PIND >> 5;
  // set countdown time accordingly
  switch (pins)
  {
    case 0b011:
      teaTime = 700;
      break;
    case 0b101:
      teaTime = 500;
      break;
    case 0b110:
      teaTime = 300;
      break;
    default:
      ; // no action
  }
}

/*
 * Assembly code adapted from example given in the datasheet
 */
void __WDT_off ()
{  asm volatile
  (
    ".equ ASM_SREG, 0x3f \r\n"
    ".equ ASM_MCUSR, 0x34 \r\n"
    ".equ ASM_WDTCSR, 0x60 \r\n"
    // Preserve SREG and r16
    "push r16 \r\n"
    "in r16, ASM_SREG \r\n"
    "push r16 \r\n"
    // Turn off global interrupt
    "cli \r\n"
    // Reset Watchdog Timer
    "wdr \r\n"
    // Clear WDRF in MCUSR
    "in r16, ASM_MCUSR \r\n"
    "andi r16, 0b00000111 \r\n"
    "out ASM_MCUSR, r16 \r\n"
    // Write logical onr to WDCE and WDE
    // Keeping old prescaler to prevent unintended time-out
    "lds r16, ASM_WDTCSR \r\n"
    "ori r16, 0b00011000 \r\n"
    "sts ASM_WDTCSR, r16 \r\n"
    // Turn off WDT
    "ldi r16, 0b10000000 \r\n"
    "sts ASM_WDTCSR, r16 \r\n"
     // Restore global interupt
    "sei \r\n"
    // recover SREG and r16
    "pop r16 \r\n"
    "out ASM_SREG, r16 \r\n"
    "pop r16 \r\n"
  );
}
