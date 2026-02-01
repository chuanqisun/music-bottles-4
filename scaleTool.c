/**

Music Bottles v4 by Tal Achituv

Measuring tool, for scale calibration

*/

#include "hx711.h"
#include <unistd.h>



int main(int argc, char **argv) {
	int i;
	printf("Initializing Scale...\n");


	initHX711();
	printf("Acquiring Tare ... ");
	fflush(stdout);
	long tare = getCleanSample(150,4);
	printf("Tare: (%d)\n",tare);
	
	long sample = 0;
	while (1) {
		long raw = getCleanSample(4,4) - tare;
	    sample = sample * 0.85 +  raw * 0.15;
		if (sample>0) {
			printf("\r                       \r ");
		} else {
			printf("\r                       \r");
		}
		printf("%.1f\t%d\t",(sample)/100.0,raw/100);
		fflush(stdout);
		usleep(50000);
	}

}
