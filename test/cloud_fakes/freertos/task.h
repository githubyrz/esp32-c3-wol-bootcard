#pragma once
#include "FreeRTOS.h"
// The test explicitly advances one production cloud tick at a time.
inline BaseType_t xTaskCreate(void (*)(void*), const char*,int,void*,int,void*) { return pdPASS; }
inline void vTaskDelay(unsigned long n) { delay(n); }
