#include "../doomdef.h"
#include "../p_local.h"
#include "../r_local.h"
#include "../sounds.h"
#include "platform.h"

#include <pspaudio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_RATE 44100
#define AUDIO_FRAMES 512
#define SFX_CHANNELS 16
#define MAX_SOUND_VOLUME 124
#define S_CLIPPING_DIST 1700
#define S_MAX_DIST (127 * S_CLIPPING_DIST)
#define S_CLOSE_DIST 200
#define S_ATTENUATOR (S_CLIPPING_DIST - S_CLOSE_DIST)
#define S_STEREO_SWING 96

#define SOUND_NAME(id) [id] = #id
static const char *const sound_names[NUMSFX] = {
	SOUND_NAME(sfx_punch),
	SOUND_NAME(sfx_spawn),
	SOUND_NAME(sfx_explode),
	SOUND_NAME(sfx_implod),
	SOUND_NAME(sfx_pistol),
	SOUND_NAME(sfx_shotgun),
	SOUND_NAME(sfx_plasma),
	SOUND_NAME(sfx_bfg),
	SOUND_NAME(sfx_sawup),
	SOUND_NAME(sfx_sawidle),
	SOUND_NAME(sfx_saw1),
	SOUND_NAME(sfx_saw2),
	SOUND_NAME(sfx_missile),
	SOUND_NAME(sfx_bfgexp),
	SOUND_NAME(sfx_pstart),
	SOUND_NAME(sfx_pstop),
	SOUND_NAME(sfx_doorup),
	SOUND_NAME(sfx_doordown),
	SOUND_NAME(sfx_secmove),
	SOUND_NAME(sfx_switch1),
	SOUND_NAME(sfx_switch2),
	SOUND_NAME(sfx_itemup),
	SOUND_NAME(sfx_sgcock),
	SOUND_NAME(sfx_oof),
	SOUND_NAME(sfx_telept),
	SOUND_NAME(sfx_noway),
	SOUND_NAME(sfx_sht2fire),
	SOUND_NAME(sfx_sht2load1),
	SOUND_NAME(sfx_sht2load2),
	SOUND_NAME(sfx_plrpain),
	SOUND_NAME(sfx_plrdie),
	SOUND_NAME(sfx_slop),
	SOUND_NAME(sfx_possit1),
	SOUND_NAME(sfx_possit2),
	SOUND_NAME(sfx_possit3),
	SOUND_NAME(sfx_posdie1),
	SOUND_NAME(sfx_posdie2),
	SOUND_NAME(sfx_posdie3),
	SOUND_NAME(sfx_posact),
	SOUND_NAME(sfx_dbpain1),
	SOUND_NAME(sfx_dbpain2),
	SOUND_NAME(sfx_dbact),
	SOUND_NAME(sfx_scratch),
	SOUND_NAME(sfx_impsit1),
	SOUND_NAME(sfx_impsit2),
	SOUND_NAME(sfx_impdth1),
	SOUND_NAME(sfx_impdth2),
	SOUND_NAME(sfx_impact),
	SOUND_NAME(sfx_sargsit),
	SOUND_NAME(sfx_sargatk),
	SOUND_NAME(sfx_sargdie),
	SOUND_NAME(sfx_bos1sit),
	SOUND_NAME(sfx_bos1die),
	SOUND_NAME(sfx_headsit),
	SOUND_NAME(sfx_headdie),
	SOUND_NAME(sfx_skullatk),
	SOUND_NAME(sfx_bos2sit),
	SOUND_NAME(sfx_bos2die),
	SOUND_NAME(sfx_pesit),
	SOUND_NAME(sfx_pepain),
	SOUND_NAME(sfx_pedie),
	SOUND_NAME(sfx_bspisit),
	SOUND_NAME(sfx_bspidie),
	SOUND_NAME(sfx_bspilift),
	SOUND_NAME(sfx_bspistomp),
	SOUND_NAME(sfx_fattatk),
	SOUND_NAME(sfx_fattsit),
	SOUND_NAME(sfx_fatthit),
	SOUND_NAME(sfx_fattdie),
	SOUND_NAME(sfx_bdmissile),
	SOUND_NAME(sfx_skelact),
	SOUND_NAME(sfx_tracer),
	SOUND_NAME(sfx_dart),
	SOUND_NAME(sfx_dartshoot),
	SOUND_NAME(sfx_cybsit),
	SOUND_NAME(sfx_cybdth),
	SOUND_NAME(sfx_cybhoof),
	SOUND_NAME(sfx_metal),
	SOUND_NAME(sfx_door2up),
	SOUND_NAME(sfx_door2dwn),
	SOUND_NAME(sfx_powerup),
	SOUND_NAME(sfx_laser),
	SOUND_NAME(sfx_electric),
	SOUND_NAME(sfx_thndrlow),
	SOUND_NAME(sfx_thndrhigh),
	SOUND_NAME(sfx_quake),
	SOUND_NAME(sfx_darthit),
	SOUND_NAME(sfx_rectact),
	SOUND_NAME(sfx_rectatk),
	SOUND_NAME(sfx_rectdie),
	SOUND_NAME(sfx_rectpain),
	SOUND_NAME(sfx_rectsit)
};
#undef SOUND_NAME

typedef struct {
	int16_t *samples;
	size_t sample_count;
	unsigned sample_rate;
} sound_sample_t;

typedef struct {
	int active;
	int handle;
	int sound_id;
	mobj_t *origin;
	float position;
	int volume;
	int pan;
	int looping;
	size_t loop_start;
} sound_voice_t;

sfxhnd_t sounds[NUMSFX];
float soundscale = 1.0f;

static sound_sample_t sound_samples[NUMSFX];
static sound_voice_t voices[SFX_CHANNELS];
static int next_handle = 1;
static int audio_channel = -1;
static SceLwMutexWorkarea audio_mutex;
static int audio_mutex_ready;
static int audio_thread = -1;
static int audio_paused;
static int music_volume = 100;
static int music_file = FILEHND_INVALID;
static int music_loop;
static int music_sequence;
static size_t music_file_size;
static int16_t __attribute__((aligned(64))) output_buffer[AUDIO_FRAMES * 2];
static int16_t music_buffer[AUDIO_FRAMES * 2];
static uint8_t music_adpcm_buffer[AUDIO_FRAMES];
static int music_predictor[2];
static int music_step[2] = { 127, 127 };
extern int from_menu;

static uint16_t ReadLE16(const uint8_t *data)
{
	return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t ReadLE32(const uint8_t *data)
{
	return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
		((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static int16_t ClampSample(int32_t sample)
{
	if (sample > INT16_MAX)
		return INT16_MAX;
	if (sample < INT16_MIN)
		return INT16_MIN;
	return (int16_t)sample;
}

static int16_t DecodeYamahaADPCMSample(unsigned code, int channel)
{
	static const int scale_table[8] = {
		230, 230, 230, 230, 307, 409, 512, 614
	};
	int difference = ((int)(code & 7) * 2 + 1) *
		music_step[channel] / 8;

	music_predictor[channel] += (code & 8) ? -difference : difference;
	if (music_predictor[channel] > INT16_MAX)
		music_predictor[channel] = INT16_MAX;
	else if (music_predictor[channel] < INT16_MIN)
		music_predictor[channel] = INT16_MIN;
	music_step[channel] =
		music_step[channel] * scale_table[code & 7] / 256;
	if (music_step[channel] < 127)
		music_step[channel] = 127;
	else if (music_step[channel] > 24576)
		music_step[channel] = 24576;

	return (int16_t)music_predictor[channel];
}

static int16_t *DecodeYamahaADPCM(const uint8_t *source, size_t length,
		size_t *sample_count)
{
	static const int scale_table[8] = { 230, 230, 230, 230, 307, 409, 512, 614 };
	size_t count = length * 2;
	int16_t *samples = malloc(count * sizeof(*samples));
	int predictor = 0;
	int step = 127;

	if (!samples)
		I_Error("could not allocate decoded sound samples");

	for (size_t i = 0; i < count; i++) {
		unsigned byte = source[i >> 1];
		unsigned code = (i & 1) ? byte >> 4 : byte & 0x0f;
		int difference = ((int)(code & 7) * 2 + 1) * step / 8;

		predictor += (code & 8) ? -difference : difference;
		if (predictor > INT16_MAX)
			predictor = INT16_MAX;
		else if (predictor < INT16_MIN)
			predictor = INT16_MIN;
		step = step * scale_table[code & 7] / 256;
		if (step < 127)
			step = 127;
		else if (step > 24576)
			step = 24576;
		samples[i] = (int16_t)predictor;
	}

	*sample_count = count;
	return samples;
}

static sound_sample_t DecodeWaveSound(const uint8_t *wave, size_t length,
		const char *name)
{
	sound_sample_t result = { 0 };
	const uint8_t *format_chunk = NULL;
	const uint8_t *sample_chunk = NULL;
	size_t format_size = 0;
	size_t sample_size = 0;
	uint16_t format, channels, bits_per_sample;
	unsigned sample_rate;

	if (length < 12 || memcmp(wave, "RIFF", 4) ||
			memcmp(wave + 8, "WAVE", 4))
		I_Error("invalid WAV header for %s", name);

	for (size_t offset = 12; offset + 8 <= length; ) {
		size_t chunk_size = ReadLE32(wave + offset + 4);
		size_t chunk_data = offset + 8;

		if (chunk_size > length - chunk_data)
			I_Error("truncated WAV chunk in %s", name);
		if (!memcmp(wave + offset, "fmt ", 4)) {
			format_chunk = wave + chunk_data;
			format_size = chunk_size;
		} else if (!memcmp(wave + offset, "data", 4)) {
			sample_chunk = wave + chunk_data;
			sample_size = chunk_size;
		}
		offset = chunk_data + chunk_size + (chunk_size & 1);
	}
	if (!format_chunk || format_size < 16 || !sample_chunk || !sample_size)
		I_Error("WAV is missing format or sample data: %s", name);

	format = ReadLE16(format_chunk);
	channels = ReadLE16(format_chunk + 2);
	sample_rate = ReadLE32(format_chunk + 4);
	bits_per_sample = ReadLE16(format_chunk + 14);
	if (channels != 1 || !sample_rate)
		I_Error("unsupported WAV layout for %s", name);

	if (format == 0x0014 && bits_per_sample == 4) {
		result.samples = DecodeYamahaADPCM(sample_chunk, sample_size,
			&result.sample_count);
	} else if (format == 1 && bits_per_sample == 16) {
		result.sample_count = sample_size / sizeof(int16_t);
		result.samples = malloc(result.sample_count * sizeof(int16_t));
		if (!result.samples)
			I_Error("could not allocate decoded sound samples");
		for (size_t i = 0; i < result.sample_count; i++)
			result.samples[i] = (int16_t)ReadLE16(sample_chunk + i * 2);
	} else if (format == 1 && bits_per_sample == 8) {
		result.sample_count = sample_size;
		result.samples = malloc(result.sample_count * sizeof(int16_t));
		if (!result.samples)
			I_Error("could not allocate decoded sound samples");
		for (size_t i = 0; i < result.sample_count; i++)
			result.samples[i] = ((int)sample_chunk[i] - 128) << 8;
	} else {
		I_Error("unsupported WAV encoding %u/%u for %s",
			(unsigned)format, (unsigned)bits_per_sample, name);
	}

	result.sample_rate = sample_rate;
	return result;
}

static void LoadSounds(void)
{
	char path[320];

	for (int id = 1; id <= sfx_rectsit; id++) {
		void *wave = NULL;
		ssize_t wave_length;

		if (!sound_names[id])
			I_Error("missing PSP sound name for index %d", id);
		if (snprintf(path, sizeof(path), "%s/sfx/%s.wav", fnpre,
				sound_names[id]) >= (int)sizeof(path))
			I_Error("PSP sound path is too long");
		wave_length = PSP_LoadFile(path, &wave);
		if (wave_length <= 0 || !wave)
			I_Error("could not load sound file %s", path);
		sound_samples[id] = DecodeWaveSound(wave, (size_t)wave_length,
			sound_names[id]);
		free(wave);
		sounds[id] = id;
	}
}

static void LockAudio(void)
{
	if (sceKernelLockLwMutex(&audio_mutex, 1, NULL) < 0)
		I_Error("could not lock PSP audio mutex");
}

static void UnlockAudio(void)
{
	if (sceKernelUnlockLwMutex(&audio_mutex, 1) < 0)
		I_Error("could not unlock PSP audio mutex");
}

static void StopVoice(sound_voice_t *voice)
{
	if (voice->origin && voice->origin->sfx_chn == voice->handle)
		voice->origin->sfx_chn = 0;
	memset(voice, 0, sizeof(*voice));
}

static void ReadMusicFrames(void)
{
	size_t offset = 0;
	const size_t buffer_size = sizeof(music_adpcm_buffer);

	memset(music_buffer, 0, sizeof(music_buffer));
	while (music_file >= 0 && offset < buffer_size) {
		SceSize request = (SceSize)(buffer_size - offset);
		ssize_t bytes = sceIoRead(music_file,
			music_adpcm_buffer + offset, request);

		if (bytes < 0)
			I_Error("error reading PSP music stream");
		if (bytes > 0) {
			for (ssize_t i = 0; i < bytes; i++) {
				uint8_t packed = music_adpcm_buffer[offset + (size_t)i];
				size_t frame = offset + (size_t)i;

				music_buffer[frame * 2] =
					DecodeYamahaADPCMSample(packed & 0x0f, 0);
				music_buffer[frame * 2 + 1] =
					DecodeYamahaADPCMSample(packed >> 4, 1);
			}
			offset += (size_t)bytes;
			continue;
		}
		if (!music_loop) {
			sceIoClose(music_file);
			music_file = -1;
			break;
		}
		if (sceIoLseek(music_file, 0, PSP_SEEK_SET) < 0)
			I_Error("could not rewind PSP music stream");
		if (offset == 0 && music_file_size == 0)
			break;
	}
}

static int AudioThread(SceSize args, void *argp)
{
	(void)args;
	(void)argp;

	for (;;) {
		LockAudio();
		if (audio_paused) {
			memset(output_buffer, 0, sizeof(output_buffer));
		} else {
			ReadMusicFrames();
			for (int frame = 0; frame < AUDIO_FRAMES; frame++) {
				int32_t left = (int32_t)music_buffer[frame * 2] *
					music_volume / 100;
				int32_t right = (int32_t)music_buffer[frame * 2 + 1] *
					music_volume / 100;

				for (int channel = 0; channel < SFX_CHANNELS; channel++) {
					sound_voice_t *voice = &voices[channel];
					sound_sample_t *sample;
					size_t sample_index;
					int32_t value;

					if (!voice->active)
						continue;
					sample = &sound_samples[voice->sound_id];
					sample_index = (size_t)voice->position;
					if (sample_index >= sample->sample_count) {
						if (voice->looping && sample->sample_count) {
							voice->position = (float)voice->loop_start;
							sample_index = voice->loop_start;
						} else {
							StopVoice(voice);
							continue;
						}
					}
					value = (int32_t)(sample->samples[sample_index] *
						((float)voice->volume * soundscale /
							(float)MAX_SOUND_VOLUME));
					left += value * (255 - voice->pan) / 255;
					right += value * voice->pan / 255;
					voice->position +=
						(float)sample->sample_rate / (float)AUDIO_RATE;
				}
				output_buffer[frame * 2] = ClampSample(left);
				output_buffer[frame * 2 + 1] = ClampSample(right);
			}
		}
		UnlockAudio();

		if (sceAudioOutputBlocking(audio_channel, PSP_AUDIO_VOLUME_MAX,
				output_buffer) < 0)
			I_Error("PSP audio output failed");
	}
	return 0;
}

void S_RemoveOrigin(mobj_t *origin)
{
	if (!origin || !audio_mutex_ready)
		return;
	LockAudio();
	for (int i = 0; i < SFX_CHANNELS; i++)
		if (voices[i].active && voices[i].origin == origin)
			StopVoice(&voices[i]);
	UnlockAudio();
}

void S_ResetSound(void)
{
	if (!audio_mutex_ready)
		return;
	LockAudio();
	for (int i = 0; i < SFX_CHANNELS; i++)
		StopVoice(&voices[i]);
	UnlockAudio();
}

void S_UpdateSounds(void)
{
	if (!cameratarget || !audio_mutex_ready)
		return;
	LockAudio();
	for (int i = 0; i < SFX_CHANNELS; i++) {
		sound_voice_t *voice = &voices[i];
		int volume, pan;

		if (!voice->active || !voice->origin ||
				voice->origin == cameratarget)
			continue;
		if (!S_AdjustSoundParams(cameratarget, voice->origin,
				&volume, &pan)) {
			StopVoice(voice);
			continue;
		}
		voice->volume = volume;
		voice->pan = pan;
	}
	UnlockAudio();
}

void S_PauseSound(void)
{
	if (!audio_mutex_ready)
		return;
	LockAudio();
	audio_paused = 1;
	UnlockAudio();
}

void S_ResumeSound(void)
{
	if (!audio_mutex_ready)
		return;
	LockAudio();
	audio_paused = 0;
	UnlockAudio();
}

void S_StopSound(mobj_t *origin, int seqnum)
{
	if (!audio_mutex_ready)
		return;
	LockAudio();
	for (int i = 0; i < SFX_CHANNELS; i++) {
		sound_voice_t *voice = &voices[i];
		if (voice->active && (!origin || voice->origin == origin) &&
				(!seqnum || voice->sound_id == seqnum ||
				 voice->handle == seqnum))
			StopVoice(voice);
	}
	UnlockAudio();
}

void S_StopAll(void)
{
	S_ResetSound();
	S_StopMusic();
}

int S_SoundStatus(int seqnum)
{
	int active = 0;

	if (!audio_mutex_ready)
		return 0;
	LockAudio();
	for (int i = 0; i < SFX_CHANNELS; i++)
		if (voices[i].active && voices[i].handle == seqnum) {
			active = 1;
			break;
		}
	UnlockAudio();
	return active;
}

int S_StartSound(mobj_t *origin, int sound_id)
{
	int volume, pan, selected = -1, handle;
	sound_sample_t *sample;

	if (disabledrawing || sound_id <= sfx_None || sound_id > sfx_rectsit ||
			!sound_samples[sound_id].samples)
		return -1;
	if (origin && origin != cameratarget) {
		if (!cameratarget || !S_AdjustSoundParams(cameratarget, origin,
				&volume, &pan))
			return -1;
	} else {
		volume = MAX_SOUND_VOLUME;
		pan = 128;
	}

	LockAudio();
	for (int i = 0; i < SFX_CHANNELS; i++) {
		if (voices[i].active && voices[i].origin == origin &&
				voices[i].sound_id == sound_id &&
				sound_id == sfx_electric) {
			handle = voices[i].handle;
			UnlockAudio();
			return handle;
		}
		if (!voices[i].active && selected < 0)
			selected = i;
	}
	if (selected < 0) {
		selected = 0;
		for (int i = 1; i < SFX_CHANNELS; i++)
			if (voices[i].volume < voices[selected].volume)
				selected = i;
		StopVoice(&voices[selected]);
	}

	handle = next_handle++;
	if (next_handle <= 0)
		next_handle = 1;
	sample = &sound_samples[sound_id];
	voices[selected].active = 1;
	voices[selected].handle = handle;
	voices[selected].sound_id = sound_id;
	voices[selected].origin = origin;
	voices[selected].position = 0.0f;
	voices[selected].volume = volume;
	voices[selected].pan = pan;
	voices[selected].looping = sound_id == sfx_electric;
	voices[selected].loop_start =
		voices[selected].looping && sample->sample_count > 1713 ? 1713 : 0;
	if (origin)
		origin->sfx_chn = handle;
	UnlockAudio();
	return handle;
}

int S_AdjustSoundParams(mobj_t *listener, mobj_t *origin, int *volume,
		int *pan)
{
	fixed_t approximate_distance;
	angle_t angle;
	int calculated_volume;
	int calculated_pan = 128;

	if (!listener || !origin || !volume || !pan)
		return 0;
	approximate_distance = P_AproxDistance(listener->x - origin->x,
		listener->y - origin->y) >> FRACBITS;
	if (approximate_distance > S_CLIPPING_DIST)
		return 0;
	if (listener->x != origin->x || listener->y != origin->y) {
		angle = R_PointToAngle2(listener->x, listener->y,
			origin->x, origin->y);
		if (angle <= listener->angle)
			angle += 0xffffffff;
		angle -= listener->angle;
		calculated_pan -=
			(finesine[angle >> ANGLETOFINESHIFT] * S_STEREO_SWING) >>
			FRACBITS;
	}
	if (approximate_distance < S_CLOSE_DIST) {
		calculated_volume = MAX_SOUND_VOLUME;
	} else {
		approximate_distance = -approximate_distance;
		calculated_volume = (((approximate_distance << 7) -
			approximate_distance) + S_MAX_DIST) / S_ATTENUATOR;
	}
	if (calculated_volume > MAX_SOUND_VOLUME)
		calculated_volume = MAX_SOUND_VOLUME;
	*volume = calculated_volume;
	*pan = calculated_pan;
	return calculated_volume > 0;
}

void S_Init(void)
{
	memset(sounds, 0, sizeof(sounds));
	memset(sound_samples, 0, sizeof(sound_samples));
	memset(voices, 0, sizeof(voices));
	LoadSounds();
	audio_channel = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL,
		AUDIO_FRAMES, PSP_AUDIO_FORMAT_STEREO);
	if (audio_channel < 0)
		I_Error("could not reserve PSP audio output channel");
	if (sceKernelCreateLwMutex(&audio_mutex, "DoomAudio", 0, 0, NULL) < 0)
		I_Error("could not create PSP audio mutex");
	audio_mutex_ready = 1;
	S_SetSoundVolume(menu_settings.SfxVolume);
	S_SetMusicVolume(menu_settings.MusVolume);
	audio_thread = sceKernelCreateThread("DoomAudio", AudioThread,
		0x18, 0x10000, PSP_THREAD_ATTR_USER, NULL);
	if (audio_thread < 0)
		I_Error("could not create PSP audio thread");
	if (sceKernelStartThread(audio_thread, 0, NULL) < 0)
		I_Error("could not start PSP audio thread");
}

void S_SetSoundVolume(int volume)
{
	if (volume < 0)
		volume = 0;
	if (volume > 100)
		volume = 100;
	if (audio_mutex_ready)
		LockAudio();
	soundscale = (float)volume / 100.0f;
	if (audio_mutex_ready)
		UnlockAudio();
}

void S_SetMusicVolume(int volume)
{
	if (volume < 0)
		volume = 0;
	if (volume > 100)
		volume = 100;
	if (audio_mutex_ready)
		LockAudio();
	music_volume = volume;
	if (audio_mutex_ready)
		UnlockAudio();
}

static const char *MusicName(int sequence)
{
	static const char *const names[] = {
		"musamb01", "musamb02", "musamb03", "musamb04", "musamb05",
		"musamb06", "musamb07", "musamb08", "musamb09", "musamb10",
		"musamb11", "musamb12", "musamb13", "musamb14", "musamb15",
		"musamb16", "musamb17", "musamb18", "musamb19", "musamb20",
		"musfinal", "musdone", "musintro", "mustitle"
	};

	if (sequence < 93 || sequence > 116)
		return NULL;
	return names[sequence - 93];
}

void S_StartMusic(int mus_seq)
{
	char path[320];
	const char *name;
	int looping = 1;
	int file;
	ssize_t length;

	if (disabledrawing)
		return;
	if (!from_menu && gamemap > 40 && !(mus_seq >= 113 && mus_seq <= 116)) {
		if (snprintf(path, sizeof(path), "%s/mus/e1m%d.adpcm", fnpre,
				gamemap - 40) >= (int)sizeof(path))
			I_Error("PSP music path is too long");
	} else {
		name = MusicName(mus_seq);
		if (!name)
			I_Error("unknown music sequence %d", mus_seq);
		if (snprintf(path, sizeof(path), "%s/mus/%s.adpcm", fnpre,
				name) >= (int)sizeof(path))
			I_Error("PSP music path is too long");
		if (mus_seq == 114 || mus_seq == 115)
			looping = 0;
	}

	file = sceIoOpen(path, PSP_O_RDONLY, 0);
	if (file < 0)
		I_Error("could not open music file %s", path);
	length = sceIoLseek(file, 0, PSP_SEEK_END);
	if (length <= 0 || sceIoLseek(file, 0, PSP_SEEK_SET) < 0) {
		sceIoClose(file);
		I_Error("invalid Yamaha ADPCM music file %s", path);
	}

	LockAudio();
	if (music_file >= 0)
		sceIoClose(music_file);
	music_file = file;
	music_file_size = (size_t)length;
	music_loop = looping;
	music_sequence = mus_seq;
	music_predictor[0] = 0;
	music_predictor[1] = 0;
	music_step[0] = 127;
	music_step[1] = 127;
	UnlockAudio();
}

void S_StopMusic(void)
{
	if (audio_mutex_ready) {
		LockAudio();
		if (music_file >= 0)
			sceIoClose(music_file);
		music_file = -1;
		music_file_size = 0;
		music_sequence = 0;
		UnlockAudio();
	}
}

int rumble_patterns[NUM_RUMBLE];

void I_InitRumble(i_rumble_pak_t rumblepak)
{
	(void)rumblepak;
	memset(rumble_patterns, 0, sizeof(rumble_patterns));
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

void P_StartElectricLoop(void)
{
	S_StartSound(NULL, sfx_electric);
}

void P_StopElectricLoop(void)
{
	S_StopSound(NULL, sfx_electric);
}
