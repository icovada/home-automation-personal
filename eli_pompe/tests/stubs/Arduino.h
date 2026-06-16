// Minimal host-side stub of the Arduino core, ONLY for the native unit tests in
// eli_pompe/tests/. It is never seen by the real Arduino/Controllino build — it
// lives in tests/stubs, which the Arduino IDE does not add to the include path
// (only the sketch root and a `src/` subfolder are compiled).
//
// It models the GPIO as plain arrays the tests can poke (g_din / g_ain) and read
// back (g_dout), plus a controllable clock (g_millis). Compile with -std=c++17.
#ifndef ARDUINO_TEST_STUB_H
#define ARDUINO_TEST_STUB_H

#include <cstdint>

typedef uint8_t byte;

#define INPUT 0
#define OUTPUT 1
#define HIGH 1
#define LOW 0
#define F(x) (x)

// ----- virtual hardware backing store (inline globals, C++17) -----
inline unsigned long g_millis = 0; // mock clock, advanced by the test runner
inline int g_din[64] = {0};        // digital input level per pin
inline int g_dout[64] = {0};       // digital output level per pin (what the code drives)
inline int g_ain[64] = {0};        // analog input value per pin
inline int g_pinmode[64] = {0};

inline unsigned long millis() { return g_millis; }
inline void pinMode(uint8_t p, uint8_t m) { if (p < 64) g_pinmode[p] = m; }
inline int digitalRead(uint8_t p) { return (p < 64) ? g_din[p] : 0; }
inline void digitalWrite(uint8_t p, uint8_t v) { if (p < 64) g_dout[p] = (v ? 1 : 0); }
inline int analogRead(uint8_t p) { return (p < 64) ? g_ain[p] : 0; }

// ----- minimal Serial (no-op; tests assert on GPIO, not on text) -----
struct SerialStub_
{
  void begin(long) {}
  void print(const char *) {}
  void print(char) {}
  void print(int) {}
  void print(unsigned long) {}
  void print(bool) {}
  void print(double, int = 2) {}
  void println(const char *) {}
  void println(int) {}
  void println(bool) {}
  void println() {}
};
inline SerialStub_ Serial;

#endif
