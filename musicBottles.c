/**

Music Bottles v4 by Tal Achituv

Based on code by Tomer Weller, Jasmin Rubinovitz, as well as the general idea of previous Music Bottles versions

*/

#include "audio.h"
#include "hx711.h"
#include "minimal_gpio.c"

//Buttons connections
#define RETARE_PIN  26
#define MUSIC1_PIN  19
#define MUSIC2_PIN  13
#define MUSIC3_PIN  6
#define MUSIC4_PIN  5

#define BOT1_PIN 18
#define CAP1_PIN 17
#define BOT2_PIN 27
#define CAP2_PIN 22
#define BOT3_PIN 23
#define CAP3_PIN 24

#define RETARE_BTN (GPIO_IN0  & (1 << RETARE_PIN))
#define MUSIC1_BTN (GPIO_IN0  & (1 << MUSIC1_PIN))
#define MUSIC2_BTN (GPIO_IN0  & (1 << MUSIC2_PIN))
#define MUSIC3_BTN (GPIO_IN0  & (1 << MUSIC3_PIN))
#define MUSIC4_BTN (GPIO_IN0  & (1 << MUSIC4_PIN))

//TODO: run analysis to determine correct stable thresh for this specific load cell and weights. 50 is a good educated guess meanwhile
#define STABLE_THRESH 43

long tare =0;

int bot1;
int cap1;
int bot2;
int cap2;
int bot3;
int cap3;

void setBottleStates(int a, int b, int c) {
	//set output pins for Arduino to handle light
	gpioWrite(BOT1_PIN,a<2);
	gpioWrite(CAP1_PIN,a<1);
	gpioWrite(BOT2_PIN,b<2);
	gpioWrite(CAP2_PIN,b<1);
	gpioWrite(BOT3_PIN,c<2);
	gpioWrite(CAP3_PIN,c<1);
	
	//handle sounds (max volume = 128, must be above 100 to play due to fade-out logic in audio.c)
	(a==1)?volume(0,105):fadeOut(0);
	(b==1)?volume(1,105):fadeOut(1);
	(c==1)?volume(2,105):fadeOut(2);

	//restart (and pause) current stream if all bottles are off
	if (a==2 && b==2 && c==2) {
		rewindFiles();
	}

}

int knownState = 0;

void handleWeightChangeAbsolute(int weight) {
	int i;
	int found = 0;
	printf("Handling Absolute Weight: %d", weight);

	int bestDistance = 9999;
	for (i=0; i<27; i++) {
		int a = i%3;
		int b = (i/3)%3;
		int c = (i/9)%3; 

		// for each bottle: 0 is both bottle and cap are on, 1 is only the bottle is on, and 2 is both bottle and cap are missing...
		int weightTarget = 0;
		if (a==1) { weightTarget -= cap1; }
		else if (a==2) { weightTarget -= (bot1 + cap1); }

		if (b==1) { weightTarget -= cap2; }
		else if (b==2) { weightTarget -= (bot2 + cap2); }

		if (c==1) { weightTarget -= cap3; }
		else if (c==2) { weightTarget -= (bot3 + cap3); }

		int distance = abs(weightTarget - weight);
			//TODO: make STABLE_THRESH configurable from runtime
		if (distance<STABLE_THRESH) {
		  
		  printf("%d : %d%d%d [%d] {%d} <<< MATCH",i,a,b,c,weightTarget,distance);
		  found++;
		  if (distance<bestDistance) {
		    setBottleStates(a,b,c);
		    bestDistance = distance;      
		  }
		  printf("\n");
		} 
	}
	if (found==1) {
		knownState=1;
	} else {
		knownState=0;
		if (found==0) {
			printf("!!!!!!!!! FOUND NO MATCHING SETTINGS !!!!!!!!!!!!\n");
		} else if (found>1) {
			printf("!!!!!!!!! FOUND MORE THAN ONE MATCHING SETTINGS !!!!!!!!!!!!\n");
		}
	}

}

int isStable = 0;
long lastSample;
void handleScale() {
	//TODO: MAKE SAMPLE COUNT AND SPREAD CONFIGURABLE FROM RUNTIME
	long sample = (getCleanSample(2,5) - tare)/100;

	long distance = abs(lastSample-sample);

	printf("\r                                  \rgot sample: %d\t%d",sample,distance);
	fflush(stdout);

	//TODO: MAKE DISTANCE MAX/MIN PARAMS CONFIGURABLE FROM RUNTIME
	if (distance > 20) { //DISTANCE should be smaller than 1/2 stable_threshold
		isStable = 0;
	}

	if (distance < 10) {
	    if (isStable < 1 ) {
			isStable++;
		} else if (isStable==1) {	
			handleWeightChangeAbsolute(sample);
			isStable++;
		}	
	}

	lastSample = sample;
}

void handleButtons() {
	//printf("%d %d %d %d %d\n",RETARE_BTN,MUSIC1_BTN,MUSIC2_BTN,MUSIC3_BTN,MUSIC4_BTN);
	if (RETARE_BTN == 0) {
		printf("RE-TARE!");
		tare = getCleanSample(35,4);
	}

	if (MUSIC1_BTN == 0) {
		setFiles(0);
		lastSample+=9999;
	} else if (MUSIC2_BTN == 0) {
		setFiles(1);
		lastSample+=9999;
	} else if (MUSIC3_BTN == 0) {
		setFiles(2);
		lastSample+=9999;
	} else if (MUSIC4_BTN == 0) {
		setFiles(3);
		lastSample+=9999;
	} 

}

void setupButtons() {
	if (gpioInitialise() < 0) exit(-1);

	gpioSetMode(RETARE_PIN,PI_INPUT);
	gpioSetPullUpDown(RETARE_PIN,PI_PUD_UP);

	gpioSetMode(MUSIC1_PIN,PI_INPUT);
	gpioSetPullUpDown(MUSIC1_PIN,PI_PUD_UP);
	gpioSetMode(MUSIC2_PIN,PI_INPUT);
	gpioSetPullUpDown(MUSIC2_PIN,PI_PUD_UP);
	gpioSetMode(MUSIC3_PIN,PI_INPUT);
	gpioSetPullUpDown(MUSIC3_PIN,PI_PUD_UP);
	gpioSetMode(MUSIC4_PIN,PI_INPUT);
	gpioSetPullUpDown(MUSIC4_PIN,PI_PUD_UP);

	//output pins to Arduino
	gpioSetMode(BOT1_PIN,PI_OUTPUT);
	gpioSetMode(CAP1_PIN,PI_OUTPUT);
	gpioSetMode(BOT2_PIN,PI_OUTPUT);
	gpioSetMode(CAP2_PIN,PI_OUTPUT);
	gpioSetMode(BOT3_PIN,PI_OUTPUT);
	gpioSetMode(CAP3_PIN,PI_OUTPUT);
}



int main(int argc, char **argv) {
	int i;
	initHX711();

	if (argc!=7 && argc!=8) {
		printf("Usage: musicBottles bot1 cap2 bot2 cap2 bot3 cap3 [tare]\n(where bot/cap are integer weights of the respective objects, and the optional tare sets a custom tare instead of performing a tare at the beginning)\n");
		return -1;
	}

	bot1 = atoi(argv[1]);
	cap1 = atoi(argv[2]);
	bot2 = atoi(argv[3]);
	cap2 = atoi(argv[4]);
	bot3 = atoi(argv[5]);
	cap3 = atoi(argv[6]);

	if (argc==8) {
		tare = atoi(argv[7]);
	} else {
		tare = getCleanSample(150,4);
	}

	// float sps = speedTest();
	// printf("Test shows: %f SPS\n",sps);
	
	
	setupButtons();
	initSound();
	
	while (1) {
		handleScale();
		handleButtons();
		handleFade(); //in audio.c
	}

}
