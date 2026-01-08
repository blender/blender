/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <optional>
#include <thread>

#include "MEM_guardedalloc.h"

#include "BLI_build_config.h"
#include "BLI_enum_flags.hh"
#include "BLI_listbase.h"
#include "BLI_math_base.h"
#include "BLI_math_rotation.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
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
#include "DNA_userdef_types.h"

#ifdef WITH_AUDASPACE
#  include "BLI_set.hh"

#  include <Exception.h>
#  include <IReader.h>
#  include <devices/DeviceManager.h>
#  include <devices/IDeviceFactory.h>
#  include <devices/IHandle.h>
#  include <devices/NULLDevice.h>
#  include <devices/ReadDevice.h>
#  include <file/File.h>
#  include <file/FileManager.h>
#  include <file/FileWriter.h>
#  include <fx/Accumulator.h>
#  include <fx/AnimateableTimeStretchPitchScale.h>
#  include <fx/Envelope.h>
#  include <fx/Highpass.h>
#  include <fx/Limiter.h>
#  include <fx/Lowpass.h>
#  include <fx/Sum.h>
#  include <fx/Threshold.h>
#  include <generator/Silence.h>
#  include <plugin/PluginManager.h>
#  include <respec/ChannelMapper.h>
#  include <respec/LinearResample.h>
#  include <sequence/Sequence.h>
#  include <sequence/SequenceEntry.h>
#  include <util/StreamBuffer.h>

#  include <fmt/format.h>

#endif

#include "BKE_bpath.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_packedFile.hh"
#include "BKE_scene_runtime.hh"
#include "BKE_sound.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "BLO_read_write.hh"

#include "SEQ_sequencer.hh"
#include "SEQ_sound.hh"

#include "CLG_log.h"

namespace blender {

namespace bke {
struct SceneAudioRuntime;

enum class SoundTags {
  None = 0,
  /* Do not free/reset waveform on sound load, only used by undo code. */
  WaveformNoReload = 1 << 0,
  WaveformLoading = 1 << 1,
};
ENUM_OPERATORS(SoundTags);

struct SoundRuntime {
  AUD_Sound handle;
  AUD_Sound cache;
  /* The audaspace handle that should actually be played back.
   * Should be cache if cache != NULL; otherwise its handle. */
  AUD_Sound playback_handle;
  /* Spin-lock for asynchronous loading of sounds. */
  SpinLock spinlock;

  /* Note: not by-value since #sound_foreach_cache can only
   * save/restore a pointer. */
  Vector<float> *waveform = nullptr;
  SoundTags tags = SoundTags::None;
};

}  // namespace bke

static void sound_free_audio(bSound *sound);

static void sound_init_runtime(bSound *sound)
{
  sound->runtime = MEM_new<bke::SoundRuntime>(__func__);
  BLI_spin_init(&sound->runtime->spinlock);
}

static void sound_free_waveform(bSound *sound)
{
  bke::SoundRuntime *runtime = sound->runtime;
  if (!flag_is_set(runtime->tags, bke::SoundTags::WaveformNoReload)) {
    MEM_SAFE_DELETE(runtime->waveform);
  }
  /* This tag is only valid once. */
  runtime->tags &= ~bke::SoundTags::WaveformNoReload;
}

static void sound_copy_data(Main * /*bmain*/,
                            std::optional<Library *> /*owner_library*/,
                            ID *id_dst,
                            const ID *id_src,
                            const int /*flag*/)
{
  bSound *sound_dst = id_cast<bSound *>(id_dst);
  const bSound *sound_src = id_cast<const bSound *>(id_src);

  /* Just to be sure, should not have any value actually after reading time. */
  sound_dst->newpackedfile = nullptr;

  if (sound_src->packedfile != nullptr) {
    sound_dst->packedfile = BKE_packedfile_duplicate(sound_src->packedfile);
  }

  sound_init_runtime(sound_dst);
}

static void sound_free_data(ID *id)
{
  bSound *sound = id_cast<bSound *>(id);

  if (sound->packedfile) {
    BKE_packedfile_free(sound->packedfile);
    sound->packedfile = nullptr;
  }

  sound_free_audio(sound);
  sound_free_waveform(sound);
  BLI_spin_end(&sound->runtime->spinlock);
  MEM_delete(sound->runtime);
}

static void sound_foreach_cache(ID *id,
                                IDTypeForeachCacheFunctionCallback function_callback,
                                void *user_data)
{
  bSound *sound = id_cast<bSound *>(id);
  IDCacheKey key = {id->session_uid, 1};
  function_callback(id, &key, reinterpret_cast<void **>(&sound->runtime->waveform), 0, user_data);
}

static void sound_foreach_path(ID *id, BPathForeachPathData *bpath_data)
{
  bSound *sound = id_cast<bSound *>(id);
  if (sound->packedfile != nullptr && (bpath_data->flag & BKE_BPATH_FOREACH_PATH_SKIP_PACKED) != 0)
  {
    return;
  }

  /* FIXME: This does not check for empty path... */
  BKE_bpath_foreach_path_fixed_process(bpath_data, sound->filepath, sizeof(sound->filepath));
}

static void sound_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bSound *sound = id_cast<bSound *>(id);
  const bool is_undo = BLO_write_is_undo(writer);

  /* Clean up, important in undo case to reduce false detection of changed datablocks. */
  sound->runtime = nullptr;

  /* Do not store packed files in case this is a library override ID. */
  if (ID_IS_OVERRIDE_LIBRARY(sound) && !is_undo) {
    sound->packedfile = nullptr;
  }

  /* write LibData */
  writer->write_id_struct(id_address, sound);
  BKE_id_blend_write(writer, &sound->id);

  BKE_packedfile_blend_write(writer, sound->packedfile);
}

static void sound_blend_read_data(BlendDataReader *reader, ID *id)
{
  bSound *sound = id_cast<bSound *>(id);
  sound_init_runtime(sound);
  if (BLO_read_data_is_undo(reader)) {
    sound->runtime->tags |= bke::SoundTags::WaveformNoReload;
  }

  BKE_packedfile_blend_read(reader, &sound->packedfile, sound->filepath);
  BKE_packedfile_blend_read(reader, &sound->newpackedfile, sound->filepath);
}

IDTypeInfo IDType_ID_SO = {
    /*id_code*/ bSound::id_type,
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
    /*foreach_id*/ nullptr,
    /*foreach_cache*/ sound_foreach_cache,
    /*foreach_path*/ sound_foreach_path,
    /*foreach_working_space_color*/ nullptr,
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
   * ID_TAG_COPIED_ON_EVAL tag set on them. But if some of data-blocks during its evaluation
   * decides to re-allocate its nested one (for example, object evaluation could re-allocate mesh
   * when evaluating modifier stack). Such data-blocks will have
   * ID_TAG_COPIED_ON_EVAL_FINAL_RESULT tag set on them.
   *
   * Additionally, we also allow data-blocks outside of main database. Those can not be "original"
   * and could be used as a temporary evaluated result during operations like baking.
   *
   * NOTE: We consider ID evaluated if ANY of those flags is set. We do NOT require ALL of them.
   */
  BLI_assert(id->tag &
             (ID_TAG_COPIED_ON_EVAL | ID_TAG_COPIED_ON_EVAL_FINAL_RESULT | ID_TAG_NO_MAIN));
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
  sound_init_runtime(sound);

  /* Extract sound specs for bSound */
  SoundInfo info;
  bool success = BKE_sound_info_get(bmain, sound, &info);
  if (success) {
    sound->samplerate = info.specs.samplerate;
    sound->audio_channels = info.specs.channels;
  }

  return sound;
}

static bSound *sound_new_file_exists_ex(Main *bmain, const char *filepath, bool *r_exists)
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
  return sound_new_file_exists_ex(bmain, filepath, nullptr);
}

static void sound_free_audio(bSound *sound)
{
#ifdef WITH_AUDASPACE
  bke::SoundRuntime *runtime = sound->runtime;
  runtime->handle.reset();
  runtime->playback_handle.reset();
  runtime->cache.reset();
#else
  UNUSED_VARS(sound);
#endif /* WITH_AUDASPACE */
}

#ifdef WITH_AUDASPACE
static CLG_LogRef LOG = {"sound"};

namespace {

struct GlobalState {
  const char *force_device = nullptr;

  /* Parameters of the opened device */
  const char *device_name = nullptr;
  aud::DeviceSpecs initialized_specs;

  /* Device handle and its synchronization mutex. */
  AUD_Device sound_device;
  int buffer_size = 0;
  std::mutex sound_device_mutex;

  bool need_exit = false;
  bool use_delayed_close = true;
  std::thread delayed_close_thread;
  std::condition_variable delayed_close_cv;

  int num_device_users = 0;
  std::chrono::time_point<std::chrono::steady_clock> last_user_disconnect_time_point;

  ~GlobalState()
  {
    /* Ensure that we don't end up in a deadlock if the global state is being cleaned up
     * before BKE_sound_exit_once has been called. (For example if someone called exit()
     * to quickly close the program without cleaning up)
     *
     * If we don't do this, we could end up in a state where this destructor is waiting for
     * other threads to let go of delayed_close_cv forever. See #146640.
     */
    exit_threads();
  }

  void exit_threads()
  {
    {
      std::unique_lock lock(sound_device_mutex);
      need_exit = true;
    }

    if (delayed_close_thread.joinable()) {
      delayed_close_cv.notify_all();
      delayed_close_thread.join();
    }
  }
};

GlobalState g_state;
}  // namespace

static void sound_device_close_no_lock()
{
  if (g_state.sound_device) {
    CLOG_DEBUG(&LOG, "Closing audio device");
    bke::sound_device_exit();
    g_state.sound_device = nullptr;
  }
}

static void sound_device_open_no_lock(const aud::DeviceSpecs &requested_specs)
{
  BLI_assert(!g_state.sound_device);

  CLOG_DEBUG(&LOG, "Opening audio device name:%s", g_state.device_name);

  g_state.sound_device = bke::sound_device_init(
      g_state.device_name, requested_specs, g_state.buffer_size, "Blender");
  if (!g_state.sound_device) {
    g_state.sound_device = bke::sound_device_init(
        "None", requested_specs, g_state.buffer_size, "Blender");
  }

  g_state.initialized_specs = g_state.sound_device->getSpecs();
}

static void sound_device_use_begin()
{
  ++g_state.num_device_users;

  if (g_state.sound_device) {
    return;
  }

  sound_device_open_no_lock(g_state.initialized_specs);
}

static void sound_device_use_end_after(const std::chrono::milliseconds after_ms)
{
  BLI_assert(g_state.num_device_users > 0);
  if (g_state.num_device_users == 0) {
    return;
  }

  --g_state.num_device_users;
  if (g_state.num_device_users == 0) {
    g_state.last_user_disconnect_time_point = std::chrono::steady_clock::now() + after_ms;
    g_state.delayed_close_cv.notify_one();
  }
}

static void sound_device_use_end()
{
  sound_device_use_end_after(std::chrono::milliseconds(0));
}

/* Return true if we need a thread which checks for device usage and closes it when it is inactive.
 * Only runtime-invariant checks are done here, such as possible platform-specific requirements.
 */
static bool sound_use_close_thread()
{
  /* No point starting a thread if sound is disabled and we're running headless. */
  if (g_state.force_device && STREQ(g_state.force_device, "None")) {
#  if defined(WITH_PYTHON_MODULE) || defined(WITH_HEADLESS)
    return false;
#  endif
    if (G.background) {
      return false;
    }
  }

#  if OS_MAC
  /* Closing audio device on macOS prior to 15.2 could lead to interference with other software.
   * See #121911 for details. */
  if (__builtin_available(macOS 15.2, *)) {
    return true;
  }
  return false;
#  else
  return true;
#  endif
}

static void delayed_close_thread_run()
{
  constexpr std::chrono::milliseconds device_close_delay{30000};

  std::unique_lock lock(g_state.sound_device_mutex);

  while (!g_state.need_exit) {
    if (!g_state.use_delayed_close) {
      CLOG_DEBUG(&LOG, "Delayed device close is disabled");
      /* Don't do anything here as delayed close is disabled.
       * Wait so that we don't spin around in the while loop. */
      g_state.delayed_close_cv.wait(lock);
      continue;
    }

    if (g_state.num_device_users == 0) {
      if (g_state.sound_device == nullptr) {
        /* There are no device users, wait until there is device to be waited for to close. */
        g_state.delayed_close_cv.wait(lock);
      }
      else {
        g_state.delayed_close_cv.wait_until(
            lock, g_state.last_user_disconnect_time_point + device_close_delay);
      }
    }
    else {
      /* If there are active device users wait indefinitely, until the system is requested to be
       * closed or the user stops using device.
       * It is not really guaranteed that the CV is notified for every user that stops using
       * device, only the last one is guaranteed to notify the CV. */
      g_state.delayed_close_cv.wait(lock);
    }

    if (g_state.need_exit) {
      CLOG_DEBUG(&LOG, "System exit requested");
      break;
    }

    if (!g_state.use_delayed_close) {
      /* Take into account corner case where you switch from a delayed close device while Blender
       * is running and a delayed close has already been queued up. */
      continue;
    }

    if (!g_state.sound_device) {
      CLOG_DEBUG(&LOG, "Device is not open, nothing to do");
      continue;
    }

    CLOG_DEBUG(&LOG, "Checking last device usage and timestamp");

    if (g_state.num_device_users) {
      CLOG_DEBUG(&LOG, "Device is used by %d user(s)", g_state.num_device_users);
      continue;
    }

    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if ((now - g_state.last_user_disconnect_time_point) >= device_close_delay) {
      sound_device_close_no_lock();
    }
  }

  CLOG_DEBUG(&LOG, "Delayed device close thread finished");
}

static SoundJackSyncCallback sound_jack_sync_callback = nullptr;

static void sound_sync_callback(void *data, int mode, float time)
{
  if (sound_jack_sync_callback == nullptr) {
    return;
  }
  Main *bmain = (Main *)data;
  sound_jack_sync_callback(bmain, mode, time);
}

void BKE_sound_force_device(const char *device)
{
  g_state.force_device = device;
}

void BKE_sound_init_once()
{
  bke::sound_system_initialize();
  if (sound_use_close_thread()) {
    CLOG_DEBUG(&LOG, "Using delayed device close thread");
    g_state.delayed_close_thread = std::thread(delayed_close_thread_run);
  }
}

void BKE_sound_exit_once()
{
  g_state.exit_threads();

  std::lock_guard lock(g_state.sound_device_mutex);
  sound_device_close_no_lock();

  if (audio_device_names != nullptr) {
    int i;
    for (i = 0; audio_device_names[i]; i++) {
      free(audio_device_names[i]);
    }
    free(audio_device_names);
    audio_device_names = nullptr;
  }
}

void BKE_sound_init(Main *bmain)
{
  std::lock_guard lock(g_state.sound_device_mutex);
  /* Make sure no instance of the sound system is running, otherwise we get leaks. */
  sound_device_close_no_lock();

  aud::DeviceSpecs requested_specs;
  requested_specs.channels = aud::Channels(U.audiochannels);
  requested_specs.format = aud::SampleFormat(U.audioformat);
  requested_specs.rate = U.audiorate;

  if (g_state.force_device == nullptr) {
    char **names = BKE_sound_get_device_names();
    g_state.device_name = names[0];

    /* make sure device is within the bounds of the array */
    for (int i = 0; names[i]; i++) {
      if (i == U.audiodevice) {
        g_state.device_name = names[i];
      }
    }
  }
  else {
    g_state.device_name = g_state.force_device;
  }

  g_state.buffer_size = U.mixbufsize < 128 ? 1024 : U.mixbufsize;

  if (requested_specs.rate < double(aud::RATE_8000)) {
    requested_specs.rate = aud::RATE_48000;
  }

  if (requested_specs.format <= aud::FORMAT_INVALID) {
    requested_specs.format = aud::FORMAT_S16;
  }

  if (requested_specs.channels <= aud::CHANNELS_INVALID) {
    requested_specs.channels = aud::CHANNELS_STEREO;
  }

  /* Make sure that we have our initalized_specs */
  sound_device_open_no_lock(requested_specs);
  if (STR_ELEM(g_state.device_name, "JACK", "PulseAudio", "PipeWire")) {
    /* JACK:
     * Do not close the device when using JACK. If we close it, we will not be able to
     * respond to JACK audio bus commands.
     *
     * PulseAudio, PipeWire:
     * These APIs are built around the idea that the program using them keeps the device open.
     * Instead it uses audio streams to determine if something is playing back audio or not.
     * These streams are only active when Audaspace is playing back, so we don't need to
     * do anything manually.
     * If we close these devices, it will become very hard and tedious for end users to
     * control the volume or route audio from Blender.
     */
    g_state.use_delayed_close = false;
    aud::DeviceManager::getDevice()->setSyncCallback(sound_sync_callback, bmain);
  }
  else {
    g_state.use_delayed_close = true;
    sound_device_close_no_lock();
  }
}

void BKE_sound_refresh_callback_bmain(Main *bmain)
{
  std::lock_guard lock(g_state.sound_device_mutex);
  if (g_state.sound_device) {
    aud::DeviceManager::getDevice()->setSyncCallback(sound_sync_callback, bmain);
  }
}

static void sound_load_audio(Main *bmain, bSound *sound, bool free_waveform)
{
  bke::SoundRuntime *runtime = sound->runtime;
  runtime->cache.reset();
  runtime->handle.reset();
  runtime->playback_handle.reset();
  if (free_waveform) {
    sound_free_waveform(sound);
  }

  {
    char fullpath[FILE_MAX];

    /* load sound */
    PackedFile *pf = sound->packedfile;

    /* Don't modify `sound->filepath`, only change a copy. */
    STRNCPY(fullpath, sound->filepath);
    BLI_path_abs(fullpath, ID_BLEND_PATH(bmain, &sound->id));

    /* but we need a packed file then */
    if (pf) {
      runtime->handle = AUD_Sound(new aud::File((uchar *)pf->data, pf->size));
    }
    else {
      /* or else load it from disk */
      runtime->handle = AUD_Sound(new aud::File(fullpath));
    }
  }
  if (sound->flags & SOUND_FLAGS_MONO) {
    aud::DeviceSpecs specs;
    specs.channels = aud::CHANNELS_MONO;
    specs.rate = aud::RATE_INVALID;
    specs.format = aud::FORMAT_INVALID;
    runtime->handle = AUD_Sound(new aud::ChannelMapper(runtime->handle, specs));
  }

  if (sound->flags & SOUND_FLAGS_CACHING) {
    try {
      runtime->cache = AUD_Sound(new aud::StreamBuffer(runtime->handle));
    }
    catch (aud::Exception &) {
    }
  }

  if (runtime->cache) {
    runtime->playback_handle = runtime->cache;
  }
  else {
    runtime->playback_handle = runtime->handle;
  }
}

void BKE_sound_load(Main *bmain, bSound *sound)
{
  sound_verify_evaluated_id(&sound->id);
  sound_load_audio(bmain, sound, true);
}

void BKE_sound_packfile_ensure(Main *bmain, bSound *sound, ReportList *reports)
{
  if (sound->packedfile != nullptr) {
    /* Sound is already packed and considered unmodified, do not attempt to repack it, since its
     * original file may not be available anymore on the current FS.
     *
     * See #152638.
     */
    return;
  }

  sound->packedfile = BKE_packedfile_new(
      reports, sound->filepath, ID_BLEND_PATH(bmain, &sound->id));
}

AUD_Device BKE_sound_mixdown(const Scene *scene,
                             const aud::DeviceSpecs &specs,
                             int start,
                             float volume)
{
  sound_verify_evaluated_id(&scene->id);
  try {
    std::shared_ptr<aud::ReadDevice> device(new aud::ReadDevice(specs));
    device->setQuality(aud::ResampleQuality::MEDIUM);
    device->setVolume(volume);

    aud::Sequence *f = dynamic_cast<aud::Sequence *>(scene->runtime->audio.sound_scene.get());
    f->setSpecs(specs.specs);

    AUD_Handle handle = device->play(f->createQualityReader(aud::ResampleQuality::MEDIUM));
    if (handle.get()) {
      handle->seek(start / scene->frames_per_second());
    }

    return device;
  }
  catch (aud::Exception &) {
    return nullptr;
  }
}

void BKE_sound_create_scene(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  /* should be done in version patch, but this gets called before */
  if (scene->r.frs_sec_base == 0) {
    scene->r.frs_sec_base = 1;
  }

  bke::SceneAudioRuntime &audio = scene->runtime->audio;

  aud::Specs specs;
  specs.channels = aud::CHANNELS_STEREO;
  specs.rate = aud::RATE_48000;
  audio.sound_scene = AUD_Sequence(
      new aud::Sequence(specs, scene->frames_per_second(), scene->audio.flag & AUDIO_MUTE));
  audio.sound_scene->setSpeedOfSound(scene->audio.speed_of_sound);
  audio.sound_scene->setDopplerFactor(scene->audio.doppler_factor);
  audio.sound_scene->setDistanceModel(aud::DistanceModel(scene->audio.distance_model));
  audio.playback_handle = nullptr;
  audio.sound_scrub_handle = nullptr;
  audio.speaker_handles.clear();
}

void BKE_sound_destroy_scene(Scene *scene)
{
  bke::SceneAudioRuntime &audio = scene->runtime->audio;
  if (audio.playback_handle) {
    audio.playback_handle->stop();
    audio.playback_handle.reset();
  }
  if (audio.sound_scrub_handle) {
    audio.sound_scrub_handle->stop();
    audio.sound_scrub_handle.reset();
  }
  for (AUD_SequenceEntry handle : audio.speaker_handles) {
    audio.sound_scene->remove(handle);
  }
  audio.speaker_handles.clear();
  audio.sound_scene.reset();
}

void BKE_sound_lock()
{
  g_state.sound_device_mutex.lock();
  if (g_state.sound_device != nullptr) {
    g_state.sound_device->lock();
  }
}

void BKE_sound_unlock()
{
  g_state.sound_device_mutex.unlock();
  if (g_state.sound_device != nullptr) {
    g_state.sound_device->unlock();
  }
}

void BKE_sound_reset_scene_specs(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  if (scene->runtime->audio.sound_scene) {
    scene->runtime->audio.sound_scene->setSpecs(g_state.initialized_specs.specs);
  }
}

void BKE_sound_mute_scene(Scene *scene, bool muted)
{
  sound_verify_evaluated_id(&scene->id);
  if (scene->runtime->audio.sound_scene) {
    scene->runtime->audio.sound_scene->mute(muted);
  }
}

void BKE_sound_update_fps(Main *bmain, Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  if (scene->runtime->audio.sound_scene) {
    scene->runtime->audio.sound_scene->setFPS(scene->frames_per_second());
  }

  seq::sound_update_length(bmain, scene);
}

void BKE_sound_update_scene_listener(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  AUD_Sequence sound = scene->runtime->audio.sound_scene;
  sound->setSpeedOfSound(scene->audio.speed_of_sound);
  sound->setDopplerFactor(scene->audio.doppler_factor);
  sound->setDistanceModel(aud::DistanceModel(scene->audio.distance_model));
}

AUD_SequenceEntry BKE_sound_scene_add_scene_sound(Scene *scene, Strip *strip)
{
  sound_verify_evaluated_id(&scene->id);
  if (strip->scene && scene != strip->scene) {
    int startframe = strip->left_handle();
    int endframe = strip->right_handle(scene);
    int frameskip = strip->startofs + strip->anim_startofs;
    const double fps = scene->frames_per_second();
    return AUD_SequenceEntry(
        scene->runtime->audio.sound_scene->add(strip->scene->runtime->audio.sound_scene,
                                               startframe / fps,
                                               endframe / fps,
                                               frameskip / fps));
  }
  return nullptr;
}

AUD_SequenceEntry BKE_sound_add_scene_sound(Scene *scene, Strip *strip)
{
  sound_verify_evaluated_id(&scene->id);
  /* Happens when sequence's sound data-block was removed. */
  if (strip->sound == nullptr) {
    return nullptr;
  }
  sound_verify_evaluated_id(&strip->sound->id);

  int startframe = strip->left_handle();
  int endframe = strip->right_handle(scene);
  int frameskip = strip->startofs + strip->anim_startofs;

  const double fps = scene->frames_per_second();
  const double offset_time = strip->sound->offset_time + strip->sound_offset - frameskip / fps;
  if (offset_time >= 0.0f) {
    return AUD_SequenceEntry(
        scene->runtime->audio.sound_scene->add(strip->sound->runtime->playback_handle,
                                               startframe / fps + offset_time,
                                               endframe / fps,
                                               0.0f));
  }
  return AUD_SequenceEntry(scene->runtime->audio.sound_scene->add(
      strip->sound->runtime->playback_handle, startframe / fps, endframe / fps, -offset_time));
}

void BKE_sound_remove_scene_sound(Scene *scene, AUD_SequenceEntry handle)
{
  scene->runtime->audio.sound_scene->remove(handle);
}

void BKE_sound_mute_scene_sound(AUD_SequenceEntry handle, bool mute)
{
  handle->mute(mute);
}

void BKE_sound_move_scene_sound(const Scene *scene,
                                AUD_SequenceEntry handle,
                                int startframe,
                                int endframe,
                                int frameskip,
                                double audio_offset)
{
  sound_verify_evaluated_id(&scene->id);
  const double fps = scene->frames_per_second();
  const double offset_time = audio_offset - frameskip / fps;
  if (offset_time >= 0.0f) {
    handle->move(startframe / fps + offset_time, endframe / fps, 0.0f);
  }
  else {
    handle->move(startframe / fps, endframe / fps, -offset_time);
  }
}

void BKE_sound_move_scene_sound_defaults(Scene *scene, Strip *strip)
{
  sound_verify_evaluated_id(&scene->id);
  if (strip->runtime->scene_sound) {
    double offset_time = 0.0f;
    if (strip->sound != nullptr) {
      offset_time = strip->sound->offset_time + strip->sound_offset;
    }
    BKE_sound_move_scene_sound(scene,
                               strip->runtime->scene_sound,
                               strip->left_handle(),
                               strip->right_handle(scene),
                               strip->startofs + strip->anim_startofs,
                               offset_time);
  }
}

void BKE_sound_update_scene_sound(AUD_SequenceEntry handle, bSound *sound)
{
  handle->setSound(sound->runtime->playback_handle);
}

#endif /* WITH_AUDASPACE */

void BKE_sound_update_sequence_handle(AUD_SequenceEntry handle, AUD_Sound sound_handle)
{
#ifdef WITH_AUDASPACE
  handle->setSound(sound_handle);
#else
  UNUSED_VARS(handle, sound_handle);
#endif
}

#ifdef WITH_AUDASPACE

template<typename T>
static void set_audaspace_anim_property(std::shared_ptr<T> sound,
                                        aud::AnimateablePropertyType type,
                                        const int frame,
                                        float *value,
                                        bool animated)
{
  aud::AnimateableProperty *prop = sound->getAnimProperty(type);
  if (animated) {
    if (frame >= 0) {
      prop->write(value, frame, 1);
    }
  }
  else {
    prop->write(value);
  }
}

void BKE_sound_set_scene_volume(Scene *scene, float volume)
{
  sound_verify_evaluated_id(&scene->id);
  if (scene->runtime->audio.sound_scene == nullptr) {
    return;
  }
  const bool animated = (scene->audio.flag & AUDIO_VOLUME_ANIMATED) != 0;
  const int frame = scene->r.cfra;
  set_audaspace_anim_property(
      scene->runtime->audio.sound_scene, aud::AP_VOLUME, frame, &volume, animated);
}

void BKE_sound_set_scene_sound_volume_at_frame(AUD_SequenceEntry handle,
                                               const int frame,
                                               float volume,
                                               bool animated)
{
  set_audaspace_anim_property(handle, aud::AP_VOLUME, frame, &volume, animated);
}

void BKE_sound_set_scene_sound_pitch_at_frame(AUD_SequenceEntry handle,
                                              const int frame,
                                              float pitch,
                                              bool animated)
{
  set_audaspace_anim_property(handle, aud::AP_PITCH, frame, &pitch, animated);
}

void BKE_sound_set_scene_sound_pitch_constant_range(AUD_SequenceEntry handle,
                                                    int frame_start,
                                                    int frame_end,
                                                    float pitch)
{
  frame_start = max_ii(0, frame_start);
  frame_end = max_ii(0, frame_end);
  aud::AnimateableProperty *prop = handle->getAnimProperty(aud::AP_PITCH);
  prop->writeConstantRange(&pitch, frame_start, frame_end);
}

void BKE_sound_set_scene_sound_pan_at_frame(AUD_SequenceEntry handle,
                                            const int frame,
                                            float pan,
                                            bool animated)
{
  set_audaspace_anim_property(handle, aud::AP_PANNING, frame, &pan, animated);
}

void BKE_sound_update_sequencer(Main *main, bSound *sound)
{
  BLI_assert_msg(0, "is not supposed to be used, is weird function.");

  Scene *scene;

  for (scene = static_cast<Scene *>(main->scenes.first); scene;
       scene = static_cast<Scene *>(scene->id.next))
  {
    seq::sound_update(scene, sound);
  }
}

/* This function assumes that you have already held the g_state.sound_device mutex. */
static void sound_start_play_scene(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  bke::SceneAudioRuntime &audio = scene->runtime->audio;
  if (audio.playback_handle) {
    audio.playback_handle->stop();
    audio.playback_handle.reset();
  }

  BKE_sound_reset_scene_specs(scene);

  audio.playback_handle = bke::sound_device_play(g_state.sound_device, audio.sound_scene);
  if (audio.playback_handle) {
    audio.playback_handle->setLoopCount(-1);
  }
}

void BKE_sound_play_scene(Scene *scene)
{
  std::lock_guard lock(g_state.sound_device_mutex);
  sound_device_use_begin();
  sound_verify_evaluated_id(&scene->id);

  const double cur_time = FRA2TIME(scene->r.cfra + scene->r.subframe);

  g_state.sound_device->lock();

  bke::SceneAudioRuntime &audio = scene->runtime->audio;
  if (audio.sound_scrub_handle && audio.sound_scrub_handle->getStatus() != aud::STATUS_INVALID) {
    /* If the audio scrub handle is playing back, stop to make sure it is not active.
     * Otherwise, it will trigger a callback that will stop audio playback. */
    audio.sound_scrub_handle->stop();
    audio.sound_scrub_handle = nullptr;
    /* The scrub_handle started playback with playback_handle, stop it so we can
     * properly restart it. */
    audio.playback_handle->pause();
  }

  aud::Status status = audio.playback_handle ? audio.playback_handle->getStatus() :
                                               aud::STATUS_INVALID;

  if (status == aud::STATUS_INVALID) {
    sound_start_play_scene(scene);

    if (!audio.playback_handle) {
      g_state.sound_device->unlock();
      return;
    }
  }

  if (status != aud::STATUS_PLAYING) {
    /* Seeking the synchronizer will also seek the playback handle.
     * Even if we don't have A/V sync on, keep the synchronizer and handle seek time in sync. */
    aud::DeviceManager::getDevice()->seekSynchronizer(cur_time);
    audio.playback_handle->seek(cur_time);
    audio.playback_handle->resume();
  }

  if (scene->audio.flag & AUDIO_SYNC) {
    aud::DeviceManager::getDevice()->playSynchronizer();
  }

  g_state.sound_device->unlock();
}

void BKE_sound_stop_scene(Scene *scene)
{
  std::lock_guard lock(g_state.sound_device_mutex);
  BLI_assert(g_state.sound_device);
  if (scene->runtime->audio.playback_handle) {
    scene->runtime->audio.playback_handle->pause();

    if (scene->audio.flag & AUDIO_SYNC) {
      aud::DeviceManager::getDevice()->stopSynchronizer();
    }
  }
  sound_device_use_end();
}

void BKE_sound_seek_scene(Main *bmain, Scene *scene)
{
  std::lock_guard lock(g_state.sound_device_mutex);
  bool animation_playing = false;
  for (bScreen *screen = static_cast<bScreen *>(bmain->screens.first); screen;
       screen = static_cast<bScreen *>(screen->id.next))
  {
    if (screen->animtimer) {
      animation_playing = true;
      break;
    }
  }

  bool do_audio_scrub = scene->audio.flag & AUDIO_SCRUB && !animation_playing;

  if (do_audio_scrub) {
    /* Make sure the sound device is open for scrubbing. */
    sound_device_use_begin();
  }
  else if (g_state.sound_device == nullptr) {
    /* Nothing to do if there is no sound device and we are not doing audio scrubbing. */
    return;
  }
  sound_verify_evaluated_id(&scene->id);

  g_state.sound_device->lock();

  bke::SceneAudioRuntime &audio = scene->runtime->audio;
  aud::Status status = audio.playback_handle ? audio.playback_handle->getStatus() :
                                               aud::STATUS_INVALID;
  if (status == aud::STATUS_INVALID) {
    sound_start_play_scene(scene);

    if (!audio.playback_handle) {
      g_state.sound_device->unlock();
      if (do_audio_scrub) {
        sound_device_use_end();
      }
      return;
    }

    audio.playback_handle->pause();
  }

  const double one_frame = 1.0 / scene->frames_per_second() +
                           (U.audiorate > 0 ? U.mixbufsize / double(U.audiorate) : 0.0);
  const double cur_time = FRA2TIME(scene->r.cfra);

  if (do_audio_scrub) {
    /* Playback one frame of audio without advancing the timeline. */
    audio.playback_handle->seek(cur_time);
    audio.playback_handle->resume();
    if (audio.sound_scrub_handle && audio.sound_scrub_handle->getStatus() != aud::STATUS_INVALID) {
      audio.sound_scrub_handle->seek(0);
    }
    else {
      if (audio.sound_scrub_handle) {
        audio.sound_scrub_handle->stop();
      }
      audio.sound_scrub_handle = bke::sound_pause_after(audio.playback_handle, one_frame);
    }
    sound_device_use_end_after(std::chrono::milliseconds(int(one_frame * 1000)));
  }
  else if (status == aud::STATUS_PLAYING) {
    /* Seeking the synchronizer will also seek the playback handle.
     * Even if we don't have A/V sync on, keep the synchronizer and handle
     * seek time in sync.
     */
    aud::DeviceManager::getDevice()->seekSynchronizer(cur_time);
    audio.playback_handle->seek(cur_time);
  }

  g_state.sound_device->unlock();
}

double BKE_sound_sync_scene(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  /* Ugly: Blender doesn't like it when the animation is played back during rendering */
  if (G.is_rendering) {
    return NAN_FLT;
  }

  if (scene->runtime->audio.playback_handle) {
    if (scene->audio.flag & AUDIO_SYNC) {
      return aud::DeviceManager::getDevice()->getSynchronizerPosition();
    }

    return scene->runtime->audio.playback_handle->getPosition();
  }
  return NAN_FLT;
}

static int sound_read(
    AUD_Sound sound, float *buffer, int length, int samples_per_second, bool *interrupt)
{
  using namespace aud;
  DeviceSpecs specs;
  float *buf;
  Buffer aBuffer;

  specs.rate = RATE_INVALID;
  specs.channels = CHANNELS_MONO;
  specs.format = FORMAT_INVALID;

  std::shared_ptr<IReader> reader = ChannelMapper(sound, specs).createReader();

  specs.specs = reader->getSpecs();
  int len;
  float samplejump = specs.rate / samples_per_second;
  float min, max, power, overallmax;
  bool eos;

  overallmax = 0;

  for (int i = 0; i < length; i++) {
    len = floor(samplejump * (i + 1)) - floor(samplejump * i);

    if (*interrupt)
      return 0;

    aBuffer.assureSize(len * AUD_SAMPLE_SIZE(specs));
    buf = aBuffer.getBuffer();

    reader->read(len, eos, buf);

    max = min = *buf;
    power = *buf * *buf;
    for (int j = 1; j < len; j++) {
      if (buf[j] < min)
        min = buf[j];
      if (buf[j] > max)
        max = buf[j];
      power += buf[j] * buf[j];
    }

    buffer[i * 3] = min;
    buffer[i * 3 + 1] = max;
    buffer[i * 3 + 2] = std::sqrt(power / len);

    if (overallmax < max)
      overallmax = max;
    if (overallmax < -min)
      overallmax = -min;

    if (eos) {
      length = i;
      break;
    }
  }

  if (overallmax > 1.0f) {
    for (int i = 0; i < length * 3; i++) {
      buffer[i] /= overallmax;
    }
  }

  return length;
}

void BKE_sound_read_waveform(Main *bmain, bSound *sound, bool *stop)
{
  bool need_close_audio_handles = false;
  bke::SoundRuntime *runtime = sound->runtime;
  if (runtime->playback_handle == nullptr) {
    /* TODO(sergey): Make it fully independent audio handle. */
    sound_load_audio(bmain, sound, true);
    need_close_audio_handles = true;
  }

  SoundInfo info = bke::sound_info_get(runtime->playback_handle);

  Vector<float> *waveform = MEM_new<Vector<float>>(__func__);
  if (info.length > 0) {
    int length = info.length * SOUND_WAVE_SAMPLES_PER_SECOND;

    waveform->resize(3 * length);
    length = sound_read(
        runtime->playback_handle, waveform->data(), length, SOUND_WAVE_SAMPLES_PER_SECOND, stop);
    waveform->resize(3 * length);
  }

  if (*stop) {
    MEM_SAFE_DELETE(runtime->waveform);
    BLI_spin_lock(&runtime->spinlock);
    runtime->tags &= ~bke::SoundTags::WaveformLoading;
    BLI_spin_unlock(&runtime->spinlock);
    return;
  }

  sound_free_waveform(sound);

  BLI_spin_lock(&runtime->spinlock);
  runtime->waveform = waveform;
  runtime->tags &= ~bke::SoundTags::WaveformLoading;
  BLI_spin_unlock(&runtime->spinlock);

  if (need_close_audio_handles) {
    sound_free_audio(sound);
  }
}

static void sound_update_base(Scene *scene, Object *object, Set<AUD_SequenceEntry> &new_set)
{
  Speaker *speaker;
  float quat[4];

  sound_verify_evaluated_id(&scene->id);
  sound_verify_evaluated_id(&object->id);

  if ((object->type != OB_SPEAKER) || !object->adt) {
    return;
  }

  for (NlaTrack &track : object->adt->nla_tracks) {
    for (NlaStrip &strip : track.strips) {
      if (strip.type != NLASTRIP_TYPE_SOUND) {
        continue;
      }
      speaker = (Speaker *)object->data;

      bke::NlaStripRuntime &strip_runtime = strip.runtime_get();

      if (scene->runtime->audio.speaker_handles.remove(strip_runtime.speaker_handle)) {
        if (speaker->sound) {
          strip_runtime.speaker_handle->move(
              double(strip.start) / scene->frames_per_second(), FLT_MAX, 0);
        }
        else {
          scene->runtime->audio.sound_scene->remove(strip_runtime.speaker_handle);
          strip_runtime.speaker_handle = nullptr;
        }
      }
      else {
        if (speaker->sound) {
          strip_runtime.speaker_handle = AUD_SequenceEntry(scene->runtime->audio.sound_scene->add(
              speaker->sound->runtime->playback_handle,
              double(strip.start) / scene->frames_per_second(),
              FLT_MAX,
              0));
          strip_runtime.speaker_handle->setRelative(false);
        }
      }

      if (strip_runtime.speaker_handle) {
        const bool mute = ((strip.flag & NLASTRIP_FLAG_MUTED) || (speaker->flag & SPK_MUTED));
        new_set.add(strip_runtime.speaker_handle);
        strip_runtime.speaker_handle->setVolumeMaximum(speaker->volume_max);
        strip_runtime.speaker_handle->setVolumeMinimum(speaker->volume_min);
        strip_runtime.speaker_handle->setDistanceMaximum(speaker->distance_max);
        strip_runtime.speaker_handle->setDistanceReference(speaker->distance_reference);
        strip_runtime.speaker_handle->setAttenuation(speaker->attenuation);
        strip_runtime.speaker_handle->setConeAngleOuter(speaker->cone_angle_outer);
        strip_runtime.speaker_handle->setConeAngleInner(speaker->cone_angle_inner);
        strip_runtime.speaker_handle->setConeVolumeOuter(speaker->cone_volume_outer);

        mat4_to_quat(quat, object->object_to_world().ptr());
        float3 location = object->object_to_world().location();
        set_audaspace_anim_property(
            strip_runtime.speaker_handle, aud::AP_LOCATION, scene->r.cfra, location, 1);
        set_audaspace_anim_property(
            strip_runtime.speaker_handle, aud::AP_ORIENTATION, scene->r.cfra, quat, 1);
        set_audaspace_anim_property(
            strip_runtime.speaker_handle, aud::AP_VOLUME, scene->r.cfra, &speaker->volume, 1);
        set_audaspace_anim_property(
            strip_runtime.speaker_handle, aud::AP_PITCH, scene->r.cfra, &speaker->pitch, 1);
        strip_runtime.speaker_handle->setSound(speaker->sound->runtime->playback_handle);
        strip_runtime.speaker_handle->mute(mute);
      }
    }
  }
}

void BKE_sound_update_scene(Depsgraph *depsgraph, Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  Set<AUD_SequenceEntry> new_set;
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

  bke::SceneAudioRuntime &audio = scene->runtime->audio;
  for (AUD_SequenceEntry handle : audio.speaker_handles) {
    audio.sound_scene->remove(handle);
  }
  audio.speaker_handles.clear();

  if (scene->camera) {
    mat4_to_quat(quat, scene->camera->object_to_world().ptr());
    float3 location = scene->camera->object_to_world().location();
    set_audaspace_anim_property(audio.sound_scene, aud::AP_LOCATION, scene->r.cfra, location, 1);
    set_audaspace_anim_property(audio.sound_scene, aud::AP_ORIENTATION, scene->r.cfra, quat, 1);
  }

  audio.speaker_handles = new_set;
}

AUD_Sound BKE_sound_get_factory(void *sound)
{
  return ((bSound *)sound)->runtime->playback_handle;
}

float BKE_sound_get_length(Main *bmain, bSound *sound)
{
  if (sound->runtime->playback_handle != nullptr) {
    SoundInfo info = bke::sound_info_get(sound->runtime->playback_handle);
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

    std::vector<std::string> v_names = aud::DeviceManager::getAvailableDeviceNames();
    char **names = (char **)malloc(sizeof(char *) * (v_names.size() + 1));

    for (int i = 0; i < v_names.size(); i++) {
      std::string name = v_names[i];
      names[i] = (char *)malloc(sizeof(char) * (name.length() + 1));
      strcpy(names[i], name.c_str());
    }
    names[v_names.size()] = nullptr;
    audio_device_names = names;
  }

  return audio_device_names;
}

bool BKE_sound_info_get(Main *main, bSound *sound, SoundInfo *sound_info)
{
  if (sound->runtime->playback_handle != nullptr) {
    *sound_info = bke::sound_info_get(sound->runtime->playback_handle);
    return true;
  }
  /* TODO(sergey): Make it fully independent audio handle. */
  /* Don't free waveforms during non-destructive queries.
   * This causes unnecessary recalculation - see #69921 */
  sound_load_audio(main, sound, false);
  const bool result = sound->runtime->playback_handle != nullptr;
  if (result) {
    *sound_info = bke::sound_info_get(sound->runtime->playback_handle);
  }
  sound_free_audio(sound);
  return result;
}

bool BKE_sound_stream_info_get(Main *main,
                               const char *filepath,
                               int stream,
                               SoundStreamInfo *sound_info)
{
  char filepath_abs[FILE_MAX];
  STRNCPY(filepath_abs, filepath);
  const char *blendfile_path = BKE_main_blendfile_path(main);
  BLI_path_abs(filepath_abs, blendfile_path);

  std::vector<aud::StreamInfo> streams;
  try {
    streams = aud::FileManager::queryStreams(filepath_abs);
  }
  catch (aud::Exception &) {
    return false;
  }

  if (stream < 0 || stream >= streams.size()) {
    return false;
  }

  sound_info->start = streams[stream].start;
  sound_info->duration = streams[stream].duration;
  return true;
}

#  ifdef WITH_RUBBERBAND
AUD_Sound BKE_sound_ensure_time_stretch_effect(const Strip *strip, float fps)
{
  seq::StripRuntime &runtime = *strip->runtime;

  /* If we already have a time stretch effect with the same frame-rate, use that. */
  if (runtime.sound_time_stretch != nullptr && runtime.sound_time_stretch_fps == fps) {
    return runtime.sound_time_stretch;
  }

  /* Otherwise create the time stretch effect. */
  runtime.clear_sound_time_stretch();
  runtime.sound_time_stretch = AUD_Sound(
      new aud::AnimateableTimeStretchPitchScale(BKE_sound_playback_handle_get(strip->sound),
                                                fps,
                                                1.0f,
                                                1.0f,
                                                aud::StretcherQuality::HIGH,
                                                false));
  runtime.sound_time_stretch_fps = fps;
  return runtime.sound_time_stretch;
}

void BKE_sound_set_scene_sound_time_stretch_at_frame(AUD_Sound handle,
                                                     int frame,
                                                     float time_stretch,
                                                     bool animated)
{
  std::shared_ptr<aud::AnimateableProperty> prop =
      std::dynamic_pointer_cast<aud::AnimateableTimeStretchPitchScale>(handle)->getAnimProperty(
          aud::AP_TIME_STRETCH);
  if (animated) {
    if (frame >= 0) {
      prop->write(&time_stretch, frame, 1);
    }
  }
  else {
    prop->write(&time_stretch);
  }
}

void BKE_sound_set_scene_sound_time_stretch_constant_range(AUD_Sound handle,
                                                           int frame_start,
                                                           int frame_end,
                                                           float time_stretch)
{
  frame_start = max_ii(0, frame_start);
  frame_end = max_ii(0, frame_end);
  std::shared_ptr<aud::AnimateableProperty> prop =
      std::dynamic_pointer_cast<aud::AnimateableTimeStretchPitchScale>(handle)->getAnimProperty(
          aud::AP_TIME_STRETCH);
  prop->writeConstantRange(&time_stretch, frame_start, frame_end);
}
#  endif /* WITH_RUBBERBAND */

SoundInfo bke::sound_info_get(AUD_Sound sound)
{
  SoundInfo res;
  res.length = 0.0f;
  res.specs.channels = SOUND_CHANNELS_INVALID;
  res.specs.samplerate = 0;

  try {
    std::shared_ptr<aud::IReader> reader = sound->createReader();
    if (reader.get()) {
      aud::Specs specs = reader->getSpecs();
      res.specs.channels = eSoundChannels(specs.channels);
      res.specs.samplerate = specs.rate;
      res.length = reader->getLength() / float(specs.rate);
    }
  }
  catch (aud::Exception &) {
  }
  return res;
}

void bke::sound_system_initialize()
{
  aud::PluginManager::loadPlugins();
  aud::NULLDevice::registerPlugin();
}

AUD_Device bke::sound_device_init(const char *device,
                                  const aud::DeviceSpecs &specs,
                                  int buffersize,
                                  const char *name)
{
  using namespace aud;
  try {
    std::shared_ptr<IDeviceFactory> factory = device ? DeviceManager::getDeviceFactory(device) :
                                                       DeviceManager::getDefaultDeviceFactory();

    if (factory) {
      factory->setName(name);
      factory->setBufferSize(buffersize);
      factory->setSpecs(specs);
      auto device = factory->openDevice();
      DeviceManager::setDevice(device);

      return AUD_Device(device);
    }
  }
  catch (Exception &) {
  }
  return nullptr;
}

void bke::sound_device_exit()
{
  aud::DeviceManager::releaseDevice();
}

AUD_Handle bke::sound_device_play(AUD_Device device, AUD_Sound sound)
{
  if (!device) {
    device = aud::DeviceManager::getDevice();
  }
  try {
    return device->play(sound, true);
  }
  catch (aud::Exception &) {
  }
  return nullptr;
}

bool bke::sound_device_read(AUD_Device device, unsigned char *buffer, int length)
{
  BLI_assert(buffer);
  auto read_device = std::dynamic_pointer_cast<aud::ReadDevice>(device);
  if (!read_device) {
    return false;
  }
  try {
    return read_device->read(buffer, length);
  }
  catch (aud::Exception &) {
    return false;
  }
}

AUD_Handle bke::sound_pause_after(AUD_Handle handle, double seconds)
{
  auto device = aud::DeviceManager::getDevice();

  AUD_Sound silence = AUD_Sound(new aud::Silence(device->getSpecs().rate));
  AUD_Sound limiter = AUD_Sound(new aud::Limiter(silence, 0, seconds));

  std::lock_guard<aud::ILockable> lock(*device);

  try {
    AUD_Handle handle2 = device->play(limiter);
    if (handle2.get()) {
      AUD_Handle *data = new AUD_Handle(handle);
      handle2->setStopCallback(
          [](void *data) {
            AUD_Handle *handle = (AUD_Handle *)data;
            (*handle)->pause();
            delete handle;
          },
          data);
      return handle2;
    }
  }
  catch (aud::Exception &) {
  }
  return nullptr;
}

float *bke::sound_read_file_buffer(const char *filename,
                                   float low,
                                   float high,
                                   float attack,
                                   float release,
                                   float threshold,
                                   bool accumulate,
                                   bool additive,
                                   bool square,
                                   float sthreshold,
                                   double samplerate,
                                   int stream,
                                   int *length)
{
  using namespace aud;

  DeviceSpecs specs;
  specs.channels = CHANNELS_MONO;
  specs.rate = samplerate;

  AUD_Sound file = AUD_Sound(new File(filename, stream));
  int position = 0;

  Buffer buffer;

  try {
    std::shared_ptr<IReader> reader = file->createReader();
    SampleRate rate = reader->getSpecs().rate;

    AUD_Sound sound = AUD_Sound(new ChannelMapper(file, specs));

    if (high < rate)
      sound = AUD_Sound(new Lowpass(sound, high));
    if (low > 0)
      sound = AUD_Sound(new Highpass(sound, low));

    sound = AUD_Sound(new Envelope(sound, attack, release, threshold, 0.1f));
    sound = AUD_Sound(new LinearResample(sound, specs));

    if (square)
      sound = AUD_Sound(new Threshold(sound, sthreshold));

    if (accumulate)
      sound = AUD_Sound(new Accumulator(sound, additive));
    else if (additive)
      sound = AUD_Sound(new Sum(sound));

    reader = sound->createReader();

    if (!reader.get())
      return nullptr;

    int len;
    bool eos;
    do {
      len = samplerate;
      buffer.resize((position + len) * sizeof(float), true);
      reader->read(len, eos, buffer.getBuffer() + position);
      position += len;
    } while (!eos);
  }
  catch (Exception &) {
    return nullptr;
  }

  float *result = MEM_new_array_uninitialized<float>(position, __func__);
  memcpy(result, buffer.getBuffer(), position * sizeof(float));
  *length = position;
  return result;
}

bool bke::sound_mixdown(AUD_Sequence sequence,
                        unsigned int start,
                        unsigned int length,
                        unsigned int buffersize,
                        const char *filename,
                        const aud::DeviceSpecs &specs,
                        aud::Container format,
                        aud::Codec codec,
                        unsigned int bitrate,
                        bool split_channels,
                        std::string &r_error)
{
  using namespace aud;
  try {
    sequence->setSpecs(specs.specs);
    std::shared_ptr<IReader> reader = sequence->createQualityReader(ResampleQuality::MEDIUM);
    reader->seek(start);

    if (!split_channels) {
      std::shared_ptr<IWriter> writer = FileWriter::createWriter(
          filename, specs, format, codec, bitrate);
      FileWriter::writeReader(reader, writer, length, buffersize, nullptr, nullptr);
    }
    else {
      std::vector<std::shared_ptr<IWriter>> writers;
      int channels = specs.channels;

      aud::DeviceSpecs specs_mono = specs;
      specs_mono.channels = CHANNELS_MONO;
      for (int i = 0; i < channels; i++) {
        std::string stream;
        std::string fn = filename;
        size_t index = fn.find_last_of('.');
        size_t index_slash = fn.find_last_of('/');
        size_t index_backslash = fn.find_last_of('\\');

        if ((index == std::string::npos) ||
            ((index < index_slash) && (index_slash != std::string::npos)) ||
            ((index < index_backslash) && (index_backslash != std::string::npos)))
        {
          stream = fmt::format("{}_{}", filename, i + 1);
        }
        else {
          stream = fmt::format("{}_{}{}", fn.substr(0, index), i + 1, fn.substr(index));
        }
        writers.push_back(FileWriter::createWriter(stream, specs_mono, format, codec, bitrate));
      }
      FileWriter::writeReader(reader, writers, length, buffersize, nullptr, nullptr);
    }
    return true;
  }
  catch (Exception &e) {
    r_error = e.getMessage();
    return false;
  }
}

#else /* WITH_AUDASPACE */

#  include "BLI_utildefines.h"

void BKE_sound_force_device(const char * /*device*/) {}
void BKE_sound_init_once() {}
void BKE_sound_init(Main * /*bmain*/) {}
void BKE_sound_exit_once() {}
void BKE_sound_load(Main * /*bmain*/, bSound * /*sound*/) {}
void BKE_sound_packfile_ensure(Main * /*bmain*/, bSound * /*sound*/, ReportList * /*reports*/) {}
void BKE_sound_create_scene(Scene * /*scene*/) {}
void BKE_sound_destroy_scene(Scene * /*scene*/) {}
void BKE_sound_lock() {}
void BKE_sound_unlock() {}
void BKE_sound_refresh_callback_bmain(Main * /*bmain*/) {}
void BKE_sound_reset_scene_specs(Scene * /*scene*/) {}
void BKE_sound_mute_scene(Scene * /*scene*/, bool /*muted*/) {}
AUD_SequenceEntry BKE_sound_scene_add_scene_sound(Scene * /*scene*/, Strip * /*strip*/)
{
  return nullptr;
}
AUD_SequenceEntry BKE_sound_add_scene_sound(Scene * /*scene*/, Strip * /*strip*/)
{
  return nullptr;
}
void BKE_sound_remove_scene_sound(Scene * /*scene*/, AUD_SequenceEntry /*handle*/) {}
void BKE_sound_mute_scene_sound(AUD_SequenceEntry /*handle*/, bool /*mute*/) {}
void BKE_sound_move_scene_sound(const Scene * /*scene*/,
                                AUD_SequenceEntry /*handle*/,
                                int /*startframe*/,
                                int /*endframe*/,
                                int /*frameskip*/,
                                double /*audio_offset*/)
{
}
void BKE_sound_move_scene_sound_defaults(Scene * /*scene*/, Strip * /*strip*/) {}
void BKE_sound_play_scene(Scene * /*scene*/) {}
void BKE_sound_stop_scene(Scene * /*scene*/) {}
void BKE_sound_seek_scene(Main * /*bmain*/, Scene * /*scene*/) {}
double BKE_sound_sync_scene(Scene * /*scene*/)
{
  return NAN_FLT;
}
void BKE_sound_read_waveform(Main *bmain,
                             bSound *sound,
                             /* NOLINTNEXTLINE: readability-non-const-parameter. */
                             bool *stop)
{
  UNUSED_VARS(sound, stop, bmain);
}

void BKE_sound_update_sequencer(Main * /*main*/, bSound * /*sound*/) {}
void BKE_sound_update_scene(Depsgraph * /*depsgraph*/, Scene * /*scene*/) {}
void BKE_sound_update_scene_sound(AUD_SequenceEntry /*handle*/, bSound * /*sound*/) {}
void BKE_sound_update_scene_listener(Scene * /*scene*/) {}
void BKE_sound_update_fps(Main * /*bmain*/, Scene * /*scene*/) {}
void BKE_sound_set_scene_sound_volume_at_frame(AUD_SequenceEntry /*handle*/,
                                               int /*frame*/,
                                               float /*volume*/,
                                               bool /*animated*/)
{
}
void BKE_sound_set_scene_sound_pan_at_frame(AUD_SequenceEntry /*handle*/,
                                            int /*frame*/,
                                            float /*pan*/,
                                            bool /*animated*/)
{
}
void BKE_sound_set_scene_volume(Scene * /*scene*/, float /*volume*/) {}
void BKE_sound_set_scene_sound_pitch_at_frame(AUD_SequenceEntry /*handle*/,
                                              int /*frame*/,
                                              float /*pitch*/,
                                              bool /*animated*/)
{
}
void BKE_sound_set_scene_sound_pitch_constant_range(AUD_SequenceEntry /*handle*/,
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

#if !defined(WITH_AUDASPACE) || !defined(WITH_RUBBERBAND)
AUD_Sound BKE_sound_ensure_time_stretch_effect(const Strip * /*strip*/, float /*fps*/)
{
  return nullptr;
}

void BKE_sound_set_scene_sound_time_stretch_at_frame(AUD_Sound /*handle*/,
                                                     int /*frame*/,
                                                     float /*time_stretch*/,
                                                     bool /*animated*/)
{
}
void BKE_sound_set_scene_sound_time_stretch_constant_range(void * /*handle*/,
                                                           int /*frame_start*/,
                                                           int /*frame_end*/,
                                                           float /*time_stretch*/)
{
}
#endif

void BKE_sound_ensure_scene(Scene *scene)
{
  if (scene->runtime->audio.sound_scene != nullptr) {
    return;
  }
  BKE_sound_create_scene(scene);
}

static void sound_ensure_loaded(Main *bmain, bSound *sound)
{
  if (sound->runtime->cache != nullptr) {
    return;
  }
  BKE_sound_load(bmain, sound);
}

void BKE_sound_jack_sync_callback_set(SoundJackSyncCallback callback)
{
#if defined(WITH_AUDASPACE)
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
#ifdef WITH_AUDASPACE
  g_state.sound_device->lock();

  if (mode) {
    BKE_sound_play_scene(scene);
  }
  else {
    BKE_sound_stop_scene(scene);
  }
  if (scene->runtime->audio.playback_handle != nullptr) {
    scene->runtime->audio.playback_handle->seek(time);
  }
  g_state.sound_device->unlock();
#else
  UNUSED_VARS(mode, time);
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
  sound_ensure_loaded(bmain, sound);
}

void BKE_sound_runtime_state_get_and_clear(const bSound *sound,
                                           AUD_Sound *r_cache,
                                           AUD_Sound *r_playback_handle,
                                           Vector<float> **r_waveform)
{
  bke::SoundRuntime *runtime = sound->runtime;
  *r_cache = runtime->cache;
  *r_playback_handle = runtime->playback_handle;
  *r_waveform = runtime->waveform;
  runtime->cache = nullptr;
  runtime->playback_handle = nullptr;
  runtime->waveform = nullptr;
}

void BKE_sound_runtime_state_set(const bSound *sound,
                                 AUD_Sound cache,
                                 AUD_Sound playback_handle,
                                 Vector<float> *waveform)
{
  bke::SoundRuntime *runtime = sound->runtime;
  runtime->cache = cache;
  runtime->playback_handle = playback_handle;
  runtime->waveform = waveform;
}

AUD_Sound BKE_sound_playback_handle_get(const bSound *sound)
{
  if (sound == nullptr) {
    return nullptr;
  }
  return sound->runtime->playback_handle;
}

void BKE_sound_runtime_clear_waveform_loading_tag(bSound *sound)
{
  bke::SoundRuntime *runtime = sound->runtime;
  BLI_spin_lock(&runtime->spinlock);
  runtime->tags &= ~bke::SoundTags::WaveformLoading;
  BLI_spin_unlock(&runtime->spinlock);
}

bool BKE_sound_runtime_start_waveform_loading(bSound *sound)
{
  bke::SoundRuntime *runtime = sound->runtime;
  bool result = false;
  BLI_spin_lock(&runtime->spinlock);
  if (runtime->waveform == nullptr) {
    /* Load the waveform data if it hasn't been loaded and cached already. */
    if (!flag_is_set(runtime->tags, bke::SoundTags::WaveformLoading)) {
      /* Prevent sounds from reloading. */
      runtime->tags |= bke::SoundTags::WaveformLoading;
      result = true;
    }
  }
  BLI_spin_unlock(&runtime->spinlock);
  return result;
}

const Vector<float> *BKE_sound_runtime_get_waveform(const bSound *sound)
{
  return sound->runtime->waveform;
}

}  // namespace blender
