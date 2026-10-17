#include "audio.h"

// File paths - classic tracks and birthday only
const char *CLAS1_PATH = "music-files/classic1.wav";
const char *CLAS2_PATH = "music-files/classic2.wav";
const char *CLAS3_PATH = "music-files/classic3.wav";
const char *BIRTHDAY_PATH = "music-files/birthday.wav";
const char *SONGBIRD_PATH = "music-files/songbird.wav";

// Wave chunks
Mix_Chunk *CLAS1 = NULL;
Mix_Chunk *CLAS2 = NULL;
Mix_Chunk *CLAS3 = NULL;
Mix_Chunk *BIRTHDAY = NULL;
Mix_Chunk *SONGBIRD = NULL;
int birthdayPlaying = 0;
int keepAlivePlaying = 0;

Mix_Chunk *chanA = NULL;
Mix_Chunk *chanB = NULL;
Mix_Chunk *chanC = NULL;

#define FADE_TARGET_VOL 105
#define FADE_IN_MS 400
#define FADE_OUT_MS 200

#define KEEPALIVE_CHANNEL 4
#define KEEPALIVE_VOL 1

// Internal volume and target tracking for smooth time-based fading
static float channelVol[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static float channelTarget[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static Uint32 lastFadeTicks = 0;

void initSound()
{

	printf("Initializing Audio\n");

	// Initialize SDL.
	if (SDL_Init(SDL_INIT_AUDIO) < 0)
	{
		printf("Error initializing SDL\n");
		return;
	}

	Mix_AllocateChannels(5); // 3 for music tracks + 1 for birthday + 1 for keep-alive

	// Initialize SDL_mixer
	if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096) == -1)
	{
		printf("Error initializing MIXER: %s\n", Mix_GetError());
		return;
	}

	volume(0, 0);
	volume(1, 0);
	volume(2, 0);
	Mix_Volume(3, 0);									// Birthday channel
	Mix_Volume(KEEPALIVE_CHANNEL, 0); // Keep-alive channel
	channelVol[3] = 0.0f;
	channelTarget[3] = 0.0f;
	lastFadeTicks = SDL_GetTicks();

	printf("Loading sounds to memory...\n");

	CLAS1 = Mix_LoadWAV(CLAS1_PATH);
	if (CLAS1 == NULL)
	{
		printf("Error loading classic1.wav: %s\n", Mix_GetError());
		return;
	}
	CLAS2 = Mix_LoadWAV(CLAS2_PATH);
	if (CLAS2 == NULL)
	{
		printf("Error loading classic2.wav: %s\n", Mix_GetError());
		return;
	}
	CLAS3 = Mix_LoadWAV(CLAS3_PATH);
	if (CLAS3 == NULL)
	{
		printf("Error loading classic3.wav: %s\n", Mix_GetError());
		return;
	}
	printf("Loaded 'Classic' tracks.\n");

	BIRTHDAY = Mix_LoadWAV(BIRTHDAY_PATH);
	if (BIRTHDAY == NULL)
	{
		printf("Warning: Could not load birthday.wav: %s\n", Mix_GetError());
	}
	else
	{
		printf("Loaded 'Birthday'.\n");
	}

	SONGBIRD = Mix_LoadWAV(SONGBIRD_PATH);
	if (SONGBIRD == NULL)
	{
		printf("Warning: Could not load songbird.wav: %s\n", Mix_GetError());
	}
	else
	{
		printf("Loaded 'Songbird' (keep-alive).\n");
	}

	setFiles(); // Initialize classic tracks
}

int isPlaying = 0;

void rewindFiles()
{
	setFiles();
}

// Set files to classic tracks (only supported sound set)
void setFiles()
{
	Mix_HaltChannel(0);
	Mix_HaltChannel(1);
	Mix_HaltChannel(2);
	isPlaying = 0;
	chanA = CLAS1;
	chanB = CLAS2;
	chanC = CLAS3;
	for (int i = 0; i < 3; i++)
	{
		channelVol[i] = 0.0f;
		channelTarget[i] = 0.0f;
		Mix_Volume(i, 0);
	}
}

void fadeIn(int chan)
{
	if (chan < 0 || chan >= 4)
		return;
	channelTarget[chan] = (float)FADE_TARGET_VOL;

	// Start playback if not already playing
	if (isPlaying == 0)
	{
		play();
	}
}

void fadeOut(int chan)
{
	if (chan < 0 || chan >= 4)
		return;
	channelTarget[chan] = 0.0f;
}

void handleFade()
{
	Uint32 now = SDL_GetTicks();
	if (lastFadeTicks == 0)
	{
		lastFadeTicks = now;
		return;
	}

	Uint32 dt = now - lastFadeTicks;
	lastFadeTicks = now;

	if (dt == 0)
		return;
	if (dt > 500)
		dt = 500; // Cap dt to avoid huge jumps if execution was suspended

	float inStep = ((float)FADE_TARGET_VOL / (float)FADE_IN_MS) * (float)dt;
	float outStep = ((float)FADE_TARGET_VOL / (float)FADE_OUT_MS) * (float)dt;

	for (int i = 0; i < 4; i++)
	{
		if (channelVol[i] < channelTarget[i])
		{
			// Fading in
			channelVol[i] += inStep;
			if (channelVol[i] >= channelTarget[i])
			{
				channelVol[i] = channelTarget[i];
			}
			Mix_Volume(i, (int)(channelVol[i] + 0.5f));
		}
		else if (channelVol[i] > channelTarget[i])
		{
			// Fading out
			channelVol[i] -= outStep;
			if (channelVol[i] <= channelTarget[i])
			{
				channelVol[i] = channelTarget[i];
			}
			Mix_Volume(i, (int)(channelVol[i] + 0.5f));

			// Stop birthday channel when fade completes
			if (i == 3 && channelVol[3] <= 0.0f && birthdayPlaying)
			{
				birthdayPlaying = 0;
				Mix_HaltChannel(3);
			}
		}
	}

	// Rewind music tracks (0, 1, 2) once they have all completely faded to silence
	if (isPlaying &&
			channelVol[0] <= 0.0f && channelTarget[0] <= 0.0f &&
			channelVol[1] <= 0.0f && channelTarget[1] <= 0.0f &&
			channelVol[2] <= 0.0f && channelTarget[2] <= 0.0f)
	{
		rewindFiles();
	}
}

void volume(int chan, int vol)
{
	if (chan < 0 || chan >= 4)
		return;
	if (vol > 128)
		vol = 128;
	if (vol < 0)
		vol = 0;

	channelVol[chan] = (float)vol;
	channelTarget[chan] = (float)vol;
	Mix_Volume(chan, vol);

	if (vol >= 100 && isPlaying == 0)
	{
		play();
	}
}

int getVolume(int chan)
{
	return Mix_Volume(chan, -1);
}

// Debug functions
void playDebugSound()
{
	const char *DEBUG_PATH = "music-files/songbird.wav";
	Mix_Chunk *debugChunk = Mix_LoadWAV(DEBUG_PATH);
	if (debugChunk == NULL)
	{
		printf("Error loading debug sound %s: %s\n", DEBUG_PATH, Mix_GetError());
		return;
	}

	printf("DEBUG: Playing %s for 10 seconds...\n", DEBUG_PATH);
	Mix_Volume(0, 105);
	if (Mix_PlayChannel(0, debugChunk, 0) == -1)
	{
		printf("Error playing debug sound: %s\n", Mix_GetError());
	}
	else
	{
		SDL_Delay(10000);
		Mix_HaltChannel(0);
	}
	Mix_Volume(0, 0);
	Mix_FreeChunk(debugChunk);
	printf("DEBUG: Done.\n");
}

// Birthday mode functions
void playBirthday()
{
	if (BIRTHDAY == NULL)
		return;

	if (!birthdayPlaying)
	{
		birthdayPlaying = 1;
		channelVol[3] = 0.0f;
		Mix_Volume(3, 0);
		if (Mix_PlayChannel(3, BIRTHDAY, -1) == -1)
		{
			printf("Error playing BIRTHDAY: %s\n", Mix_GetError());
			birthdayPlaying = 0;
			return;
		}
	}
	fadeIn(3);
}

void fadeOutBirthday()
{
	if (birthdayPlaying)
	{
		fadeOut(3);
	}
}

void stopBirthday()
{
	if (birthdayPlaying)
	{
		birthdayPlaying = 0;
		channelVol[3] = 0.0f;
		channelTarget[3] = 0.0f;
		Mix_HaltChannel(3);
		Mix_Volume(3, 0);
	}
}

int isBirthdayPlaying()
{
	return birthdayPlaying;
}

// Keep-alive functions (plays songbird at inaudible volume to prevent Bluetooth speaker auto-sleep)
void startKeepAlive()
{
	if (SONGBIRD == NULL)
		return;

	if (!keepAlivePlaying)
	{
		keepAlivePlaying = 1;
		Mix_Volume(KEEPALIVE_CHANNEL, KEEPALIVE_VOL);
		if (Mix_PlayChannel(KEEPALIVE_CHANNEL, SONGBIRD, -1) == -1)
		{
			printf("Error playing keep-alive: %s\n", Mix_GetError());
			keepAlivePlaying = 0;
			return;
		}
	}
}

void stopKeepAlive()
{
	if (keepAlivePlaying)
	{
		keepAlivePlaying = 0;
		Mix_HaltChannel(KEEPALIVE_CHANNEL);
	}
}

int isKeepAlivePlaying()
{
	return keepAlivePlaying;
}

void play()
{

	if (isPlaying == 0)
	{
		isPlaying = 1;

		if (Mix_PlayChannel(0, chanA, -1) == -1)
		{
			printf("Error playing CHAN_A: %s\n", Mix_GetError());
			return;
		}

		if (Mix_PlayChannel(1, chanB, -1) == -1)
		{
			printf("Error playing CHAN_B: %s\n", Mix_GetError());
			return;
		}

		if (Mix_PlayChannel(2, chanC, -1) == -1)
		{
			printf("Error playing CHAN_C: %s\n", Mix_GetError());
			return;
		}
	}
}
