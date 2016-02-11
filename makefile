CXXFLAGS ?= -O3 -Wfatal-errors
CXXFLAGS += -std=c++14 -Wall -Wextra -pedantic -Werror
TARGETS := test-kec test minimal morseex test-scaling

all: $(TARGETS)

%: %.cpp Map.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	$(RM) $(TARGETS)

.PHONY: all clean
