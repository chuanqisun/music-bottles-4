/**

Music Bottles v4 by Tal Achituv

Based on code by Tomer Weller, Jasmin Rubinovitz, as well as the general idea of previous Music Bottles versions

Simplified weight change detection system:
- Auto tare on start
- Detect cap removal by weight delta matching
- Uses precomputed weight table for all 8 cap combinations
- Plays classic tracks and birthday song (when all caps removed)

*/

#include "audio.h"
#include "hx711.h"
#include "minimal_gpio.c"
#include <unistd.h>

// GPIO output pins to Arduino for LED control
#define BOT1_PIN 18
#define CAP1_PIN 17
#define BOT2_PIN 27
#define CAP2_PIN 22
#define BOT3_PIN 23
#define CAP3_PIN 24

// Weight detection error margin (+-20)
#define WEIGHT_MARGIN 20

// Global state
long tare = 0;
int cap1, cap2, cap3;  // Cap weights from CLI args
long smoothedWeight = 0;  // Smoothed weight reading (relative to tare)

// Precomputed weight table for all 8 states (indexed by cap state bitmask)
// Index: bit0=cap1, bit1=cap2, bit2=cap3 (1=removed, 0=on)
// Value: expected weight delta when those caps are removed
long weightTable[8];

// State names for debugging
const char *stateNames[8] = {
	"All caps on",      // 0b000: off, off, off
	"Cap1 removed",     // 0b001: on, off, off -> track 1
	"Cap2 removed",     // 0b010: off, on, off -> track 2
	"Cap1+2 removed",   // 0b011: on, on, off -> track 1+2
	"Cap3 removed",     // 0b100: off, off, on -> track 3
	"Cap1+3 removed",   // 0b101: on, off, on -> track 1+3
	"Cap2+3 removed",   // 0b110: off, on, on -> track 2+3
	"BIRTHDAY MODE"     // 0b111: on, on, on -> birthday
};

// Current detected state (0-7)
int currentState = 0;

void setupGPIO() {
	if (gpioInitialise() < 0) exit(-1);
	
	// Output pins to Arduino for LED control
	gpioSetMode(BOT1_PIN, PI_OUTPUT);
	gpioSetMode(CAP1_PIN, PI_OUTPUT);
	gpioSetMode(BOT2_PIN, PI_OUTPUT);
	gpioSetMode(CAP2_PIN, PI_OUTPUT);
	gpioSetMode(BOT3_PIN, PI_OUTPUT);
	gpioSetMode(CAP3_PIN, PI_OUTPUT);
}

void setBottleLEDs(int state) {
	// Signal Arduino based on which caps are removed
	int cap1Removed = state & 0x01;
	int cap2Removed = state & 0x02;
	int cap3Removed = state & 0x04;
	
	gpioWrite(BOT1_PIN, 1);
	gpioWrite(CAP1_PIN, cap1Removed ? 0 : 1);
	gpioWrite(BOT2_PIN, 1);
	gpioWrite(CAP2_PIN, cap2Removed ? 0 : 1);
	gpioWrite(BOT3_PIN, 1);
	gpioWrite(CAP3_PIN, cap3Removed ? 0 : 1);
}

// Initialize weight table for all 8 cap combinations
void initWeightTable() {
	// Weight delta is negative sum of removed cap weights
	weightTable[0] = 0;                         // No caps removed
	weightTable[1] = -cap1;                     // Cap1 removed
	weightTable[2] = -cap2;                     // Cap2 removed
	weightTable[3] = -cap1 - cap2;              // Cap1+2 removed
	weightTable[4] = -cap3;                     // Cap3 removed
	weightTable[5] = -cap1 - cap3;              // Cap1+3 removed
	weightTable[6] = -cap2 - cap3;              // Cap2+3 removed
	weightTable[7] = -cap1 - cap2 - cap3;       // All caps removed
}

// Find which state matches the current weight delta
int matchState(long weightDelta) {
	for (int i = 0; i < 8; i++) {
		if (weightDelta >= weightTable[i] - WEIGHT_MARGIN &&
		    weightDelta <= weightTable[i] + WEIGHT_MARGIN) {
			return i;
		}
	}
	return -1;  // Unknown state
}

// Apply audio based on cap state
// Logic is decoupled: we determine target state for each of 4 tracks
// based on 3 cap states, then apply appropriate transitions
void applyAudioState(int state) {
	// Determine target state for each track
	int birthdayMode = (state == 7);  // All 3 caps removed
	
	// Target state for each track (1 = should play, 0 = should stop)
	int track1Target = !birthdayMode && (state & 0x01);  // Cap1 removed, not birthday
	int track2Target = !birthdayMode && (state & 0x02);  // Cap2 removed, not birthday
	int track3Target = !birthdayMode && (state & 0x04);  // Cap3 removed, not birthday
	int birthdayTarget = birthdayMode;
	
	// Apply transitions for tracks 1-3
	// Each track: if target is on -> volume up, if target is off -> fade out
	track1Target ? volume(0, 105) : fadeOut(0);
	track2Target ? volume(1, 105) : fadeOut(1);
	track3Target ? volume(2, 105) : fadeOut(2);
	
	// Apply transition for birthday track
	if (birthdayTarget) {
		playBirthday();
	} else if (isBirthdayPlaying()) {
		fadeOutBirthday();
	}
	
	// Rewind files when all caps are on (state 0 = reset point)
	if (state == 0) {
		rewindFiles();
	}
}

int main(int argc, char **argv) {
	// Parse CLI arguments
	if (argc != 4) {
		printf("Usage: musicBottles cap1 cap2 cap3\n");
		printf("  cap1, cap2, cap3: integer weights of the caps (e.g., 629 728 426)\n");
		printf("  Weight detection margin: +/-%d\n", WEIGHT_MARGIN);
		return -1;
	}
	
	cap1 = atoi(argv[1]);
	cap2 = atoi(argv[2]);
	cap3 = atoi(argv[3]);
	
	printf("=== Music Bottles v4 ===\n");
	printf("Sound set: Classic\n");
	printf("Cap weights: Cap1=%d, Cap2=%d, Cap3=%d\n", cap1, cap2, cap3);
	printf("Detection margin: +/-%d\n\n", WEIGHT_MARGIN);
	
	// Initialize weight table
	initWeightTable();
	printf("Weight table initialized:\n");
	for (int i = 0; i < 8; i++) {
		printf("  State %d (%s): %ld\n", i, stateNames[i], weightTable[i]);
	}
	printf("\n");
	
	// Initialize hardware
	printf("Initializing scale...\n");
	initHX711();
	
	setupGPIO();
	initSound();
	
	// Auto tare on start
	printf("Acquiring tare... ");
	fflush(stdout);
	tare = getCleanSample(150, 4);
	printf("Tare: %ld\n\n", tare);
	
	printf("Monitoring weight changes...\n");
	printf("(Weight delta shown relative to tared zero)\n\n");
	
	// Main loop
	while (1) {
		long raw = getCleanSample(4, 4) - tare;
		smoothedWeight = smoothedWeight * 0.85 + raw * 0.15;
		
		long displayWeight = smoothedWeight / 100;
		long rawDisplay = raw / 100;
		
		// Find matching state
		int newState = matchState(displayWeight);
		
		// Clear line and display current weight
		printf("\r                                                              \r");
		printf("Delta: %5ld | Raw: %5ld | ", displayWeight, rawDisplay);
		
		if (newState >= 0) {
			printf("%s", stateNames[newState]);
		} else {
			printf("Unknown     ");
		}
		fflush(stdout);
		
		// Handle state change
		if (newState >= 0 && newState != currentState) {
			printf("\n>>> State change: %s -> %s\n", 
			       stateNames[currentState], stateNames[newState]);
			currentState = newState;
			setBottleLEDs(currentState);
			applyAudioState(currentState);
		}
		
		// Handle audio fade
		handleFade();
		
		usleep(50000);  // 50ms delay
	}
	
	return 0;
}
