#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/watchdog.h"
#include "bsp/board_api.h"
#include "tusb.h"
#include "class/hid/hid.h"
#include "class/cdc/cdc_device.h"

#include "Joystick.h"

const unsigned long debounce_delay = 50;

static inline uint64_t millis() {
   return to_ms_since_boot(get_absolute_time());
}

struct DebouncedButton{
   int pin;
   int state = 1; // 1 = HIGH, 0 = LOW
   int lastReading = 1;

   uint64_t lastChangeTime = 0;
   const uint64_t debounceDelay = 50;

   void begin(){
      gpio_init(pin);
      gpio_set_dir(pin, GPIO_IN);
      gpio_pull_up(pin);
   }

   void update(){
      int reading = gpio_get(pin);

      if (reading != lastReading){
         lastChangeTime = millis();
      }

      if (millis() - lastChangeTime > debounceDelay){
         state = reading;
      }

      lastReading = reading;
   }

   bool isHeld() const {
      return state == 0;
   }
};


// ------------ ROTARY DIAL ------------
const int dial_key_pin = 5;
const int dial_S1_pin = 4;
const int dial_S2_pin = 3;

static int last_A = -1;

DebouncedButton dial_S1 = { dial_S1_pin };
DebouncedButton dial_S2 = { dial_S2_pin };
DebouncedButton dial_key = { dial_key_pin };


// ---------------- J1 ----------------
const int j1_up_pin =     14; // USE GP** NUMBER, NOT PIN NUMBER
const int j1_down_pin =   15;
const int j1_left_pin =   12;
const int j1_right_pin =  13;
const int j1_button_pin = 11;

enum J1Direction {
   J1_CENTER,
   J1_BUTTON,
   J1_UP,
   J1_DOWN,
   J1_LEFT,
   J1_RIGHT
};
J1Direction last_j1_sent = J1_CENTER;

DebouncedButton j1_up =     { j1_up_pin };
DebouncedButton j1_down =   { j1_down_pin };
DebouncedButton j1_left =   { j1_left_pin };
DebouncedButton j1_right =  { j1_right_pin };
DebouncedButton j1_button = { j1_button_pin };


// ---------------- J2 ----------------
Joystick joystick(1, 0); // USE ADC *, NOT PIN OR GP** NUMBER
const int j2_button = 28;

int last_j2_button_state = 0;
int j2_button_state;
uint64_t last_j2_button_debounce_time = 0;

Joystick::Direction8 last_consumed_dir = Joystick::CENTER;
bool dir_event_consumed = false;

uint8_t last_key = 0;
bool key_down = false;
uint64_t last_send_time = 0;

void send_key(uint8_t key, bool shift = false){
   if (!tud_hid_ready()) return;

   uint8_t modifier = shift ? KEYBOARD_MODIFIER_LEFTSHIFT : 0;

   uint8_t keycode[6] = { key };
   tud_hid_keyboard_report(0, modifier, keycode);

   key_down = true;
   last_key = key;
   last_send_time = millis();
}


// ---------------- GUI ----------------
// Sends joystick direction to /dev/ttyACM0 through CDC
void send_gui_action(const char* action){
   if (!tud_cdc_connected()) return;

   tud_cdc_write_str(action);
   tud_cdc_write_str("\n");
   tud_cdc_write_flush(); 
}

const char* j1_dir_to_string(int dir){
   switch(dir){
      case 0:  return "J1: BUTTON";
      case 1:  return "J1: UP";
      case 2:  return "J1: DOWN";
      case 3:  return "J1: LEFT";
      case 4:  return "J1: RIGHT";
      default: return "J1: CENTER";
   }
}

const char* j2_dir_to_string(Joystick::Direction8 dir) {
   switch(dir){
      case Joystick::UP:         return "J2: UP";
      case Joystick::DOWN:       return "J2: DOWN";
      case Joystick::LEFT:       return "J2: LEFT";
      case Joystick::RIGHT:      return "J2: RIGHT";
      case Joystick::UP_LEFT:    return "J2: UP_LEFT";
      case Joystick::UP_RIGHT:   return "J2: UP_RIGHT";
      case Joystick::DOWN_LEFT:  return "J2: DOWN_LEFT";
      case Joystick::DOWN_RIGHT: return "J2: DOWN_RIGHT";
      default:                   return "J2: CENTER";
   }
}


int main() {
   // SETUP() {}
   stdio_init_all();
   board_init();


   // USB CDC init
   tusb_rhport_init_t dev_init = {
      .role = TUSB_ROLE_DEVICE,
      .speed = TUSB_SPEED_AUTO
   };
   tusb_init(BOARD_TUD_RHPORT, &dev_init);
   while (!tud_mounted){
      tud_task();
      sleep_ms(10);
   }


   // Rotary Dial
   dial_key.begin();
   dial_S1.begin();
   dial_S2.begin();
   last_A = gpio_get(dial_S1_pin);

   // J1
   j1_up.begin();
   j1_down.begin();
   j1_left.begin();
   j1_right.begin();
   j1_button.begin();


   // J2
   gpio_init(j2_button);
   gpio_set_dir(j2_button, GPIO_IN);
   gpio_pull_up(j2_button);
   joystick.begin();


   // Auto-reboot (watchdog) example code
   if (watchdog_caused_reboot()) {
      printf("Rebooted by Watchdog!\n");
   }
   // Enable the watchdog, requiring the watchdog to be updated every *ms or the chip will reboot
   // second arg is pause on debug which means the watchdog will pause when stepping through code
   sleep_ms(2000);
   // watchdog_enable(1000, 1);


   // LOOP() {}
   while (true) {
      tud_task();

      // ------------ ROTARY DIAL ------------
      dial_key.update();

      int A = gpio_get(dial_S1_pin);
      int B = gpio_get(dial_S2_pin);
      
      if (A != last_A){
         if (last_A == 1 && A == 0){
            if (B != A){
               send_gui_action("DIAL: -ROT");
            }
            else{
               send_gui_action("DIAL: +ROT");
            }
         }
         last_A = A;
      }

      static bool last_key = false;
      bool key = dial_key.isHeld();

      if (key && !last_key){
         send_gui_action("DIAL: BUTTON");
      }
      last_key = key;


      // -------- J1 --------
      j1_up.update();
      j1_down.update();
      j1_left.update();
      j1_right.update();
      j1_button.update();

      bool up = j1_up.isHeld();
      bool down = j1_down.isHeld();
      bool left = j1_left.isHeld();
      bool right = j1_right.isHeld();
      bool btn = j1_button.isHeld();

      // -------- J1 EVENT SYSTEM --------
      J1Direction current_j1 = J1_CENTER;

      if (btn)          current_j1 = J1_BUTTON;
      else if (up)      current_j1 = J1_UP;
      else if (down)    current_j1 = J1_DOWN;
      else if (left)    current_j1 = J1_LEFT;
      else if (right)   current_j1 = J1_RIGHT;
      else              current_j1 = J1_CENTER;

      if (current_j1 != last_j1_sent) {
         switch(current_j1){
            case J1_BUTTON: send_gui_action("J1: BUTTON"); break;
            case J1_UP:     send_gui_action("J1: UP");     break;
            case J1_DOWN:   send_gui_action("J1: DOWN");   break;
            case J1_LEFT:   send_gui_action("J1: LEFT");   break;
            case J1_RIGHT:  send_gui_action("J1: RIGHT");  break;
            case J1_CENTER: send_gui_action("J1: CENTER"); break;
         }
         last_j1_sent = current_j1;
      }


      // -------- J2 --------
      if (key_down && (millis() - last_send_time > 30)){
         uint8_t empty[6] = {0};
         tud_hid_keyboard_report(0, 0, empty);
         key_down = false;
         last_key = 0;
      }

      static Joystick::Direction8 stable_dir = Joystick::CENTER;
      static Joystick::Direction8 armed_dir = Joystick::CENTER;

      joystick.update();

      Joystick::Direction8 dir = joystick.direction();
      static Joystick::Direction8 last_sent_dir = Joystick::CENTER;

      if (dir != last_sent_dir) {
         send_gui_action(j2_dir_to_string(dir));
         last_sent_dir = dir;
      }


      stable_dir = dir;
      if (stable_dir != Joystick::CENTER && armed_dir == Joystick::CENTER){
         armed_dir = stable_dir;
      }
      if (stable_dir == Joystick::CENTER){
         armed_dir = Joystick::CENTER;
      }

      bool dirChanged = joystick.changed();

      if (dir == Joystick::CENTER){
         dir_event_consumed = false;
         last_consumed_dir = Joystick::CENTER;
      }


      if (armed_dir != Joystick::CENTER){
         // L-JOYSTICK BUTTON DOWN
         if (btn) {
            if (armed_dir == Joystick::UP)         send_key(0x38, false); // /
            if (armed_dir == Joystick::DOWN)       send_key(0x33, true);  // :
            if (armed_dir == Joystick::LEFT)       send_key(0x25, true);  // *
            if (armed_dir == Joystick::RIGHT)      send_key(0x2e, false); // =
            if (armed_dir == Joystick::UP_LEFT)    send_key(0x2e, true);  // +
            if (armed_dir == Joystick::UP_RIGHT)   send_key(0x2d, false); // - 
            if (armed_dir == Joystick::DOWN_LEFT)  send_key(0x34, true);  // "
            if (armed_dir == Joystick::DOWN_RIGHT) send_key(0x34, false); // '
         }

         // L-JOYSTICK UP
         else if (up) {
            if (armed_dir == Joystick::UP)         send_key(0x1a); // w
            if (armed_dir == Joystick::DOWN)       send_key(0x14); // q
            // if (dir == Joystick::LEFT)       send_key(0x); // 
            if (armed_dir == Joystick::RIGHT)      send_key(0x37, false); // .
            if (armed_dir == Joystick::UP_LEFT)    send_key(0x36, false); // ,
            if (armed_dir == Joystick::UP_RIGHT)   send_key(0x33, false); // ;
            if (armed_dir == Joystick::DOWN_LEFT)  send_key(0x31, false); // '\'
            if (armed_dir == Joystick::DOWN_RIGHT) send_key(0x31, true);  // |
         }

         // L-JOYSTICK DOWN
         else if (down) {
            if (armed_dir == Joystick::UP)         send_key(0x12); // o
            if (armed_dir == Joystick::DOWN)       send_key(0x04); // a
            if (armed_dir == Joystick::LEFT)       send_key(0x0c); // i
            if (armed_dir == Joystick::RIGHT)      send_key(0x08); // e
            if (armed_dir == Joystick::UP_LEFT)    send_key(0x18); // u
            if (armed_dir == Joystick::UP_RIGHT)   send_key(0x1c); // y
            if (armed_dir == Joystick::DOWN_LEFT)  send_key(0x17); // t
            if (armed_dir == Joystick::DOWN_RIGHT) send_key(0x11); // n
         }

         // L-JOYSTICK RIGHT
         else if (right) {
            if (armed_dir == Joystick::UP)         send_key(0x16); // s
            if (armed_dir == Joystick::DOWN)       send_key(0x0b); // h
            if (armed_dir == Joystick::LEFT)       send_key(0x15); // r
            if (armed_dir == Joystick::RIGHT)      send_key(0x07); // d
            if (armed_dir == Joystick::UP_LEFT)    send_key(0x0f); // l
            if (armed_dir == Joystick::UP_RIGHT)   send_key(0x06); // c
            if (armed_dir == Joystick::DOWN_LEFT)  send_key(0x10); // m
            if (armed_dir == Joystick::DOWN_RIGHT) send_key(0x1b); // x
         }

         // L-JOYSTICK LEFT
         else if (left) {
            if (armed_dir == Joystick::UP)         send_key(0x09); // f
            if (armed_dir == Joystick::DOWN)       send_key(0x0a); // g
            if (armed_dir == Joystick::LEFT)       send_key(0x1d); // z
            if (armed_dir == Joystick::RIGHT)      send_key(0x13); // p
            if (armed_dir == Joystick::UP_LEFT)    send_key(0x05); // b
            if (armed_dir == Joystick::UP_RIGHT)   send_key(0x19); // v
            if (armed_dir == Joystick::DOWN_LEFT)  send_key(0x0e); // k
            if (armed_dir == Joystick::DOWN_RIGHT) send_key(0x0d); // j
         }

         // L-JOYSTICK BUTTON UP (NO INPUT)
         else{
            if (armed_dir == Joystick::UP)         send_key(0x2a, false); // BACKSPACE
            if (armed_dir == Joystick::DOWN)       send_key(0x2c, false); // SPACE
            if (armed_dir == Joystick::LEFT)       send_key(0x29, false); // ESCAPE
            if (armed_dir == Joystick::RIGHT)      send_key(0x28, false); // ENTER
            if (armed_dir == Joystick::UP_LEFT)    send_key(0x1e, true);  // !
            if (armed_dir == Joystick::UP_RIGHT)   send_key(0x38, true);  // ?
            if (armed_dir == Joystick::DOWN_LEFT)  send_key(0x21, true);  // $
            if (armed_dir == Joystick::DOWN_RIGHT) send_key(0x1f, true);  // @
         }
      }
      armed_dir = Joystick::CENTER;
      // watchdog_update();
      // sleep_ms(1);
   }
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  // This example doesn't use multiple report and report ID
  (void) itf;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) bufsize;
}