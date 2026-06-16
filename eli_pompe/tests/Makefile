# Host-side unit tests for the eli_pompe controller logic.
# These do NOT touch the Controllino; they fake the Arduino core (stubs/) and
# exercise ../pompe.h in simulated time. Run with: make   (or: make run)

CXX      ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O0 -g
INCLUDES  = -I stubs -I ..
BIN       = test_pompe

.PHONY: all run clean
all: run

$(BIN): test_pompe.cpp ../pompe.h stubs/Arduino.h
	$(CXX) $(CXXFLAGS) $(INCLUDES) test_pompe.cpp -o $(BIN)

run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
