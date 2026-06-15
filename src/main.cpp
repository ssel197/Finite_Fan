// #include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

/* Forward declarations of datatypes */

// succinctly defines all the possible states we can have
// typedef specifies these enums are State types
typedef enum { S0, S1, S2, NUM_STATES } State;

typedef enum { SPD, PWR, NUM_EVENTS } Event;

// creates a function pointer object named action_fn
typedef void (*action_fn)(void);

// creating a Transition object that contains a "next state" and "action"
// associated with an entry in the table
typedef struct {
  State next_state;
  action_fn action;
} Transition;

/* Forward declarations of methods */
void new_state(
    State);       // takes in the current state and performs the transition
void fan_low();   // NEED TO ADD: turns on motor with a low speed
void fan_high();  // NEED TO ADD: turns on the motor with a high speed
void fan_off();   // NEED TO ADD: turns off motor

/* Global state management */
State state;  // creates a new state object (NB: not volatile by default!)

Transition state_table[NUM_EVENTS][NUM_STATES] = {

    // S0           S1              S2
    {{S0, fan_off}, {S2, fan_high}, {S1, fan_low}},  // E0 - press speed
    {{S1, fan_low}, {S0, fan_off}, {S0, fan_off}}    // E1 - press power

};

// method that changes the state and executes the method as per state table
void change_state(Event ev) {
  // get the transition object associated with the event and current state
  const Transition* t = &state_table[ev][state];

  // execute the action
  t->action();

  // transition to the next state if there is a state change
  // NB: New state method declared further down just manipulates global!
  if (t->next_state != state) {
    new_state(t->next_state);
  }
}

/* Main Program */

int main(void) {
  /* TEMP - Lights to see the program working */
  DDRB |= (1 << DDB3);  // LED output on Pin 11 for LOW
  DDRB |= (1 << DDB4);  // LED output on Pin 12 for HIGH
  DDRB |= (1 << DDB5);  // LED output on Pin 13 for OFF

  // turn LEDs off
  PORTB &= ~(1 << PB3);
  PORTB &= ~(1 << PB4);
  PORTB &= ~(1 << PB5);

  /* Button Control */

  // Input pins are PD2 and PD3
  DDRD &= ~(1 << DDD2);  // PD2 - INT0 (Pin 2 - power)
  DDRD &= ~(1 << DDD3);  // PD3 - INT1 (Pin 3 - speed)

  cli();

  // enable interrupts on pins 2 and 3
  EIMSK |= (1 << INT0) | (1 << INT1);

  // on rising edge only
  EICRA |= (1 << ISC00) | (1 << ISC01);  // INT0
  EICRA |= (1 << ISC10) | (1 << ISC11);  // INT1

  sei();

  // initially new state is S0 (entry point of program)
  new_state(S0);

  fan_off();

  /* Main while loop */
  while (1) {
    asm("nop");  // assembly code "No Operation"
  }

  return 0;
}

void new_state(State ns) { state = ns; }

/* Interrupt Handlers */

// INT0 - (Pin 2 - power)

ISR(INT0_vect) { change_state(PWR); }

// INT1 - (Pin 3 - speed)

ISR(INT1_vect) { change_state(SPD); }

// temporarily this program shows states with 3 LEDs

void fan_low() {
  // Set LED on pin 11 to high and turn off all others

  PORTB &= ~(1 << PB4);
  PORTB &= ~(1 << PB5);
  PORTB |= (1 << PB3);
}

void fan_high() {
  // Set LED on pin 12 to high and turn off all others
  PORTB &= ~(1 << PB3);
  PORTB &= ~(1 << PB5);
  PORTB |= (1 << PB4);
}

void fan_off() {
  // Set LED on pin 13 to high and turn off all others
  PORTB &= ~(1 << PB3);
  PORTB &= ~(1 << PB4);
  PORTB |= (1 << PB5);
}
