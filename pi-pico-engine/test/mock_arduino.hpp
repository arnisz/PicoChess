#ifndef MOCK_ARDUINO_HPP
#define MOCK_ARDUINO_HPP
#define MOCK_ARDUINO

#include <iostream>
#include <string>
#include <cstdint>

using uint8_t = std::uint8_t;
using uint16_t = std::uint16_t;
using uint32_t = std::uint32_t;

#ifdef ARDUINO_ENV
class String : public std::string {
public:
    using std::string::string;
    String(const std::string& s): std::string(s) {}
    String(const char* s): std::string(s) {}
    String& operator=(const std::string& s){ std::string::operator=(s); return *this; }
    String& operator=(const char* s){ std::string::operator=(s); return *this; }
    int indexOf(const std::string& sub) const { size_t p=find(sub); return p==std::string::npos?-1:(int)p; }
    String substring(size_t pos) const { return String(std::string::substr(pos)); }
    String substring(size_t pos,size_t len) const { return String(std::string::substr(pos,len)); }
    void trim(){ size_t s=find_first_not_of(' '); size_t e=find_last_not_of(' '); if(s==std::string::npos){ *this=String(); return;} *this=String(std::string::substr(s,e-s+1)); }
    bool startsWith(const std::string& sub) const { return rfind(sub,0)==0; }
    int toInt() const { return std::stoi(*this); }
    size_t length() const { return size(); }
};
#endif

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1

inline void digitalWrite(int pin, int value) {
    std::cout << "digitalWrite(" << pin << ", " << value << ")" << std::endl;
}

inline int digitalRead(int pin) {
    return LOW;
}

inline void pinMode(int pin, int mode) {
    std::cout << "pinMode(" << pin << ", " << mode << ")" << std::endl;
}

inline void delay(unsigned long ms) {
    std::cout << "delay(" << ms << "ms)" << std::endl;
}

class MockSerial {
public:
    void begin(long baud) { std::cout << "Serial.begin(" << baud << ")" << std::endl; }
    void print(const char* str) { std::cout << str; }
    void print(const std::string& str) { std::cout << str; }
    void println(const char* str) { std::cout << str << std::endl; }
    void println(const std::string& str) { std::cout << str << std::endl; }
    void println(int value) { std::cout << value << std::endl; }
    bool available() { return false; }
    char read() { return 0; }
    operator bool() const { return true; }
};

extern MockSerial Serial;

#endif // MOCK_ARDUINO_HPP
