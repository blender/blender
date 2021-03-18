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

#include <stdlib.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_iterator.h"
#include "BLI_math.h"
#include "BLI_threads.h"

#include "BLT_translation.h"

/* Allow using deprecated functionality for .blend file I/O. */
#define DNA_DEPRECATED_ALLOW

#include "DNA_anim_types.h"
#include "DNA_object_types.h"
#include "DNA_packedFile_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_sequence_types.h"
#include "DNA_sound_types.h"
#include "DNA_speaker_types.h"
#include "DNA_windowmanager_types.h"

#ifdef WITH_AUDASPACE
#  include "../../../intern/audaspace/intern/AUD_Set.h"
#  include <AUD_Handle.h>
#  include <AUD_Sequence.h>
#  include <AUD_Sound.h>
#  include <AUD_Special.h>
#endif

#include "BKE_global.h"
#include "BKE_idtype.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_packedFile.h"
#include "BKE_scene.h"
#include "BKE_sound.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "BLO_read_write.h"

#include "SEQ_sequencer.h"
#include "SEQ_sound.h"

static void sound_free_audio(bSound *sound);

static void sound_copy_data(Main *UNUSED(bmain),
                            ID *id_dst,
                            const ID *id_src,
                            const int UNUSED(flag))
{
  bSound *sound_dst = (bSound *)id_dst;
  const bSound *sound_src = (const bSound *)id_src;

  sound_dst->handle = NULL;
  sound_dst->cache = NULL;
  sound_dst->waveform = NULL;
  sound_dst->playback_handle = NULL;
  sound_dst->spinlock = MEM_mallocN(sizeof(SpinLock), "sound_spinlock");
  BLI_spin_init(sound_dst->spinlock);

  /* Just to be sure, should not have any value actually after reading time. */
  sound_dst->ipo = NULL;
  sound_dst->newpackedfile = NULL;

  if (sound_src->packedfile != NULL) {
    sound_dst->packedfile = BKE_packedfile_duplicate(sound_src->packedfile);
  }

  BKE_sound_reset_runtime(sound_dst);
}

static void sound_free_data(ID *id)
{
  bSound *sound = (bSound *)id;

  /* No animdata here. */

  if (sound->packedfile) {
    BKE_packedfile_free(sound->packedfile);
    sound->packedfile = NULL;
  }

  sound_free_audio(sound);
  BKE_sound_free_waveform(sound);

  if (sound->spinlock) {
    BLI_spin_end(sound->spinlock);
    MEM_freeN(sound->spinlock);
    sound->spinlock = NULL;
  }
}

static void sound_foreach_cache(ID *id,
                                IDTypeForeachCacheFunctionCallback function_callback,
                                void *user_data)
{
  bSound *sound = (bSound *)id;
  IDCacheKey key = {
      .id_session_uuid = id->session_uuid,
      .offset_in_ID = offsetof(bSound, waveform),
      .cache_v = sound->waveform,
  };

  function_callback(id, &key, &sound->waveform, 0, user_data);
}

static void sound_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bSound *sound = (bSound *)id;
  const bool is_undo = BLO_write_is_undo(writer);
  if (sound->id.us > 0 || is_undo) {
    /* Clean up, important in undo case to reduce false detection of changed datablocks. */
    sound->tags = 0;
    sound->handle = NULL;
    sound->playback_handle = NULL;
    sound->spinlock = NULL;

    /* Do not store packed files in case this is a library override ID. */
    if (ID_IS_OVERRIDE_LIBRARY(sound) && !is_undo) {
      sound->packedfile = NULL;
    }

    /* write LibData */
    BLO_write_id_struct(writer, bSound, id_address, &sound->id);
    BKE_id_blend_write(writer, &sound->id);

    BKE_packedfile_blend_write(writer, sound->packedfile);
  }
}

static void sound_blend_read_data(BlendDataReader *reader, ID *id)
{
  bSound *sound = (bSound *)id;
  sound->tags = 0;
  sound->handle = NULL;
  sound->playback_handle = NULL;

  /* versioning stuff, if there was a cache, then we enable caching: */
  if (sound->cache) {
    sound->flags |= SOUND_FLAGS_CACHING;
    sound->cache = NULL;
  }

  if (BLO_read_data_is_undo(reader)) {
    sound->tags |= SOUND_TAGS_WAVEFORM_NO_RELOAD;
  }

  sound->spinlock = MEM_mallocN(sizeof(SpinLock), "sound_spinlock");
  BLI_spin_init(sound->spinlock);

  /* clear waveform loading flag */
  sound->tags &= ~SOUND_TAGS_WAVEFORM_LOADING;

  BKE_packedfile_blend_read(reader, &sound->packedfile);
  BKE_packedfile_blend_read(reader, &sound->newpackedfile);
}

static void sound_blend_read_lib(BlendLibReader *reader, ID *id)
{
  bSound *sound = (bSound *)id;
  BLO_read_id_address(
      reader, sound->id.lib, &sound->ipo); /* XXX deprecated - old animation system */
}

static void sound_blend_read_expand(BlendExpander *expander, ID *id)
{
  bSound *snd = (bSound *)id;
  BLO_expand(expander, snd->ipo); /* XXX deprecated - old animation system */
}

IDTypeInfo IDType_ID_SO = {
    .id_code = ID_SO,
    .id_filter = FILTER_ID_SO,
    .main_listbase_index = INDEX_ID_SO,
    .struct_size = sizeof(bSound),
    .name = "Sound",
    .name_plural = "sounds",
    .translation_context = BLT_I18NCONTEXT_ID_SOUND,
    .flags = IDTYPE_FLAGS_NO_ANIMDATA,

    /* A fuzzy case, think NULLified content is OK here... */
    .init_data = NULL,
    .copy_data = sound_copy_data,
    .free_data = sound_free_data,
    .make_local = NULL,
    .foreach_id = NULL,
    .foreach_cache = sound_foreach_cache,
    .owner_get = NULL,

    .blend_write = sound_blend_write,
    .blend_read_data = sound_blend_read_data,
    .blend_read_lib = sound_blend_read_lib,
    .blend_read_expand = sound_blend_read_expand,

    .blend_read_undo_preserve = NULL,

    .lib_override_apply_post = NULL,
};

#ifdef WITH_AUDASPACE
/* evil globals ;-) */
static int sound_cfra;
static char **audio_device_names = NULL;
#endif

BLI_INLINE void sound_verify_evaluated_id(const ID *id)
{
  UNUSED_VARS_NDEBUG(id);
  /* This is a bit tricky and not quite reliable, but good enough check.
   *
   * We don't want audio system handles to be allocated on an original data-blocks, and only want
   * them to be allocated on a data-blocks which are result of dependency graph evaluation.
   *
   * Data-blocks which are covered by a copy-on-write system of dependency graph will have
   * LIB_TAG_COPIED_ON_WRITE tag set on them. But if some of data-blocks during its evaluation
   * decides to re-allocate its nested one (for example, object evaluation could re-allocate mesh
   * when evaluating modifier stack). Such data-blocks will have
   * LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT tag set on them.
   *
   * Additionally, we also allow data-blocks outside of main database. Those can not be "original"
   * and could be used as a temporary evaluated result during operations like baking.
   *
   * NOTE: We consider ID evaluated if ANY of those flags is set. We do NOT require ALL of them.
   */
  BLI_assert(id->tag &
             (LIB_TAG_COPIED_ON_WRITE | LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT | LIB_TAG_NO_MAIN));
}

bSound *BKE_sound_new_file(Main *bmain, const char *filepath)
{
  bSound *sound;
  const char *path;
  char str[FILE_MAX];

  BLI_strncpy(str, filepath, sizeof(str));

  path = BKE_main_blendfile_path(bmain);

  BLI_path_abs(str, path);

  sound = BKE_libblock_alloc(bmain, ID_SO, BLI_path_basename(filepath), 0);
  BLI_strncpy(sound->filepath, filepath, FILE_MAX);
  /* sound->type = SOUND_TYPE_FILE; */ /* XXX unused currently */

  sound->spinlock = MEM_mallocN(sizeof(SpinLock), "sound_spinlock");
  BLI_spin_init(sound->spinlock);

  BKE_sound_reset_runtime(sound);

  return sound;
}

bSound *BKE_sound_new_file_exists_ex(Main *bmain, const char *filepath, bool *r_exists)
{
  bSound *sound;
  char str[FILE_MAX], strtest[FILE_MAX];

  BLI_strncpy(str, filepath, sizeof(str));
  BLI_path_abs(str, BKE_main_blendfile_path(bmain));

  /* first search an identical filepath */
  for (sound = bmain->sounds.first; sound; sound = sound->id.next) {
    BLI_strncpy(strtest, sound->filepath, sizeof(sound->filepath));
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

bSound *BKE_sound_new_file_exists(Main *bmain, const char *filepath)
{
  return BKE_sound_new_file_exists_ex(bmain, filepath, NULL);
}

static void sound_free_audio(bSound *sound)
{
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
#else
  UNUSED_VARS(sound);
#endif /* WITH_AUDASPACE */
}

#ifdef WITH_AUDASPACE

static const char *force_device = NULL;

#  ifdef WITH_JACK
static SoundJackSyncCallback sound_jack_sync_callback = NULL;

static void sound_sync_callback(void *data, int mode, float time)
{
  if (sound_jack_sync_callback == NULL) {
    return;
  }
  Main *bmain = (Main *)data;
  sound_jack_sync_callback(bmain, mode, time);
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

void BKE_sound_init(Main *bmain)
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
    sound_device = AUD_init("None", specs, buffersize, "Blender");
  }

  BKE_sound_init_main(bmain);
}

void BKE_sound_init_main(Main *bmain)
{
#  ifdef WITH_JACK
  if (sound_device) {
    AUD_setSynchronizerCallback(sound_sync_callback, bmain);
  }
#  else
  UNUSED_VARS(bmain);
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
bSound *BKE_sound_new_buffer(Main *bmain, bSound *source)
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

bSound *BKE_sound_new_limiter(Main *bmain, bSound *source, float start, float end)
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
  sound_verify_evaluated_id(&sound->id);

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
  if (sound->cache) {
    AUD_Sound_free(sound->cache);
    sound->cache = NULL;
    sound->playback_handle = sound->handle;
  }
}

static void sound_load_audio(Main *bmain, bSound *sound, bool free_waveform)
{

  if (sound->cache) {
    AUD_Sound_free(sound->cache);
    sound->cache = NULL;
  }

  if (sound->handle) {
    AUD_Sound_free(sound->handle);
    sound->handle = NULL;
    sound->playback_handle = NULL;
  }

  if (free_waveform) {
    BKE_sound_free_waveform(sound);
  }

/* XXX unused currently */
#  if 0
    switch (sound->type) {
      case SOUND_TYPE_FILE:
#  endif
  {
    char fullpath[FILE_MAX];

    /* load sound */
    PackedFile *pf = sound->packedfile;

    /* don't modify soundact->sound->filepath, only change a copy */
    BLI_strncpy(fullpath, sound->filepath, sizeof(fullpath));
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
}

void BKE_sound_load(Main *bmain, bSound *sound)
{
  sound_verify_evaluated_id(&sound->id);
  sound_load_audio(bmain, sound, true);
}

AUD_Device *BKE_sound_mixdown(const Scene *scene, AUD_DeviceSpecs specs, int start, float volume)
{
  sound_verify_evaluated_id(&scene->id);
  return AUD_openMixdownDevice(specs, scene->sound_scene, volume, start / FPS);
}

void BKE_sound_create_scene(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

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

void BKE_sound_destroy_scene(Scene *scene)
{
  if (scene->playback_handle) {
    AUD_Handle_stop(scene->playback_handle);
  }
  if (scene->sound_scrub_handle) {
    AUD_Handle_stop(scene->sound_scrub_handle);
  }
  if (scene->speaker_handles) {
    void *handle;

    while ((handle = AUD_getSet(scene->speaker_handles))) {
      AUD_Sequence_remove(scene->sound_scene, handle);
    }

    AUD_destroySet(scene->speaker_handles);
  }
  if (scene->sound_scene) {
    AUD_Sequence_free(scene->sound_scene);
  }
}

void BKE_sound_lock()
{
  AUD_Device_lock(sound_device);
}

void BKE_sound_unlock()
{
  AUD_Device_unlock(sound_device);
}

void BKE_sound_reset_scene_specs(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  if (scene->sound_scene) {
    AUD_Specs specs;

    specs.channels = AUD_Device_getChannels(sound_device);
    specs.rate = AUD_Device_getRate(sound_device);

    AUD_Sequence_setSpecs(scene->sound_scene, specs);
  }
}

void BKE_sound_mute_scene(Scene *scene, int muted)
{
  sound_verify_evaluated_id(&scene->id);
  if (scene->sound_scene) {
    AUD_Sequence_setMuted(scene->sound_scene, muted);
  }
}

void BKE_sound_update_fps(Main *bmain, Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  if (scene->sound_scene) {
    AUD_Sequence_setFPS(scene->sound_scene, FPS);
  }

  SEQ_sound_update_length(bmain, scene);
}

void BKE_sound_update_scene_listener(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  AUD_Sequence_setSpeedOfSound(scene->sound_scene, scene->audio.speed_of_sound);
  AUD_Sequence_setDopplerFactor(scene->sound_scene, scene->audio.doppler_factor);
  AUD_Sequence_setDistanceModel(scene->sound_scene, scene->audio.distance_model);
}

void *BKE_sound_scene_add_scene_sound(
    Scene *scene, Sequence *sequence, int startframe, int endframe, int frameskip)
{
  sound_verify_evaluated_id(&scene->id);
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

void *BKE_sound_scene_add_scene_sound_defaults(Scene *scene, Sequence *sequence)
{
  return BKE_sound_scene_add_scene_sound(scene,
                                         sequence,
                                         sequence->startdisp,
                                         sequence->enddisp,
                                         sequence->startofs + sequence->anim_startofs);
}

void *BKE_sound_add_scene_sound(
    Scene *scene, Sequence *sequence, int startframe, int endframe, int frameskip)
{
  sound_verify_evaluated_id(&scene->id);
  /* Happens when sequence's sound data-block was removed. */
  if (sequence->sound == NULL) {
    return NULL;
  }
  sound_verify_evaluated_id(&sequence->sound->id);
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

void *BKE_sound_add_scene_sound_defaults(Scene *scene, Sequence *sequence)
{
  return BKE_sound_add_scene_sound(scene,
                                   sequence,
                                   sequence->startdisp,
                                   sequence->enddisp,
                                   sequence->startofs + sequence->anim_startofs);
}

void BKE_sound_remove_scene_sound(Scene *scene, void *handle)
{
  AUD_Sequence_remove(scene->sound_scene, handle);
}

void BKE_sound_mute_scene_sound(void *handle, char mute)
{
  AUD_SequenceEntry_setMuted(handle, mute);
}

void BKE_sound_move_scene_sound(
    Scene *scene, void *handle, int startframe, int endframe, int frameskip)
{
  sound_verify_evaluated_id(&scene->id);
  const double fps = FPS;
  AUD_SequenceEntry_move(handle, startframe / fps, endframe / fps, frameskip / fps);
}

void BKE_sound_move_scene_sound_defaults(Scene *scene, Sequence *sequence)
{
  sound_verify_evaluated_id(&scene->id);
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

void BKE_sound_set_scene_volume(Scene *scene, float volume)
{
  sound_verify_evaluated_id(&scene->id);
  if (scene->sound_scene == NULL) {
    return;
  }
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
  AUD_SequenceEntry_setAnimationData(handle, AUD_AP_PANNING, sound_cfra, &pan, animated);
}

void BKE_sound_update_sequencer(Main *main, bSound *sound)
{
  BLI_assert(!"is not supposed to be used, is weird function.");

  Scene *scene;

  for (scene = main->scenes.first; scene; scene = scene->id.next) {
    SEQ_sound_update(scene, sound);
  }
}

static void sound_start_play_scene(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  if (scene->playback_handle) {
    AUD_Handle_stop(scene->playback_handle);
  }

  BKE_sound_reset_scene_specs(scene);

  if ((scene->playback_handle = AUD_Device_play(sound_device, scene->sound_scene, 1))) {
    AUD_Handle_setLoopCount(scene->playback_handle, -1);
  }
}

static double get_cur_time(Scene *scene)
{
  /* We divide by the current framelen to take into account time remapping.
   * Otherwise we will get the wrong starting time which will break A/V sync.
   * See T74111 for further details. */
  return FRA2TIME((CFRA + SUBFRA) / (double)scene->r.framelen);
}

void BKE_sound_play_scene(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  AUD_Status status;
  const double cur_time = get_cur_time(scene);

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

void BKE_sound_stop_scene(Scene *scene)
{
  if (scene->playback_handle) {
    AUD_Handle_pause(scene->playback_handle);

    if (scene->audio.flag & AUDIO_SYNC) {
      AUD_stopSynchronizer();
    }
  }
}

void BKE_sound_seek_scene(Main *bmain, Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  AUD_Status status;
  bScreen *screen;
  int animation_playing;

  const double one_frame = 1.0 / FPS;
  const double cur_time = FRA2TIME(CFRA);

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

double BKE_sound_sync_scene(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  /* Ugly: Blender doesn't like it when the animation is played back during rendering */
  if (G.is_rendering) {
    return NAN_FLT;
  }

  if (scene->playback_handle) {
    if (scene->audio.flag & AUDIO_SYNC) {
      return AUD_getSynchronizerPosition(scene->playback_handle);
    }

    return AUD_Handle_getPosition(scene->playback_handle);
  }
  return NAN_FLT;
}

int BKE_sound_scene_playing(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  /* Ugly: Blender doesn't like it when the animation is played back during rendering */
  if (G.is_rendering) {
    return -1;
  }

  /* In case of a "None" audio device, we have no playback information. */
  if (AUD_Device_getRate(sound_device) == AUD_RATE_INVALID) {
    return -1;
  }

  if (scene->audio.flag & AUDIO_SYNC) {
    return AUD_isSynchronizerPlaying();
  }

  return -1;
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

void BKE_sound_read_waveform(Main *bmain, bSound *sound, short *stop)
{
  bool need_close_audio_handles = false;
  if (sound->playback_handle == NULL) {
    /* TODO(sergey): Make it fully independent audio handle. */
    sound_load_audio(bmain, sound, true);
    need_close_audio_handles = true;
  }

  AUD_SoundInfo info = AUD_getInfo(sound->playback_handle);
  SoundWaveform *waveform = MEM_mallocN(sizeof(SoundWaveform), "SoundWaveform");

  if (info.length > 0) {
    int length = info.length * SOUND_WAVE_SAMPLES_PER_SECOND;

    waveform->data = MEM_mallocN(sizeof(float[3]) * length, "SoundWaveform.samples");
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

  if (need_close_audio_handles) {
    sound_free_audio(sound);
  }
}

static void sound_update_base(Scene *scene, Object *object, void *new_set)
{
  NlaTrack *track;
  NlaStrip *strip;
  Speaker *speaker;
  float quat[4];

  sound_verify_evaluated_id(&scene->id);
  sound_verify_evaluated_id(&object->id);

  if ((object->type != OB_SPEAKER) || !object->adt) {
    return;
  }

  for (track = object->adt->nla_tracks.first; track; track = track->next) {
    for (strip = track->strips.first; strip; strip = strip->next) {
      if (strip->type != NLASTRIP_TYPE_SOUND) {
        continue;
      }
      speaker = (Speaker *)object->data;

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

        mat4_to_quat(quat, object->obmat);
        AUD_SequenceEntry_setAnimationData(
            strip->speaker_handle, AUD_AP_LOCATION, CFRA, object->obmat[3], 1);
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

void BKE_sound_update_scene(Depsgraph *depsgraph, Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  void *new_set = AUD_createSet();
  void *handle;
  float quat[4];

  /* cheap test to skip looping over all objects (no speakers is a common case) */
  if (DEG_id_type_any_exists(depsgraph, ID_SPK)) {
    DEG_OBJECT_ITER_BEGIN (depsgraph,
                           object,
                           (DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_INDIRECTLY |
                            DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET)) {
      sound_update_base(scene, object, new_set);
    }
    DEG_OBJECT_ITER_END;
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

float BKE_sound_get_length(Main *bmain, bSound *sound)
{
  if (sound->playback_handle != NULL) {
    AUD_SoundInfo info = AUD_getInfo(sound->playback_handle);
    return info.length;
  }
  SoundInfo info;
  if (!BKE_sound_info_get(bmain, sound, &info)) {
    return 0.0f;
  }
  return info.length;
}

char **BKE_sound_get_device_names(void)
{
  if (audio_device_names == NULL) {
    audio_device_names = AUD_getDeviceNames();
  }

  return audio_device_names;
}

static bool sound_info_from_playback_handle(void *playback_handle, SoundInfo *sound_info)
{
  if (playback_handle == NULL) {
    return false;
  }
  AUD_SoundInfo info = AUD_getInfo(playback_handle);
  sound_info->specs.channels = (eSoundChannels)info.specs.channels;
  sound_info->length = info.length;
  return true;
}

bool BKE_sound_info_get(struct Main *main, struct bSound *sound, SoundInfo *sound_info)
{
  if (sound->playback_handle != NULL) {
    return sound_info_from_playback_handle(sound->playback_handle, sound_info);
  }
  /* TODO(sergey): Make it fully independent audio handle. */
  /* Don't free waveforms during non-destructive queries.
   * This causes unnecessary recalculation - see T69921 */
  sound_load_audio(main, sound, false);
  const bool result = sound_info_from_playback_handle(sound->playback_handle, sound_info);
  sound_free_audio(sound);
  return result;
}

#else /* WITH_AUDASPACE */

#  include "BLI_utildefines.h"

void BKE_sound_force_device(const char *UNUSED(device))
{
}
void BKE_sound_init_once(void)
{
}
void BKE_sound_init(Main *UNUSED(bmain))
{
}
void BKE_sound_exit(void)
{
}
void BKE_sound_exit_once(void)
{
}
void BKE_sound_cache(bSound *UNUSED(sound))
{
}
void BKE_sound_delete_cache(bSound *UNUSED(sound))
{
}
void BKE_sound_load(Main *UNUSED(bmain), bSound *UNUSED(sound))
{
}
void BKE_sound_create_scene(Scene *UNUSED(scene))
{
}
void BKE_sound_destroy_scene(Scene *UNUSED(scene))
{
}
void BKE_sound_lock(void)
{
}
void BKE_sound_unlock(void)
{
}
void BKE_sound_reset_scene_specs(Scene *UNUSED(scene))
{
}
void BKE_sound_mute_scene(Scene *UNUSED(scene), int UNUSED(muted))
{
}
void *BKE_sound_scene_add_scene_sound(Scene *UNUSED(scene),
                                      Sequence *UNUSED(sequence),
                                      int UNUSED(startframe),
                                      int UNUSED(endframe),
                                      int UNUSED(frameskip))
{
  return NULL;
}
void *BKE_sound_scene_add_scene_sound_defaults(Scene *UNUSED(scene), Sequence *UNUSED(sequence))
{
  return NULL;
}
void *BKE_sound_add_scene_sound(Scene *UNUSED(scene),
                                Sequence *UNUSED(sequence),
                                int UNUSED(startframe),
                                int UNUSED(endframe),
                                int UNUSED(frameskip))
{
  return NULL;
}
void *BKE_sound_add_scene_sound_defaults(Scene *UNUSED(scene), Sequence *UNUSED(sequence))
{
  return NULL;
}
void BKE_sound_remove_scene_sound(Scene *UNUSED(scene), void *UNUSED(handle))
{
}
void BKE_sound_mute_scene_sound(void *UNUSED(handle), char UNUSED(mute))
{
}
void BKE_sound_move_scene_sound(Scene *UNUSED(scene),
                                void *UNUSED(handle),
                                int UNUSED(startframe),
                                int UNUSED(endframe),
                                int UNUSED(frameskip))
{
}
void BKE_sound_move_scene_sound_defaults(Scene *UNUSED(scene), Sequence *UNUSED(sequence))
{
}
void BKE_sound_play_scene(Scene *UNUSED(scene))
{
}
void BKE_sound_stop_scene(Scene *UNUSED(scene))
{
}
void BKE_sound_seek_scene(Main *UNUSED(bmain), Scene *UNUSED(scene))
{
}
double BKE_sound_sync_scene(Scene *UNUSED(scene))
{
  return NAN_FLT;
}
int BKE_sound_scene_playing(Scene *UNUSED(scene))
{
  return -1;
}
void BKE_sound_read_waveform(Main *bmain,
                             bSound *sound,
                             /* NOLINTNEXTLINE: readability-non-const-parameter. */
                             short *stop)
{
  UNUSED_VARS(sound, stop, bmain);
}
void BKE_sound_init_main(Main *UNUSED(bmain))
{
}
void BKE_sound_set_cfra(int UNUSED(cfra))
{
}
void BKE_sound_update_sequencer(Main *UNUSED(main), bSound *UNUSED(sound))
{
}
void BKE_sound_update_scene(Depsgraph *UNUSED(depsgraph), Scene *UNUSED(scene))
{
}
void BKE_sound_update_scene_sound(void *UNUSED(handle), bSound *UNUSED(sound))
{
}
void BKE_sound_update_scene_listener(Scene *UNUSED(scene))
{
}
void BKE_sound_update_fps(Main *UNUSED(bmain), Scene *UNUSED(scene))
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
void BKE_sound_set_scene_volume(Scene *UNUSED(scene), float UNUSED(volume))
{
}
void BKE_sound_set_scene_sound_pitch(void *UNUSED(handle),
                                     float UNUSED(pitch),
                                     char UNUSED(animated))
{
}
float BKE_sound_get_length(struct Main *UNUSED(bmain), bSound *UNUSED(sound))
{
  return 0;
}
char **BKE_sound_get_device_names(void)
{
  static char *names[1] = {NULL};
  return names;
}

void BKE_sound_free_waveform(bSound *UNUSED(sound))
{
}

bool BKE_sound_info_get(struct Main *UNUSED(main),
                        struct bSound *UNUSED(sound),
                        SoundInfo *UNUSED(sound_info))
{
  return false;
}

#endif /* WITH_AUDASPACE */

void BKE_sound_reset_scene_runtime(Scene *scene)
{
  scene->sound_scene = NULL;
  scene->playback_handle = NULL;
  scene->sound_scrub_handle = NULL;
  scene->speaker_handles = NULL;
}

void BKE_sound_ensure_scene(struct Scene *scene)
{
  if (scene->sound_scene != NULL) {
    return;
  }
  BKE_sound_create_scene(scene);
}

void BKE_sound_reset_runtime(bSound *sound)
{
  sound->cache = NULL;
  sound->playback_handle = NULL;
}

void BKE_sound_ensure_loaded(Main *bmain, bSound *sound)
{
  if (sound->cache != NULL) {
    return;
  }
  BKE_sound_load(bmain, sound);
}

void BKE_sound_jack_sync_callback_set(SoundJackSyncCallback callback)
{
#if defined(WITH_AUDASPACE) && defined(WITH_JACK)
  sound_jack_sync_callback = callback;
#else
  UNUSED_VARS(callback);
#endif
}

void BKE_sound_jack_scene_update(Scene *scene, int mode, double time)
{
  sound_verify_evaluated_id(&scene->id);

  /* Ugly: Blender doesn't like it when the animation is played back during rendering. */
  if (G.is_rendering) {
    return;
  }

  if (mode) {
    BKE_sound_play_scene(scene);
  }
  else {
    BKE_sound_stop_scene(scene);
  }
#ifdef WITH_AUDASPACE
  if (scene->playback_handle != NULL) {
    AUD_Handle_setPosition(scene->playback_handle, time);
  }
#else
  UNUSED_VARS(time);
#endif
}

void BKE_sound_evaluate(Depsgraph *depsgraph, Main *bmain, bSound *sound)
{
  DEG_debug_print_eval(depsgraph, __func__, sound->id.name, sound);
  if (sound->id.recalc & ID_RECALC_AUDIO) {
    BKE_sound_load(bmain, sound);
    return;
  }
  BKE_sound_ensure_loaded(bmain, sound);
}
