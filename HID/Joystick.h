#pragma once

#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

class Joystick {
   public:
      enum Direction8 {
         CENTER,
         UP,
         DOWN,
         LEFT,
         RIGHT,
         UP_LEFT,
         UP_RIGHT,
         DOWN_LEFT,
         DOWN_RIGHT
      };

      static inline uint64_t millis() {
         return to_ms_since_boot(get_absolute_time());
      }

      Joystick(int adcX, 
               int adcY, 
               int rightEnter = 235, 
               int leftEnter = 20, 
               int upEnter = 235, 
               int downEnter = 20, 
               uint64_t debounceMs = 50
            ): _adcX(adcX), 
               _adcY(adcY), 
               _gpioX(26 + adcX),
               _gpioY(26 + adcY),
               _rightEnter(rightEnter), 
               _leftEnter(leftEnter), 
               _upEnter(upEnter), 
               _downEnter(downEnter), 
               _debounceMs(debounceMs) 
               {}

      void begin() {
         adc_init();
         adc_gpio_init(_gpioX);
         adc_gpio_init(_gpioY);
         gpio_set_dir(_gpioX, GPIO_IN);
         gpio_set_dir(_gpioY, GPIO_IN);
      }

      void update() {
         int x = readAxis(_adcX);
         int y = readAxis(_adcY);

         Direction8 newDir = computeDirection(x, y);

         if (newDir != _lastReading) {
            _lastChangeTime = millis();
         }

         if (millis() - _lastChangeTime > _debounceMs) {
            if (newDir != _direction) {
               _direction = newDir;
               _changed = true;
            }
            else {
               _changed = false;
            }
         }

         _lastReading = newDir;
      }

      bool changed() const {
         return _changed;
      }

      Direction8 direction() const {
         return _direction;
      }

      const char *directionName() const {
         switch (_direction) {
            case UP:
               return "J2 Up";
            case DOWN:
               return "J2 Down";
            case LEFT:
               return "J2 Left";
            case RIGHT:
               return "J2 Right";
            case UP_LEFT:
               return "J2 Up-Left";
            case UP_RIGHT:
               return "J2 Up-Right";
            case DOWN_LEFT:
               return "J2 Down-Left";
            case DOWN_RIGHT:
               return "J2 Down-Right";
            default:
               return "Center";
         }
      }

   private:
      int _adcX, _adcY;
      int _gpioX, _gpioY;
      int _rightEnter, _leftEnter, _upEnter, _downEnter;
      uint64_t _debounceMs;

      Direction8 _direction = CENTER;
      Direction8 _lastReading = CENTER;

      uint64_t _lastChangeTime = 0;
      bool _changed = false;

      int readAxis(int adcChannel) {
         adc_select_input(adcChannel);
         uint16_t raw = adc_read();
         return raw >> 4;
         // return map(adc_read(pin), 0, 4095, 0, 255);
      }

      Direction8 computeDirection(int x, int y) {
         bool right = x >= _rightEnter;
         bool left = x <= _leftEnter;
         bool up = y >= _upEnter;
         bool down = y <= _downEnter;

         if (right && up)
            return UP_RIGHT;
         if (left && up)
            return UP_LEFT;
         if (right && down)
            return DOWN_RIGHT;
         if (left && down)
            return DOWN_LEFT;
         if (right)
            return RIGHT;
         if (left)
            return LEFT;
         if (up)
            return UP;
         if (down)
            return DOWN;

         return CENTER;
      }
};