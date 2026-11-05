#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>
using byte = uint8_t;
class String : public std::string {
 public:
  using std::string::string;
  using std::string::operator=;
};
extern uint32_t hostMillis;
inline unsigned long millis() { return hostMillis; }
inline void yield() { ++hostMillis; }
inline void delay(unsigned long ms) { hostMillis += ms; }
inline void configTime(long, int, const char*, const char*, const char*) {}
#include "../fakes/WiFi.h"
