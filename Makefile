.PHONY: all test clean

all: musicbottles

musicbottles: musicBottles.c hx711.c audio.c
	gcc -o musicBottles musicBottles.c hx711.c audio.c gb_common.c -lSDL2 -lSDL2main -lSDL2_mixer

lowpass: lowpass.c hx711.c gb_common.c
	gcc -o lowpasstest lowpass.c hx711.c gb_common.c

# Run unit tests
test:
	$(MAKE) -C tests test

# Clean build artifacts
clean:
	rm -f musicBottles lowpasstest *.o
	$(MAKE) -C tests clean-tests
