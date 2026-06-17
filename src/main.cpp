// #include <Arduino.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
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
    State);  // takes in the current state and performs the transition

void run_motor(uint8_t speed_mode);  // runs motor at speed (0 to 255)
void fan_low();                      // turns on motor with a low speed
void fan_high();                     // turns on the motor with a high speed
void fan_off();                      // turns off motor
bool debounce(
    volatile uint16_t* debounce_counter);  // debounces motor buttons with help
                                           // of timer overflow

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

// Global variables
// volatile variables for clock ticks that manage debounce values
#define DEBOUNCE_REFACTORY 200

volatile uint16_t pwr_debounce, spd_debounce = 0;

// global variable for speed management (default is 1/2 maximum)
volatile uint8_t medium_speed = 128;
volatile uint8_t adc_counter = 0;  // used for timing / storing ADC conversions

/* Main Program */

int main(void) {
  cli();  // setup starts here

  /* DIAGNOSTIC OUTPUT - Lights to see the program working */
  DDRD |= (1 << DDD7);  // LED output on Pin 7 for LOW (to free PWM output)
  DDRB |= (1 << DDB4);  // LED output on Pin 12 for HIGH
  DDRB |= (1 << DDB5);  // LED output on Pin 13 for OFF

  // turn LEDs off (for diagnostic mode)
  PORTD &= ~(1 << PD7);
  PORTB &= ~(1 << PB4);
  PORTB &= ~(1 << PB5);

  /* Button Control */

  // Input pins are PD2 and PD3
  DDRD &= ~(1 << DDD2);  // PD2 - INT0 (Pin 2 - power)
  DDRD &= ~(1 << DDD3);  // PD3 - INT1 (Pin 3 - speed)

  // enable interrupts on pins 2 and 3
  EIMSK |= (1 << INT0) | (1 << INT1);

  // on rising edge only
  EICRA |= (1 << ISC00) | (1 << ISC01);  // INT0
  EICRA |= (1 << ISC10) | (1 << ISC11);  // INT1

  /* Timer0 = Switch debouncing / general timer based operations */

  // timer overflows should have a resolution that works with a ~1ms period
  // Prescaler: 64 => overflow period = 2^8 * 64/16MHz = 1.024ms
  TCCR0B |= (1 << CS01) | (1 << CS00);

  // enable timer overflow interrupt
  TIMSK0 |= (1 << TOIE0);

  // set initial timer value to 0
  TCNT0 = 0;

  /* Timer2 = Motor PWM Control */

  // Setting timer 2 mode to PWM
  DDRB |= (1 << DDB3);

  TCCR2A |= (1 << WGM21) | (1 << WGM20);

  // Set OC2A at compare match and clear at bottom
  TCCR2A |= (1 << COM2A1);
  TCCR2A &= ~(1 << COM2A0);
  // (non-inverting)

  // 50 % duty cycle: D = OCR1x/(TOP + 1) => OCR1x = D(TOP + 1) = 0.5(255 + 1) =
  // 128
  OCR2A = 254;  // OC2A outputs to Pin 11

  // No prescaler
  TCCR2B |= (1 << CS20);

  // reset the timer
  TCNT2 = 0;

  /* ADC - Reads Pin A1 for variable speed using internal V_Ref ~ 5V*/

  // select A1 as output with ref 5V
  ADMUX |= (1 << MUX0) | (1 << REFS0);

  // set ADC prescaler to maximum (speed is not an issue here)
  ADCSRA |= (1 << ADPS0) | (1 << ADPS1) | (1 << ADPS2);

  // For only 8 bit output left shift
  ADMUX |= (1 << ADLAR);

  // Enable ADC and ADC interrupt
  ADCSRA |= (1 << ADIE) | (1 << ADEN);

  // initially new state is S0 (entry point of program)
  new_state(S0);

  fan_off();

  sei();  // set up ends here

  /* Main while loop */
  while (1) {
    asm("nop");  // assembly code "No Operation"
  }

  return 0;
}

void new_state(State ns) { state = ns; }

/* Interrupt Handlers */

// INT0 - (Pin 2 - power)

ISR(INT0_vect) {
  if (debounce(&pwr_debounce)) change_state(PWR);
}

// INT1 - (Pin 3 - speed)

ISR(INT1_vect) {
  if (debounce(&spd_debounce)) change_state(SPD);
}

//  TIMER0_OVF_vect (1.024ms timer overflow counter interrupt)
ISR(TIMER0_OVF_vect) {
  // only increment these counters if it will not cause overflow and lead
  // to inaccurate counting (DEBOUNCE_REFACTORY must be less than maximum value)
  if (pwr_debounce < (DEBOUNCE_REFACTORY + 1)) pwr_debounce += 1;
  if (spd_debounce < (DEBOUNCE_REFACTORY + 1)) spd_debounce += 1;

  // triggering ADC conversion every ~ 10 ms
  adc_counter++;
  if (adc_counter > 10) {
    ADCSRA |= (1 << ADSC);
    adc_counter = 0;
  }
}

// ADC_vect (stores ADC value after ADC conversion complete)
ISR(ADC_vect) { medium_speed = ADCH; }

/* Program Output Functions */

void run_motor(uint8_t speed_mode) {
  // Change the PWM signal going to the motor

  OCR2A = speed_mode % 256;
}

void fan_low() {
  // Set LED on pin 11 to high and turn off all others

  PORTB &= ~(1 << PB4);
  PORTB &= ~(1 << PB5);
  PORTD |= (1 << PD7);
  run_motor(medium_speed);
}

void fan_high() {
  // Set LED on pin 12 to high and turn off all others
  PORTD &= ~(1 << PD7);
  PORTB &= ~(1 << PB5);
  PORTB |= (1 << PB4);
  run_motor(255);
}

void fan_off() {
  // Set LED on pin 13 to high and turn off all others
  PORTD &= ~(1 << PD7);
  PORTB &= ~(1 << PB4);
  PORTB |= (1 << PB5);
  run_motor(0);
}

bool debounce(volatile uint16_t* debounce_counter) {
  // if the debounce time has been less than ~200 ms return false else begin the
  // debounce counting
  if (*debounce_counter < DEBOUNCE_REFACTORY) {
    return false;
  }

  *debounce_counter = 0;
  return true;
}
