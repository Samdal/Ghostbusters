
#include <FastLED.h>
#include <DFPlayerMini_Fast.h>


//tweaking variables
int overheat_time = 5000; //in ms
int proton_default_hp = 100;
int pack_volume = 30; //max is 30

unsigned long default_cyclotron_time = 600; //in ms
unsigned long shooting_time = 20000; //in ms
unsigned long idle_time = 550000; //in ms

//set pins to their values
#define front_potentiomiter A8
#define pack_power A9
#define gun_power A10
#define proton_indicator A11
#define activate A12
#define intensify_front A13
#define intensify A14

#define mR 9
#define mG 10
#define mB 11
#define PACK_LEDS 13
#define GUN_LEDS 12

#define cyclotron1 0
#define cyclotron2 1
#define cyclotron3 2
#define cyclotron4 3
#define N_filter 4

#define VENT_LED 0
#define White_LED 1
#define Front_LED 2

//proton_graph pins
const int proton_graph[10] = {
  22,
  23,
  24,
  25,
  26,
  27,
  28,
  29,
  30,
  31
};
#define proton_graph_max 9
int proton_graph_stage = 0;



//sound tracks
int power_down_sound = 1;
int pack_hum_sound = 2;
int gun_trail_sound = 3;
int start_up_sound = 4;
int shoot_sound = 5;
int beep_sound = 6;
int beep_shoot_sound = 7;
int gun_overheat_sound = 8;

//mode variables
#define protonAccelerator 1
#define darkMatterGenerator 2
#define plasmDistrubutionSystem 3
#define CompositeParticleSystem 4

int currentMode = protonAccelerator;

DFPlayerMini_Fast myMP3;

//overheating and shooting
unsigned long previus_overheat = 0;
int proton_hp = proton_default_hp;
int proton_reduction;
unsigned long previus_hp_reduction = 0;
bool beeped = false;
bool shooting = false;
unsigned long last_shooting = 0;
int animation_stage = 0;
unsigned long last_animation = 0;
bool animation_ascending = true;
bool system_on = false;
int max_power = 100; //warning, may destroy leds
int red_state = 0;
int green_state = 0;
int blue_state = 0;
unsigned long high_power_LED_delay = 2; //in ms
unsigned long previus_LED_update = 0;
unsigned long previus_color_change = 0;
bool color_change;
long rng = 0;

//cyclotron
unsigned long cyclotron_time = default_cyclotron_time / 100 * proton_hp;
unsigned long cyclotron_previus_time = 0;
unsigned long cyclotron_previus_fade = 0;
unsigned long cyclotron_fade_time = 15;
int cyclotron_stage = 0;
bool cyclotron_on = false;
#define PACK_NUM_LEDS 5
CRGB pack_leds[PACK_NUM_LEDS];
#define GUN_NUM_LEDS 3
CRGB gun_leds[GUN_NUM_LEDS];

//gun lights
bool gun_lights_on;

//powercell
int previus_pot_input;
unsigned long powercell_previus_time = 0; //in ms
//powercell graph pins
const int powercell[20] {
  32,
  33,
  34,
  35,
  36,
  37,
  38,
  39,
  40,
  41,
  42,
  43,
  44,
  45,
  46,
  47,
  48,
  49,
  50,
  51,
};
#define powercell_max 19
int powercell_stage = 0;


//LED pulse
unsigned long last_pulse = 0;
bool front_LED_on = true;


//global time value to run the different timings
int t = 0;


//declaration of all the different functions
void run_powercell();
void start_up();
void gun_lights_off();
void reload(bool overheat);
void run_proton_pack();
void shoot();
bool high_power_LED(long R, long GB, unsigned long spacing_delay);
void reduce_proton_hp();
void gun_switches();
void pulse_LED();
void run_proton_indicator();
void run_cyclotron();
void reset_pack();


//the setup code is ran when the arduino starts
void setup() {
  //set pinput pins (buttons and switches)
  pinMode(pack_power, INPUT);
  pinMode(gun_power, INPUT);
  pinMode(proton_indicator, INPUT);
  pinMode(activate, INPUT);
  pinMode(intensify, INPUT);
  pinMode(intensify_front, INPUT);

  //set pinmode of proton_graph and powercell
  for (int i = 22; i <= 51; i++) {
    pinMode(i, OUTPUT);
  }

  //set output pins
  //high power leds
  pinMode(mR, OUTPUT);
  pinMode(mG, OUTPUT);
  pinMode(mB, OUTPUT);

  //neopixel leds
  FastLED.addLeds<NEOPIXEL, PACK_LEDS>(pack_leds, PACK_NUM_LEDS);

  FastLED.addLeds<NEOPIXEL, GUN_LEDS>(gun_leds, GUN_NUM_LEDS);

  //set up sound
  Serial1.begin(9600);
  myMP3.begin(Serial1);
  delay(500);

  constrain(pack_volume, 0, 30);
  myMP3.volume(pack_volume);
  FastLED.clear(true);
}


//loop while the arduino has power
void loop() {

  //read if the main power buttons are off
  bool gun_power_on = digitalRead(gun_power);
  bool pack_power_on = digitalRead(pack_power);

  //set time to current time
  t = millis();

  //check if the proton gun power switch is on
  if (gun_power_on) {
    run_proton_pack();
    gun_switches();
    system_on = true;
    return;
  } else if (pack_power_on) {
    run_proton_pack();
    if (gun_lights_on) {
      gun_lights_off();
    }
    system_on = true;
    return;
  } else if (system_on) {
    reset_pack();
    myMP3.play(power_down_sound);
  }

}

//turn on / keep running the proton pack lights
void run_proton_pack() {
  if (!system_on) {
    start_up();
  }
  reduce_proton_hp();

  run_cyclotron();

  run_powercell();
}

//reads the potentiomiter at the front and writes a value to proton_hp
void read_potentiomiter(){
  int pot_value = map(analogRead(front_potentiomiter), 0, 1023, 0, 5);
  if (pot_value < 1) {
    proton_reduction = proton_default_hp - 20;
  } else {
    proton_reduction = proton_default_hp - pot_value*20;
  }
}

//animation and sound when the proton pack starts
void start_up() {
  myMP3.play(start_up_sound);
    for (int i = 0; i <= powercell_max; i++) {
        digitalWrite(powercell[i], HIGH);
    }
  for (int i = 0; i < 100; i++) {
    pack_leds[cyclotron1].setRGB( i, 0, 0);
    pack_leds[cyclotron2].setRGB( i, 0, 0);
    pack_leds[cyclotron3].setRGB( i, 0, 0);
    pack_leds[cyclotron4].setRGB( i, 0, 0);
    FastLED.show();

    delay(17);
  }

  for (int i = 0; i <= powercell_max; i++) {
    digitalWrite(powercell[i], LOW);
  }
  powercell_stage = 0;
  cyclotron_off();  
  for (int i = 0; i <= proton_graph_max; i++) {
    digitalWrite(proton_graph[i], LOW);
  }
  proton_graph_stage = 0;
  analogWrite(mR, 0);
  analogWrite(mG, 0);
  analogWrite(mB, 0);
  red_state = 0;
  green_state = 0;
  blue_state = 0;
}

//check the gun switches and act accordingly
void gun_switches() {
  delay(20);
  gun_lights_on = true;
  //check the gun switches
  bool proton_indicator_on = digitalRead(proton_indicator);
  bool activate_on = digitalRead(activate);
  bool intensify_on = digitalRead(intensify);
  bool intensify_reload = digitalRead(intensify_front);

  if (proton_indicator_on) {
    read_potentiomiter();
    run_proton_indicator();  //display the proton_hp on the protn indicator

    if (activate_on) {
      pulse_LED();  //pulse the White LED inversly of the Front_LED
      vent_color();
      FastLED.show();

      if (intensify_on) {
        shoot(); //play shooting sounds and animation whilst depleeting amuniton
      } else if (shooting) {
        myMP3.play(gun_trail_sound);
        shooting = false;
        analogWrite(mR, 0);
        analogWrite(mG, 0);
        analogWrite(mB, 0);
        red_state = 0;
        green_state = 0;
        blue_state = 0;
      }

    } else {
      //do theese if the proton indicator is on but not the generator switch
      gun_leds[VENT_LED] = CRGB::Black;
      gun_leds[Front_LED] = CRGB::Black;
      gun_leds[White_LED] = CRGB::White;
      FastLED.show();
      front_LED_on = true;

    }
    if (intensify_reload) {
      shooting = false;
      reload(false);
    }
  } else {
    gun_leds[VENT_LED] = CRGB::Black;
    gun_leds[Front_LED] = CRGB::Black;
    gun_leds[White_LED] = CRGB::Black;
    FastLED.show();
    for (int i = 0; i <= proton_graph_max; i++) {
    digitalWrite(proton_graph[i], LOW);
    }
    proton_graph_stage = 0;
  }


}

//cycle the powercell
void run_powercell() {
  if (t - powercell_previus_time >= cyclotron_time /9 + 30) {
    digitalWrite(powercell[powercell_stage], HIGH);

    powercell_stage++;
    if (powercell_stage >= powercell_max ) {
      for (int i = 0; i <= powercell_max; i++) {
        digitalWrite(powercell[i], LOW);
      }
      powercell_stage = 0;
    }
    
    powercell_previus_time = t;
  }
}

//sycles to cyclotron
void run_cyclotron() {
  fade_cyclotron();
  //the cyclotron time is directly relative to the proton_hp
  cyclotron_time = default_cyclotron_time / 100 * proton_hp + 40;

  //while the cyclotron is on, it should stay on for a set amount of time
  if ((cyclotron_on) && (t - cyclotron_previus_time >= cyclotron_time /3*2)) {
    cyclotron_stage++;
    cyclotron_previus_time = t;
    cyclotron_on = false;
  }

  //if the cyclotron is off, turn it on after a time has exeeded.
  else if ((!cyclotron_on) && (t - cyclotron_previus_time >= cyclotron_time )) {
  if (cyclotron_stage >= 4) {
    cyclotron_stage = 0;
  }
    cyclotron_color(cyclotron_stage);
    FastLED.show();

    cyclotron_on = true;
    cyclotron_previus_time = t;
  }
}

//reload the proton pack
void reload(bool overheat) {

  myMP3.play(gun_overheat_sound);

  unsigned long reload_time = millis();

  cyclotron_off();
  for (int i = 0; i <= proton_graph_max; i++) {
    digitalWrite(proton_graph[i], LOW);
  }
  proton_graph_stage = 0;
  pack_leds[N_filter] = CRGB::Red;
  gun_leds[White_LED] = CRGB::White;
  gun_leds[Front_LED] = CRGB::Red;
  vent_color();
  FastLED.show();
  analogWrite(mR, 0);
  analogWrite(mG, 0);
  analogWrite(mB, 0);
  for (int i = 0; i <= powercell_max; i++) {
        digitalWrite(powercell[i], HIGH);
  }
  powercell_stage = 10;
  red_state = 0;
  green_state = 0;
  blue_state = 0;
  
  if (overheat) {
    delay(overheat_time);
  }
  else {
    delay(overheat_time / 2);
  }
  reset_pack();
  start_up();


}

//resets variables and resets functions
void reset_pack() {
  t = millis();
  gun_lights_off();
  FastLED.clear(true);
  for (int i = 0; i <= powercell_max; i++) {
    digitalWrite(powercell[i], LOW);
  }
  powercell_stage = 0;
  FastLED.show();
  cyclotron_stage = 0;
  cyclotron_on = false;
  front_LED_on = true;
  shooting = false;
  cyclotron_previus_time = t;
  powercell_previus_time = t;

  beeped = false;
  animation_ascending = true;
  proton_hp = proton_default_hp;
  system_on = false;
}

//reduces the proton_hp (ammonution) accordingly
void reduce_proton_hp() {

  //automaticly reload if the proton hp is a zero
  if (proton_hp - proton_reduction <= 0) {
    reload(true);
    return;
  }
  //reduce the proton_hp slowly while the pack is ideling
  if (t - previus_hp_reduction >= idle_time / proton_default_hp) {
    proton_hp -= 1;
    previus_hp_reduction = t;
  }
  //if there is less than x hp left, play the overheat warning sound
  if ((proton_hp <= 12) && (!beeped)) {
    if (shooting) {
      myMP3.play(beep_shoot_sound);
    } else {
      myMP3.play(beep_sound);
    }

    beeped = true;   //make shure that it only beeps once
  }

}

//shoot and depleet amunition
void shoot() {
  //letting the program know it has shot so that when it turns of it can play the trail effect
  if (!shooting) {
    shooting = true;
    myMP3.play(shoot_sound);
  }

  

  if (t - previus_LED_update >= high_power_LED_delay) {
    
    if (color_change) {
      //unsigned long random_num = analogRead(RNG);
    //  randomSeed(random_num);
      rng = random(10);
      color_change = false;
    }

    switch (rng)  {
      case 0:
        high_power_LED(80, 0, 0, 200); //rød
        break;
      case 1:
        high_power_LED(0 , 0, 90, 100); //blå
        break;
      case 2:
        high_power_LED(90, 0, 0, 200); //rød
        break;
      case 3:
        high_power_LED(0 , 30, 70, 150); //lyseblå
        break;
      case 4:
      high_power_LED(100, 30, 0, 300); //rød
        break;
      case 5:
        high_power_LED(70 , 60, 0, 200); //gul
        break;
      case 6:
        high_power_LED(70, 10, 0, 250); //rød
        break;
      case 7:
        high_power_LED(90 , 80, 80, 200); //hvit
        break;
      case 8:
        high_power_LED(80, 0, 10, 250); //rød
        break;
      case 9:
        high_power_LED( 60 , 0, 80, 200); //lilla
        break;
      default:
      color_change = true;
        break;
    }
    previus_LED_update = t;
  }


  //reduce the proton_hp (ammonution)
  if (t - last_shooting >= shooting_time / proton_default_hp) {
    proton_hp -= 1;
    last_shooting = t;
  }

}

//changes brightness in the LED at the end of the gun accordingly
void high_power_LED(long R, long G, long B, unsigned long spacing_delay) {

  R = constrain(R, 0, max_power);
  G = constrain(G, 0, max_power);
  B = constrain(B, 0, max_power);



  if (red_state < R) {
    red_state++;
  } else if (red_state > R) {
    red_state--;
  }
  if (green_state < G) {
    green_state++;
  } else if (green_state > G) {
    green_state--;
  }
  if (blue_state < B) {
    blue_state++;
  } else if (blue_state > B) {
    blue_state--;
  }

  analogWrite(mR, red_state);
  analogWrite(mG, green_state);
  analogWrite(mB, blue_state);
  if ((red_state == R) && (green_state == G) && (blue_state == B) && (t - previus_color_change >= spacing_delay)) {
    previus_color_change = t;
    color_change = true;
  }
}

void gun_lights_off() {
  gun_lights_on = false;
  gun_leds[White_LED] = CRGB::Black;
  gun_leds[Front_LED] = CRGB::Black;
  gun_leds[VENT_LED] = CRGB::Black;
  FastLED.show();
  if (shooting) {
    delay(100);
    myMP3.play(gun_trail_sound);
    shooting = false;
  }
  analogWrite(mR, 0);
  analogWrite(mG, 0);
  analogWrite(mB, 0);
  red_state = 0;
  green_state = 0;
  blue_state = 0;
  for (int i = 0; i <= proton_graph_max; i++) {
    digitalWrite(proton_graph[i], LOW);
  }
  proton_graph_stage = 0;
}


//pulses the White_LED inversly with the Front_LED
void pulse_LED() {
  if ((t - last_pulse >= cyclotron_time / 2) && (front_LED_on)) {
    gun_leds[Front_LED] = CRGB::Black;
    front_LED_on = false;
    gun_leds[White_LED] = CRGB::White;
    last_pulse = t;
    FastLED.show();
  }
  else if ((t - last_pulse >= cyclotron_time / 2) && (!front_LED_on)) {
    gun_leds[White_LED] = CRGB::Black;
    front_LED_on = true;
    gun_leds[Front_LED] = CRGB::Red;
    last_pulse = t;
    FastLED.show();
  }
}

//runs the proton indicator graph on the proton gun
void run_proton_indicator() {
  int proton_val = map(proton_hp, 0, 100, 0, proton_graph_max);
  
  for (int i = 2; i <= proton_graph_max; i++) {
    if ( i <= proton_val) {
      digitalWrite(proton_graph[i], HIGH);
    } else {
      digitalWrite(proton_graph[i], LOW);
   }
  }
}

void cyclotron_color(int currentled) {
  switch (currentMode)  {
  case protonAccelerator:
    pack_leds[currentled] = CRGB::Red;
    break;
  case darkMatterGenerator:
    pack_leds[currentled] = CRGB::Blue;
    break;
  case plasmDistrubutionSystem:
    pack_leds[currentled] = CRGB::Green;
    break;
  case CompositeParticleSystem:
    pack_leds[currentled] = CRGB::Orange;
    break;
  default:
    pack_leds[currentled] = CRGB::White;
  }
}

void vent_color() {
  switch (currentMode)  {
  case protonAccelerator:
    gun_leds[VENT_LED] = CRGB::White;
    break;
  case darkMatterGenerator:
    gun_leds[VENT_LED] = CRGB::Blue;
    break;
  case plasmDistrubutionSystem:
    gun_leds[VENT_LED] = CRGB::Green;
    break;
  case CompositeParticleSystem:
    gun_leds[VENT_LED] = CRGB::Yellow;
    break;
  default:
    gun_leds[VENT_LED] = CRGB::Red;
  }
}

void cyclotron_off(){
  pack_leds[cyclotron1] = CRGB::Black;
  pack_leds[cyclotron2] = CRGB::Black;
  pack_leds[cyclotron3] = CRGB::Black;
  pack_leds[cyclotron4] = CRGB::Black;
  FastLED.show();
  cyclotron_stage = 3;
}

void fade_cyclotron(){
  if(t - cyclotron_previus_fade < cyclotron_fade_time) {
    return;
  }
  int cyclotron_decrease = 3;
  for(int i = 0; i < 4; i++) {
    if (i == cyclotron_stage) {
      continue;
    }
    if (pack_leds[i].r -cyclotron_decrease > 0) {
      pack_leds[i].r -= cyclotron_decrease;
    } else {
      pack_leds[i].r = 0;
    }
    if (pack_leds[i].g -cyclotron_decrease > 0) {
      pack_leds[i].g -= cyclotron_decrease;
    } else {
      pack_leds[i].g = 0;
    }
    if (pack_leds[i].b -cyclotron_decrease > 0) {
      pack_leds[i].b -= cyclotron_decrease;
    } else {
      pack_leds[i].b = 0;
    }
  }
  cyclotron_previus_fade = t;
  FastLED.show();
}



