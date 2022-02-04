/**
*                      REAL TIME SYSTEMS
* ---------------------------------------------------------------------------------
*
*    Title: Arduino program to reproduce the sound of a file using PWM
*
*    Authors: Diego del Río Manzanas / Jaime Ortega Pérez / Fernando Burrieza Galán
*
*    Version: 1.5
*
*/

/**********************************************************
 *  INCLUDES
 *********************************************************/
#include <Wire.h>
#include "let_it_be.h"

/**********************************************************
 *  CONSTANTS
 *********************************************************/

// UNCOMMENT THIS LINE TO EXECUTE WITH RASPBERRY PI
//#define RASPBERRYPI 1

#define SLAVE_ADDR 0x8

#define SAMPLE_TIME 250
#define BUF_SIZE 256
#define SOUND_PIN  11
#define BUTTON_PIN 7
#define LED_PIN 13
#define SECOND_MICRO 1000000
#define CLOCK_SYSTEM 16000000

/**********************************************************
 *  GLOBALS
 *********************************************************/

unsigned char buffer[BUF_SIZE];
unsigned long timeOrig;

// Global variable that stores the reproduction state
int isMuted = 0;

// Global variable that stores the value read from the button
int pastState = 0;

/**********************************************************
 * Function: receiveEvent
 *********************************************************/
void receiveEvent(int num) {

   static int count = BUF_SIZE/2;

   for (int j=0; j<num; j++) {

      buffer[count] = Wire.read();
      count = (count + 1) % BUF_SIZE;

   }

}

// --------------------------------------
// Handler function: requestEvent
// --------------------------------------
void requestEvent() {

}

/**********************************************************
 * Function: play_bit
 *********************************************************/
void play_byte() {

    // Variables
    static unsigned char data = 0;
    static int music_count = 0;

    #ifdef RASPBERRYPI
        data = buffer[music_count];
        music_count = (music_count + 1) % BUF_SIZE;
    #else
        data = pgm_read_byte_near(music + music_count);
        music_count = (music_count + 1) % MUSIC_LEN;
    #endif

    // If muted
    if (isMuted) {
        // Then write a 0
        data = 0;
    }

    // Assign the byte to OCR2 Register
    OCR2A = data;

}

/**********************************************************
 * Function: read_button
 *********************************************************/
void read_button() {

  // Read the button
  int actualState = digitalRead(BUTTON_PIN);

  // Check if the state has changed
  if(actualState != pastState) {

      // If the button is being pressed
      if (actualState == 1) {
          // Changing the muted state
          isMuted = 1 - isMuted;
      }
      // Updating the state
      pastState = actualState;

  }

  // Writing the value to the led
  digitalWrite(LED_PIN, isMuted);

}

/**********************************************************
 * Function: setup
 *********************************************************/
void setup () {

    // Initialize I2C communications as Slave
    Wire.begin(SLAVE_ADDR);
    // Function to run when data requested from master
    Wire.onRequest(requestEvent);
    // Function to run when data received from master
    Wire.onReceive(receiveEvent);
    // Set I2C speed to 1 MHz
    Wire.setClock(1000000L);

    // Define input pins
    pinMode(BUTTON_PIN, INPUT); // ButtonPin

    // Define output pins
    pinMode(SOUND_PIN, OUTPUT); // SoundPin
    pinMode(LED_PIN, OUTPUT); // LedPin


    // Set buffer
    memset(buffer, 0, BUF_SIZE);

    // Setting muted global variable to not muted
    isMuted = 0;

    // Timer 1 setup for the interrupt
    TCCR1A = 0;
    TCCR1B = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11); // Set prescaler to x8 and mode of operation to CTC (best fitted for interrupts)
    TIMSK1 = _BV(OCIE1A); // Set OCIE1A to generate an interruption when it reaches OCR1A
    OCR1A = CLOCK_SYSTEM/(8 * (1000000 / SAMPLE_TIME)) - 1; // 1 second in microseconds divided by the sample time needed for task of playing back the songs received. 8 as the prescaler is x8.

    // Setup for timer 2
    TCCR2A = 0;
    TCCR2B = 0;
    TCCR2A |= _BV(WGM21) | _BV(WGM20) | _BV(COM2A1); // Set fast PWM as mode. Clear OC2A on Compare Match, set OCR2A at BOTTOM (non-inverting mode)
    TCCR2B |= _BV(CS20); // No prescaling

    timeOrig = micros(); // Storing starting time

}


/**********************************************************
 * Interrupt the task with more priority
 *********************************************************/
ISR(TIMER1_COMPA_vect) {
    // Call the function to play the byte received
    play_byte();

}

/**********************************************************
 * Function: loop
 *********************************************************/
void loop () {

  unsigned long t_sleep = micros();
  read_button();
  t_sleep = micros() - t_sleep;
  delay(100-t_sleep/1000);

}
