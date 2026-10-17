
#ifndef AUDIO_H
#define AUDIO_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <time.h>

void initSound();
void setFiles();
void rewindFiles();
void fadeIn(int chan);
void fadeOut(int chan);
void handleFade();
void play();
void volume(int c, int v);
int getVolume(int chan);

// Debug functions
void playDebugSound();

// Keep-alive functions
void startKeepAlive();
void stopKeepAlive();
int isKeepAlivePlaying();

#endif