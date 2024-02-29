/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_iterator.h"
#include "BLI_math_rotation.h"
#include "BLI_threads.h"

#include "BLT_translation.hh"

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

#include "BKE_bpath.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_packedFile.h"
#include "BKE_sound.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "BLO_read_write.hh"

#include "SEQ_sound.hh"
#include "SEQ_time.hh"

static void sound_free_audio(bSound *sound);

static void sound_copy_data(Main * /*bmain*/, ID *id_dst, const ID *id_src, const int /*flag*/)
{
  bSound *sound_dst = (bSound *)id_dst;
  const bSound *sound_src = (const bSound *)id_src;

  sound_dst->handle = nullptr;
  sound_dst->cache = nullptr;
  sound_dst->waveform = nullptr;
  sound_dst->playback_handle = nullptr;
  sound_dst->spinlock = MEM_mallocN(sizeof(SpinLock), "sound_spinlock");
  BLI_spin_init(static_cast<SpinLock *>(sound_dst->spinlock));

  /* Just to be sure, should not have any value actually after reading time. */
  sound_dst->ipo = nullptr;
  sound_dst->newpackedfile = nullptr;

  if (sound_src->packedfile != nullptr) {
    sound_dst->packedfile = BKE_packedfile_duplicate(sound_src->packedfile);
  }

  BKE_sound_reset_runtime(sound_dst);
}

static void sound_free_data(ID *id)
{
  bSound *sound = (bSound *)id;

  /* No animation-data here. */

  if (sound->packedfile) {
    BKE_packedfile_free(sound->packedfile);
    sound->packedfile = nullptr;
  }

  sound_free_audio(sound);
  BKE_sound_free_waveform(sound);

  if (sound->spinlock) {
    BLI_spin_end(static_cast<SpinLock *>(sound->spinlock));
    MEM_freeN(sound->spinlock);
    sound->spinlock = nullptr;
  }
}

static void sound_foreach_id(ID *id, LibraryForeachIDData *data)
{
  bSound *sound = reinterpret_cast<bSound *>(id);
  const int flag = BKE_lib_query_foreachid_process_flags_get(data);

  if (flag & IDWALK_DO_DEPRECATED_POINTERS) {
    BKE_LIB_FOREACHID_PROCESS_ID_NOCHECK(data, sound->ipo, IDWALK_CB_USER);
  }
}

static void sound_foreach_cache(ID *id,
                                IDTypeForeachCacheFunctionCallback function_callback,
                                void *user_data)
{
  bSound *sound = (bSound *)id;
  IDCacheKey key{};
  key.id_session_uid = id->session_uid;
  key.identifier = offsetof(bSound, waveform);

  function_callback(id, &key, &sound->waveform, 0, user_data);
}

static void sound_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  bSound *sound = (bSound *)id;
  if (sound->packedfile != nullptr && (bpath_data->flag & BKE_BPATH_FOREACH_PATH_SKIP_PACKED) != 0)
  {
    return;
  }

  /* FIXME: This does not check for empty path... */
  BKE_bpath_foreach_path_fixed_process(bpath_data, sound->filepath, sizeof(sound->filepath));
}

static void sound_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bSound *sound = (bSound *)id;
  const bool is_undo = BLO_write_is_undo(writer);

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  sound->tags = 0;
  sound->handle = nullptr;
  sound->playback_handle = nullptr;
  sound->spinlock = nullptr;

  /* Do not store packed files in case this is a library override ID. */
  if (ID_IS_OVERRIDE_LIBRARY(sound) && !is_undo) {
    sound->packedfile = nullptr;
  }

  /* write LibData */
  BLO_write_id_struct(writer, bSound, id_address, &sound->id);
  BKE_id_blend_write(writer, &sound->id);

  BKE_packedfile_blend_write(writer, sound->packedfile);
}

static void sound_blend_read_data(BlendDataReader *reader, ID *id)
{
  bSound *sound = (bSound *)id;
  sound->tags = 0;
  sound->handle = nullptr;
  sound->playback_handle = nullptr;

  /* versioning stuff, if there was a cache, then we enable caching: */
  if (sound->cache) {
    sound->flags |= SOUND_FLAGS_CACHING;
    sound->cache = nullptr;
  }

  if (BLO_read_data_is_undo(reader)) {
    sound->tags |= SOUND_TAGS_WAVEFORM_NO_RELOAD;
  }

  sound->spinlock = MEM_mallocN(sizeof(SpinLock), "sound_spinlock");
  BLI_spin_init(static_cast<SpinLock *>(sound->spinlock));

  /* clear waveform loading flag */
  sound->tags &= ~SOUND_TAGS_WAVEFORM_LOADING;

  BKE_packedfile_blend_read(reader, &sound->packedfile);
  BKE_packedfile_blend_read(reader, &sound->newpackedfile);
}

IDTypeInfo IDType_ID_SO = {
    /*id_code*/ ID_SO,
    /*id_filter*/ FILTER_ID_SO,
    /*dependencies_id_types*/ 0,
    /*main_listbase_index*/ INDEX_ID_SO,
    /*struct_size*/ sizeof(bSound),
    /*name*/ "Sound",
    /*name_plural*/ N_("sounds"),
    /*translation_context*/ BLT_I18NCONTEXT_ID_SOUND,
    /*flags*/ IDTYPE_FLAGS_NO_ANIMDATA | IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    /*asset_type_info*/ nullptr,

    /* A fuzzy case, think NULLified content is OK here... */
    /*init_data*/ nullptr,
    /*copy_data*/ sound_copy_data,
    /*free_data*/ sound_free_data,
    /*make_local*/ nullptr,
    /*foreach_id*/ sound_foreach_id,
    /*foreach_cache*/ sound_foreach_cache,
    /*foreach_path*/ sound_foreach_path,
    /*owner_pointer_get*/ nullptr,

    /*blend_write*/ sound_blend_write,
    /*blend_read_data*/ sound_blend_read_data,
    /*blend_read_after_liblink*/ nullptr,

    /*blend_read_undo_preserve*/ nullptr,

    /*lib_override_apply_post*/ nullptr,
};

#ifdef WITH_AUDASPACE
/* evil globals ;-) */
static char **audio_device_names = nullptr;
#endif

BLI_INLINE void sound_verify_evaluated_id(const ID *id)
{
  UNUSED_VARS_NDEBUG(id);
  /* This is a bit tricky and not quite reliable, but good enough check.
   *
   * We don't want audio system handles to be allocated on an original data-blocks, and only want
   * them to be allocated on a data-blocks which are result of dependency graph evaluation.
   *
   * Data-blocks which are covered by a copy-on-evaluation system of dependency graph will have
   * LIB_TAG_COPIED_ON_EVAL tag set on them. But if some of data-blocks during its evaluation
   * decides to re-allocate its nested one (for example, object evaluation could re-allocate mesh
   * when evaluating modifier stack). Such data-blocks will have
   * LIB_TAG_COPIED_ON_EVAL_FINAL_RESULT tag set on them.
   *
   * Additionally, we also allow data-blocks outside of main database. Those can not be "original"
   * and could be used as a temporary evaluated result during operations like baking.
   *
   * NOTE: We consider ID evaluated if ANY of those flags is set. We do NOT require ALL of them.
   */
  BLI_assert(id->tag &
             (LIB_TAG_COPIED_ON_EVAL | LIB_TAG_COPIED_ON_EVAL_FINAL_RESULT | LIB_TAG_NO_MAIN));
}

bSound *BKE_sound_new_file(Main *bmain, const char *filepath)
{
  bSound *sound;
  const char *blendfile_path = BKE_main_blendfile_path(bmain);
  char filepath_abs[FILE_MAX];

  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, blendfile_path);

  sound = static_cast<bSound *>(BKE_libblock_alloc(bmain, ID_SO, BLI_path_basename(filepath), 0));
  STRNCPY(sound->filepath, filepath);
  // sound->type = SOUND_TYPE_FILE; /* UNUSED. */

  /* Extract sound specs for bSound */
  SoundInfo info;
  bool success = BKE_sound_info_get(bmain, sound, &info);
  if (success) {
    sound->samplerate = info.specs.samplerate;
    sound->audio_channels = info.specs.channels;
  }

  sound->spinlock = MEM_mallocN(sizeof(SpinLock), "sound_spinlock");
  BLI_spin_init(static_cast<SpinLock *>(sound->spinlock));

  BKE_sound_reset_runtime(sound);

  return sound;
}

bSound *BKE_sound_new_file_exists_ex(Main *bmain, const char *filepath, bool *r_exists)
{
  bSound *sound;
  char filepath_abs[FILE_MAX], filepath_test[FILE_MAX];

  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, BKE_main_blendfile_path(bmain));

  /* first search an identical filepath */
  for (sound = static_cast<bSound *>(bmain->sounds.first); sound;
       sound = static_cast<bSound *>(sound->id.next))
  {
    STRNCPY(filepath_test, sound->filepath);
    BLI_path_abs(filepath_test, ID_BLEND_PATH(bmain, &sound->id));

    if (BLI_path_cmp(filepath_test, filepath_abs) == 0) {
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
  return BKE_sound_new_file_exists_ex(bmain, filepath, nullptr);
}

static void sound_free_audio(bSound *sound)
{
#ifdef WITH_AUDASPACE
  if (sound->handle) {
    AUD_Sound_free(sound->handle);
    sound->handle = nullptr;
    sound->playback_handle = nullptr;
  }

  if (sound->cache) {
    AUD_Sound_free(sound->cache);
    sound->cache = nullptr;
  }
#else
  UNUSED_VARS(sound);
#endif /* WITH_AUDASPACE */
}

#ifdef WITH_AUDASPACE

static const char *force_device = nullptr;

#  ifdef WITH_JACK
static SoundJackSyncCallback sound_jack_sync_callback = nullptr;

static void sound_sync_callback(void *data, int mode, float time)
{
  if (sound_jack_sync_callback == nullptr) {
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

void BKE_sound_init_once()
{
  AUD_initOnce();
  atexit(BKE_sound_exit_once);
}

static AUD_Device *sound_device = nullptr;

void *BKE_sound_get_device()
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
  specs.channels = AUD_Channels(U.audiochannels);
  specs.format = AUD_SampleFormat(U.audioformat);
  specs.rate = U.audiorate;

  if (force_device == nullptr) {
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

void BKE_sound_exit()
{
  AUD_exit(sound_device);
  sound_device = nullptr;
}

void BKE_sound_exit_once()
{
  AUD_exit(sound_device);
  sound_device = nullptr;
  AUD_exitOnce();

  if (audio_device_names != nullptr) {
    int i;
    for (i = 0; audio_device_names[i]; i++) {
      free(audio_device_names[i]);
    }
    free(audio_device_names);
    audio_device_names = nullptr;
  }
}

/* XXX unused currently */
#  if 0
bSound *BKE_sound_new_buffer(Main *bmain, bSound *source)
{
  bSound *sound = nullptr;

  char name[MAX_ID_NAME + 5];
  BLI_string_join(name, sizeof(name), "buf_", source->id.name);

  sound = BKE_libblock_alloc(bmain, ID_SO, name);

  sound->child_sound = source;
  sound->type = SOUND_TYPE_BUFFER;

  sound_load(bmain, sound);

  return sound;
}

bSound *BKE_sound_new_limiter(Main *bmain, bSound *source, float start, float end)
{
  bSound *sound = nullptr;

  char name[MAX_ID_NAME + 5];
  BLI_string_join(name, sizeof(name), "lim_", source->id.name);

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
    sound->cache = nullptr;
    sound->playback_handle = sound->handle;
  }
}

static void sound_load_audio(Main *bmain, bSound *sound, bool free_waveform)
{

  if (sound->cache) {
    AUD_Sound_free(sound->cache);
    sound->cache = nullptr;
  }

  if (sound->handle) {
    AUD_Sound_free(sound->handle);
    sound->handle = nullptr;
    sound->playback_handle = nullptr;
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

    /* Don't modify `sound->filepath`, only change a copy. */
    STRNCPY(fullpath, sound->filepath);
    BLI_path_abs(fullpath, ID_BLEND_PATH(bmain, &sound->id));

    /* but we need a packed file then */
    if (pf) {
      sound->handle = AUD_Sound_bufferFile((uchar *)pf->data, pf->size);
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
  return AUD_openMixdownDevice(
      specs, scene->sound_scene, volume, AUD_RESAMPLE_QUALITY_MEDIUM, start / FPS);
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
  AUD_Sequence_setDistanceModel(scene->sound_scene,
                                AUD_DistanceModel(scene->audio.distance_model));
  scene->playback_handle = nullptr;
  scene->sound_scrub_handle = nullptr;
  scene->speaker_handles = nullptr;
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
  AUD_Sequence_setDistanceModel(scene->sound_scene,
                                AUD_DistanceModel(scene->audio.distance_model));
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
  return nullptr;
}

void *BKE_sound_scene_add_scene_sound_defaults(Scene *scene, Sequence *sequence)
{
  return BKE_sound_scene_add_scene_sound(scene,
                                         sequence,
                                         SEQ_time_left_handle_frame_get(scene, sequence),
                                         SEQ_time_right_handle_frame_get(scene, sequence),
                                         sequence->startofs + sequence->anim_startofs);
}

void *BKE_sound_add_scene_sound(
    Scene *scene, Sequence *sequence, int startframe, int endframe, int frameskip)
{
  sound_verify_evaluated_id(&scene->id);
  /* Happens when sequence's sound data-block was removed. */
  if (sequence->sound == nullptr) {
    return nullptr;
  }
  sound_verify_evaluated_id(&sequence->sound->id);
  const double fps = FPS;
  return AUD_Sequence_add(scene->sound_scene,
                          sequence->sound->playback_handle,
                          startframe / fps,
                          endframe / fps,
                          frameskip / fps + sequence->sound->offset_time);
}

void *BKE_sound_add_scene_sound_defaults(Scene *scene, Sequence *sequence)
{
  return BKE_sound_add_scene_sound(scene,
                                   sequence,
                                   SEQ_time_left_handle_frame_get(scene, sequence),
                                   SEQ_time_right_handle_frame_get(scene, sequence),
                                   sequence->startofs + sequence->anim_startofs);
}

void BKE_sound_remove_scene_sound(Scene *scene, void *handle)
{
  AUD_Sequence_remove(scene->sound_scene, handle);
}

void BKE_sound_mute_scene_sound(void *handle, bool mute)
{
  AUD_SequenceEntry_setMuted(handle, mute);
}

void BKE_sound_move_scene_sound(const Scene *scene,
                                void *handle,
                                int startframe,
                                int endframe,
                                int frameskip,
                                double audio_offset)
{
  sound_verify_evaluated_id(&scene->id);
  const double fps = FPS;
  AUD_SequenceEntry_move(handle, startframe / fps, endframe / fps, frameskip / fps + audio_offset);
}

void BKE_sound_move_scene_sound_defaults(Scene *scene, Sequence *sequence)
{
  sound_verify_evaluated_id(&scene->id);
  if (sequence->scene_sound) {
    BKE_sound_move_scene_sound(scene,
                               sequence->scene_sound,
                               SEQ_time_left_handle_frame_get(scene, sequence),
                               SEQ_time_right_handle_frame_get(scene, sequence),
                               sequence->startofs + sequence->anim_startofs,
                               0.0);
  }
}

void BKE_sound_update_scene_sound(void *handle, bSound *sound)
{
  AUD_SequenceEntry_setSound(handle, sound->playback_handle);
}

#endif /* WITH_AUDASPACE */

void BKE_sound_update_sequence_handle(void *handle, void *sound_handle)
{
#ifdef WITH_AUDASPACE
  AUD_SequenceEntry_setSound(handle, sound_handle);
#else
  UNUSED_VARS(handle, sound_handle);
#endif
}

#ifdef WITH_AUDASPACE

void BKE_sound_set_scene_volume(Scene *scene, float volume)
{
  sound_verify_evaluated_id(&scene->id);
  if (scene->sound_scene == nullptr) {
    return;
  }
  AUD_Sequence_setAnimationData(scene->sound_scene,
                                AUD_AP_VOLUME,
                                scene->r.cfra,
                                &volume,
                                (scene->audio.flag & AUDIO_VOLUME_ANIMATED) != 0);
}

void BKE_sound_set_scene_sound_volume_at_frame(void *handle,
                                               const int frame,
                                               float volume,
                                               const char animated)
{
  AUD_SequenceEntry_setAnimationData(handle, AUD_AP_VOLUME, frame, &volume, animated);
}

void BKE_sound_set_scene_sound_pitch_at_frame(void *handle,
                                              const int frame,
                                              float pitch,
                                              const char animated)
{
  AUD_SequenceEntry_setAnimationData(handle, AUD_AP_PITCH, frame, &pitch, animated);
}

void BKE_sound_set_scene_sound_pitch_constant_range(void *handle,
                                                    const int frame_start,
                                                    const int frame_end,
                                                    float pitch)
{
  AUD_SequenceEntry_setConstantRangeAnimationData(
      handle, AUD_AP_PITCH, frame_start, frame_end, &pitch);
}

void BKE_sound_set_scene_sound_pan_at_frame(void *handle,
                                            const int frame,
                                            float pan,
                                            const char animated)
{
  AUD_SequenceEntry_setAnimationData(handle, AUD_AP_PANNING, frame, &pan, animated);
}

void BKE_sound_update_sequencer(Main *main, bSound *sound)
{
  BLI_assert_msg(0, "is not supposed to be used, is weird function.");

  Scene *scene;

  for (scene = static_cast<Scene *>(main->scenes.first); scene;
       scene = static_cast<Scene *>(scene->id.next))
  {
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
   * See #74111 for further details. */
  return FRA2TIME((scene->r.cfra + scene->r.subframe) / double(scene->r.framelen));
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
  const double cur_time = FRA2TIME(scene->r.cfra);

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
  for (screen = static_cast<bScreen *>(bmain->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
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
        AUD_Handle_getStatus(scene->sound_scrub_handle) != AUD_STATUS_INVALID)
    {
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
    SoundWaveform *waveform = static_cast<SoundWaveform *>(sound->waveform);
    if (waveform) {
      if (waveform->data) {
        MEM_freeN(waveform->data);
      }
      MEM_freeN(waveform);
    }

    sound->waveform = nullptr;
  }
  /* This tag is only valid once. */
  sound->tags &= ~SOUND_TAGS_WAVEFORM_NO_RELOAD;
}

void BKE_sound_read_waveform(Main *bmain, bSound *sound, bool *stop)
{
  bool need_close_audio_handles = false;
  if (sound->playback_handle == nullptr) {
    /* TODO(sergey): Make it fully independent audio handle. */
    sound_load_audio(bmain, sound, true);
    need_close_audio_handles = true;
  }

  AUD_SoundInfo info = AUD_getInfo(sound->playback_handle);
  SoundWaveform *waveform = static_cast<SoundWaveform *>(
      MEM_mallocN(sizeof(SoundWaveform), "SoundWaveform"));

  if (info.length > 0) {
    int length = info.length * SOUND_WAVE_SAMPLES_PER_SECOND;

    waveform->data = static_cast<float *>(
        MEM_mallocN(sizeof(float[3]) * length, "SoundWaveform.samples"));
    /* Ideally this would take a boolean argument. */
    short stop_i16 = *stop;
    waveform->length = AUD_readSound(
        sound->playback_handle, waveform->data, length, SOUND_WAVE_SAMPLES_PER_SECOND, &stop_i16);
    *stop = stop_i16 != 0;
  }
  else {
    /* Create an empty waveform here if the sound couldn't be
     * read. This indicates that reading the waveform is "done",
     * whereas just setting sound->waveform to nullptr causes other
     * code to think the waveform still needs to be created. */
    waveform->data = nullptr;
    waveform->length = 0;
  }

  if (*stop) {
    if (waveform->data) {
      MEM_freeN(waveform->data);
    }
    MEM_freeN(waveform);
    BLI_spin_lock(static_cast<SpinLock *>(sound->spinlock));
    sound->tags &= ~SOUND_TAGS_WAVEFORM_LOADING;
    BLI_spin_unlock(static_cast<SpinLock *>(sound->spinlock));
    return;
  }

  BKE_sound_free_waveform(sound);

  BLI_spin_lock(static_cast<SpinLock *>(sound->spinlock));
  sound->waveform = waveform;
  sound->tags &= ~SOUND_TAGS_WAVEFORM_LOADING;
  BLI_spin_unlock(static_cast<SpinLock *>(sound->spinlock));

  if (need_close_audio_handles) {
    sound_free_audio(sound);
  }
}

static void sound_update_base(Scene *scene, Object *object, void *new_set)
{
  Speaker *speaker;
  float quat[4];

  sound_verify_evaluated_id(&scene->id);
  sound_verify_evaluated_id(&object->id);

  if ((object->type != OB_SPEAKER) || !object->adt) {
    return;
  }

  LISTBASE_FOREACH (NlaTrack *, track, &object->adt->nla_tracks) {
    LISTBASE_FOREACH (NlaStrip *, strip, &track->strips) {
      if (strip->type != NLASTRIP_TYPE_SOUND) {
        continue;
      }
      speaker = (Speaker *)object->data;

      if (AUD_removeSet(scene->speaker_handles, strip->speaker_handle)) {
        if (speaker->sound) {
          AUD_SequenceEntry_move(strip->speaker_handle, double(strip->start) / FPS, FLT_MAX, 0);
        }
        else {
          AUD_Sequence_remove(scene->sound_scene, strip->speaker_handle);
          strip->speaker_handle = nullptr;
        }
      }
      else {
        if (speaker->sound) {
          strip->speaker_handle = AUD_Sequence_add(scene->sound_scene,
                                                   speaker->sound->playback_handle,
                                                   double(strip->start) / FPS,
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

        mat4_to_quat(quat, object->object_to_world().ptr());
        blender::float3 location = object->object_to_world().location();
        AUD_SequenceEntry_setAnimationData(
            strip->speaker_handle, AUD_AP_LOCATION, scene->r.cfra, location, 1);
        AUD_SequenceEntry_setAnimationData(
            strip->speaker_handle, AUD_AP_ORIENTATION, scene->r.cfra, quat, 1);
        AUD_SequenceEntry_setAnimationData(
            strip->speaker_handle, AUD_AP_VOLUME, scene->r.cfra, &speaker->volume, 1);
        AUD_SequenceEntry_setAnimationData(
            strip->speaker_handle, AUD_AP_PITCH, scene->r.cfra, &speaker->pitch, 1);
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
    DEGObjectIterSettings deg_iter_settings = {nullptr};
    deg_iter_settings.depsgraph = depsgraph;
    deg_iter_settings.flags = DEG_ITER_OBJECT_FLAG_LINKED_DIRECTLY |
                              DEG_ITER_OBJECT_FLAG_LINKED_INDIRECTLY |
                              DEG_ITER_OBJECT_FLAG_LINKED_VIA_SET;
    DEG_OBJECT_ITER_BEGIN (&deg_iter_settings, object) {
      sound_update_base(scene, object, new_set);
    }
    DEG_OBJECT_ITER_END;
  }

  while ((handle = AUD_getSet(scene->speaker_handles))) {
    AUD_Sequence_remove(scene->sound_scene, handle);
  }

  if (scene->camera) {
    mat4_to_quat(quat, scene->camera->object_to_world().ptr());
    blender::float3 location = scene->camera->object_to_world().location();
    AUD_Sequence_setAnimationData(scene->sound_scene, AUD_AP_LOCATION, scene->r.cfra, location, 1);
    AUD_Sequence_setAnimationData(scene->sound_scene, AUD_AP_ORIENTATION, scene->r.cfra, quat, 1);
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
  if (sound->playback_handle != nullptr) {
    AUD_SoundInfo info = AUD_getInfo(sound->playback_handle);
    return info.length;
  }
  SoundInfo info;
  if (!BKE_sound_info_get(bmain, sound, &info)) {
    return 0.0f;
  }
  return info.length;
}

char **BKE_sound_get_device_names()
{
  if (audio_device_names == nullptr) {
    audio_device_names = AUD_getDeviceNames();
  }

  return audio_device_names;
}

static bool sound_info_from_playback_handle(void *playback_handle, SoundInfo *sound_info)
{
  if (playback_handle == nullptr) {
    return false;
  }
  AUD_SoundInfo info = AUD_getInfo(playback_handle);
  sound_info->specs.channels = (eSoundChannels)info.specs.channels;
  sound_info->length = info.length;
  sound_info->specs.samplerate = info.specs.rate;
  return true;
}

bool BKE_sound_info_get(Main *main, bSound *sound, SoundInfo *sound_info)
{
  if (sound->playback_handle != nullptr) {
    return sound_info_from_playback_handle(sound->playback_handle, sound_info);
  }
  /* TODO(sergey): Make it fully independent audio handle. */
  /* Don't free waveforms during non-destructive queries.
   * This causes unnecessary recalculation - see #69921 */
  sound_load_audio(main, sound, false);
  const bool result = sound_info_from_playback_handle(sound->playback_handle, sound_info);
  sound_free_audio(sound);
  return result;
}

bool BKE_sound_stream_info_get(Main *main,
                               const char *filepath,
                               int stream,
                               SoundStreamInfo *sound_info)
{
  const char *blendfile_path = BKE_main_blendfile_path(main);
  char filepath_abs[FILE_MAX];
  AUD_Sound *sound;
  AUD_StreamInfo *stream_infos;
  int stream_count;

  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, blendfile_path);

  sound = AUD_Sound_file(filepath_abs);
  if (!sound) {
    return false;
  }

  stream_count = AUD_Sound_getFileStreams(sound, &stream_infos);

  AUD_Sound_free(sound);

  if (!stream_infos) {
    return false;
  }

  if ((stream < 0) || (stream >= stream_count)) {
    free(stream_infos);
    return false;
  }

  sound_info->start = stream_infos[stream].start;
  sound_info->duration = stream_infos[stream].duration;

  free(stream_infos);

  return true;
}

#else /* WITH_AUDASPACE */

#  include "BLI_utildefines.h"

void BKE_sound_force_device(const char * /*device*/) {}
void BKE_sound_init_once() {}
void BKE_sound_init(Main * /*bmain*/) {}
void BKE_sound_exit() {}
void BKE_sound_exit_once() {}
void BKE_sound_cache(bSound * /*sound*/) {}
void BKE_sound_delete_cache(bSound * /*sound*/) {}
void BKE_sound_load(Main * /*bmain*/, bSound * /*sound*/) {}
void BKE_sound_create_scene(Scene * /*scene*/) {}
void BKE_sound_destroy_scene(Scene * /*scene*/) {}
void BKE_sound_lock() {}
void BKE_sound_unlock() {}
void BKE_sound_reset_scene_specs(Scene * /*scene*/) {}
void BKE_sound_mute_scene(Scene * /*scene*/, int /*muted*/) {}
void *BKE_sound_scene_add_scene_sound(Scene * /*scene*/,
                                      Sequence * /*sequence*/,
                                      int /*startframe*/,
                                      int /*endframe*/,
                                      int /*frameskip*/)
{
  return nullptr;
}
void *BKE_sound_scene_add_scene_sound_defaults(Scene * /*scene*/, Sequence * /*sequence*/)
{
  return nullptr;
}
void *BKE_sound_add_scene_sound(Scene * /*scene*/,
                                Sequence * /*sequence*/,
                                int /*startframe*/,
                                int /*endframe*/,
                                int /*frameskip*/)
{
  return nullptr;
}
void *BKE_sound_add_scene_sound_defaults(Scene * /*scene*/, Sequence * /*sequence*/)
{
  return nullptr;
}
void BKE_sound_remove_scene_sound(Scene * /*scene*/, void * /*handle*/) {}
void BKE_sound_mute_scene_sound(void * /*handle*/, bool /*mute*/) {}
void BKE_sound_move_scene_sound(const Scene * /*scene*/,
                                void * /*handle*/,
                                int /*startframe*/,
                                int /*endframe*/,
                                int /*frameskip*/,
                                double /*audio_offset*/)
{
}
void BKE_sound_move_scene_sound_defaults(Scene * /*scene*/, Sequence * /*sequence*/) {}
void BKE_sound_play_scene(Scene * /*scene*/) {}
void BKE_sound_stop_scene(Scene * /*scene*/) {}
void BKE_sound_seek_scene(Main * /*bmain*/, Scene * /*scene*/) {}
double BKE_sound_sync_scene(Scene * /*scene*/)
{
  return NAN_FLT;
}
int BKE_sound_scene_playing(Scene * /*scene*/)
{
  return -1;
}
void BKE_sound_read_waveform(Main *bmain,
                             bSound *sound,
                             /* NOLINTNEXTLINE: readability-non-const-parameter. */
                             bool *stop)
{
  UNUSED_VARS(sound, stop, bmain);
}
void BKE_sound_init_main(Main * /*bmain*/) {}
void BKE_sound_update_sequencer(Main * /*main*/, bSound * /*sound*/) {}
void BKE_sound_update_scene(Depsgraph * /*depsgraph*/, Scene * /*scene*/) {}
void BKE_sound_update_scene_sound(void * /*handle*/, bSound * /*sound*/) {}
void BKE_sound_update_scene_listener(Scene * /*scene*/) {}
void BKE_sound_update_fps(Main * /*bmain*/, Scene * /*scene*/) {}
void BKE_sound_set_scene_sound_volume_at_frame(void * /*handle*/,
                                               int /* frame */,
                                               float /*volume*/,
                                               char /*animated*/)
{
}
void BKE_sound_set_scene_sound_pan_at_frame(void * /*handle*/,
                                            int /* frame */,
                                            float /*pan*/,
                                            char /*animated*/)
{
}
void BKE_sound_set_scene_volume(Scene * /*scene*/, float /*volume*/) {}
void BKE_sound_set_scene_sound_pitch_at_frame(void * /*handle*/,
                                              int /*frame*/,
                                              float /*pitch*/,
                                              char /*animated*/)
{
}
void BKE_sound_set_scene_sound_pitch_constant_range(void * /*handle*/,
                                                    int /*frame_start*/,
                                                    int /*frame_end*/,
                                                    float /*pitch*/)
{
}
float BKE_sound_get_length(Main * /*bmain*/, bSound * /*sound*/)
{
  return 0;
}
char **BKE_sound_get_device_names()
{
  static char *names[1] = {nullptr};
  return names;
}

void BKE_sound_free_waveform(bSound * /*sound*/) {}

bool BKE_sound_info_get(Main * /*main*/, bSound * /*sound*/, SoundInfo * /*sound_info*/)
{
  return false;
}

bool BKE_sound_stream_info_get(Main * /*main*/,
                               const char * /*filepath*/,
                               int /*stream*/,
                               SoundStreamInfo * /*sound_info*/)
{
  return false;
}

#endif /* WITH_AUDASPACE */

void BKE_sound_reset_scene_runtime(Scene *scene)
{
  scene->sound_scene = nullptr;
  scene->playback_handle = nullptr;
  scene->sound_scrub_handle = nullptr;
  scene->speaker_handles = nullptr;
}

void BKE_sound_ensure_scene(Scene *scene)
{
  if (scene->sound_scene != nullptr) {
    return;
  }
  BKE_sound_create_scene(scene);
}

void BKE_sound_reset_runtime(bSound *sound)
{
  sound->cache = nullptr;
  sound->playback_handle = nullptr;
}

void BKE_sound_ensure_loaded(Main *bmain, bSound *sound)
{
  if (sound->cache != nullptr) {
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
  if (scene->playback_handle != nullptr) {
    AUD_Handle_setPosition(scene->playback_handle, time);
  }
#else
  UNUSED_VARS(time);
#endif
}

void BKE_sound_evaluate(Depsgraph *depsgraph, Main *bmain, bSound *sound)
{
  DEG_debug_print_eval(depsgraph, __func__, sound->id.name, sound);
  if (sound->id.recalc & ID_RECALC_SOURCE) {
    /* Sequencer checks this flag to see if the strip sound is to be updated from the Audaspace
     * side. */
    sound->id.recalc |= ID_RECALC_AUDIO;
  }

  if (sound->id.recalc & ID_RECALC_AUDIO) {
    BKE_sound_load(bmain, sound);
    return;
  }
  BKE_sound_ensure_loaded(bmain, sound);
}
