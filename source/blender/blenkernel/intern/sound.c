/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bke
 */

#include <string.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_threads.h"

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_screen_types.h"
#include "DNA_sound_types.h"
#include "DNA_speaker_types.h"

#ifdef WITH_AUDASPACE
#  include <AUD_Sound.h>
#  include <AUD_Sequence.h>
#  include <AUD_Handle.h>
#  include <AUD_Special.h>
#  include "../../../intern/audaspace/intern/AUD_Set.h"
#endif

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_sound.h"
#include "BKE_library.h"
#include "BKE_packedFile.h"
#include "BKE_sequencer.h"
#include "BKE_scene.h"

#ifdef WITH_AUDASPACE
/* evil globals ;-) */
static int sound_cfra;
static char **audio_device_names = NULL;
#endif

bSound *BKE_sound_new_file(struct Main *bmain, const char *filepath)
{
  bSound *sound;
  const char *path;
  char str[FILE_MAX];

  BLI_strncpy(str, filepath, sizeof(str));

  path = BKE_main_blendfile_path(bmain);

  BLI_path_abs(str, path);

  sound = BKE_libblock_alloc(bmain, ID_SO, BLI_path_basename(filepath), 0);
  BLI_strncpy(sound->name, filepath, FILE_MAX);
  /* sound->type = SOUND_TYPE_FILE; */ /* XXX unused currently */

  BKE_sound_load(bmain, sound);

  return sound;
}

bSound *BKE_sound_new_file_exists_ex(struct Main *bmain, const char *filepath, bool *r_exists)
{
  bSound *sound;
  char str[FILE_MAX], strtest[FILE_MAX];

  BLI_strncpy(str, filepath, sizeof(str));
  BLI_path_abs(str, BKE_main_blendfile_path(bmain));

  /* first search an identical filepath */
  for (sound = bmain->sounds.first; sound; sound = sound->id.next) {
    BLI_strncpy(strtest, sound->name, sizeof(sound->name));
    BLI_path_abs(strtest, ID_BLEND_PATH(bmain, &sound->id));

    if (BLI_path_cmp(strtest, str) == 0) {
      id_us_plus(&sound->id); /* officially should not, it doesn't link here! */
      if (r_exists) {
        *r_exists = true;
      }
      return sound;
    }
  }

  if (r_exists) {
    *r_exists = false;
  }
  return BKE_sound_new_file(bmain, filepath);
}

bSound *BKE_sound_new_file_exists(struct Main *bmain, const char *filepath)
{
  return BKE_sound_new_file_exists_ex(bmain, filepath, NULL);
}

/** Free (or release) any data used by this sound (does not free the sound itself). */
void BKE_sound_free(bSound *sound)
{
  /* No animdata here. */

  if (sound->packedfile) {
    freePackedFile(sound->packedfile);
    sound->packedfile = NULL;
  }

#ifdef WITH_AUDASPACE
  if (sound->handle) {
    AUD_Sound_free(sound->handle);
    sound->handle = NULL;
    sound->playback_handle = NULL;
  }

  if (sound->cache) {
    AUD_Sound_free(sound->cache);
    sound->cache = NULL;
  }

  BKE_sound_free_waveform(sound);

#endif /* WITH_AUDASPACE */
  if (sound->spinlock) {
    BLI_spin_end(sound->spinlock);
    MEM_freeN(sound->spinlock);
    sound->spinlock = NULL;
  }
}

/**
 * Only copy internal data of Sound ID from source
 * to already allocated/initialized destination.
 * You probably never want to use that directly,
 * use #BKE_id_copy or #BKE_id_copy_ex for typical needs.
 *
 * WARNING! This function will not handle ID user count!
 *
 * \param flag: Copying options (see BKE_library.h's LIB_ID_COPY_... flags for more).
 */
void BKE_sound_copy_data(Main *bmain,
                         bSound *sound_dst,
                         const bSound *UNUSED(sound_src),
                         const int UNUSED(flag))
{
  sound_dst->handle = NULL;
  sound_dst->cache = NULL;
  sound_dst->waveform = NULL;
  sound_dst->playback_handle = NULL;
  sound_dst->spinlock =
      NULL; /* Think this is OK? Otherwise, easy to create new spinlock here... */

  /* Just to be sure, should not have any value actually after reading time. */
  sound_dst->ipo = NULL;
  sound_dst->newpackedfile = NULL;

  if (sound_dst->packedfile) {
    sound_dst->packedfile = dupPackedFile(sound_dst->packedfile);
  }

  /* Initialize whole runtime (audaspace) stuff. */
  BKE_sound_load(bmain, sound_dst);
}

void BKE_sound_make_local(Main *bmain, bSound *sound, const bool lib_local)
{
  BKE_id_make_local_generic(bmain, &sound->id, true, lib_local);
}

#ifdef WITH_AUDASPACE

static const char *force_device = NULL;

#  ifdef WITH_JACK
static void sound_sync_callback(void *data, int mode, float time)
{
  // Ugly: Blender doesn't like it when the animation is played back during rendering
  if (G.is_rendering) {
    return;
  }

  struct Main *bmain = (struct Main *)data;
  struct Scene *scene;

  scene = bmain->scenes.first;
  while (scene) {
    if (scene->audio.flag & AUDIO_SYNC) {
      if (mode) {
        BKE_sound_play_scene(scene);
      }
      else {
        BKE_sound_stop_scene(scene);
      }
      if (scene->playback_handle) {
        AUD_Handle_setPosition(scene->playback_handle, time);
      }
    }
    scene = scene->id.next;
  }
}
#  endif

void BKE_sound_force_device(const char *device)
{
  force_device = device;
}

void BKE_sound_init_once(void)
{
  AUD_initOnce();
  atexit(BKE_sound_exit_once);
}

static AUD_Device *sound_device = NULL;

void *BKE_sound_get_device(void)
{
  return sound_device;
}

void BKE_sound_init(struct Main *bmain)
{
  /* Make sure no instance of the sound system is running, otherwise we get leaks. */
  BKE_sound_exit();

  AUD_DeviceSpecs specs;
  int device, buffersize;
  const char *device_name;

  device = U.audiodevice;
  buffersize = U.mixbufsize;
  specs.channels = U.audiochannels;
  specs.format = U.audioformat;
  specs.rate = U.audiorate;

  if (force_device == NULL) {
    int i;
    char **names = BKE_sound_get_device_names();
    device_name = names[0];

    /* make sure device is within the bounds of the array */
    for (i = 0; names[i]; i++) {
      if (i == device) {
        device_name = names[i];
      }
    }
  }
  else {
    device_name = force_device;
  }

  if (buffersize < 128) {
    buffersize = 1024;
  }

  if (specs.rate < AUD_RATE_8000) {
    specs.rate = AUD_RATE_48000;
  }

  if (specs.format <= AUD_FORMAT_INVALID) {
    specs.format = AUD_FORMAT_S16;
  }

  if (specs.channels <= AUD_CHANNELS_INVALID) {
    specs.channels = AUD_CHANNELS_STEREO;
  }

  if (!(sound_device = AUD_init(device_name, specs, buffersize, "Blender"))) {
    sound_device = AUD_init("Null", specs, buffersize, "Blender");
  }

  BKE_sound_init_main(bmain);
}

void BKE_sound_init_main(struct Main *bmain)
{
#  ifdef WITH_JACK
  if (sound_device) {
    AUD_setSynchronizerCallback(sound_sync_callback, bmain);
  }
#  else
  (void)bmain; /* unused */
#  endif
}

void BKE_sound_exit(void)
{
  AUD_exit(sound_device);
  sound_device = NULL;
}

void BKE_sound_exit_once(void)
{
  AUD_exit(sound_device);
  sound_device = NULL;
  AUD_exitOnce();

  if (audio_device_names != NULL) {
    int i;
    for (i = 0; audio_device_names[i]; i++) {
      free(audio_device_names[i]);
    }
    free(audio_device_names);
    audio_device_names = NULL;
  }
}

/* XXX unused currently */
#  if 0
bSound *BKE_sound_new_buffer(struct Main *bmain, bSound *source)
{
  bSound *sound = NULL;

  char name[MAX_ID_NAME + 5];
  strcpy(name, "buf_");
  strcpy(name + 4, source->id.name);

  sound = BKE_libblock_alloc(bmain, ID_SO, name);

  sound->child_sound = source;
  sound->type = SOUND_TYPE_BUFFER;

  sound_load(bmain, sound);

  return sound;
}

bSound *BKE_sound_new_limiter(struct Main *bmain, bSound *source, float start, float end)
{
  bSound *sound = NULL;

  char name[MAX_ID_NAME + 5];
  strcpy(name, "lim_");
  strcpy(name + 4, source->id.name);

  sound = BKE_libblock_alloc(bmain, ID_SO, name);

  sound->child_sound = source;
  sound->start = start;
  sound->end = end;
  sound->type = SOUND_TYPE_LIMITER;

  sound_load(bmain, sound);

  return sound;
}
#  endif

void BKE_sound_cache(bSound *sound)
{
  sound->flags |= SOUND_FLAGS_CACHING;
  if (sound->cache) {
    AUD_Sound_free(sound->cache);
  }

  sound->cache = AUD_Sound_cache(sound->handle);
  if (sound->cache) {
    sound->playback_handle = sound->cache;
  }
  else {
    sound->playback_handle = sound->handle;
  }
}

void BKE_sound_delete_cache(bSound *sound)
{
  sound->flags &= ~SOUND_FLAGS_CACHING;
  if (sound->cache) {
    AUD_Sound_free(sound->cache);
    sound->cache = NULL;
    sound->playback_handle = sound->handle;
  }
}

void BKE_sound_load(struct Main *bmain, bSound *sound)
{
  if (sound) {
    if (sound->cache) {
      AUD_Sound_free(sound->cache);
      sound->cache = NULL;
    }

    if (sound->handle) {
      AUD_Sound_free(sound->handle);
      sound->handle = NULL;
      sound->playback_handle = NULL;
    }

    BKE_sound_free_waveform(sound);

/* XXX unused currently */
#  if 0
    switch (sound->type) {
      case SOUND_TYPE_FILE:
#  endif
    {
      char fullpath[FILE_MAX];

      /* load sound */
      PackedFile *pf = sound->packedfile;

      /* don't modify soundact->sound->name, only change a copy */
      BLI_strncpy(fullpath, sound->name, sizeof(fullpath));
      BLI_path_abs(fullpath, ID_BLEND_PATH(bmain, &sound->id));

      /* but we need a packed file then */
      if (pf) {
        sound->handle = AUD_Sound_bufferFile((unsigned char *)pf->data, pf->size);
      }
      else {
        /* or else load it from disk */
        sound->handle = AUD_Sound_file(fullpath);
      }
    }
/* XXX unused currently */
#  if 0
      break;
    }
    case SOUND_TYPE_BUFFER:
      if (sound->child_sound && sound->child_sound->handle) {
        sound->handle = AUD_bufferSound(sound->child_sound->handle);
      }
      break;
    case SOUND_TYPE_LIMITER:
      if (sound->child_sound && sound->child_sound->handle) {
        sound->handle = AUD_limitSound(sound->child_sound, sound->start, sound->end);
      }
      break;
  }
#  endif
    if (sound->flags & SOUND_FLAGS_MONO) {
      void *handle = AUD_Sound_rechannel(sound->handle, AUD_CHANNELS_MONO);
      AUD_Sound_free(sound->handle);
      sound->handle = handle;
    }

    if (sound->flags & SOUND_FLAGS_CACHING) {
      sound->cache = AUD_Sound_cache(sound->handle);
    }

    if (sound->cache) {
      sound->playback_handle = sound->cache;
    }
    else {
      sound->playback_handle = sound->handle;
    }

    BKE_sound_update_sequencer(bmain, sound);
  }
}

AUD_Device *BKE_sound_mixdown(struct Scene *scene, AUD_DeviceSpecs specs, int start, float volume)
{
  return AUD_openMixdownDevice(specs, scene->sound_scene, volume, start / FPS);
}

void BKE_sound_create_scene(struct Scene *scene)
{
  /* should be done in version patch, but this gets called before */
  if (scene->r.frs_sec_base == 0) {
    scene->r.frs_sec_base = 1;
  }

  scene->sound_scene = AUD_Sequence_create(FPS, scene->audio.flag & AUDIO_MUTE);
  AUD_Sequence_setSpeedOfSound(scene->sound_scene, scene->audio.speed_of_sound);
  AUD_Sequence_setDopplerFactor(scene->sound_scene, scene->audio.doppler_factor);
  AUD_Sequence_setDistanceModel(scene->sound_scene, scene->audio.distance_model);
  scene->playback_handle = NULL;
  scene->sound_scrub_handle = NULL;
  scene->speaker_handles = NULL;
}

void BKE_sound_destroy_scene(struct Scene *scene)
{
  if (scene->playback_handle) {
    AUD_Handle_stop(scene->playback_handle);
  }
  if (scene->sound_scrub_handle) {
    AUD_Handle_stop(scene->sound_scrub_handle);
  }
  if (scene->sound_scene) {
    AUD_Sequence_free(scene->sound_scene);
  }
  if (scene->speaker_handles) {
    AUD_destroySet(scene->speaker_handles);
  }
}

void BKE_sound_reset_scene_specs(struct Scene *scene)
{
  AUD_Specs specs;

  specs.channels = AUD_Device_getChannels(sound_device);
  specs.rate = AUD_Device_getRate(sound_device);

  AUD_Sequence_setSpecs(scene->sound_scene, specs);
}

void BKE_sound_mute_scene(struct Scene *scene, int muted)
{
  if (scene->sound_scene) {
    AUD_Sequence_setMuted(scene->sound_scene, muted);
  }
}

void BKE_sound_update_fps(struct Scene *scene)
{
  if (scene->sound_scene) {
    AUD_Sequence_setFPS(scene->sound_scene, FPS);
  }

  BKE_sequencer_refresh_sound_length(scene);
}

void BKE_sound_update_scene_listener(struct Scene *scene)
{
  AUD_Sequence_setSpeedOfSound(scene->sound_scene, scene->audio.speed_of_sound);
  AUD_Sequence_setDopplerFactor(scene->sound_scene, scene->audio.doppler_factor);
  AUD_Sequence_setDistanceModel(scene->sound_scene, scene->audio.distance_model);
}

void *BKE_sound_scene_add_scene_sound(
    struct Scene *scene, struct Sequence *sequence, int startframe, int endframe, int frameskip)
{
  if (sequence->scene && scene != sequence->scene) {
    const double fps = FPS;
    return AUD_Sequence_add(scene->sound_scene,
                            sequence->scene->sound_scene,
                            startframe / fps,
                            endframe / fps,
                            frameskip / fps);
  }
  return NULL;
}

void *BKE_sound_scene_add_scene_sound_defaults(struct Scene *scene, struct Sequence *sequence)
{
  return BKE_sound_scene_add_scene_sound(scene,
                                         sequence,
                                         sequence->startdisp,
                                         sequence->enddisp,
                                         sequence->startofs + sequence->anim_startofs);
}

void *BKE_sound_add_scene_sound(
    struct Scene *scene, struct Sequence *sequence, int startframe, int endframe, int frameskip)
{
  /* Happens when sequence's sound datablock was removed. */
  if (sequence->sound == NULL) {
    return NULL;
  }
  const double fps = FPS;
  void *handle = AUD_Sequence_add(scene->sound_scene,
                                  sequence->sound->playback_handle,
                                  startframe / fps,
                                  endframe / fps,
                                  frameskip / fps);
  AUD_SequenceEntry_setMuted(handle, (sequence->flag & SEQ_MUTE) != 0);
  AUD_SequenceEntry_setAnimationData(handle, AUD_AP_VOLUME, CFRA, &sequence->volume, 0);
  AUD_SequenceEntry_setAnimationData(handle, AUD_AP_PITCH, CFRA, &sequence->pitch, 0);
  AUD_SequenceEntry_setAnimationData(handle, AUD_AP_PANNING, CFRA, &sequence->pan, 0);
  return handle;
}

void *BKE_sound_add_scene_sound_defaults(struct Scene *scene, struct Sequence *sequence)
{
  return BKE_sound_add_scene_sound(scene,
                                   sequence,
                                   sequence->startdisp,
                                   sequence->enddisp,
                                   sequence->startofs + sequence->anim_startofs);
}

void BKE_sound_remove_scene_sound(struct Scene *scene, void *handle)
{
  AUD_Sequence_remove(scene->sound_scene, handle);
}

void BKE_sound_mute_scene_sound(void *handle, char mute)
{
  AUD_SequenceEntry_setMuted(handle, mute);
}

void BKE_sound_move_scene_sound(
    struct Scene *scene, void *handle, int startframe, int endframe, int frameskip)
{
  const double fps = FPS;
  AUD_SequenceEntry_move(handle, startframe / fps, endframe / fps, frameskip / fps);
}

void BKE_sound_move_scene_sound_defaults(struct Scene *scene, struct Sequence *sequence)
{
  if (sequence->scene_sound) {
    BKE_sound_move_scene_sound(scene,
                               sequence->scene_sound,
                               sequence->startdisp,
                               sequence->enddisp,
                               sequence->startofs + sequence->anim_startofs);
  }
}

void BKE_sound_update_scene_sound(void *handle, bSound *sound)
{
  AUD_SequenceEntry_setSound(handle, sound->playback_handle);
}

void BKE_sound_set_cfra(int cfra)
{
  sound_cfra = cfra;
}

void BKE_sound_set_scene_volume(struct Scene *scene, float volume)
{
  AUD_Sequence_setAnimationData(scene->sound_scene,
                                AUD_AP_VOLUME,
                                CFRA,
                                &volume,
                                (scene->audio.flag & AUDIO_VOLUME_ANIMATED) != 0);
}

void BKE_sound_set_scene_sound_volume(void *handle, float volume, char animated)
{
  AUD_SequenceEntry_setAnimationData(handle, AUD_AP_VOLUME, sound_cfra, &volume, animated);
}

void BKE_sound_set_scene_sound_pitch(void *handle, float pitch, char animated)
{
  AUD_SequenceEntry_setAnimationData(handle, AUD_AP_PITCH, sound_cfra, &pitch, animated);
}

void BKE_sound_set_scene_sound_pan(void *handle, float pan, char animated)
{
  printf("%s\n", __func__);
  AUD_SequenceEntry_setAnimationData(handle, AUD_AP_PANNING, sound_cfra, &pan, animated);
}

void BKE_sound_update_sequencer(struct Main *main, bSound *sound)
{
  struct Scene *scene;

  for (scene = main->scenes.first; scene; scene = scene->id.next) {
    BKE_sequencer_update_sound(scene, sound);
  }
}

static void sound_start_play_scene(struct Scene *scene)
{
  if (scene->playback_handle) {
    AUD_Handle_stop(scene->playback_handle);
  }

  BKE_sound_reset_scene_specs(scene);

  if ((scene->playback_handle = AUD_Device_play(sound_device, scene->sound_scene, 1))) {
    AUD_Handle_setLoopCount(scene->playback_handle, -1);
  }
}

void BKE_sound_play_scene(struct Scene *scene)
{
  AUD_Status status;
  const float cur_time = (float)((double)CFRA / FPS);

  AUD_Device_lock(sound_device);

  status = scene->playback_handle ? AUD_Handle_getStatus(scene->playback_handle) :
                                    AUD_STATUS_INVALID;

  if (status == AUD_STATUS_INVALID) {
    sound_start_play_scene(scene);

    if (!scene->playback_handle) {
      AUD_Device_unlock(sound_device);
      return;
    }
  }

  if (status != AUD_STATUS_PLAYING) {
    AUD_Handle_setPosition(scene->playback_handle, cur_time);
    AUD_Handle_resume(scene->playback_handle);
  }

  if (scene->audio.flag & AUDIO_SYNC) {
    AUD_playSynchronizer();
  }

  AUD_Device_unlock(sound_device);
}

void BKE_sound_stop_scene(struct Scene *scene)
{
  if (scene->playback_handle) {
    AUD_Handle_pause(scene->playback_handle);

    if (scene->audio.flag & AUDIO_SYNC) {
      AUD_stopSynchronizer();
    }
  }
}

void BKE_sound_seek_scene(struct Main *bmain, struct Scene *scene)
{
  AUD_Status status;
  bScreen *screen;
  int animation_playing;

  const float one_frame = (float)(1.0 / FPS);
  const float cur_time = (float)((double)CFRA / FPS);

  AUD_Device_lock(sound_device);

  status = scene->playback_handle ? AUD_Handle_getStatus(scene->playback_handle) :
                                    AUD_STATUS_INVALID;

  if (status == AUD_STATUS_INVALID) {
    sound_start_play_scene(scene);

    if (!scene->playback_handle) {
      AUD_Device_unlock(sound_device);
      return;
    }

    AUD_Handle_pause(scene->playback_handle);
  }

  animation_playing = 0;
  for (screen = bmain->screens.first; screen; screen = screen->id.next) {
    if (screen->animtimer) {
      animation_playing = 1;
      break;
    }
  }

  if (scene->audio.flag & AUDIO_SCRUB && !animation_playing) {
    AUD_Handle_setPosition(scene->playback_handle, cur_time);
    if (scene->audio.flag & AUDIO_SYNC) {
      AUD_seekSynchronizer(scene->playback_handle, cur_time);
    }
    AUD_Handle_resume(scene->playback_handle);
    if (scene->sound_scrub_handle &&
        AUD_Handle_getStatus(scene->sound_scrub_handle) != AUD_STATUS_INVALID) {
      AUD_Handle_setPosition(scene->sound_scrub_handle, 0);
    }
    else {
      if (scene->sound_scrub_handle) {
        AUD_Handle_stop(scene->sound_scrub_handle);
      }
      scene->sound_scrub_handle = AUD_pauseAfter(scene->playback_handle, one_frame);
    }
  }
  else {
    if (scene->audio.flag & AUDIO_SYNC) {
      AUD_seekSynchronizer(scene->playback_handle, cur_time);
    }
    else {
      if (status == AUD_STATUS_PLAYING) {
        AUD_Handle_setPosition(scene->playback_handle, cur_time);
      }
    }
  }

  AUD_Device_unlock(sound_device);
}

float BKE_sound_sync_scene(struct Scene *scene)
{
  // Ugly: Blender doesn't like it when the animation is played back during rendering
  if (G.is_rendering) {
    return NAN_FLT;
  }

  if (scene->playback_handle) {
    if (scene->audio.flag & AUDIO_SYNC) {
      return AUD_getSynchronizerPosition(scene->playback_handle);
    }
    else {
      return AUD_Handle_getPosition(scene->playback_handle);
    }
  }
  return NAN_FLT;
}

int BKE_sound_scene_playing(struct Scene *scene)
{
  // Ugly: Blender doesn't like it when the animation is played back during rendering
  if (G.is_rendering) {
    return -1;
  }

  if (scene->audio.flag & AUDIO_SYNC) {
    return AUD_isSynchronizerPlaying();
  }
  else {
    return -1;
  }
}

void BKE_sound_free_waveform(bSound *sound)
{
  if ((sound->tags & SOUND_TAGS_WAVEFORM_NO_RELOAD) == 0) {
    SoundWaveform *waveform = sound->waveform;
    if (waveform) {
      if (waveform->data) {
        MEM_freeN(waveform->data);
      }
      MEM_freeN(waveform);
    }

    sound->waveform = NULL;
  }
  /* This tag is only valid once. */
  sound->tags &= ~SOUND_TAGS_WAVEFORM_NO_RELOAD;
}

void BKE_sound_read_waveform(bSound *sound, short *stop)
{
  AUD_SoundInfo info = AUD_getInfo(sound->playback_handle);
  SoundWaveform *waveform = MEM_mallocN(sizeof(SoundWaveform), "SoundWaveform");

  if (info.length > 0) {
    int length = info.length * SOUND_WAVE_SAMPLES_PER_SECOND;

    waveform->data = MEM_mallocN(length * sizeof(float) * 3, "SoundWaveform.samples");
    waveform->length = AUD_readSound(
        sound->playback_handle, waveform->data, length, SOUND_WAVE_SAMPLES_PER_SECOND, stop);
  }
  else {
    /* Create an empty waveform here if the sound couldn't be
     * read. This indicates that reading the waveform is "done",
     * whereas just setting sound->waveform to NULL causes other
     * code to think the waveform still needs to be created. */
    waveform->data = NULL;
    waveform->length = 0;
  }

  if (*stop) {
    if (waveform->data) {
      MEM_freeN(waveform->data);
    }
    MEM_freeN(waveform);
    BLI_spin_lock(sound->spinlock);
    sound->tags &= ~SOUND_TAGS_WAVEFORM_LOADING;
    BLI_spin_unlock(sound->spinlock);
    return;
  }

  BKE_sound_free_waveform(sound);

  BLI_spin_lock(sound->spinlock);
  sound->waveform = waveform;
  sound->tags &= ~SOUND_TAGS_WAVEFORM_LOADING;
  BLI_spin_unlock(sound->spinlock);
}

static void sound_update_base(Scene *scene, Base *base, void *new_set)
{
  Object *ob = base->object;
  NlaTrack *track;
  NlaStrip *strip;
  Speaker *speaker;
  float quat[4];

  if ((ob->id.tag & LIB_TAG_DOIT) == 0) {
    return;
  }

  ob->id.tag &= ~LIB_TAG_DOIT;

  if ((ob->type != OB_SPEAKER) || !ob->adt) {
    return;
  }

  for (track = ob->adt->nla_tracks.first; track; track = track->next) {
    for (strip = track->strips.first; strip; strip = strip->next) {
      if (strip->type != NLASTRIP_TYPE_SOUND) {
        continue;
      }
      speaker = (Speaker *)ob->data;

      if (AUD_removeSet(scene->speaker_handles, strip->speaker_handle)) {
        if (speaker->sound) {
          AUD_SequenceEntry_move(strip->speaker_handle, (double)strip->start / FPS, FLT_MAX, 0);
        }
        else {
          AUD_Sequence_remove(scene->sound_scene, strip->speaker_handle);
          strip->speaker_handle = NULL;
        }
      }
      else {
        if (speaker->sound) {
          strip->speaker_handle = AUD_Sequence_add(scene->sound_scene,
                                                   speaker->sound->playback_handle,
                                                   (double)strip->start / FPS,
                                                   FLT_MAX,
                                                   0);
          AUD_SequenceEntry_setRelative(strip->speaker_handle, 0);
        }
      }

      if (strip->speaker_handle) {
        const bool mute = ((strip->flag & NLASTRIP_FLAG_MUTED) || (speaker->flag & SPK_MUTED));
        AUD_addSet(new_set, strip->speaker_handle);
        AUD_SequenceEntry_setVolumeMaximum(strip->speaker_handle, speaker->volume_max);
        AUD_SequenceEntry_setVolumeMinimum(strip->speaker_handle, speaker->volume_min);
        AUD_SequenceEntry_setDistanceMaximum(strip->speaker_handle, speaker->distance_max);
        AUD_SequenceEntry_setDistanceReference(strip->speaker_handle, speaker->distance_reference);
        AUD_SequenceEntry_setAttenuation(strip->speaker_handle, speaker->attenuation);
        AUD_SequenceEntry_setConeAngleOuter(strip->speaker_handle, speaker->cone_angle_outer);
        AUD_SequenceEntry_setConeAngleInner(strip->speaker_handle, speaker->cone_angle_inner);
        AUD_SequenceEntry_setConeVolumeOuter(strip->speaker_handle, speaker->cone_volume_outer);

        mat4_to_quat(quat, ob->obmat);
        AUD_SequenceEntry_setAnimationData(
            strip->speaker_handle, AUD_AP_LOCATION, CFRA, ob->obmat[3], 1);
        AUD_SequenceEntry_setAnimationData(
            strip->speaker_handle, AUD_AP_ORIENTATION, CFRA, quat, 1);
        AUD_SequenceEntry_setAnimationData(
            strip->speaker_handle, AUD_AP_VOLUME, CFRA, &speaker->volume, 1);
        AUD_SequenceEntry_setAnimationData(
            strip->speaker_handle, AUD_AP_PITCH, CFRA, &speaker->pitch, 1);
        AUD_SequenceEntry_setSound(strip->speaker_handle, speaker->sound->playback_handle);
        AUD_SequenceEntry_setMuted(strip->speaker_handle, mute);
      }
    }
  }
}

void BKE_sound_update_scene(Main *bmain, Scene *scene)
{
  Base *base;
  Scene *sce_it;

  void *new_set = AUD_createSet();
  void *handle;
  float quat[4];

  /* cheap test to skip looping over all objects (no speakers is a common case) */
  if (!BLI_listbase_is_empty(&bmain->speakers)) {
    BKE_main_id_tag_listbase(&bmain->objects, LIB_TAG_DOIT, true);

    for (ViewLayer *view_layer = scene->view_layers.first; view_layer;
         view_layer = view_layer->next) {
      for (base = view_layer->object_bases.first; base; base = base->next) {
        sound_update_base(scene, base, new_set);
      }
    }

    for (SETLOOPER_SET_ONLY(scene, sce_it, base)) {
      sound_update_base(scene, base, new_set);
    }
  }

  while ((handle = AUD_getSet(scene->speaker_handles))) {
    AUD_Sequence_remove(scene->sound_scene, handle);
  }

  if (scene->camera) {
    mat4_to_quat(quat, scene->camera->obmat);
    AUD_Sequence_setAnimationData(
        scene->sound_scene, AUD_AP_LOCATION, CFRA, scene->camera->obmat[3], 1);
    AUD_Sequence_setAnimationData(scene->sound_scene, AUD_AP_ORIENTATION, CFRA, quat, 1);
  }

  AUD_destroySet(scene->speaker_handles);
  scene->speaker_handles = new_set;
}

void *BKE_sound_get_factory(void *sound)
{
  return ((bSound *)sound)->playback_handle;
}

/* stupid wrapper because AUD_C-API.h includes Python.h which makesrna doesn't like */
float BKE_sound_get_length(bSound *sound)
{
  AUD_SoundInfo info = AUD_getInfo(sound->playback_handle);

  return info.length;
}

char **BKE_sound_get_device_names(void)
{
  if (audio_device_names == NULL) {
    audio_device_names = AUD_getDeviceNames();
  }

  return audio_device_names;
}

#else /* WITH_AUDASPACE */

#  include "BLI_utildefines.h"

void BKE_sound_force_device(const char *UNUSED(device))
{
}
void BKE_sound_init_once(void)
{
}
void BKE_sound_init(struct Main *UNUSED(bmain))
{
}
void BKE_sound_exit(void)
{
}
void BKE_sound_exit_once(void)
{
}
void BKE_sound_cache(struct bSound *UNUSED(sound))
{
}
void BKE_sound_delete_cache(struct bSound *UNUSED(sound))
{
}
void BKE_sound_load(struct Main *UNUSED(bmain), struct bSound *UNUSED(sound))
{
}
void BKE_sound_create_scene(struct Scene *UNUSED(scene))
{
}
void BKE_sound_destroy_scene(struct Scene *UNUSED(scene))
{
}
void BKE_sound_reset_scene_specs(struct Scene *UNUSED(scene))
{
}
void BKE_sound_mute_scene(struct Scene *UNUSED(scene), int UNUSED(muted))
{
}
void *BKE_sound_scene_add_scene_sound(struct Scene *UNUSED(scene),
                                      struct Sequence *UNUSED(sequence),
                                      int UNUSED(startframe),
                                      int UNUSED(endframe),
                                      int UNUSED(frameskip))
{
  return NULL;
}
void *BKE_sound_scene_add_scene_sound_defaults(struct Scene *UNUSED(scene),
                                               struct Sequence *UNUSED(sequence))
{
  return NULL;
}
void *BKE_sound_add_scene_sound(struct Scene *UNUSED(scene),
                                struct Sequence *UNUSED(sequence),
                                int UNUSED(startframe),
                                int UNUSED(endframe),
                                int UNUSED(frameskip))
{
  return NULL;
}
void *BKE_sound_add_scene_sound_defaults(struct Scene *UNUSED(scene),
                                         struct Sequence *UNUSED(sequence))
{
  return NULL;
}
void BKE_sound_remove_scene_sound(struct Scene *UNUSED(scene), void *UNUSED(handle))
{
}
void BKE_sound_mute_scene_sound(void *UNUSED(handle), char UNUSED(mute))
{
}
void BKE_sound_move_scene_sound(struct Scene *UNUSED(scene),
                                void *UNUSED(handle),
                                int UNUSED(startframe),
                                int UNUSED(endframe),
                                int UNUSED(frameskip))
{
}
void BKE_sound_move_scene_sound_defaults(struct Scene *UNUSED(scene),
                                         struct Sequence *UNUSED(sequence))
{
}
void BKE_sound_play_scene(struct Scene *UNUSED(scene))
{
}
void BKE_sound_stop_scene(struct Scene *UNUSED(scene))
{
}
void BKE_sound_seek_scene(struct Main *UNUSED(bmain), struct Scene *UNUSED(scene))
{
}
float BKE_sound_sync_scene(struct Scene *UNUSED(scene))
{
  return NAN_FLT;
}
int BKE_sound_scene_playing(struct Scene *UNUSED(scene))
{
  return -1;
}
void BKE_sound_read_waveform(struct bSound *sound, short *stop)
{
  UNUSED_VARS(sound, stop);
}
void BKE_sound_init_main(struct Main *UNUSED(bmain))
{
}
void BKE_sound_set_cfra(int UNUSED(cfra))
{
}
void BKE_sound_update_sequencer(struct Main *UNUSED(main), struct bSound *UNUSED(sound))
{
}
void BKE_sound_update_scene(struct Main *UNUSED(bmain), struct Scene *UNUSED(scene))
{
}
void BKE_sound_update_scene_sound(void *UNUSED(handle), struct bSound *UNUSED(sound))
{
}
void BKE_sound_update_scene_listener(struct Scene *UNUSED(scene))
{
}
void BKE_sound_update_fps(struct Scene *UNUSED(scene))
{
}
void BKE_sound_set_scene_sound_volume(void *UNUSED(handle),
                                      float UNUSED(volume),
                                      char UNUSED(animated))
{
}
void BKE_sound_set_scene_sound_pan(void *UNUSED(handle), float UNUSED(pan), char UNUSED(animated))
{
}
void BKE_sound_set_scene_volume(struct Scene *UNUSED(scene), float UNUSED(volume))
{
}
void BKE_sound_set_scene_sound_pitch(void *UNUSED(handle),
                                     float UNUSED(pitch),
                                     char UNUSED(animated))
{
}
float BKE_sound_get_length(struct bSound *UNUSED(sound))
{
  return 0;
}
char **BKE_sound_get_device_names(void)
{
  static char *names[1] = {NULL};
  return names;
}

#endif /* WITH_AUDASPACE */
