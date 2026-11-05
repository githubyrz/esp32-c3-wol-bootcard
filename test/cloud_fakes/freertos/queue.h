#pragma once
#include "FreeRTOS.h"
#include <deque>
struct HostQueue { size_t capacity, size; std::deque<std::vector<uint8_t>> data; };
using QueueHandle_t=HostQueue*;
inline QueueHandle_t xQueueCreate(size_t n,size_t size) { return new HostQueue{n,size,{}}; }
inline void vQueueDelete(QueueHandle_t q) { delete q; }
inline int xQueueSend(QueueHandle_t q,const void* data,int) {
  if(q->data.size()==q->capacity)return 0;
  auto b=static_cast<const uint8_t*>(data);q->data.emplace_back(b,b+q->size);return pdTRUE;
}
inline int xQueueReceive(QueueHandle_t q,void* data,int) {
  if(q->data.empty())return 0;
  memcpy(data,q->data.front().data(),q->size);q->data.pop_front();return pdTRUE;
}
