#pragma once
#include "Arduino.h"
class Print {
 public:
  virtual ~Print() = default;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* b, size_t n) { size_t i=0; for (; i<n; ++i) if (!write(b[i])) break; return i; }
};
class Stream : public Print {
 public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;
  virtual void flush() = 0;
  void setTimeout(unsigned long timeout) { timeout_ = timeout; }
  size_t readBytes(uint8_t* b, size_t n) {
    size_t done=0;
    while (done<n) {
      const uint32_t start=millis(); int c;
      do { c=read(); if(c>=0) break; yield(); } while(uint32_t(millis()-start)<timeout_);
      if(c<0) break;
      b[done++]=static_cast<uint8_t>(c);
    }
    return done;
  }
 private: unsigned long timeout_=1000;
};
class Client : public Stream {
 public:
  virtual int read() = 0;
  virtual int connect(IPAddress, uint16_t) = 0;
  virtual int connect(const char*, uint16_t) = 0;
  virtual int read(uint8_t*, size_t) = 0;
  virtual void stop() = 0;
  virtual uint8_t connected() = 0;
  virtual operator bool() = 0;
};
