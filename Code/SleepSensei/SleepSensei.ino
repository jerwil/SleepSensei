#include <math.h>
#include <EEPROM.h>
#include <avr/sleep.h>

// Sleep Sensei using ATtiny85

//Programming ATtiny85 learned from: http://www.instructables.com/id/Program-an-ATtiny-with-Arduino/step2/Wire-the-circuit/

// Setting up bits for sleep mode (from: http://www.insidegadgets.com/2011/02/05/reduce-attiny-power-consumption-by-sleeping-with-the-watchdog-timer/)

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif



char* mode = "initialize"; // Default mode is "initialize"
// Time choose mode is where the user choses between 7, 14, 21, and 28 minutes of sleep coaching
// sleep coach mode is the mode with pulsating light
// off is when the sleep coaching is complete. A button press will bring it into time choose mode
// Intermediate modes are used to indicate switching between modes

int profile = 1;

const int LEDPin = 0;
const int pin_A = 4;  // Rotary Encoder Pin A
const int pin_B = 3;  // Rotary Encoder Pin B
const int ButtonPin = 1;

unsigned char encoder_A;
unsigned char encoder_B;
unsigned char encoder_A_prev=0;

int button_state = 0;
int button_pushed = 0; // This is the indicator that the button was pushed and released
int button_counter = 0; // This is used to detect how long the button is held for

int blink = 1; // This is used for blinking the LEDs
int blink_time = 500;
int blink_value = 254;
int blinks_until_break = 1;

int timeout = 0; // This is the timeout counter
int timeout_setting = 60; // This is the setting for timeout in time_choose mode. If this is exceeded, the device will go to off mode

int brightness_mult = 9;    // how bright the LED is, start at half brightness
int fadeAmount = 1;    // how many points to fade the LED by


unsigned long currentTime;
double milis_timer[1] = {0}; // This is used to keep track of the timer used to tick for each milisecond
double second_timer[1] = {0}; // This is used to keep track of the timer used to tick for each second
double blink_timer[1] = {0};  // This is used to keep track of each half second for blinking
int ticked = 0;

int clockwise = 0;
int counterclockwise = 0;

int delay_int = 1;
int brightness = 0;
int max_brightness = 244;
double brightincrease = 1;
double k = 0.00108*5;
double k_initial = 0.00108*5;
double k_final = 0.00065;
double x = 3*3.14159/2/k; // This starts it at 0 brightness

// K-factors - these are used in the sine wave generation portion to change the period of the sine wave. (a k of .0054 corresponds to 6 second breath, a k of .001 corresponds to an 18 second breath) //

double k_delta = .0002; // The amount of change in k for each rotary encoder tick

double k_values[8] = {.0054, .0054, .0054, .0054, .003, .0025, .0022, .0019}; // {k1_initial, k2_initial, k3_initial, k4_initial, k1_final, k2_final, k3_final, k4_final}

int profile_times_array[4] = {1, 2, 3, 4}; // Each profile is stored as a tens digit. Available options are 10, 20, 30, 40, 50, 60, 70, 80, and 90 minute long sessions

// EEPROM is mapped as follows : {k1_initial, k2_initial, k3_initial, k4_initial, k1_final, k2_final, k3_final, k4_final, Profile 1 duration, Profile 2 duration, Profile 3 duration, Profile 4 duration}
// EEPROM location 25 is used to see if program has written to EEPROM before. It will contain a value of 1 if this program has written to it, indicating that the rest of the EEPROM can be read.

int val = 200; // Current value read from EEPROM

double total_time = 600; // seconds for entire breathing coaching
double current_time = 0;

int button_press_initiate[1];     // storage for button press function
int button_press_completed[1];    // storage for button press function

int tens_digit = 0; // Tens digit used to choose session time
int ones_digit = 7; // Ones digit used to choose session time


void setup() {                
  pinMode(LEDPin, OUTPUT);
  pinMode(ButtonPin, INPUT);
  pinMode(pin_A, INPUT);
  pinMode(pin_B, INPUT);
  
  sbi(GIMSK,PCIE); // Turn on Pin Change interrupt
  sbi(PCMSK,PCINT1); // Which pins are affected by the interrupt
//  Serial.begin(9600); 


}


void loop() {
  
if (mode == "initialize"){
  
if (digitalRead(ButtonPin) == 1){
  eeprom_reset();
  flash_led(LEDPin, 10, 50);
}
 
else if (EEPROM.read(25) == 1){

for (int i = 0; i < 8; i++){
  val = EEPROM.read(i);
  k_values[i] = val;
  k_values[i] = k_values[i]/10000;
}
for (int i = 8; i < 12; i++){
  val = EEPROM.read(i);
  profile_times_array[i-8] = val;
}
}

else {
   eeprom_reset();
}
  
mode = "time_choose";
}
  
    clockwise = 0;
    counterclockwise = 0;
    encoder_A = digitalRead(pin_A);    // Read encoder pins
    encoder_B = digitalRead(pin_B);   
    if((!encoder_A) && (encoder_A_prev)){
      // A has gone from high to low 
      if(encoder_B) {
        // B is high so clockwise
        // increase the brightness multiplier, dont go over 25
        clockwise = 1;            
      }   
      else {
        // B is low so counter-clockwise      
        // decrease the brightness, dont go below 0
         counterclockwise = 1;        
      } 
    }
encoder_A_prev = encoder_A;  
max_brightness = brightness_mult*10;

button_state = digitalRead(ButtonPin);
button_pushed = button_press (button_state, button_press_initiate, button_press_completed);
  
if (mode == "time_choose"){
  
  if (clockwise == 1){if(brightness_mult + fadeAmount <= 25) brightness_mult += fadeAmount;}
  if (counterclockwise == 1){if(brightness_mult - fadeAmount >= 2) brightness_mult -= fadeAmount;}
  
x = 0;
  
//delay(10);

if (button_pushed == 1){
profile += 1;
button_pushed = 0;
blinks_until_break = profile + 1;
}
if (profile > 4){
profile = 0;
blinks_until_break = 0;
tick_reset(blink_timer);
mode = "off";
}
  
//blink_time = blink_times_array[profile-1];
 
if (tick(1000,second_timer) == 1){
timeout += 1;
if (button_state == 1){button_counter += 1;}
//flash_led(LEDPin,profile,150);
}

if (button_state == 0){button_counter = 0;}

if (tick(200,blink_timer) == 1){
 if (blink == 1){
   blink = 0;
 }
 else if (blink == 0){
   blink = 1;   
     if (blinks_until_break == 1){blink = 0;}
     if (blinks_until_break <= 0){
     blink = 0;
     blinks_until_break = profile + 1;
     }
     else {blinks_until_break -= 1;}

 }
}

if (button_state == 1){
timeout = 0;
}

blink_value = max_brightness;

if (blink == 0){  
blink_value = 0;
}

analogWrite(LEDPin, blink_value);


if (button_counter >= 3){ // If the user holds the button for 3 seconds, start the sleep coach
button_pushed = 0;
mode = "sleep_coach";
button_counter = 0;

total_time = (profile_times_array[profile-1]*600);
k_initial = k_values[profile-1];
k_final = k_values[profile+3];

}

if (timeout >= timeout_setting){mode = "off";}
 
//flash_led(LEDPin,profile,250);

 
}
  
if (mode == "sleep_coach"){
  
if (clockwise == 1){if(brightness_mult + fadeAmount <= 25) brightness_mult += fadeAmount;}
if (counterclockwise == 1){if(brightness_mult - fadeAmount >= 2) brightness_mult -= fadeAmount;}
  
timeout = 0;
  
if (current_time >= total_time && brightness <= 10){
current_time = 0;
profile = 0;
mode = "off";
x = 0;
}
  
brightness = max_brightness/2*(1 + sin(k*x));
  
if (tick(delay_int,milis_timer) == 1){
  x += brightincrease;
}
if (tick(1000,second_timer) == 1){
  current_time += 1;
  k = k_initial + current_time*(k_final-k_initial)/total_time;
  if (button_state == 1){button_counter += 1;} 
}
if (x*k >= 2*3.14159){x=0;}
//else if (brightness <= 0){brightincrease = 1;}
  analogWrite(LEDPin,brightness);

if (button_state == 0){button_counter = 0;}

if (button_counter >= 3){ // If the user holds the button for 3 seconds, start the sleep coach
button_pushed = 0;
mode = "to_initial_adjust";
button_counter = 0;
}

if (button_pushed == 1 && current_time > 5){ // Turn the device off by pushing the button, but do not create false press after starting
mode = "off";
button_pushed = 0;
button_counter = 0;
current_time = 0;
profile = 0;
x = 3*3.14159/2/k; // Start it back at 0 brightness
}

}

if (mode == "to_initial_adjust"){
  
  x = 0;
  button_pushed = 0;
  flash_led(LEDPin, 3, 100);
  k = k_values[profile-1];
  mode = "initial_adjust";

}

if (mode == "initial_adjust"){

// Adjusts the initial breath length of the currently selected profile
  
if (counterclockwise == 1)
  {if(k + k_delta <= .01) 
    {k += k_delta;
    timeout = 0;}
  }
else if (clockwise == 1)
  {if(k - k_delta >= .0019) 
    {k -= k_delta;
    timeout = 0;}
  }
brightness = 127*(1 + sin(k*x));  
if (tick(delay_int,second_timer) == 1){
  x += brightincrease;
}

if (x*k >= 2*3.14159){x=0;}

analogWrite(LEDPin,brightness);

if (tick(1000,blink_timer) == 1){
timeout += 1;
if (button_state == 1){button_counter += 1;}
}

if (button_state == 0){button_counter = 0;}

if (button_counter >= 3){ // If the user holds the button for 3 seconds, go to mode to adjust final breath length
button_pushed = 0;
mode = "to_final_adjust";
button_counter = 0;
timeout = 0;
}

if (button_state == 1){
timeout = 0;
}

if (timeout >= timeout_setting){
mode = "off";
timeout = 0;
}

}

if (mode == "to_final_adjust"){
  
x = 0;
k_values[profile-1] = k;
val = k*10000;
EEPROM.write(profile-1,val);
EEPROM.write(25, 1);  
flash_led(LEDPin, 5, 100);
mode = "final_adjust";
k = k_values[profile+3];

}


if (mode == "final_adjust"){

// Adjusts the final breath length of the currently selected profile
  
if (counterclockwise == 1)
  {if(k + k_delta <= .01) 
    {k += k_delta;
    timeout = 0;}
  }
else if (clockwise == 1)
  {if(k - k_delta >= .0019) 
    {k -= k_delta;
    timeout = 0;}
  }

brightness = 127*(1 + sin(k*x));  
if (tick(delay_int,second_timer) == 1){
  x += brightincrease;
}

if (x*k >= 2*3.14159){x=0;}

analogWrite(LEDPin,brightness);


if (tick(1000,blink_timer) == 1){
timeout += 1;
if (button_state == 1){button_counter += 1;}
}

if (button_state == 0){button_counter = 0;}

if (button_counter >= 3){ // If the user holds the button for 3 seconds, go to adjust tens digit of duration
button_pushed = 0;
mode = "to_tens_digit_adjust";
button_counter = 0;
timeout = 0;

k_values[profile+3] = k;
val = k*10000;
EEPROM.write(profile+3,val);
EEPROM.write(25, 1);

}

if (button_state == 1){
timeout = 0;
}

if (timeout >= timeout_setting){
mode = "off";
timeout = 0;
}

}

if (mode == "to_tens_digit_adjust"){
tens_digit = profile_times_array[profile-1];
mode = "tens_digit_adjust";
blinks_until_break = tens_digit + 1;
for (int n=0; n < 3; n++) {pulse_led(LEDPin, 1);}
}

if (mode == "tens_digit_adjust"){
  
if (tick(1000,second_timer) == 1){
timeout += 1;
if (button_state == 1){button_counter += 1;}
}

if (button_state == 0){button_counter = 0;}
  
if (counterclockwise == 1)
  {if(tens_digit > 1) 
    {tens_digit -= 1;
     timeout = 0;}
  }
else if (clockwise == 1)
  {if(tens_digit < 9) 
    {tens_digit += 1;
     timeout = 0;}
  }

//if (button_pushed == 1 && tens_digit > 0){flash_led(LEDPin, tens_digit, 200);}

if (tick(200,blink_timer) == 1){
 if (blink == 1){
   blink = 0;
 }
 else if (blink == 0){
   blink = 1;   
     if (blinks_until_break == 1){blink = 0;}
     if (blinks_until_break <= 0){
     blink = 0;
     blinks_until_break = tens_digit + 1;
     }
     else {blinks_until_break -= 1;}

 }
}

blink_value = max_brightness;

if (blink == 0){  
blink_value = 0;
}

analogWrite(LEDPin, blink_value);

if (timeout >= timeout_setting){
mode = "off";
timeout = 0;
}

if (button_counter >= 3){ // If the user holds the button for 3 seconds, go to adjust ones digit of duration
button_pushed = 0;
mode = "back_to_menu";
button_counter = 0;
timeout = 0;
blinks_until_break = profile + 1;

profile_times_array[profile-1] = tens_digit;
EEPROM.write(profile+7,tens_digit);
EEPROM.write(25, 1);

}

}

if (mode == "back_to_menu"){
  
x = 0;  
for (int n=0; n < 5; n++) {pulse_led(LEDPin, 1);}
button_pushed = 0;
button_counter = 0;
mode = "time_choose";
profile -= 1;

}

if (mode == "off"){
  
x = 0;

analogWrite(LEDPin,  0);  
  
system_sleep();
  
delay(1);

if (button_state == 1){ // Turn the device on by pushing the button
mode = "time_choose";
button_pushed = 0;
button_counter = 0;
timeout = 0;
profile = 0;
}

current_time = 0;
  
}

}

int button_press (int button_indicator, int button_press_initiated[1], int button_press_complete[1]){
	if (button_indicator == 0 && button_press_initiated[0] == 1) {
	button_press_complete[0] = 1;
	button_press_initiated[0] = 0;
	}
	else if (button_indicator == 1){
	button_press_initiated[0] = 1;
	button_press_complete[0] = 0;
	}
	else {button_press_complete[0] = 0;}
return button_press_complete[0];
}

int tick(int delay, double timekeeper[1]){
currentTime = millis();
if(currentTime >= (timekeeper[0] + delay)){
	timekeeper[0] = currentTime;
	return 1;
  }
else {return 0;}
}

void tick_reset(double timekeeper[1]){
currentTime = millis();
timekeeper[0] = currentTime;
}

void flash_led(int pin, int times_to_flash, int wait_time){
  int i = 1;
  while (i <= times_to_flash){
  analogWrite(pin,244);
  delay(wait_time);               
  analogWrite(pin,0);
  delay(wait_time);
  i++;
  }
}

void pulse_led(int pin, int wait_time){
  int i = 1;
  while (i <= 255){
  analogWrite(pin,i);
  delay(wait_time);               
  i++;
  }
  while (i >= 0){
  analogWrite(pin,i);
  delay(wait_time);               
  i--;
  }
}


void eeprom_reset(){
  for (int i = 0; i < 512; i++)
  EEPROM.write(i, 0);
  for (int i = 0; i < 8; i++){
  EEPROM.write(i,k_values[i]*10000);
  }
  for (int i = 8; i < 12; i++){
  EEPROM.write(i,profile_times_array[i-8]);
  }
  EEPROM.write(25,1); 
}

void system_sleep() {
  profile = 0;
  cbi(ADCSRA,ADEN); // Switch Analog to Digital converter OFF
  set_sleep_mode(SLEEP_MODE_PWR_DOWN); // Set sleep mode
  sleep_mode(); // System sleeps here
  sbi(ADCSRA,ADEN);  // Switch Analog to Digital converter ON
}

ISR(PCINT0_vect) {
}
