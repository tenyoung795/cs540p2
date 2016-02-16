CXXFLAGS ?= -O3
CXXFLAGS += -std=c++14 -Wall -Wextra -pedantic -Werror -Wfatal-errors
EXTRA_TARGETS :=
REQUIRED_TARGETS := test-kec test minimal morseex test-scaling

extra: $(EXTRA_TARGETS)
required: $(REQUIRED_TARGETS)
all: extra required

%: %.cpp Map.hpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	$(RM) $(TARGETS)

.PHONY: all clean extra required
