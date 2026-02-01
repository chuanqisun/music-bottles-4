/**

Music Bottles v4 by Tal Achituv

Based on code by Tomer Weller, Jasmin Rubinovitz, as well as the general idea of previous Music Bottles versions

Simplified weight change detection system:
- Auto tare on start
- Detect cap removal by weight delta matching
- Trigger music when cap weight is detected within error margin

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

// Weight detection error margin (+-30)
#define WEIGHT_MARGIN 30

// Global state
long tare = 0;
int cap1, cap2, cap3;  // Cap weights from CLI args
int soundSet = 0;      // Sound set selection (0=jazz, 1=classic, 2=synth, 3=boston)
long smoothedWeight = 0;  // Smoothed weight reading (relative to tare)

// Track which caps are currently detected as removed
int cap1Removed = 0;
int cap2Removed = 0;
int cap3Removed = 0;

// Birthday mode: when all three caps are removed
int birthdayMode = 0;

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

void setBottleLED(int bottle, int capRemoved) {
	// Signal Arduino: bottle present, cap removed state
	switch(bottle) {
		case 1:
			gpioWrite(BOT1_PIN, 1);  // Bottle present
			gpioWrite(CAP1_PIN, capRemoved ? 0 : 1);
			break;
		case 2:
			gpioWrite(BOT2_PIN, 1);
			gpioWrite(CAP2_PIN, capRemoved ? 0 : 1);
			break;
		case 3:
			gpioWrite(BOT3_PIN, 1);
			gpioWrite(CAP3_PIN, capRemoved ? 0 : 1);
			break;
	}
}

// Check if weight delta matches a cap weight within margin
int matchesCap(long weightDelta, int capWeight) {
	long target = -capWeight;  // Negative because cap removal reduces weight
	return (weightDelta >= target - WEIGHT_MARGIN) && (weightDelta <= target + WEIGHT_MARGIN);
}

// Detect which cap is removed based on current weight delta and trigger music
// Uses fade-in/fade-out pattern from reference: either set volume OR fadeOut each cycle
void detectCapAndTriggerMusic(long weightDelta) {
	int newCap1 = matchesCap(weightDelta, cap1);
	int newCap2 = matchesCap(weightDelta, cap2);
	int newCap3 = matchesCap(weightDelta, cap3);
	int noCapRemoved = (weightDelta >= -WEIGHT_MARGIN && weightDelta <= WEIGHT_MARGIN);
	
	// Log state changes
	if (newCap1 && !cap1Removed) {
		printf("\n>>> CAP 1 REMOVED (weight: %ld, expected: -%d) - Playing Track 1\n", weightDelta, cap1);
		setBottleLED(1, 1);
		cap1Removed = 1;
	} else if (!newCap1 && cap1Removed) {
		printf("\n>>> CAP 1 REPLACED - Fading Track 1\n");
		setBottleLED(1, 0);
		cap1Removed = 0;
	}
	
	if (newCap2 && !cap2Removed) {
		printf("\n>>> CAP 2 REMOVED (weight: %ld, expected: -%d) - Playing Track 2\n", weightDelta, cap2);
		setBottleLED(2, 1);
		cap2Removed = 1;
	} else if (!newCap2 && cap2Removed) {
		printf("\n>>> CAP 2 REPLACED - Fading Track 2\n");
		setBottleLED(2, 0);
		cap2Removed = 0;
	}
	
	if (newCap3 && !cap3Removed) {
		printf("\n>>> CAP 3 REMOVED (weight: %ld, expected: -%d) - Playing Track 3\n", weightDelta, cap3);
		setBottleLED(3, 1);
		cap3Removed = 1;
	} else if (!newCap3 && cap3Removed) {
		printf("\n>>> CAP 3 REPLACED - Fading Track 3\n");
		setBottleLED(3, 0);
		cap3Removed = 0;
	}
	
	// Check for birthday mode: all three caps removed
	int allCapsRemoved = cap1Removed && cap2Removed && cap3Removed;
	
	if (allCapsRemoved && !birthdayMode) {
		// Enter birthday mode: fade out all tracks and play birthday
		printf("\n>>> ALL CAPS REMOVED - BIRTHDAY MODE! <<<\n");
		fadeOut(0);
		fadeOut(1);
		fadeOut(2);
		playBirthday();
		birthdayMode = 1;
	} else if (!allCapsRemoved && birthdayMode) {
		// Exit birthday mode: stop birthday track
		printf("\n>>> Exiting birthday mode <<<\n");
		stopBirthday();
		birthdayMode = 0;
	}
	
	// Apply volume or fade-out each cycle (reference pattern)
	// Volume 105 is used (must be >= 100 to trigger play, per audio.c logic)
	// Skip normal volume control in birthday mode
	if (!birthdayMode) {
		cap1Removed ? volume(0, 105) : fadeOut(0);
		cap2Removed ? volume(1, 105) : fadeOut(1);
		cap3Removed ? volume(2, 105) : fadeOut(2);
	}
	
	// Rewind files if all caps are back on
	if (noCapRemoved && !cap1Removed && !cap2Removed && !cap3Removed) {
		rewindFiles();
	}
}

int main(int argc, char **argv) {
	// Parse CLI arguments
	if (argc != 4 && argc != 5) {
		printf("Usage: musicBottles [sound] cap1 cap2 cap3\n");
		printf("  sound: 1=classic (default), 2=jazz, 3=synth, 4=boston\n");
		printf("  cap1, cap2, cap3: integer weights of the caps (e.g., 629 728 426)\n");
		printf("  Weight detection margin: +/-%d\n", WEIGHT_MARGIN);
		return -1;
	}
	
	// Parse arguments based on count
	if (argc == 5) {
		// Sound set specified
		int soundArg = atoi(argv[1]);
		if (soundArg < 1 || soundArg > 4) soundArg = 1;
		// Map: 1=classic(1), 2=jazz(0), 3=synth(2), 4=boston(3)
		switch(soundArg) {
			case 1: soundSet = 1; break;  // classic
			case 2: soundSet = 0; break;  // jazz
			case 3: soundSet = 2; break;  // synth
			case 4: soundSet = 3; break;  // boston
		}
		cap1 = atoi(argv[2]);
		cap2 = atoi(argv[3]);
		cap3 = atoi(argv[4]);
	} else {
		// Default to classic (soundSet = 1)
		soundSet = 1;
		cap1 = atoi(argv[1]);
		cap2 = atoi(argv[2]);
		cap3 = atoi(argv[3]);
	}
	
	const char *soundNames[] = {"Jazz", "Classic", "Synth", "Boston"};
	printf("=== Music Bottles v4 ===\n");
	printf("Sound set: %s\n", soundNames[soundSet]);
	printf("Cap weights: Cap1=%d, Cap2=%d, Cap3=%d\n", cap1, cap2, cap3);
	printf("Detection margin: +/-%d\n\n", WEIGHT_MARGIN);
	
	// Initialize hardware
	printf("Initializing scale...\n");
	initHX711();
	
	setupGPIO();
	initSound();
	setFiles(soundSet);  // Load selected music set
	
	// Auto tare on start (same as scaleTool.c)
	printf("Acquiring tare... ");
	fflush(stdout);
	tare = getCleanSample(150, 4);
	printf("Tare: %ld\n\n", tare);
	
	printf("Monitoring weight changes...\n");
	printf("(Weight delta shown relative to tared zero)\n\n");
	
	// Main loop - simplified weight monitoring (based on scaleTool.c)
	while (1) {
		long raw = getCleanSample(4, 4) - tare;
		smoothedWeight = smoothedWeight * 0.85 + raw * 0.15;
		
		long displayWeight = smoothedWeight / 100;
		long rawDisplay = raw / 100;
		
		// Clear line and display current weight
		printf("\r                                                              \r");
		printf("Delta: %5ld | Raw: %5ld | ", displayWeight, rawDisplay);
		
		// Show which cap is detected
		if (matchesCap(displayWeight, cap1)) {
			printf("Cap1(-%-4d)", cap1);
		} else if (matchesCap(displayWeight, cap2)) {
			printf("Cap2(-%-4d)", cap2);
		} else if (matchesCap(displayWeight, cap3)) {
			printf("Cap3(-%-4d)", cap3);
		} else if (displayWeight >= -WEIGHT_MARGIN && displayWeight <= WEIGHT_MARGIN) {
			printf("All caps on ");
		} else {
			printf("Unknown     ");
		}
		fflush(stdout);
		
		// Detect cap removal and trigger music
		detectCapAndTriggerMusic(displayWeight);
		
		// Handle audio fade
		handleFade();
		
		usleep(50000);  // 50ms delay (same as scaleTool.c)
	}
	
	return 0;
}
