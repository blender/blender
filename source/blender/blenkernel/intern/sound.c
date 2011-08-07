/** \file blender/blenkernel/intern/sound.c
 *  \ingroup bke
 */
/**
 * sound.c (mar-2001 nzc)
 *
 * $Id$
 */

#include <string.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"

#include "DNA_anim_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_screen_types.h"
#include "DNA_sound_types.h"

#ifdef WITH_AUDASPACE
#  include "AUD_C-API.h"
#endif

#include "BKE_utildefines.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_sound.h"
#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_packedFile.h"
#include "BKE_animsys.h"
#include "BKE_sequencer.h"

// evil global ;-)
static int sound_cfra;

struct bSound* sound_new_file(struct Main *bmain, const char *filename)
{
	bSound* sound = NULL;

	char str[FILE_MAX];
	char *path;

	int len;

	strcpy(str, filename);

	path = /*bmain ? bmain->name :*/ G.main->name;

	BLI_path_abs(str, path);

	len = strlen(filename);
	while(len > 0 && filename[len-1] != '/' && filename[len-1] != '\\')
		len--;

	sound = alloc_libblock(&bmain->sound, ID_SO, filename+len);
	BLI_strncpy(sound->name, filename, FILE_MAX);
// XXX unused currently	sound->type = SOUND_TYPE_FILE;

	sound_load(bmain, sound);

	if(!sound->playback_handle)
	{
		free_libblock(&bmain->sound, sound);
		sound = NULL;
	}

	return sound;
}

void sound_free(struct bSound* sound)
{
	if (sound->packedfile)
	{
		freePackedFile(sound->packedfile);
		sound->packedfile = NULL;
	}

#ifdef WITH_AUDASPACE
	if(sound->handle)
	{
		AUD_unload(sound->handle);
		sound->handle = NULL;
		sound->playback_handle = NULL;
	}

	if(sound->cache)
	{
		AUD_unload(sound->cache);
		sound->cache = NULL;
	}
#endif // WITH_AUDASPACE
}

#ifdef WITH_AUDASPACE

static int force_device = -1;

#ifdef WITH_JACK
static void sound_sync_callback(void* data, int mode, float time)
{
	struct Main* bmain = (struct Main*)data;
	struct Scene* scene;

	scene = bmain->scene.first;
	while(scene)
	{
		if(scene->audio.flag & AUDIO_SYNC)
		{
			if(mode)
				sound_play_scene(scene);
			else
				sound_stop_scene(scene);
			if(scene->sound_scene_handle)
				AUD_seek(scene->sound_scene_handle, time);
		}
		scene = scene->id.next;
	}
}
#endif

int sound_define_from_str(const char *str)
{
	if (BLI_strcaseeq(str, "NULL"))
		return AUD_NULL_DEVICE;
	if (BLI_strcaseeq(str, "SDL"))
		return AUD_SDL_DEVICE;
	if (BLI_strcaseeq(str, "OPENAL"))
		return AUD_OPENAL_DEVICE;
	if (BLI_strcaseeq(str, "JACK"))
		return AUD_JACK_DEVICE;

	return -1;
}

void sound_force_device(int device)
{
	force_device = device;
}

void sound_init_once(void)
{
	AUD_initOnce();
}

void sound_init(struct Main *bmain)
{
	AUD_DeviceSpecs specs;
	int device, buffersize;

	device = U.audiodevice;
	buffersize = U.mixbufsize;
	specs.channels = U.audiochannels;
	specs.format = U.audioformat;
	specs.rate = U.audiorate;

	if(force_device >= 0)
		device = force_device;

	if(buffersize < 128)
		buffersize = AUD_DEFAULT_BUFFER_SIZE;

	if(specs.rate < AUD_RATE_8000)
		specs.rate = AUD_RATE_44100;

	if(specs.format <= AUD_FORMAT_INVALID)
		specs.format = AUD_FORMAT_S16;

	if(specs.channels <= AUD_CHANNELS_INVALID)
		specs.channels = AUD_CHANNELS_STEREO;

	if(!AUD_init(device, specs, buffersize))
		AUD_init(AUD_NULL_DEVICE, specs, buffersize);
		
#ifdef WITH_JACK
	AUD_setSyncCallback(sound_sync_callback, bmain);
#else
	(void)bmain; /* unused */
#endif
}

void sound_exit(void)
{
	AUD_exit();
}

// XXX unused currently
#if 0
struct bSound* sound_new_buffer(struct bContext *C, struct bSound *source)
{
	bSound* sound = NULL;

	char name[25];
	strcpy(name, "buf_");
	strcpy(name + 4, source->id.name);

	sound = alloc_libblock(&CTX_data_main(C)->sound, ID_SO, name);

	sound->child_sound = source;
	sound->type = SOUND_TYPE_BUFFER;

	sound_load(CTX_data_main(C), sound);

	if(!sound->playback_handle)
	{
		free_libblock(&CTX_data_main(C)->sound, sound);
		sound = NULL;
	}

	return sound;
}

struct bSound* sound_new_limiter(struct bContext *C, struct bSound *source, float start, float end)
{
	bSound* sound = NULL;

	char name[25];
	strcpy(name, "lim_");
	strcpy(name + 4, source->id.name);

	sound = alloc_libblock(&CTX_data_main(C)->sound, ID_SO, name);

	sound->child_sound = source;
	sound->start = start;
	sound->end = end;
	sound->type = SOUND_TYPE_LIMITER;

	sound_load(CTX_data_main(C), sound);

	if(!sound->playback_handle)
	{
		free_libblock(&CTX_data_main(C)->sound, sound);
		sound = NULL;
	}

	return sound;
}
#endif

void sound_delete(struct bContext *C, struct bSound* sound)
{
	if(sound)
	{
		sound_free(sound);

		free_libblock(&CTX_data_main(C)->sound, sound);
	}
}

void sound_cache(struct bSound* sound)
{
	sound->flags |= SOUND_FLAGS_CACHING;
	if(sound->cache)
		AUD_unload(sound->cache);

	sound->cache = AUD_bufferSound(sound->handle);
	sound->playback_handle = sound->cache;
}

void sound_cache_notifying(struct Main* main, struct bSound* sound)
{
	sound_cache(sound);
	sound_update_sequencer(main, sound);
}

void sound_delete_cache(struct bSound* sound)
{
	sound->flags &= ~SOUND_FLAGS_CACHING;
	if(sound->cache)
	{
		AUD_unload(sound->cache);
		sound->cache = NULL;
		sound->playback_handle = sound->handle;
	}
}

void sound_load(struct Main *bmain, struct bSound* sound)
{
	if(sound)
	{
		if(sound->cache)
		{
			AUD_unload(sound->cache);
			sound->cache = NULL;
		}

		if(sound->handle)
		{
			AUD_unload(sound->handle);
			sound->handle = NULL;
			sound->playback_handle = NULL;
		}

// XXX unused currently
#if 0
		switch(sound->type)
		{
		case SOUND_TYPE_FILE:
#endif
		{
			char fullpath[FILE_MAX];
			char *path;

			/* load sound */
			PackedFile* pf = sound->packedfile;

			/* dont modify soundact->sound->name, only change a copy */
			BLI_strncpy(fullpath, sound->name, sizeof(fullpath));

			if(sound->id.lib)
				path = sound->id.lib->filepath;
			else
				path = bmain->name;

			BLI_path_abs(fullpath, path);

			/* but we need a packed file then */
			if (pf)
				sound->handle = AUD_loadBuffer((unsigned char*) pf->data, pf->size);
			/* or else load it from disk */
			else
				sound->handle = AUD_load(fullpath);
		}
// XXX unused currently
#if 0
			break;
		}
		case SOUND_TYPE_BUFFER:
			if(sound->child_sound && sound->child_sound->handle)
				sound->handle = AUD_bufferSound(sound->child_sound->handle);
			break;
		case SOUND_TYPE_LIMITER:
			if(sound->child_sound && sound->child_sound->handle)
				sound->handle = AUD_limitSound(sound->child_sound, sound->start, sound->end);
			break;
		}
#endif
		if(sound->flags & SOUND_FLAGS_CACHING)
		{
			sound->cache = AUD_bufferSound(sound->handle);
		}

		if(sound->cache)
			sound->playback_handle = sound->cache;
		else
			sound->playback_handle = sound->handle;

		sound_update_sequencer(bmain, sound);
	}
}

AUD_Device* sound_mixdown(struct Scene *scene, AUD_DeviceSpecs specs, int start, float volume)
{
	AUD_Device* mixdown = AUD_openReadDevice(specs);

	AUD_setDeviceVolume(mixdown, volume);

	AUD_setSequencerSpecs(scene->sound_scene, specs.specs);
	AUD_freeHandle(AUD_playDevice(mixdown, scene->sound_scene, start / FPS));

	return mixdown;
}

void sound_create_scene(struct Scene *scene)
{
	scene->sound_scene = AUD_createSequencer(FPS, scene->audio.flag & AUDIO_MUTE);
	AUD_updateSequencerData(scene->sound_scene, scene->audio.speed_of_sound,
							scene->audio.doppler_factor, scene->audio.distance_model);
	scene->sound_scene_handle = NULL;
	scene->sound_scrub_handle = NULL;
	scene->speaker_handles = NULL;
}

void sound_destroy_scene(struct Scene *scene)
{
	if(scene->sound_scene_handle)
		AUD_stop(scene->sound_scene_handle);
	if(scene->sound_scrub_handle)
		AUD_stop(scene->sound_scrub_handle);
	if(scene->sound_scene)
		AUD_destroySequencer(scene->sound_scene);
	if(scene->speaker_handles)
		AUD_destroySet(scene->speaker_handles);
}

void sound_mute_scene(struct Scene *scene, int muted)
{
	if(scene->sound_scene)
		AUD_setSequencerMuted(scene->sound_scene, muted);
}

void sound_update_fps(struct Scene *scene)
{
	if(scene->sound_scene)
		AUD_setSequencerFPS(scene->sound_scene, FPS);
}

void sound_update_scene_listener(struct Scene *scene)
{
	AUD_updateSequencerData(scene->sound_scene, scene->audio.speed_of_sound,
							scene->audio.doppler_factor, scene->audio.distance_model);
}

void* sound_scene_add_scene_sound(struct Scene *scene, struct Sequence* sequence, int startframe, int endframe, int frameskip)
{
	if(scene != sequence->scene)
		return AUD_addSequence(scene->sound_scene, sequence->scene->sound_scene, startframe / FPS, endframe / FPS, frameskip / FPS);
	return NULL;
}

void* sound_add_scene_sound(struct Scene *scene, struct Sequence* sequence, int startframe, int endframe, int frameskip)
{
	void* handle = AUD_addSequence(scene->sound_scene, sequence->sound->playback_handle, startframe / FPS, endframe / FPS, frameskip / FPS);
	AUD_muteSequence(handle, (sequence->flag & SEQ_MUTE) != 0);
	AUD_setSequenceAnimData(handle, AUD_AP_VOLUME, CFRA, &sequence->volume, 0);
	AUD_setSequenceAnimData(handle, AUD_AP_PITCH, CFRA, &sequence->pitch, 0);
	AUD_setSequenceAnimData(handle, AUD_AP_PANNING, CFRA, &sequence->pan, 0);
	return handle;
}

void sound_remove_scene_sound(struct Scene *scene, void* handle)
{
	AUD_removeSequence(scene->sound_scene, handle);
}

void sound_mute_scene_sound(void* handle, char mute)
{
	AUD_muteSequence(handle, mute);
}

void sound_move_scene_sound(struct Scene *scene, void* handle, int startframe, int endframe, int frameskip)
{
	AUD_moveSequence(handle, startframe / FPS, endframe / FPS, frameskip / FPS);
}

void sound_update_scene_sound(void* handle, struct bSound* sound)
{
	AUD_updateSequenceSound(handle, sound->playback_handle);
}

void sound_set_cfra(int cfra)
{
	sound_cfra = cfra;
}

void sound_set_scene_volume(struct Scene *scene, float volume)
{
	AUD_setSequencerAnimData(scene->sound_scene, AUD_AP_VOLUME, CFRA, &volume, (scene->audio.flag & AUDIO_VOLUME_ANIMATED) != 0);
}

void sound_set_scene_sound_volume(void* handle, float volume, char animated)
{
	AUD_setSequenceAnimData(handle, AUD_AP_VOLUME, sound_cfra, &volume, animated);
}

void sound_set_scene_sound_pitch(void* handle, float pitch, char animated)
{
	AUD_setSequenceAnimData(handle, AUD_AP_PITCH, sound_cfra, &pitch, animated);
}

void sound_set_scene_sound_pan(void* handle, float pan, char animated)
{
	AUD_setSequenceAnimData(handle, AUD_AP_PANNING, sound_cfra, &pan, animated);
}

void sound_update_sequencer(struct Main* main, struct bSound* sound)
{
	struct Scene* scene;

	for(scene = main->scene.first; scene; scene = scene->id.next)
		seq_update_sound(scene, sound);
}

static void sound_start_play_scene(struct Scene *scene)
{
	if(scene->sound_scene_handle)
		AUD_stop(scene->sound_scene_handle);

	AUD_setSequencerDeviceSpecs(scene->sound_scene);

	if((scene->sound_scene_handle = AUD_play(scene->sound_scene, 1)))
		AUD_setLoop(scene->sound_scene_handle, -1);
}

void sound_play_scene(struct Scene *scene)
{
	AUD_Status status;
	AUD_lock();

	status = scene->sound_scene_handle ? AUD_getStatus(scene->sound_scene_handle) : AUD_STATUS_INVALID;

	if(status == AUD_STATUS_INVALID)
		sound_start_play_scene(scene);

	if(!scene->sound_scene_handle)
	{
		AUD_unlock();
		return;
	}

	if(status != AUD_STATUS_PLAYING)
	{
		AUD_seek(scene->sound_scene_handle, CFRA / FPS);
		AUD_resume(scene->sound_scene_handle);
	}

	if(scene->audio.flag & AUDIO_SYNC)
		AUD_startPlayback();

	AUD_unlock();
}

void sound_stop_scene(struct Scene *scene)
{
	if(scene->sound_scene_handle)
	{
		AUD_pause(scene->sound_scene_handle);

		if(scene->audio.flag & AUDIO_SYNC)
			AUD_stopPlayback();
	}
}

void sound_seek_scene(struct bContext *C)
{
	struct Scene *scene = CTX_data_scene(C);
	AUD_Status status;

	AUD_lock();

	status = scene->sound_scene_handle ? AUD_getStatus(scene->sound_scene_handle) : AUD_STATUS_INVALID;

	if(status == AUD_STATUS_INVALID)
	{
		sound_start_play_scene(scene);

		if(!scene->sound_scene_handle)
		{
			AUD_unlock();
			return;
		}

		AUD_pause(scene->sound_scene_handle);
	}

	if(scene->audio.flag & AUDIO_SCRUB && !CTX_wm_screen(C)->animtimer)
	{
		if(scene->audio.flag & AUDIO_SYNC)
		{
			AUD_seek(scene->sound_scene_handle, CFRA / FPS);
			AUD_seekSequencer(scene->sound_scene_handle, CFRA / FPS);
		}
		else
			AUD_seek(scene->sound_scene_handle, CFRA / FPS);
		AUD_resume(scene->sound_scene_handle);
		if(scene->sound_scrub_handle && AUD_getStatus(scene->sound_scrub_handle) != AUD_STATUS_INVALID)
			AUD_seek(scene->sound_scrub_handle, 0);
		else
		{
			if(scene->sound_scrub_handle)
				AUD_stop(scene->sound_scrub_handle);
			scene->sound_scrub_handle = AUD_pauseAfter(scene->sound_scene_handle, 1 / FPS);
		}
	}
	else
	{
		if(scene->audio.flag & AUDIO_SYNC)
			AUD_seekSequencer(scene->sound_scene_handle, CFRA / FPS);
		else
		{
			if(status == AUD_STATUS_PLAYING)
				AUD_seek(scene->sound_scene_handle, CFRA / FPS);
		}
	}

	AUD_unlock();
}

float sound_sync_scene(struct Scene *scene)
{
	if(scene->sound_scene_handle)
	{
		if(scene->audio.flag & AUDIO_SYNC)
			return AUD_getSequencerPosition(scene->sound_scene_handle);
		else
			return AUD_getPosition(scene->sound_scene_handle);
	}
	return 0.0f;
}

int sound_scene_playing(struct Scene *scene)
{
	if(scene->audio.flag & AUDIO_SYNC)
		return AUD_doesPlayback();
	else
		return -1;
}

int sound_read_sound_buffer(struct bSound* sound, float* buffer, int length, float start, float end)
{
	AUD_Sound* limiter = AUD_limitSound(sound->cache, start, end);
	int ret= AUD_readSound(limiter, buffer, length);
	AUD_unload(limiter);
	return ret;
}

int sound_get_channels(struct bSound* sound)
{
	AUD_SoundInfo info;

	info = AUD_getInfo(sound->playback_handle);

	return info.specs.channels;
}

void* sound_get_factory(void* sound)
{
	return ((struct bSound*) sound)->playback_handle;
}

#else // WITH_AUDASPACE

#include "BLI_utildefines.h"

int sound_define_from_str(const char *UNUSED(str)) { return -1;}
void sound_force_device(int UNUSED(device)) {}
void sound_init_once(void) {}
void sound_init(struct Main *UNUSED(bmain)) {}
void sound_exit(void) {}
void sound_cache(struct bSound* UNUSED(sound)) { }
void sound_delete_cache(struct bSound* UNUSED(sound)) {}
void sound_load(struct Main *UNUSED(bmain), struct bSound* UNUSED(sound)) {}
void sound_create_scene(struct Scene *UNUSED(scene)) {}
void sound_destroy_scene(struct Scene *UNUSED(scene)) {}
void sound_mute_scene(struct Scene *UNUSED(scene), int UNUSED(muted)) {}
void* sound_scene_add_scene_sound(struct Scene *UNUSED(scene), struct Sequence* UNUSED(sequence), int UNUSED(startframe), int UNUSED(endframe), int UNUSED(frameskip)) { return NULL; }
void* sound_add_scene_sound(struct Scene *UNUSED(scene), struct Sequence* UNUSED(sequence), int UNUSED(startframe), int UNUSED(endframe), int UNUSED(frameskip)) { return NULL; }
void sound_remove_scene_sound(struct Scene *UNUSED(scene), void* UNUSED(handle)) {}
void sound_mute_scene_sound(void* UNUSED(handle), char UNUSED(mute)) {}
void sound_move_scene_sound(struct Scene *UNUSED(scene), void* UNUSED(handle), int UNUSED(startframe), int UNUSED(endframe), int UNUSED(frameskip)) {}
static void sound_start_play_scene(struct Scene *UNUSED(scene)) {}
void sound_play_scene(struct Scene *UNUSED(scene)) {}
void sound_stop_scene(struct Scene *UNUSED(scene)) {}
void sound_seek_scene(struct bContext *UNUSED(C)) {}
float sound_sync_scene(struct Scene *UNUSED(scene)) { return 0.0f; }
int sound_scene_playing(struct Scene *UNUSED(scene)) { return -1; }
int sound_read_sound_buffer(struct bSound* UNUSED(sound), float* UNUSED(buffer), int UNUSED(length), float UNUSED(start), float UNUSED(end)) { return 0; }
int sound_get_channels(struct bSound* UNUSED(sound)) { return 1; }

#endif // WITH_AUDASPACE
