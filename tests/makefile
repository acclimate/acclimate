.PHONY: all clean

all: bin/progressbar

test: bin/progressbar
	@bin/progressbar

clean:
	@rm -rf bin

bin:
	@mkdir bin

bin/progressbar: progressbar.cpp ../progressbar.h | bin
	@$(CXX) -O3 -Wall -std=c++11 $(CXX_FLAGS) -I.. -o $@ $<
