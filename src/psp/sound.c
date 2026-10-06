#include "../doomdef.h"
#include "../sounds.h"

sfxhnd_t sounds[NUMSFX];
float soundscale = 1.0f;

void S_RemoveOrigin(mobj_t *origin)
{
	(void)origin;
}

void S_ResetSound(void) {}
void S_UpdateSounds(void) {}
void S_PauseSound(void) {}
void S_ResumeSound(void) {}
void S_StopSound(mobj_t *origin, int seqnum)
{
	(void)origin;
	(void)seqnum;
}
void S_StopAll(void) { S_StopMusic(); }
int S_SoundStatus(int seqnum)
{
	(void)seqnum;
	return 0;
}
int S_StartSound(mobj_t *origin, int sound_id)
{
	(void)origin;
	(void)sound_id;
	return -1;
}
int S_AdjustSoundParams(mobj_t *listener, mobj_t *origin, int *volume, int *pan)
{
	(void)listener;
	(void)origin;
	if (volume)
		*volume = 0;
	if (pan)
		*pan = 128;
	return 1;
}

void S_Init(void)
{
	memset(sounds, 0, sizeof(sounds));
	S_SetSoundVolume(menu_settings.SfxVolume);
}

void S_SetSoundVolume(int volume)
{
	soundscale = (float)volume / 100.0f;
}

void S_SetMusicVolume(int volume)
{
	(void)volume;
}

void S_StartMusic(int mus_seq)
{
	(void)mus_seq;
}

void S_StopMusic(void) {}

int rumble_patterns[NUM_RUMBLE];

void I_InitRumble(i_rumble_pak_t rumblepak)
{
	(void)rumblepak;
}

int I_GetDamageRumble(int damage)
{
	(void)damage;
	return 0;
}

void I_Rumble(uint32_t packet)
{
	(void)packet;
}

void P_StartElectricLoop(void) {}
void P_StopElectricLoop(void) {}
