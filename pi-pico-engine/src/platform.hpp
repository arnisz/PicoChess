#pragma once

#ifdef ARDUINO
  #ifndef MOCK_ARDUINO
    #include <Arduino.h>
  #endif
  #define PLATFORM_PRINT(x) Serial.println(x)
  #define PLATFORM_DELAY(x) delay(x)
#else
  #include <iostream>
  #include <chrono>
  #include <thread>
  #include <string>
  class String : public std::string {
  public:
    using std::string::string;
    String(const std::string& s): std::string(s) {}
    String(const char* s): std::string(s) {}
    String& operator=(const std::string& s){ std::string::operator=(s); return *this; }
    String& operator=(const char* s){ std::string::operator=(s); return *this; }
    int indexOf(const std::string& sub) const {
      size_t p=find(sub); return p==std::string::npos?-1:(int)p;
    }
    String substring(size_t pos) const { return String(std::string::substr(pos)); }
    String substring(size_t pos,size_t len) const { return String(std::string::substr(pos,len)); }
    void trim(){ size_t s=find_first_not_of(' '); size_t e=find_last_not_of(' '); if(s==std::string::npos){ *this=String(); return;} *this=String(std::string::substr(s,e-s+1)); }
    int toInt() const { return std::stoi(*this); }
    bool startsWith(const std::string& sub) const { return rfind(sub,0)==0; }
  };
  #define PLATFORM_PRINT(x) std::cout << x << std::endl
  #define PLATFORM_DELAY(x) std::this_thread::sleep_for(std::chrono::milliseconds(x))
#endif

#ifdef DEBUG_MODE
  #define DBG_PRINT(x) PLATFORM_PRINT(x)
#else
  #define DBG_PRINT(x)
#endif
