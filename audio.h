
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <time.h>

void initSound();
void setFiles(int i);
void rewindFiles();
void fadeOut(int chan);
void handleFade();
void play();
void volume(int c, int v);
int getVolume(int chan);

// Birthday mode functions
void playBirthday();
void stopBirthday();
int isBirthdayPlaying();