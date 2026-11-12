/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <climits>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <mutex>
#include <numeric>
#include <optional>
#include <string>
#include <thread>
#include <tuple>
#include <utility>

#ifdef WITH_FFTW3
#  include <fftw3.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_bounds.hh"
#include "BLI_build_config.hh"
#include "BLI_enum_flags.hh"
#include "BLI_function_ref.hh"
#include "BLI_listbase.hh"
#include "BLI_map.hh"
#include "BLI_math_base_c.hh"
#include "BLI_math_rotation_c.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.hh"
#include "BLI_threads.hh"

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
#  include "BLI_vector.hh"

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

#include "BKE_anim_data.hh"
#include "BKE_animsys.hh"
#include "BKE_bpath.hh"
#include "BKE_global.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_packedFile.hh"
#include "BKE_scene.hh"
#include "BKE_scene_runtime.hh"
#include "BKE_sound.hh"
#include "BKE_sound_sample.hh"

#include "sound_reader_cache.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "BLO_read_write.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "SEQ_iterator.hh"
#include "SEQ_sequencer.hh"
#include "SEQ_sound.hh"

#include "BLI_concurrent_map.hh"

#include "CLG_log.h"

#ifdef WITH_PYTHON
#  include "BPY_extern.hh"
#endif

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

using bSoundFrequencySamplerMap =
    ConcurrentMap<bSoundFrequencySampler::Key, std::shared_ptr<bSoundFrequencySampler>>;

#ifdef WITH_AUDASPACE
struct SoundRuntimeGeneration {
  AUD_Sound handle;
  bSoundFrequencySamplerMap samplers;
  std::shared_ptr<SoundReaderCache> reader_cache;
};
#endif

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

#ifdef WITH_AUDASPACE
  std::mutex generation_mutex;
  std::shared_ptr<SoundRuntimeGeneration> generation;
#endif
};

}  // namespace bke

static void sound_free_audio(bSound *sound);

static void sound_init_runtime(bSound *sound)
{
  sound->runtime = MEM_new<bke::SoundRuntime>(__func__);
  BLI_spin_init(&sound->runtime->spinlock);
}

#ifdef WITH_AUDASPACE
static void sound_runtime_generation_replace(
    bSound *sound, std::shared_ptr<bke::SoundRuntimeGeneration> generation)
{
  bke::SoundRuntime *runtime = sound->runtime;
  std::shared_ptr<bke::SoundRuntimeGeneration> old_generation;
  {
    std::lock_guard lock{runtime->generation_mutex};
    old_generation = std::move(runtime->generation);
    runtime->generation = std::move(generation);
  }
}

static std::shared_ptr<bke::SoundRuntimeGeneration> sound_runtime_generation_get(
    const bSound &sound)
{
  bke::SoundRuntime *runtime = sound.runtime;
  std::lock_guard lock{runtime->generation_mutex};
  return runtime->generation;
}
#endif

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

  if (sound->runtime) {
    sound_free_audio(sound);
    sound_free_waveform(sound);
    BLI_spin_end(&sound->runtime->spinlock);
    MEM_delete(sound->runtime);
  }
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
  BKE_bpath_foreach_path_fixed_process(bpath_data, sound->filepath);
}

static void sound_blend_write(BlendWriter *writer, ID *id, const void *id_address)
{
  bSound *sound = id_cast<bSound *>(id);
  const bool is_undo = writer->is_undo();

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
    .id_code = bSound::id_type,
    .id_filter = FILTER_ID_SO,
    .dependencies_id_types = 0,
    .main_listbase_index = INDEX_ID_SO,
    .struct_size = sizeof(bSound),
    .name = "Sound",
    .name_plural = N_("sounds"),
    .translation_context = BLT_I18NCONTEXT_ID_SOUND,
    .flags = IDTYPE_FLAGS_NO_ANIMDATA | IDTYPE_FLAGS_APPEND_IS_REUSABLE,
    .asset_type_info = nullptr,

    /* A fuzzy case, think NULLified content is OK here... */
    .init_data = nullptr,
    .copy_data = sound_copy_data,
    .free_data = sound_free_data,
    .make_local = nullptr,
    .foreach_id = nullptr,
    .foreach_cache = sound_foreach_cache,
    .foreach_path = sound_foreach_path,
    .foreach_working_space_color = nullptr,
    .foreach_asset_weak_reference = nullptr,
    .owner_pointer_get = nullptr,

    .blend_write = sound_blend_write,
    .blend_read_data = sound_blend_read_data,
    .blend_read_after_liblink = nullptr,

    .blend_read_undo_preserve = nullptr,

    .lib_override_apply_post = nullptr,
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
   * ID_TAG_COPIED_ON_EVAL tag set on them.
   *
   * Additionally, we also allow data-blocks outside of main database. Those can not be "original"
   * and could be used as a temporary evaluated result during operations like baking.
   *
   * NOTE: We consider ID evaluated if ANY of those flags is set. We do NOT require ALL of them.
   */
  BLI_assert(id->tag & (ID_TAG_COPIED_ON_EVAL | ID_TAG_NO_MAIN));
}

bSound *BKE_sound_new_file(Main *bmain, const char *filepath, short stream_index)
{
  bSound *sound;
  const char *blendfile_path = BKE_main_blendfile_path(bmain);
  char filepath_abs[FILE_MAX];

  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, blendfile_path);

  sound = static_cast<bSound *>(BKE_libblock_alloc(bmain, ID_SO, BLI_path_basename(filepath), 0));
  STRNCPY(sound->filepath, filepath);
  sound_init_runtime(sound);
  sound->stream_index = stream_index;

  /* Extract sound specs for bSound */
  SoundInfo info;
  bool success = BKE_sound_info_get(bmain, sound, &info);
  if (success) {
    sound->samplerate = info.specs.samplerate;
    sound->audio_channels = info.specs.channels;
  }

  return sound;
}

static bSound *sound_new_file_exists_ex(Main *bmain, const char *filepath, short stream_index)
{
  bSound *sound;
  char filepath_abs[FILE_MAX], filepath_test[FILE_MAX];

  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, BKE_main_blendfile_path(bmain));

  /* Search for an existing sound matching both filepath and stream index. */
  for (sound = bmain->sounds.first(); sound; sound = static_cast<bSound *>(sound->id.next)) {
    if (sound->stream_index != stream_index) {
      continue;
    }
    STRNCPY(filepath_test, sound->filepath);
    BLI_path_abs(filepath_test, ID_BLEND_PATH(bmain, &sound->id));

    if (BLI_path_cmp(filepath_test, filepath_abs) == 0) {
      id_us_plus(&sound->id); /* officially should not, it doesn't link here! */
      return sound;
    }
  }

  return BKE_sound_new_file(bmain, filepath, stream_index);
}

bSound *BKE_sound_new_file_exists(Main *bmain, const char *filepath, short stream_index)
{
  return sound_new_file_exists_ex(bmain, filepath, stream_index);
}

void BKE_sound_invalidate_scene_audio_animation_cache(Scene *scene)
{
  BLI_assert(DEG_is_evaluated(scene));
  scene->runtime->audio.animation_cache_dirty = true;
}

int BKE_sound_scene_audio_frame(const Depsgraph *depsgraph, const Scene *scene)
{
  if (scene != DEG_get_evaluated_scene(depsgraph) ||
      (scene->runtime->audio.animation_cache_type != bke::AudioAnimationCacheType::None &&
       scene->r.subframe != 0.0f))
  {
    return -1;
  }
  return scene->r.cfra;
}

#ifdef WITH_AUDASPACE
static Scene *sound_original_scene(const Scene *scene)
{
  return const_cast<Scene *>(DEG_get_original(scene));
}

static void sound_reset_scene_audio_animation_no_lock(Scene *scene);
static void sound_reset_scene_audio_animation(Scene *scene);
static void sound_update_scene_objects(Depsgraph *depsgraph, Scene *scene);
static void sound_discard_scene_audio_animation_cache(Depsgraph *depsgraph, Scene *scene);
static bool sound_entry_sound_range(const Scene *scene,
                                    const AUD_SequenceEntry &entry,
                                    double begin,
                                    double end,
                                    double &r_sound_begin,
                                    double &r_sound_end);

static bool sound_scene_strip_uses_sequencer(const Strip *strip)
{
  return strip->type == STRIP_TYPE_SCENE && strip->scene != nullptr &&
         (strip->flag & SEQ_SCENE_STRIPS) && strip->scene->ed != nullptr;
}

static bool sound_strip_has_audio(const Strip *strip)
{
  if (strip->type == STRIP_TYPE_SOUND) {
    return strip->sound != nullptr;
  }
  if (strip->type == STRIP_TYPE_SCENE) {
    return strip->scene != nullptr;
  }
  return false;
}

static std::optional<std::string> sound_audio_animation_path(PointerRNA &ptr, const char *property)
{
  PropertyRNA *prop = RNA_struct_find_property(&ptr, property);
  if (prop == nullptr) {
    return std::nullopt;
  }
  return RNA_path_from_ID_to_property(&ptr, prop);
}

enum class SoundAudioPropertyAnimation {
  None,
  Direct,
  Driven,
};

/* RNA paths of the evaluated #Scene's F-Curves of array index 0. */
struct SoundSceneAnimatedPaths {
  Set<StringRef> animated;
  Set<StringRef> driven;
  /* Number of F-Curves visited by Action and NLA evaluation. */
  int fcurves_num = 0;
};

static SoundSceneAnimatedPaths sound_scene_animated_paths(Scene *scene_eval)
{
  sound_verify_evaluated_id(&scene_eval->id);
  SoundSceneAnimatedPaths paths;
  BKE_fcurves_id_cb(&scene_eval->id, [&](ID * /*id*/, FCurve *fcurve) {
    if (fcurve->driver == nullptr) {
      paths.fcurves_num++;
    }
    if (fcurve->array_index == 0) {
      paths.animated.add(fcurve->rna_path());
      if (fcurve->driver != nullptr) {
        paths.driven.add(fcurve->rna_path());
      }
    }
  });
  return paths;
}

static SoundAudioPropertyAnimation sound_audio_property_animation(
    const SoundSceneAnimatedPaths &paths, const std::optional<std::string> &path)
{
  if (!path) {
    return SoundAudioPropertyAnimation::None;
  }
  if (paths.driven.contains(*path)) {
    return SoundAudioPropertyAnimation::Driven;
  }
  return paths.animated.contains(*path) ? SoundAudioPropertyAnimation::Direct :
                                          SoundAudioPropertyAnimation::None;
}

struct SoundSceneAudioProperties {
  bool scene_volume = false;
  Set<AUD_SequenceEntry> strip_volumes;
  Set<AUD_SequenceEntry> strip_pans;

  bool is_empty() const
  {
    return !scene_volume && strip_volumes.is_empty() && strip_pans.is_empty();
  }
};

struct SoundSceneAudioAnimationClassification {
  SoundSceneAudioProperties direct;
  SoundSceneAudioProperties driven;
  /* RNA paths of the directly animated properties, used to only evaluate those when sampling. */
  Set<std::string> direct_paths;
};

static SoundAudioPropertyAnimation sound_audio_property_animation_combine(
    const SoundAudioPropertyAnimation a, const SoundAudioPropertyAnimation b)
{
  if (a == SoundAudioPropertyAnimation::Driven || b == SoundAudioPropertyAnimation::Driven) {
    return SoundAudioPropertyAnimation::Driven;
  }
  if (a == SoundAudioPropertyAnimation::Direct || b == SoundAudioPropertyAnimation::Direct) {
    return SoundAudioPropertyAnimation::Direct;
  }
  return SoundAudioPropertyAnimation::None;
}

enum class SoundStripAudioProperty {
  Volume,
  Pan,
};

static void sound_audio_property_classification_add(
    SoundSceneAudioAnimationClassification &classification,
    const SoundAudioPropertyAnimation animation,
    const AUD_SequenceEntry &handle,
    const SoundStripAudioProperty property)
{
  SoundSceneAudioProperties *properties = nullptr;
  if (animation == SoundAudioPropertyAnimation::Direct) {
    properties = &classification.direct;
  }
  else if (animation == SoundAudioPropertyAnimation::Driven) {
    properties = &classification.driven;
  }
  if (properties == nullptr) {
    return;
  }

  if (property == SoundStripAudioProperty::Volume) {
    properties->strip_volumes.add(handle);
  }
  else {
    properties->strip_pans.add(handle);
  }
}

static Bounds<int> sound_strip_audible_frames(const Scene *scene, const Strip *strip);

/* Classify the audio properties of the Scene and of its strips that can be heard during
 * \a frames, all strips if not given. */
static SoundSceneAudioAnimationClassification sound_scene_audio_animation_classify(
    const SoundSceneAnimatedPaths &paths,
    Scene *scene_eval,
    const std::optional<Bounds<int>> &frames = std::nullopt)
{
  SoundSceneAudioAnimationClassification classification;

  PointerRNA scene_ptr = RNA_id_pointer_create(&scene_eval->id);
  const std::optional<std::string> scene_volume_path = sound_audio_animation_path(scene_ptr,
                                                                                  "audio_volume");
  const SoundAudioPropertyAnimation scene_volume = sound_audio_property_animation(
      paths, scene_volume_path);
  classification.direct.scene_volume = scene_volume == SoundAudioPropertyAnimation::Direct;
  classification.driven.scene_volume = scene_volume == SoundAudioPropertyAnimation::Driven;
  if (scene_volume == SoundAudioPropertyAnimation::Direct) {
    classification.direct_paths.add(*scene_volume_path);
  }

  if (scene_eval->ed == nullptr) {
    return classification;
  }

  seq::foreach_strip(&scene_eval->ed->seqbase, [&](Strip *strip) {
    if (!sound_strip_has_audio(strip) || strip->runtime == nullptr ||
        strip->runtime->scene_sound == nullptr)
    {
      return true;
    }
    if (frames) {
      const Bounds<int> audible_frames = sound_strip_audible_frames(scene_eval, strip);
      if (audible_frames.max < frames->min || audible_frames.min > frames->max) {
        return true;
      }
    }
    PointerRNA strip_ptr = RNA_pointer_create_discrete(&scene_eval->id, RNA_Strip, strip);

    const std::optional<std::string> volume_path = sound_audio_animation_path(strip_ptr, "volume");
    const SoundAudioPropertyAnimation strip_volume = sound_audio_property_animation(paths,
                                                                                    volume_path);
    SoundAudioPropertyAnimation volume = strip_volume;
    std::optional<std::string> meta_volume_path;
    SoundAudioPropertyAnimation meta_volume = SoundAudioPropertyAnimation::None;
    if (Strip *meta = seq::lookup_meta_by_strip(scene_eval->ed, strip)) {
      PointerRNA meta_ptr = RNA_pointer_create_discrete(&scene_eval->id, RNA_Strip, meta);
      meta_volume_path = sound_audio_animation_path(meta_ptr, "volume");
      meta_volume = sound_audio_property_animation(paths, meta_volume_path);
      volume = sound_audio_property_animation_combine(volume, meta_volume);
    }
    if (volume == SoundAudioPropertyAnimation::Direct) {
      if (strip_volume == SoundAudioPropertyAnimation::Direct) {
        classification.direct_paths.add(*volume_path);
      }
      if (meta_volume == SoundAudioPropertyAnimation::Direct) {
        classification.direct_paths.add(*meta_volume_path);
      }
    }
    sound_audio_property_classification_add(
        classification, volume, strip->runtime->scene_sound, SoundStripAudioProperty::Volume);

    const std::optional<std::string> pan_path = sound_audio_animation_path(strip_ptr, "pan");
    const SoundAudioPropertyAnimation pan = sound_audio_property_animation(paths, pan_path);
    if (pan == SoundAudioPropertyAnimation::Direct) {
      classification.direct_paths.add(*pan_path);
    }
    sound_audio_property_classification_add(
        classification, pan, strip->runtime->scene_sound, SoundStripAudioProperty::Pan);
    return true;
  });
  return classification;
}

static void sound_write_scene_audio_properties_at_frame(
    Scene *scene_eval, const SoundSceneAudioProperties &properties, const int frame)
{
  if (properties.scene_volume) {
    BKE_sound_set_scene_volume(scene_eval, scene_eval->audio.volume);
  }
  if (scene_eval->ed == nullptr ||
      (properties.strip_volumes.is_empty() && properties.strip_pans.is_empty()))
  {
    return;
  }

  seq::foreach_strip(&scene_eval->ed->seqbase, [&](Strip *strip) {
    if (strip->runtime == nullptr || strip->runtime->scene_sound == nullptr) {
      return true;
    }
    const AUD_SequenceEntry &handle = strip->runtime->scene_sound;
    if (properties.strip_volumes.contains(handle)) {
      float volume = strip->volume;
      if (const Strip *meta = seq::lookup_meta_by_strip(scene_eval->ed, strip)) {
        volume *= meta->volume;
      }
      BKE_sound_set_scene_sound_volume_at_frame(handle, frame, volume, true);
    }
    if (properties.strip_pans.contains(handle)) {
      BKE_sound_set_scene_sound_pan_at_frame(handle, frame, strip->pan, true);
    }
    return true;
  });
}

static void sound_evaluate_scene_audio_animation(Depsgraph *depsgraph,
                                                 Scene *scene_eval,
                                                 const float ctime,
                                                 const Set<ParsedRNAPathRef> &rna_paths)
{
  if (scene_eval->adt == nullptr) {
    return;
  }
  const AnimationEvalContext anim_eval_context = BKE_animsys_eval_context_construct(depsgraph,
                                                                                    ctime);
  BKE_animsys_evaluate_action_nla(
      {scene_eval->id, *scene_eval->adt}, anim_eval_context, false, &rna_paths);
}

static void sound_sync_scene_audio_current_frame(Depsgraph *depsgraph, Scene *scene_eval)
{
  if (scene_eval->ed != nullptr) {
    seq::eval_strips(depsgraph, scene_eval, &scene_eval->ed->seqbase);
  }
  /* Speaker handles are owned by the root Scene of the depsgraph. Nested Scene speakers are also
   * present in that graph and are updated as part of the root Scene; updating them against a child
   * Scene would reassign their handles to a different Audaspace Scene. The frame-change handlers
   * are not checked, the cache is outdated or was just stored with them, and Python can't be
   * called while the audio device is locked. */
  if (scene_eval == DEG_get_evaluated_scene(depsgraph)) {
    sound_update_scene_objects(depsgraph, scene_eval);
  }
  BKE_sound_set_scene_volume_at_frame(
      scene_eval, BKE_sound_scene_audio_frame(depsgraph, scene_eval), scene_eval->audio.volume);
}

struct SoundAnimationBakeRequest {
  Scene *scene_eval;
  /* Frames to add to the cache. */
  int frame_start;
  int frame_end;
  /* Add the frames to the frames stored in the cache instead of starting it again. */
  bool extend;
  /* Discard the stored animation instead of adding to it. */
  bool reset;
};

/* Whether sorted frame ranges contain the frames. */
static bool sound_frame_ranges_contain(const Span<Bounds<int>> ranges, const Bounds<int> frames)
{
  return std::any_of(ranges.begin(), ranges.end(), [&](const Bounds<int> &range) {
    return range.min <= frames.min && range.max >= frames.max;
  });
}

/* Add frames to sorted frame ranges, joining the ones that overlap or follow each other. */
static void sound_frame_ranges_add(Vector<Bounds<int>> &ranges, const Bounds<int> frames)
{
  ranges.append(frames);
  std::sort(ranges.begin(), ranges.end(), [](const Bounds<int> &a, const Bounds<int> &b) {
    return a.min < b.min;
  });
  int64_t last = 0;
  for (const int64_t i : ranges.index_range().drop_front(1)) {
    if (int64_t(ranges[i].min) <= int64_t(ranges[last].max) + 1) {
      ranges[last].max = std::max(ranges[last].max, ranges[i].max);
    }
    else {
      ranges[++last] = ranges[i];
    }
  }
  ranges.resize(last + 1);
}

/* The parts of the frames that sorted frame ranges don't contain. */
static Vector<Bounds<int>> sound_frame_ranges_missing(const Span<Bounds<int>> ranges,
                                                      const Bounds<int> frames)
{
  Vector<Bounds<int>> missing;
  int64_t start = frames.min;
  for (const Bounds<int> &range : ranges) {
    if (range.max < start) {
      continue;
    }
    if (range.min > frames.max) {
      break;
    }
    if (range.min > start) {
      missing.append({int(start), range.min - 1});
    }
    start = int64_t(range.max) + 1;
  }
  if (start <= frames.max) {
    missing.append({int(start), frames.max});
  }
  return missing;
}

static bool sound_scene_audio_animation_cache_has_data(const bke::SceneAudioRuntime &audio)
{
  return !audio.cached_animation_frames.is_empty();
}

/* Readers of the stored audio animation other than audio devices: renders that record into it
 * and readers of #BKE_sound_audio_animation_use. Changes only mark the caches as outdated while
 * they read it. */
static bool sound_scene_audio_animation_is_read(const bke::SceneAudioRuntime &audio)
{
  return !audio.recording.expired() || !audio.animation_use.expired();
}

static void sound_clear_scene_audio_animation_cache_state(bke::SceneAudioRuntime &audio)
{
  audio.animation_cache_dirty = true;
  audio.animation_cache_type = bke::AudioAnimationCacheType::None;
  audio.animation_cache_frame_change_handlers_hash.reset();
  audio.cached_animation_frames.clear();
}

static std::optional<uint64_t> sound_frame_change_handlers_hash()
{
#  ifdef WITH_PYTHON
  return BPY_app_handlers_frame_change_identity();
#  else
  return std::nullopt;
#  endif
}

static bool sound_scene_audio_animation_cache_is_compatible(
    const bke::SceneAudioRuntime &audio,
    const std::optional<uint64_t> &frame_change_handlers_hash,
    const Span<Bounds<int>> frames)
{
  return !audio.animation_cache_dirty &&
         ELEM(audio.animation_cache_type,
              bke::AudioAnimationCacheType::Automatic,
              bke::AudioAnimationCacheType::Explicit) &&
         std::all_of(frames.begin(),
                     frames.end(),
                     [&](const Bounds<int> &range) {
                       return sound_frame_ranges_contain(audio.cached_animation_frames, range);
                     }) &&
         audio.animation_cache_frame_change_handlers_hash == frame_change_handlers_hash;
}

static bool sound_scene_audio_animation_cache_can_extend(
    const bke::SceneAudioRuntime &audio, const std::optional<uint64_t> &frame_change_handlers_hash)
{
  return !audio.animation_cache_dirty &&
         audio.animation_cache_type == bke::AudioAnimationCacheType::Automatic &&
         audio.animation_cache_frame_change_handlers_hash == frame_change_handlers_hash;
}

/* Frames that the cache of the Scene misses to cover \a frames. A clean Automatic cache grows by
 * the frames it misses. Otherwise the cache starts again at the frames, the values stored by a
 * clean Automatic cache remain valid and are kept until they are sampled again. */
static void sound_scene_audio_animation_requests(
    Scene *scene_eval,
    const Span<Bounds<int>> frames,
    const std::optional<uint64_t> &frame_change_handlers_hash,
    Vector<SoundAnimationBakeRequest> &r_requests)
{
  if (!DEG_is_evaluated(scene_eval)) {
    return;
  }
  const bke::SceneAudioRuntime &audio = scene_eval->runtime->audio;
  if (sound_scene_audio_animation_cache_is_compatible(audio, frame_change_handlers_hash, frames)) {
    return;
  }
  if (!sound_scene_audio_animation_cache_can_extend(audio, frame_change_handlers_hash)) {
    /* Readers of recorded audio still use the stored values of earlier frames. */
    const bool reset = !sound_scene_audio_animation_is_read(audio);
    for (const int64_t i : frames.index_range()) {
      r_requests.append({scene_eval, frames[i].min, frames[i].max, i > 0, reset && i == 0});
    }
    return;
  }
  for (const Bounds<int> &range : frames) {
    for (const Bounds<int> &missing :
         sound_frame_ranges_missing(audio.cached_animation_frames, range))
    {
      r_requests.append({scene_eval, missing.min, missing.max, true, false});
    }
  }
}

/**
 * Collect the frames of the Scene and of the Scenes used by its Scene strips that are played
 * during \a frame_start to \a frame_end of the Scene. The frames of a Scene are kept as separate
 * ranges, Scene strips can play distant parts of the same Scene.
 */
static bool sound_collect_scene_audio_animation_ranges(Scene *scene_eval,
                                                       const int frame_start,
                                                       const int frame_end,
                                                       Set<const Scene *> &scene_path,
                                                       Map<Scene *, Vector<Bounds<int>>> &r_ranges)
{
  if (!scene_path.add(scene_eval)) {
    return true;
  }
  Vector<Bounds<int>> &ranges = r_ranges.lookup_or_add_default(scene_eval);
  const Vector<Bounds<int>> missing = sound_frame_ranges_missing(ranges, {frame_start, frame_end});
  sound_frame_ranges_add(ranges, {frame_start, frame_end});

  bool success = true;
  if (DEG_is_evaluated(scene_eval) && scene_eval->ed != nullptr) {
    for (const Bounds<int> &frames : missing) {
      seq::foreach_strip(&scene_eval->ed->seqbase, [&](Strip *strip) {
        if (!sound_scene_strip_uses_sequencer(strip) || strip->runtime == nullptr ||
            strip->runtime->scene_sound == nullptr)
        {
          return true;
        }
        /* Use the evaluated audio handle: Sequencer image timing clamps to content length and
         * handles reversal differently. The audio bounds already include trimming, Meta bounds
         * and the Scene start offset. Audaspace integrates the actual retiming pitch samples. */
        const double fps = scene_eval->frames_per_second();
        double sound_start, sound_end;
        if (sound_entry_sound_range(scene_eval,
                                    strip->runtime->scene_sound,
                                    frames.min / fps,
                                    (double(frames.max) + 1.0) / fps,
                                    sound_start,
                                    sound_end))
        {
          const double child_fps = strip->scene->frames_per_second();
          const double child_start = std::floor(sound_start * child_fps);
          const double child_end = std::ceil(sound_end * child_fps);
          if (!std::isfinite(child_start) || !std::isfinite(child_end) || child_start < INT_MIN ||
              child_end >= INT_MAX)
          {
            success = false;
            return false;
          }
          success = sound_collect_scene_audio_animation_ranges(
              strip->scene, int(child_start), int(child_end), scene_path, r_ranges);
          if (!success) {
            return false;
          }
        }
        return true;
      });
      if (!success) {
        break;
      }
    }
  }
  scene_path.remove(scene_eval);
  return success;
}

/* Scenes used by Scene strips that don't play their sequencer, and not used by any Scene strip
 * that does. Their caches are discarded, so that they use their evaluated values again. */
static void sound_collect_live_only_scenes(Scene *scene_eval,
                                           Set<const Scene *> &sequencer_scenes,
                                           Set<Scene *> &r_live_only_scenes)
{
  if (!DEG_is_evaluated(scene_eval) || scene_eval->ed == nullptr) {
    return;
  }
  seq::foreach_strip(&scene_eval->ed->seqbase, [&](Strip *strip) {
    if (strip->type != STRIP_TYPE_SCENE || strip->scene == nullptr) {
      return true;
    }
    if (!sound_scene_strip_uses_sequencer(strip)) {
      r_live_only_scenes.add(strip->scene);
    }
    else if (sequencer_scenes.add(strip->scene)) {
      sound_collect_live_only_scenes(strip->scene, sequencer_scenes, r_live_only_scenes);
    }
    return true;
  });
}

static Vector<Scene *> sound_live_only_scenes_with_cache(Scene *scene_eval)
{
  Set<const Scene *> sequencer_scenes = {scene_eval};
  Set<Scene *> live_only_scenes;
  sound_collect_live_only_scenes(scene_eval, sequencer_scenes, live_only_scenes);
  Vector<Scene *> scenes;
  for (Scene *live_only_scene_eval : live_only_scenes) {
    if (sequencer_scenes.contains(live_only_scene_eval) || !DEG_is_evaluated(live_only_scene_eval))
    {
      continue;
    }
    const bke::SceneAudioRuntime &audio = live_only_scene_eval->runtime->audio;
    if (audio.animation_cache_type != bke::AudioAnimationCacheType::None ||
        sound_scene_audio_animation_cache_has_data(audio))
    {
      scenes.append(live_only_scene_eval);
    }
  }
  return scenes;
}

static void sound_discard_live_only_scene_audio_animation_cache(Scene *scene_eval)
{
  bke::SceneAudioRuntime &audio = scene_eval->runtime->audio;
  const bke::AudioAnimationWriteState old_write_state = audio.animation_write_state;
  audio.animation_write_state = bke::AudioAnimationWriteState::Normal;
  sound_reset_scene_audio_animation(scene_eval);
  BKE_sound_set_scene_volume(scene_eval, scene_eval->audio.volume);
  sound_clear_scene_audio_animation_cache_state(audio);
  audio.animation_write_state = old_write_state;
}

static void sound_store_scene_audio_animation_cache(
    Scene *scene_eval,
    const Bounds<int> frames,
    const bool extend,
    const std::optional<uint64_t> &frame_change_handlers_hash,
    const bke::AudioAnimationCacheType cache_type)
{
  bke::SceneAudioRuntime &audio = scene_eval->runtime->audio;
  audio.animation_cache_dirty = false;
  audio.animation_cache_type = cache_type;
  audio.animation_cache_frame_change_handlers_hash = frame_change_handlers_hash;
  if (!extend) {
    audio.cached_animation_frames.clear();
  }
  if (frames.min <= frames.max) {
    sound_frame_ranges_add(audio.cached_animation_frames, frames);
  }
}

static bool sound_animation_is_playing(Main *bmain)
{
  for (const bScreen *screen = bmain->screens.first(); screen;
       screen = static_cast<const bScreen *>(screen->id.next))
  {
    if (screen->animtimer) {
      return true;
    }
  }
  return false;
}

static bool sound_animation_pause_playback(Scene *scene_eval)
{
  if (!DEG_is_evaluated(scene_eval) || !scene_eval->runtime->audio.playback_handle ||
      scene_eval->runtime->audio.playback_handle->getStatus() != aud::STATUS_PLAYING)
  {
    return false;
  }
  BKE_sound_stop_scene(scene_eval);
  return true;
}

static void sound_animation_resume_playback(Scene *scene_eval, const bool was_playing)
{
  if (!was_playing) {
    return;
  }
  if (DEG_is_evaluated(scene_eval)) {
    BKE_sound_play_scene(scene_eval);
  }
}

/* Stage only the directly animated scalar properties without changing Audaspace. Commit only
 * after sampling has succeeded for every Scene. Batch writes also avoid repeatedly
 * reallocating and copying Audaspace's frame buffer as it grows one frame at a time. */
struct SoundAnimationSamples {
  aud::AnimateableProperty *property;
  float *value;
  float *multiplier = nullptr;
  /* Directly animated RNA paths that the value depends on. */
  Vector<ParsedRNAPath<>> paths;
  int first_frame = 0;
  int last_frame = 0;
  /* Sampled values, starting at #first_frame. Allocated for the frames that the work allows. */
  Vector<float> values;
  /* Values of earlier frames are not read by audio, they are discarded. */
  int read_start = INT_MIN;
  /* Value at the current frame, used for frames that are not sampled when the cache is reset. */
  float current_value = 0.0f;
};

/* Frame range in which a strip can be heard, including interpolation padding. */
static Bounds<int> sound_strip_audible_frames(const Scene *scene, const Strip *strip)
{
  const Bounds<int> frames = seq::sound_bounds_get(scene, strip);
  return {frames.min - 1, frames.max + 1};
}

/* Frames of a property that get the value of another frame, which reading them returns, see
 * #sound_animation_hold_filled_frames. */
struct SoundAnimationHeldFrames {
  aud::AnimateableProperty *property;
  /* Read when the samples of earlier requests of the same preparation are written. */
  int frame;
  Bounds<int> frames;
};

struct SoundPreparedAnimation {
  SoundAnimationBakeRequest request;
  SoundSceneAudioAnimationClassification classification;
  Vector<SoundAnimationSamples> samples;
  /* Written before the samples. */
  Vector<SoundAnimationHeldFrames> held_frames;
  /* Current values of properties of Scenes used by Scene strips that are not sampled, written as
   * static values when the cache is reset. */
  Vector<std::pair<aud::AnimateableProperty *, float>> static_values;
};

/* Current value of a property, which shares ownership of its Sequence or entry. */
struct SoundPropertyValue {
  std::shared_ptr<aud::AnimateableProperty> property;
  float value;
  /* Values of earlier frames are not read by audio. */
  int read_start = INT_MIN;
  /* ID of the entry of the property, -1 for the volume of the Sequence. */
  int entry_id = -1;
  aud::AnimateablePropertyType type = aud::AP_VOLUME;
};

static void sound_collect_scene_audio_values(Scene *scene_eval,
                                             const SoundSceneAudioProperties &properties,
                                             Vector<SoundPropertyValue> &r_values)
{
  const AUD_Sequence &sound_scene = scene_eval->runtime->audio.sound_scene;
  if (properties.scene_volume && sound_scene) {
    r_values.append({std::shared_ptr<aud::AnimateableProperty>(
                         sound_scene, sound_scene->getAnimProperty(aud::AP_VOLUME)),
                     scene_eval->audio.volume});
  }
  if (scene_eval->ed == nullptr) {
    return;
  }
  seq::foreach_strip(&scene_eval->ed->seqbase, [&](Strip *strip) {
    if (strip->runtime == nullptr || strip->runtime->scene_sound == nullptr) {
      return true;
    }
    const AUD_SequenceEntry &handle = strip->runtime->scene_sound;
    const bool volume = properties.strip_volumes.contains(handle);
    const bool pan = properties.strip_pans.contains(handle);
    if (!volume && !pan) {
      return true;
    }
    const int read_start = sound_strip_audible_frames(scene_eval, strip).min - 1;
    if (volume) {
      const Strip *meta = seq::lookup_meta_by_strip(scene_eval->ed, strip);
      r_values.append({std::shared_ptr<aud::AnimateableProperty>(
                           handle, handle->getAnimProperty(aud::AP_VOLUME)),
                       strip->volume * (meta ? meta->volume : 1.0f),
                       read_start,
                       handle->getID(),
                       aud::AP_VOLUME});
    }
    if (pan) {
      r_values.append({std::shared_ptr<aud::AnimateableProperty>(
                           handle, handle->getAnimProperty(aud::AP_PANNING)),
                       strip->pan,
                       read_start,
                       handle->getID(),
                       aud::AP_PANNING});
    }
    return true;
  });
}

static void sound_collect_scene_audio_static_values(
    Scene *scene_eval,
    const SoundSceneAudioProperties &properties,
    Vector<std::pair<aud::AnimateableProperty *, float>> &r_values)
{
  Vector<SoundPropertyValue> values;
  sound_collect_scene_audio_values(scene_eval, properties, values);
  for (const SoundPropertyValue &value : values) {
    r_values.append({value.property.get(), value.value});
  }
}

/* Sampling a property value takes about as long as the filtered Action and NLA evaluation takes
 * to skip this many F-Curves, which it does for every F-Curve of the Scene at every frame. */
#  define SOUND_ANIMATION_SKIPPED_FCURVES_PER_VALUE 16
/* Sampling a property value takes about as long as Audaspace takes to fill this many frames
 * between the frames that a property stores and written ones. */
#  define SOUND_ANIMATION_FILLED_FRAMES_PER_VALUE 64

/**
 * Frames that the property stores once the samples of earlier requests of the same preparation are
 * written, see \a stored_frames. Writing other frames also fills the frames in between, see
 * #aud::AnimateableProperty::write.
 */
static std::optional<Bounds<int>> sound_animation_stored_frames(
    aud::AnimateableProperty &property,
    const bool reset,
    const Map<aud::AnimateableProperty *, Bounds<int>> &stored_frames)
{
  if (const Bounds<int> *frames = stored_frames.lookup_ptr(&property)) {
    return *frames;
  }
  std::lock_guard<aud::ILockable> lock(property);
  if (reset || !property.isAnimated()) {
    return std::nullopt;
  }
  const int start = property.getBufferPosition();
  return Bounds<int>(start, int(start + int64_t(property.getBufferLength()) - 1));
}

/**
 * With limited work, writing samples doesn't let Audaspace fill frames, which it would do for any
 * number of frames at once: the frames between the stored frames and the written ones, and the
 * unknown frames after the written ones, which it would fill again whenever frames before them are
 * written (see #aud::AnimateableProperty::getUnknownFramesAfter). They get the value that reading
 * them returns instead, as many as the work allows, next to the frames that keep their values so
 * that the others stay filled or unknown. Returns whether the samples can be written, when all
 * such frames got their value. A request that is shortened afterwards writes fewer frames, the
 * frames up to the held ones are filled, at most as many as the request has.
 */
static bool sound_animation_hold_filled_frames(
    aud::AnimateableProperty &property,
    const Bounds<int> frames,
    const Bounds<int> stored_frames,
    int64_t &work,
    Map<aud::AnimateableProperty *, Bounds<int>> &r_stored_frames,
    Vector<SoundAnimationHeldFrames> &r_held_frames)
{
  /* The frames that writing the samples fills, the frame whose value they get, and whether they
   * get it from their last frame, next to frames that keep their values. */
  Bounds<int> filled;
  int frame;
  bool from_end;
  if (int64_t(frames.min) > int64_t(stored_frames.max) + 1) {
    filled = {stored_frames.max + 1, frames.min - 1};
    frame = stored_frames.max;
    from_end = false;
  }
  else if (int64_t(frames.max) < int64_t(stored_frames.min) - 1) {
    filled = {frames.max + 1, stored_frames.min - 1};
    frame = stored_frames.min;
    from_end = true;
  }
  else if (frames.max >= stored_frames.min && frames.max < stored_frames.max) {
    /* Earlier requests of the preparation write earlier frames of the Scene, or the same values
     * into these unknown frames. */
    const int unknown = property.getUnknownFramesAfter(frames.max);
    if (unknown == 0) {
      return true;
    }
    filled = {frames.max + 1, frames.max + unknown};
    frame = filled.max;
    from_end = true;
  }
  else {
    return true;
  }
  const int64_t count = int64_t(filled.max) - filled.min + 1;
  const int64_t held = std::min(count,
                                std::min(std::max(work, int64_t(0)), count) *
                                    SOUND_ANIMATION_FILLED_FRAMES_PER_VALUE);
  if (held == 0) {
    return false;
  }
  const Bounds<int> held_frames = from_end ? Bounds<int>(int(filled.max - held + 1), filled.max) :
                                             Bounds<int>(filled.min, int(filled.min + held - 1));
  r_held_frames.append({&property, frame, held_frames});
  work -= (held + SOUND_ANIMATION_FILLED_FRAMES_PER_VALUE - 1) /
          SOUND_ANIMATION_FILLED_FRAMES_PER_VALUE;
  r_stored_frames.add_overwrite(&property, bounds::merge(stored_frames, held_frames));
  return held == count;
}

/**
 * Sample the requested frames, including the interpolation padding around them. \a work counts
 * sampled values, the skipped F-Curves and the frames that writing the values fills. Once it's
 * used up, the request is shortened, but at least its first frame is sampled. Values are only
 * allocated for the frames that are sampled. With \a hold_filled_frames, the frames that writing
 * samples fills get their values in parts first, see #sound_animation_hold_filled_frames. The
 * samples wait for a later preparation until then, the request is shortened to the frames before
 * them.
 */
static bool sound_sample_automatic_scene_audio_animation(
    Depsgraph *depsgraph,
    SoundPreparedAnimation &prepared,
    const std::optional<uint64_t> &frame_change_handlers_hash,
    Map<aud::AnimateableProperty *, Bounds<int>> &r_stored_frames,
    int64_t &work,
    const bool hold_filled_frames)
{
  SoundAnimationBakeRequest &request = prepared.request;
  Scene *scene_eval = request.scene_eval;
  if (!DEG_is_evaluated(scene_eval)) {
    return false;
  }

  const int bake_start = request.frame_start > 0 ? request.frame_start - 1 : 0;
  /* Audaspace adds the signed-int position and count before calculating the buffer size.
   * Include padding and the leading gap in that check, then validate the allocation in bytes. */
  const int64_t buffer_frames = std::max(int64_t(request.frame_end) + 2, int64_t(0));
  if (buffer_frames > INT_MAX || uint64_t(buffer_frames) > SIZE_MAX / sizeof(float)) {
    return false;
  }
  const int bake_end = int(buffer_frames) - 1;

  const SoundSceneAnimatedPaths animated_paths = sound_scene_animated_paths(scene_eval);
  /* Driven values are written for all strips when the cache is reset. */
  prepared.classification = sound_scene_audio_animation_classify(
      animated_paths,
      scene_eval,
      request.reset ? std::nullopt : std::optional<Bounds<int>>({bake_start, bake_end}));
  const SoundSceneAudioProperties &direct = prepared.classification.direct;
  if (request.reset && scene_eval != DEG_get_evaluated_scene(depsgraph)) {
    /* Their evaluated values can't be written as framed values, keep them static instead. */
    sound_collect_scene_audio_static_values(
        scene_eval, prepared.classification.driven, prepared.static_values);
    if (frame_change_handlers_hash.has_value()) {
      sound_collect_scene_audio_static_values(scene_eval, direct, prepared.static_values);
    }
  }
  if (frame_change_handlers_hash.has_value() || direct.is_empty()) {
    return true;
  }
  const Set<std::string> &direct_paths = prepared.classification.direct_paths;

  auto add_samples = [&](aud::AnimateableProperty *property,
                         float *value,
                         float *multiplier,
                         Vector<std::string> paths,
                         const Bounds<int> frames,
                         const int read_start) {
    const int first_frame = std::max(frames.min, bake_start);
    const int last_frame = std::min(frames.max, bake_end);
    const float current_value = *value * (multiplier ? *multiplier : 1.0f);
    if (first_frame > last_frame) {
      if (request.reset) {
        prepared.static_values.append({property, current_value});
      }
      return;
    }
    SoundAnimationSamples samples{property, value, multiplier, {}, first_frame, last_frame};
    samples.read_start = read_start;
    for (const std::string &path : paths) {
      samples.paths.append(*ParsedRNAPath<>::from_string(path));
    }
    samples.current_value = current_value;
    prepared.samples.append(std::move(samples));
  };

  if (direct.scene_volume && scene_eval->runtime->audio.sound_scene) {
    PointerRNA scene_ptr = RNA_id_pointer_create(&scene_eval->id);
    add_samples(scene_eval->runtime->audio.sound_scene->getAnimProperty(aud::AP_VOLUME),
                &scene_eval->audio.volume,
                nullptr,
                {*sound_audio_animation_path(scene_ptr, "audio_volume")},
                {bake_start, bake_end},
                INT_MIN);
  }
  if (scene_eval->ed) {
    seq::foreach_strip(&scene_eval->ed->seqbase, [&](Strip *strip) {
      if (!strip->runtime || !strip->runtime->scene_sound) {
        return true;
      }
      const AUD_SequenceEntry &handle = strip->runtime->scene_sound;
      const bool sample_volume = direct.strip_volumes.contains(handle);
      const bool sample_pan = direct.strip_pans.contains(handle);
      if (!sample_volume && !sample_pan) {
        return true;
      }
      /* Only evaluate strip properties while the strip can be heard. Changing the strip timing
       * invalidates the cache. */
      const Bounds<int> frames = sound_strip_audible_frames(scene_eval, strip);
      PointerRNA strip_ptr = RNA_pointer_create_discrete(&scene_eval->id, RNA_Strip, strip);
      if (sample_volume) {
        Strip *meta = seq::lookup_meta_by_strip(scene_eval->ed, strip);
        Vector<std::string> paths;
        std::optional<std::string> path = sound_audio_animation_path(strip_ptr, "volume");
        if (path && direct_paths.contains(*path)) {
          paths.append(*path);
        }
        if (meta) {
          PointerRNA meta_ptr = RNA_pointer_create_discrete(&scene_eval->id, RNA_Strip, meta);
          path = sound_audio_animation_path(meta_ptr, "volume");
          if (path && direct_paths.contains(*path)) {
            paths.append(*path);
          }
        }
        add_samples(handle->getAnimProperty(aud::AP_VOLUME),
                    &strip->volume,
                    meta ? &meta->volume : nullptr,
                    std::move(paths),
                    frames,
                    frames.min - 1);
      }
      if (sample_pan) {
        add_samples(handle->getAnimProperty(aud::AP_PANNING),
                    &strip->pan,
                    nullptr,
                    {*sound_audio_animation_path(strip_ptr, "pan")},
                    frames,
                    frames.min - 1);
      }
      return true;
    });
  }

  /* The filtered evaluation only writes the sampled properties, restore their current values
   * afterwards. */
  const int current_scene_frame = scene_eval->r.cfra;
  const float current_scene_subframe = scene_eval->r.subframe;
  Vector<std::pair<float *, float>> current_values;
  for (const SoundAnimationSamples &samples : prepared.samples) {
    current_values.append({samples.value, *samples.value});
    if (samples.multiplier) {
      current_values.append({samples.multiplier, *samples.multiplier});
    }
  }

  Vector<std::pair<int, int>> sample_starts;
  Vector<std::pair<int, int>> sample_ends;
  for (const int i : prepared.samples.index_range()) {
    const SoundAnimationSamples &samples = prepared.samples[i];
    sample_starts.append({samples.first_frame, i});
    sample_ends.append({samples.last_frame, i});
  }
  std::sort(sample_starts.begin(), sample_starts.end());
  std::sort(sample_ends.begin(), sample_ends.end());
  const int64_t frame_work = animated_paths.fcurves_num /
                             SOUND_ANIMATION_SKIPPED_FCURVES_PER_VALUE;

  /* Shorten the request to the frames that the work allows before allocating the values. */
  Array<bool> deferred(prepared.samples.size(), false);
  Array<std::optional<Bounds<int>>> stored_before(prepared.samples.size());
  int deferred_start = INT_MAX;
  int64_t active_samples_num = 0;
  int64_t next_start = 0;
  int64_t next_end = 0;
  for (int frame = bake_start; frame <= bake_end && next_end < sample_ends.size(); frame++) {
    if (work <= 0 && frame > request.frame_start + 1) {
      /* The last sampled frame is the padding of the last requested frame. */
      request.frame_end = frame - 2;
      break;
    }
    for (; next_start < sample_starts.size() && sample_starts[next_start].first == frame;
         next_start++)
    {
      const int i = sample_starts[next_start].second;
      const SoundAnimationSamples &samples = prepared.samples[i];
      const Bounds<int> frames(samples.first_frame, samples.last_frame);
      std::optional<Bounds<int>> stored_frames = sound_animation_stored_frames(
          *samples.property, request.reset, r_stored_frames);
      if (stored_frames && hold_filled_frames) {
        if (!sound_animation_hold_filled_frames(*samples.property,
                                                frames,
                                                *stored_frames,
                                                work,
                                                r_stored_frames,
                                                prepared.held_frames))
        {
          deferred[i] = true;
          deferred_start = std::min(deferred_start, frame);
          continue;
        }
        stored_frames = r_stored_frames.lookup_default(samples.property, *stored_frames);
      }
      else if (stored_frames) {
        /* Audaspace fills the frames between the stored frames and the written ones. */
        const int64_t filled = std::max(int64_t(frames.min) - stored_frames->max - 1, int64_t(0)) +
                               std::max(int64_t(stored_frames->min) - frames.max - 1, int64_t(0));
        work -= filled / SOUND_ANIMATION_FILLED_FRAMES_PER_VALUE;
      }
      stored_before[i] = stored_frames;
      r_stored_frames.add_overwrite(
          samples.property, stored_frames ? bounds::merge(frames, *stored_frames) : frames);
      active_samples_num++;
    }
    if (active_samples_num > 0) {
      work -= frame_work + active_samples_num;
    }
    for (; next_end < sample_ends.size() && sample_ends[next_end].first == frame; next_end++) {
      if (!deferred[sample_ends[next_end].second]) {
        active_samples_num--;
      }
    }
  }
  const int sample_end = request.frame_end + 1;
  for (const int i : prepared.samples.index_range()) {
    SoundAnimationSamples &samples = prepared.samples[i];
    const int frames_num = std::min(samples.last_frame, sample_end) - samples.first_frame + 1;
    samples.values.resize(deferred[i] ? 0 : std::max(frames_num, 0));
    if (!deferred[i] && samples.last_frame > sample_end) {
      /* Later requests of the preparation find only the frames that are written. */
      const std::optional<Bounds<int>> &stored = stored_before[i];
      if (frames_num > 0) {
        const Bounds<int> written(samples.first_frame, sample_end);
        r_stored_frames.add_overwrite(samples.property,
                                      stored ? bounds::merge(written, *stored) : written);
      }
      else if (stored) {
        r_stored_frames.add_overwrite(samples.property, *stored);
      }
      else {
        r_stored_frames.remove(samples.property);
      }
    }
  }

  /* Evaluate only the paths of the samples that need the frame. Multiple samples can depend on
   * the same path, such as the volume of a Meta strip. */
  Map<ParsedRNAPathRef, int> path_users;
  Set<ParsedRNAPathRef> active_paths;
  Vector<int> active_samples;
  next_start = 0;
  next_end = 0;
  for (int frame = bake_start; frame <= sample_end && next_end < sample_ends.size(); frame++) {
    for (; next_start < sample_starts.size() && sample_starts[next_start].first == frame;
         next_start++)
    {
      const int i = sample_starts[next_start].second;
      if (deferred[i]) {
        continue;
      }
      active_samples.append(i);
      for (const ParsedRNAPathRef path : prepared.samples[i].paths) {
        if (path_users.lookup_or_add(path, 0)++ == 0) {
          active_paths.add(path);
        }
      }
    }
    if (!active_samples.is_empty()) {
      BKE_scene_frame_set(scene_eval, float(frame));
      sound_evaluate_scene_audio_animation(
          depsgraph, scene_eval, BKE_scene_ctime_get(scene_eval), active_paths);
      for (const int i : active_samples) {
        SoundAnimationSamples &samples = prepared.samples[i];
        const float multiplier = samples.multiplier ? *samples.multiplier : 1.0f;
        samples.values[frame - samples.first_frame] = *samples.value * multiplier;
      }
    }
    for (; next_end < sample_ends.size() && sample_ends[next_end].first == frame; next_end++) {
      const int i = sample_ends[next_end].second;
      if (deferred[i]) {
        continue;
      }
      active_samples.remove_first_occurrence_and_reorder(i);
      for (const ParsedRNAPathRef path : prepared.samples[i].paths) {
        if (--path_users.lookup(path) == 0) {
          active_paths.remove(path);
        }
      }
    }
  }
  if (deferred_start != INT_MAX) {
    request.frame_end = std::min(request.frame_end, deferred_start - 2);
  }
  scene_eval->r.cfra = current_scene_frame;
  scene_eval->r.subframe = current_scene_subframe;
  /* Also restores the NLA strip controls evaluated along with the sampled properties, at the time
   * of the graph's evaluation. */
  Set<ParsedRNAPathRef> sampled_paths;
  for (const SoundAnimationSamples &samples : prepared.samples) {
    for (const ParsedRNAPathRef path : samples.paths) {
      sampled_paths.add(path);
    }
  }
  sound_evaluate_scene_audio_animation(
      depsgraph, scene_eval, DEG_get_ctime(depsgraph), sampled_paths);
  for (const std::pair<float *, float> &current_value : current_values) {
    *current_value.first = current_value.second;
  }
  return true;
}

static void sound_write_automatic_scene_audio_animation(const SoundPreparedAnimation &prepared)
{
  for (const SoundAnimationHeldFrames &held : prepared.held_frames) {
    std::lock_guard<aud::ILockable> lock(*held.property);
    float value;
    held.property->read(held.frame, &value);
    held.property->writeConstantRange(&value, held.frames.min, held.frames.max + 1);
  }
  for (const SoundAnimationSamples &samples : prepared.samples) {
    if (!samples.values.is_empty()) {
      samples.property->write(
          samples.values.data(), samples.first_frame, int(samples.values.size()));
      samples.property->discardBefore(samples.read_start);
    }
  }
}

/* Write the animation after the cache was reset. Frames that aren't sampled use the current
 * values. */
static void sound_write_reset_scene_audio_animation(const SoundPreparedAnimation &prepared)
{
  for (const SoundAnimationSamples &samples : prepared.samples) {
    samples.property->write(&samples.current_value);
  }
  for (const std::pair<aud::AnimateableProperty *, float> &value : prepared.static_values) {
    value.first->write(&value.second);
  }
  sound_write_automatic_scene_audio_animation(prepared);
}

static void sound_commit_automatic_scene_audio_animation(
    Depsgraph *depsgraph,
    const SoundPreparedAnimation &prepared,
    const std::optional<uint64_t> &frame_change_handlers_hash)
{
  Scene *scene_eval = prepared.request.scene_eval;
  bke::SceneAudioRuntime &audio = scene_eval->runtime->audio;
  /* Scenes used by Scene strips are not evaluated at their own frames, so only the graph's Scene
   * has values for its current frame. Other Scenes were written while holding the sound lock. */
  if (prepared.request.reset && scene_eval == DEG_get_evaluated_scene(depsgraph)) {
    const bke::AudioAnimationWriteState old_write_state = audio.animation_write_state;
    audio.animation_cache_dirty = true;
    sound_reset_scene_audio_animation(scene_eval);
    sound_write_reset_scene_audio_animation(prepared);
    audio.animation_write_state = bke::AudioAnimationWriteState::Bake;
    sound_write_scene_audio_properties_at_frame(
        scene_eval, prepared.classification.driven, BKE_scene_frame_get(scene_eval));
    if (frame_change_handlers_hash.has_value()) {
      sound_write_scene_audio_properties_at_frame(
          scene_eval, prepared.classification.direct, BKE_scene_frame_get(scene_eval));
    }
    audio.animation_write_state = old_write_state;
  }
  sound_store_scene_audio_animation_cache(
      scene_eval,
      {prepared.request.frame_start, prepared.request.frame_end},
      prepared.request.extend,
      frame_change_handlers_hash,
      bke::AudioAnimationCacheType::Automatic);
  if (prepared.request.reset) {
    sound_sync_scene_audio_current_frame(depsgraph, scene_eval);
  }
}

static void sound_collect_explicit_bake_scenes(Scene *scene_eval,
                                               Set<const Scene *> &visited_scenes,
                                               Vector<Scene *> &r_scenes)
{
  if (!visited_scenes.add(scene_eval)) {
    return;
  }

  if (!DEG_is_evaluated(scene_eval)) {
    return;
  }
  r_scenes.append(scene_eval);
  if (scene_eval->ed == nullptr) {
    return;
  }

  seq::foreach_strip(&scene_eval->ed->seqbase, [&](Strip *strip) {
    if (strip->type == STRIP_TYPE_SCENE && strip->scene != nullptr) {
      sound_collect_explicit_bake_scenes(strip->scene, visited_scenes, r_scenes);
    }
    return true;
  });
}

/* Only needed for an invalid range. Walk all reachable Scenes here so a failed mapping cannot
 * leave an unvisited child's stale animation active. Keep clean compatible Explicit caches. */
static void sound_discard_incompatible_scene_audio_animation(
    Depsgraph *depsgraph,
    Scene *scene_eval,
    const Map<Scene *, Vector<Bounds<int>>> &ranges,
    const std::optional<uint64_t> &frame_change_handlers_hash,
    const bool discard_scene = true)
{
  Set<const Scene *> visited_scenes;
  Vector<Scene *> scenes;
  sound_collect_explicit_bake_scenes(scene_eval, visited_scenes, scenes);
  const bool playback_was_playing = sound_animation_pause_playback(scene_eval);
  for (Scene *scene_eval_to_discard : scenes) {
    if (!discard_scene && scene_eval_to_discard == scene_eval) {
      continue;
    }
    const bke::SceneAudioRuntime &audio = scene_eval_to_discard->runtime->audio;
    if (sound_scene_audio_animation_is_read(audio)) {
      /* Readers of recorded audio still use the stored values. */
      continue;
    }
    const Vector<Bounds<int>> *frames = ranges.lookup_ptr(scene_eval_to_discard);
    if (audio.animation_cache_type != bke::AudioAnimationCacheType::Explicit ||
        !sound_scene_audio_animation_cache_is_compatible(
            audio,
            frame_change_handlers_hash,
            frames ? frames->as_span() : audio.cached_animation_frames.as_span()))
    {
      sound_discard_scene_audio_animation_cache(depsgraph, scene_eval_to_discard);
    }
  }
  sound_animation_resume_playback(scene_eval, playback_was_playing);
}

/**
 * Prepare the Automatic caches of the Scenes used by Scene strips of \a scene_eval to cover the
 * frames that the strips play during \a frames. Then prepare \a scene_eval itself if
 * \a sample_scene is set.
 * Its cache only grows when all nested Scenes cover the frames added to it, so that the frames of
 * a cache of the graph's Scene always include the animation of its nested Scenes.
 *
 * Sampling stops when \a work is used up, after at least one frame of each sampled Scene. Unless
 * \a work is INT64_MAX, Audaspace doesn't fill frames when writing the samples, see
 * #sound_animation_hold_filled_frames.
 */
static void sound_prepare_scene_audio_animation(Depsgraph *depsgraph,
                                                Scene *scene_eval,
                                                const Bounds<int> frames,
                                                const bool sample_scene,
                                                int64_t work)
{
  const std::optional<uint64_t> frame_change_handlers_hash = sound_frame_change_handlers_hash();
  const bool limited = work != INT64_MAX;

  Vector<SoundAnimationBakeRequest> scene_requests;
  Vector<Bounds<int>> nested_frames = {frames};
  if (sample_scene) {
    sound_scene_audio_animation_requests(
        scene_eval, {frames}, frame_change_handlers_hash, scene_requests);
    if (scene_requests.is_empty()) {
      return;
    }
    nested_frames.clear();
    for (const SoundAnimationBakeRequest &scene_request : scene_requests) {
      nested_frames.append({scene_request.frame_start, scene_request.frame_end});
    }
  }

  Set<const Scene *> scene_path;
  Map<Scene *, Vector<Bounds<int>>> ranges;
  for (const Bounds<int> &nested : nested_frames) {
    if (!sound_collect_scene_audio_animation_ranges(
            scene_eval, nested.min, nested.max, scene_path, ranges))
    {
      sound_discard_incompatible_scene_audio_animation(
          depsgraph, scene_eval, ranges, frame_change_handlers_hash, sample_scene);
      return;
    }
  }

  /* Check compatibility only after all references have contributed their ranges. A global
   * visited set during traversal would miss both wider sibling references and their descendants.
   * Sort to always spend the work on the same Scenes first, keeping the order of their frames. */
  Vector<SoundAnimationBakeRequest> requests;
  for (const auto item : ranges.items()) {
    if (item.key != scene_eval) {
      sound_scene_audio_animation_requests(
          item.key, item.value, frame_change_handlers_hash, requests);
    }
  }
  std::stable_sort(requests.begin(),
                   requests.end(),
                   [](const SoundAnimationBakeRequest &a, const SoundAnimationBakeRequest &b) {
                     const int order = strcmp(a.scene_eval->id.name, b.scene_eval->id.name);
                     return order != 0 ?
                                order < 0 :
                                a.scene_eval->id.session_uid < b.scene_eval->id.session_uid;
                   });

  /* Frames that properties store once the samples of earlier requests are written. */
  Map<aud::AnimateableProperty *, Bounds<int>> stored_frames;
  Vector<SoundPreparedAnimation> prepared;
  bool nested_complete = true;
  for (const SoundAnimationBakeRequest &request : requests) {
    SoundPreparedAnimation animation;
    animation.request = request;
    if (!sound_sample_automatic_scene_audio_animation(
            depsgraph, animation, frame_change_handlers_hash, stored_frames, work, limited))
    {
      sound_discard_incompatible_scene_audio_animation(
          depsgraph, scene_eval, ranges, frame_change_handlers_hash, sample_scene);
      return;
    }
    nested_complete &= animation.request.frame_end == request.frame_end;
    prepared.append(std::move(animation));
  }
  for (const SoundAnimationBakeRequest &scene_request : scene_requests) {
    SoundPreparedAnimation animation;
    animation.request = scene_request;
    if (!nested_complete) {
      if (!scene_request.reset) {
        break;
      }
      /* Discard outdated animation, but only add frames once nested Scenes cover them. */
      animation.request.frame_end = animation.request.frame_start - 1;
    }
    if (!sound_sample_automatic_scene_audio_animation(
            depsgraph, animation, frame_change_handlers_hash, stored_frames, work, limited))
    {
      sound_discard_incompatible_scene_audio_animation(
          depsgraph, scene_eval, ranges, frame_change_handlers_hash, sample_scene);
      return;
    }
    prepared.append(std::move(animation));
  }

  /* Commit only after every Scene has been sampled, so a failed child cannot publish a
   * partially sampled animation. Frames are published at once under the sound lock. Resetting the
   * graph's Scene also resets Speakers and the listener, that happens while playback is paused. */
  const Vector<Scene *> live_only_scenes = sound_live_only_scenes_with_cache(scene_eval);
  const bool reset_scene = !prepared.is_empty() &&
                           prepared.last().request.scene_eval == scene_eval &&
                           prepared.last().request.reset;
  const bool playback_was_playing = (reset_scene || !live_only_scenes.is_empty()) &&
                                    sound_animation_pause_playback(scene_eval);
  for (Scene *live_only_scene : live_only_scenes) {
    sound_discard_live_only_scene_audio_animation_cache(live_only_scene);
  }
  BKE_sound_lock();
  for (const SoundPreparedAnimation &animation : prepared) {
    if (!animation.request.reset) {
      sound_write_automatic_scene_audio_animation(animation);
    }
    else if (animation.request.scene_eval != scene_eval) {
      sound_reset_scene_audio_animation_no_lock(animation.request.scene_eval);
      sound_write_reset_scene_audio_animation(animation);
    }
  }
  BKE_sound_unlock();
  for (const SoundPreparedAnimation &animation : prepared) {
    sound_commit_automatic_scene_audio_animation(depsgraph, animation, frame_change_handlers_hash);
  }
  sound_animation_resume_playback(scene_eval, playback_was_playing);
}

/**
 * Call \a fn for every Scene strip of the Scene that plays the sequencer of its Scene during
 * \a begin to \a end (in seconds of the Scene's audio), with the frames of the played Scene whose
 * audio starts in that time, which can be none. Also for the Scene strips of the played Scenes.
 * \a reference holds the IDs of the entries of the Scene strips from the graph's Scene down to the
 * strip, see #aud::SequenceData::getReferenceAnimProperty.
 */
static void sound_foreach_nested_scene_frames(
    Scene *scene_eval,
    const double begin,
    const double end,
    Set<const Scene *> &scene_path,
    Vector<int> &reference,
    const FunctionRef<void(Scene &nested_scene_eval, Span<int> reference, Bounds<int> frames)> fn)
{
  if (scene_eval->ed == nullptr) {
    return;
  }
  seq::foreach_strip(&scene_eval->ed->seqbase, [&](Strip *strip) {
    double sound_begin, sound_end;
    if (!sound_scene_strip_uses_sequencer(strip) || !DEG_is_evaluated(strip->scene) ||
        strip->runtime == nullptr || strip->runtime->scene_sound == nullptr ||
        !sound_entry_sound_range(
            scene_eval, strip->runtime->scene_sound, begin, end, sound_begin, sound_end) ||
        !scene_path.add(strip->scene))
    {
      return true;
    }
    reference.append(strip->runtime->scene_sound->getID());
    /* Tolerate rounding errors of frames that start at the begin or end. */
    constexpr double epsilon = 1e-6;
    const double fps = strip->scene->frames_per_second();
    const double first = std::max(std::ceil(sound_begin * fps - epsilon), 0.0);
    const double last = std::ceil(sound_end * fps - epsilon) - 1.0;
    if (last < INT_MAX) {
      fn(*strip->scene, reference, {int(std::min(first, last + 1.0)), int(last)});
    }
    sound_foreach_nested_scene_frames(
        strip->scene, sound_begin, sound_end, scene_path, reference, fn);
    reference.remove_last();
    scene_path.remove(strip->scene);
    return true;
  });
}

/**
 * Current values of the animated properties of a Scene used by Scene strips that are not sampled
 * at the frames that the strips play: driven ones, and directly animated ones while frame-change
 * handlers can change them. Only of strips that can be heard during \a frames.
 */
static void sound_collect_nested_scene_evaluated_values(Scene &scene_eval,
                                                        const Bounds<int> frames,
                                                        const bool has_frame_change_handlers,
                                                        Vector<SoundPropertyValue> &r_values)
{
  const SoundSceneAudioAnimationClassification classification =
      sound_scene_audio_animation_classify(
          sound_scene_animated_paths(&scene_eval), &scene_eval, frames);
  sound_collect_scene_audio_values(&scene_eval, classification.driven, r_values);
  if (has_frame_change_handlers) {
    sound_collect_scene_audio_values(&scene_eval, classification.direct, r_values);
  }
}

/**
 * Scenes used by Scene strips are evaluated at the frame of the graph's Scene, like for video.
 * Their direct animation follows the time that the strips play instead, see
 * #sound_prepare_scene_audio_animation. Call \a fn with the evaluated values of their other
 * animated properties and the frames that are played during the evaluated frame, so that audio of
 * the frame uses the values of its evaluation, like for the graph's Scene. Scene strips can play
 * the same frames during other frames of the graph's Scene, the values are stored per reference.
 */
static void sound_foreach_nested_scene_evaluated_value(
    Depsgraph *depsgraph,
    const FunctionRef<void(Scene &nested_scene_eval,
                           Span<int> reference,
                           const SoundPropertyValue &value,
                           Bounds<int> frames)> fn)
{
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  const double fps = scene_eval->frames_per_second();
  const double begin = scene_eval->r.cfra / fps;
  const double end = (scene_eval->r.cfra + 1) / fps;
  const bool has_frame_change_handlers = sound_frame_change_handlers_hash().has_value();
  Set<const Scene *> scene_path = {scene_eval};
  Vector<int> reference;
  sound_foreach_nested_scene_frames(
      scene_eval,
      begin,
      end,
      scene_path,
      reference,
      [&](Scene &nested_scene_eval, const Span<int> nested_reference, const Bounds<int> frames) {
        /* Also strips next to the frames, audio interpolates with their values. */
        Vector<SoundPropertyValue> values;
        sound_collect_nested_scene_evaluated_values(nested_scene_eval,
                                                    Bounds<int>(frames.min - 1, frames.max + 1),
                                                    has_frame_change_handlers,
                                                    values);
        for (const SoundPropertyValue &value : values) {
          fn(nested_scene_eval, nested_reference, value, frames);
        }
      });
}

/* The property that a reference reads instead of the property of the value, added with the value
 * if it doesn't exist yet. */
static aud::AnimateableProperty &sound_reference_property(Scene &scene_eval,
                                                          const Span<int> reference,
                                                          const int entry_id,
                                                          const aud::AnimateablePropertyType type,
                                                          const float value)
{
  return *scene_eval.runtime->audio.sound_scene->addReferenceAnimProperty(
      std::vector<int>(reference.begin(), reference.end()), entry_id, type, value);
}

/* Values of a property of a Scene used by Scene strips for one reference, recorded by an Explicit
 * bake. */
struct SoundNestedBakeValues {
  Scene *scene_orig;
  Vector<int> reference;
  int entry_id;
  aud::AnimateablePropertyType type;
  /* Value of the first evaluated frame during which the reference plays the Scene. */
  float first_value;
  Map<int, float> values;
};

/**
 * Write the recorded values into the caches that were prepared for the bake. Frames without a
 * recorded value keep the value of the frame before, earlier frames the first value, which audio
 * of the first frame is interpolated with.
 */
static void sound_write_nested_scene_bake_values(
    Depsgraph *depsgraph,
    const Map<std::pair<std::shared_ptr<aud::AnimateableProperty>, Vector<int>>,
              SoundNestedBakeValues> &nested_values)
{
  BKE_sound_lock();
  for (const SoundNestedBakeValues &item : nested_values.values()) {
    Scene *scene_eval = DEG_get_evaluated(depsgraph, item.scene_orig);
    if (!DEG_is_evaluated(scene_eval)) {
      continue;
    }
    bke::SceneAudioRuntime &audio = scene_eval->runtime->audio;
    if (audio.animation_cache_dirty ||
        audio.animation_cache_type == bke::AudioAnimationCacheType::None)
    {
      continue;
    }
    aud::AnimateableProperty &property = sound_reference_property(
        *scene_eval, item.reference, item.entry_id, item.type, item.first_value);
    property.write(&item.first_value);
    if (!item.values.is_empty()) {
      int first = INT_MAX;
      int last = INT_MIN;
      for (const int frame : item.values.keys()) {
        first = std::min(first, frame);
        last = std::max(last, frame);
      }
      Array<float> buffer(last - first + 1);
      buffer[0] = item.values.lookup(first);
      for (const int i : buffer.index_range().drop_front(1)) {
        buffer[i] = item.values.lookup_default(first + i, buffer[i - 1]);
      }
      property.write(buffer.data(), first, int(buffer.size()));
    }
    /* Only Explicit caches hold values that depend on the evaluation. */
    audio.animation_cache_type = bke::AudioAnimationCacheType::Explicit;
  }
  BKE_sound_unlock();
}

namespace bke {

/* Time range of a sequence entry, see #aud::SequenceEntry::move. */
struct SoundEntryBounds {
  double begin;
  double end;
  double skip;

  bool operator==(const SoundEntryBounds &other) const = default;
};

/* State of a sequence entry that is read when mixing, apart from values of single frames. */
struct SoundEntryState {
  /* Time range, unless it wasn't set since the recording started. */
  std::optional<SoundEntryBounds> bounds;
  bool removed = false;
  AUD_Sound sound;
  bool muted = false;
  /* Speaker settings. */
  std::optional<std::array<float, 8>> speaker;

  bool operator==(const SoundEntryState &other) const = default;
};

struct SoundListener {
  float speed_of_sound;
  float doppler_factor;
  aud::DistanceModel distance_model;

  bool operator==(const SoundListener &other) const = default;
};

/* State of a Sequence that is read when mixing, apart from values of single frames. Values that
 * were not set since the recording started are unset. */
struct SoundSequenceState {
  std::optional<bool> muted;
  std::optional<SoundListener> listener;
  std::optional<float> fps;

  bool operator==(const SoundSequenceState &other) const = default;
};

/* A value of an animated property that is written for other frames than the evaluated one, like
 * the retiming of strips. */
struct SoundPropertyWrite {
  int frame_start;
  int frame_end;
  float value;
  /* Written with #aud::AnimateableProperty::writeConstantRange, otherwise only for the start. */
  bool constant_range;

  bool operator==(const SoundPropertyWrite &other) const = default;
};

/* State of a sequence entry after the evaluated frame, and at the last recorded frame. */
struct SoundRecordedEntry {
  AUD_Sequence sequence;
  /* None for entries that were added since. */
  std::optional<SoundEntryState> recorded;
  SoundEntryState state;
};

/* State of a Sequence after the evaluated frame, and at the last recorded frame. */
struct SoundRecordedSequence {
  SoundSequenceState recorded;
  SoundSequenceState state;
};

/* Writes of the evaluated frame, and of the last recorded frame that wrote the property. */
struct SoundRecordedProperty {
  Vector<SoundPropertyWrite> recorded;
  Vector<SoundPropertyWrite> writes;
};

/* The state that changed at a recorded frame. */
struct SoundRecordedChanges {
  int frame;
  Vector<std::tuple<AUD_Sequence, AUD_SequenceEntry, SoundEntryState>> entries;
  Vector<std::pair<AUD_Sequence, SoundSequenceState>> sequences;
  Vector<std::pair<std::shared_ptr<aud::AnimateableProperty>, Vector<SoundPropertyWrite>>>
      properties;
};

/* The frames of a Scene used by Scene strips whose values were recorded for a reference to it. */
struct SoundRecordedReference {
  /* The last recorded frame of the recording's Scene during which the reference plays the
   * Scene. */
  int last_frame;
  /* Reading the reference before this frame of the recording's Scene reads recorded values. */
  int read_frame;
  /* Frames of the played Scene that started during a recorded frame, with that frame of the
   * recording's Scene. Starts with the frames that contain the last frame read with recorded
   * values. */
  Vector<std::pair<Bounds<int>, int>> started_frames;
};

/* A property of a reference whose values were written while recording: the Sequence that owns it,
 * the reference, the entry and the type, see #sound_recording_reference_property. */
struct SoundReferencePropertyKey {
  const aud::Sequence *sequence;
  Vector<int> reference;
  int entry_id;
  aud::AnimateablePropertyType type;

  uint64_t hash() const
  {
    return get_default_hash(sequence, reference, entry_id, int(type));
  }
  friend bool operator==(const SoundReferencePropertyKey &a,
                         const SoundReferencePropertyKey &b) = default;
};

struct SoundRecordedReferenceProperty {
  /* Keeps the Sequence that owns the property. */
  AUD_Sequence sequence;
  /* The last written frame of the played Scene. */
  int last_frame = INT_MIN;
  /* Values that readers read from a frame of the recording's Scene on, in the order of the
   * frames. */
  Vector<std::pair<int, std::unique_ptr<aud::AnimateableProperty>>> versions;
};

/* See #BKE_sound_audio_animation_use. */
struct SoundAnimationUse {};

/* Mixed audio that some readers of a recording didn't read yet. */
struct SoundMixedBlock {
  int64_t sample;
  int readers;
  Array<uint8_t> data;
};

/**
 * The movie writer reads the audio of a rendered frame after later frames were evaluated. While
 * a render records its audio, evaluation doesn't change the state of the Sequences that is read
 * when mixing, apart from values of single frames, which are stored per frame instead. Changes
 * are collected per entry, Sequence and property, recorded for the evaluated frame by
 * #BKE_sound_record_scene_audio_animation, and applied when readers reach that frame. Entries,
 * sounds and properties of recorded changes are kept until then.
 */
struct SoundRecording {
  /* Held while evaluating and recording a frame, and while applying the recorded state and
   * reading audio. */
  std::mutex mutex;
  /* Protects the state below, evaluation changes it from any thread. */
  std::mutex state_mutex;
  Map<AUD_SequenceEntry, SoundRecordedEntry> entries;
  Map<AUD_Sequence, SoundRecordedSequence> sequences;
  Map<std::shared_ptr<aud::AnimateableProperty>, SoundRecordedProperty> properties;
  /* Recorded changes that readers didn't reach yet, in the order of their frames. */
  Vector<SoundRecordedChanges> changes;
  /* Only used by the evaluating thread. */
  bool has_recorded_frame = false;
  /* Values of Scenes used by Scene strips are recorded per reference for the frames that start
   * during the evaluated frame. Reading a frame also reads values of the frames after it. Used
   * while holding #mutex. */
  Map<std::pair<const aud::Sequence *, Vector<int>>, SoundRecordedReference> references;
  /* Audio before this time (in seconds) only reads values of references that were recorded. Used
   * while holding #mutex. */
  double references_recorded_time = std::numeric_limits<double>::infinity();
  /* Properties of references whose values were written, used like #references. */
  Map<SoundReferencePropertyKey, SoundRecordedReferenceProperty> reference_properties;

  /* Mixdown of the recorded audio, used while holding #mutex. Each block is mixed once with the
   * state of its frame, and kept until all readers read it. */
  AUD_Device mixdown;
  aud::DeviceSpecs mixdown_specs;
  int mixdown_readers = 0;
  int64_t mixdown_sample = 0;
  Vector<SoundMixedBlock> mixed_blocks;
};

}  // namespace bke

static bke::SoundEntryState sound_entry_state_get(aud::SequenceEntry &entry)
{
  /* The getters don't lock, and readers of a recording apply state from other threads. */
  std::lock_guard<aud::ILockable> lock(entry);
  bke::SoundEntryState state;
  state.sound = entry.getSound();
  state.muted = entry.isMuted();
  return state;
}

static void sound_entry_state_apply(const AUD_Sequence &sequence,
                                    const AUD_SequenceEntry &entry,
                                    const bke::SoundEntryState &state)
{
  if (state.removed) {
    sequence->remove(entry);
    return;
  }
  std::lock_guard<aud::ILockable> lock(*entry);
  if (state.bounds) {
    entry->move(state.bounds->begin, state.bounds->end, state.bounds->skip);
  }
  entry->setSound(state.sound);
  if (entry->isMuted() != state.muted) {
    entry->mute(state.muted);
  }
  if (!state.speaker) {
    return;
  }
  const std::array<float, 8> &speaker = *state.speaker;
  if (entry->getVolumeMaximum() != speaker[0]) {
    entry->setVolumeMaximum(speaker[0]);
  }
  if (entry->getVolumeMinimum() != speaker[1]) {
    entry->setVolumeMinimum(speaker[1]);
  }
  if (entry->getDistanceMaximum() != speaker[2]) {
    entry->setDistanceMaximum(speaker[2]);
  }
  if (entry->getDistanceReference() != speaker[3]) {
    entry->setDistanceReference(speaker[3]);
  }
  if (entry->getAttenuation() != speaker[4]) {
    entry->setAttenuation(speaker[4]);
  }
  if (entry->getConeAngleOuter() != speaker[5]) {
    entry->setConeAngleOuter(speaker[5]);
  }
  if (entry->getConeAngleInner() != speaker[6]) {
    entry->setConeAngleInner(speaker[6]);
  }
  if (entry->getConeVolumeOuter() != speaker[7]) {
    entry->setConeVolumeOuter(speaker[7]);
  }
}

static void sound_sequence_state_apply(aud::Sequence &sequence,
                                       const bke::SoundSequenceState &state)
{
  if (state.muted && sequence.isMuted() != *state.muted) {
    sequence.mute(*state.muted);
  }
  if (state.fps && sequence.getFPS() != *state.fps) {
    sequence.setFPS(*state.fps);
  }
  if (!state.listener) {
    return;
  }
  const bke::SoundListener &listener = *state.listener;
  if (sequence.getSpeedOfSound() != listener.speed_of_sound) {
    sequence.setSpeedOfSound(listener.speed_of_sound);
  }
  if (sequence.getDopplerFactor() != listener.doppler_factor) {
    sequence.setDopplerFactor(listener.doppler_factor);
  }
  if (sequence.getDistanceModel() != listener.distance_model) {
    sequence.setDistanceModel(listener.distance_model);
  }
}

static void sound_property_write_apply(aud::AnimateableProperty &property,
                                       const bke::SoundPropertyWrite &write)
{
  if (write.constant_range) {
    property.writeConstantRange(&write.value, write.frame_start, write.frame_end);
  }
  else {
    property.write(&write.value, write.frame_start, 1);
  }
}

/* While audio of the Scene is recorded, readers of earlier frames use the state of the Sequence at
 * those frames, see #bke::SoundRecording. */
static std::shared_ptr<bke::SoundRecording> sound_scene_recording(const Scene *scene)
{
  return scene->runtime->audio.recording.lock();
}

/* Change the state of an entry of the Scene's Sequence that is read when mixing. */
static void sound_entry_update(const Scene *scene,
                               const AUD_SequenceEntry &entry,
                               const FunctionRef<void(bke::SoundEntryState &state)> fn)
{
  sound_verify_evaluated_id(&scene->id);
  const std::shared_ptr<bke::SoundRecording> recording = sound_scene_recording(scene);
  if (!recording) {
    bke::SoundEntryState state = sound_entry_state_get(*entry);
    fn(state);
    sound_entry_state_apply(scene->runtime->audio.sound_scene, entry, state);
    return;
  }
  std::lock_guard lock(recording->state_mutex);
  bke::SoundRecordedEntry &recorded = recording->entries.lookup_or_add_cb(entry, [&]() {
    const bke::SoundEntryState state = sound_entry_state_get(*entry);
    return bke::SoundRecordedEntry{scene->runtime->audio.sound_scene, state, state};
  });
  fn(recorded.state);
}

static AUD_SequenceEntry sound_entry_add(const Scene *scene,
                                         const AUD_Sound &sound,
                                         const bke::SoundEntryBounds &bounds)
{
  aud::Sequence &sequence = *scene->runtime->audio.sound_scene;
  const std::shared_ptr<bke::SoundRecording> recording = sound_scene_recording(scene);
  if (!recording) {
    return AUD_SequenceEntry(sequence.add(sound, bounds.begin, bounds.end, bounds.skip));
  }
  /* Start after all audio until readers reach the frame. */
  const double hidden = std::numeric_limits<double>::infinity();
  AUD_SequenceEntry entry(sequence.add(sound, hidden, hidden, 0.0));
  bke::SoundEntryState state;
  state.bounds = bounds;
  state.sound = sound;
  std::lock_guard lock(recording->state_mutex);
  recording->entries.add_new(entry,
                             {scene->runtime->audio.sound_scene, std::nullopt, std::move(state)});
  return entry;
}

/* Change the state of the Scene's Sequence that is read when mixing. */
static void sound_sequence_update(const Scene *scene,
                                  const FunctionRef<void(bke::SoundSequenceState &state)> fn)
{
  sound_verify_evaluated_id(&scene->id);
  const AUD_Sequence &sequence = scene->runtime->audio.sound_scene;
  const std::shared_ptr<bke::SoundRecording> recording = sound_scene_recording(scene);
  if (!recording) {
    bke::SoundSequenceState state;
    fn(state);
    sound_sequence_state_apply(*sequence, state);
    return;
  }
  std::lock_guard lock(recording->state_mutex);
  fn(recording->sequences.lookup_or_add_default(sequence).state);
}

/* Write animated values of other frames than the evaluated one. */
static void sound_property_write(const Scene *scene,
                                 const std::shared_ptr<aud::AnimateableProperty> &property,
                                 const bke::SoundPropertyWrite &write)
{
  sound_verify_evaluated_id(&scene->id);
  if (const std::shared_ptr<bke::SoundRecording> recording = sound_scene_recording(scene)) {
    std::lock_guard lock(recording->state_mutex);
    recording->properties.lookup_or_add_default(property).writes.append(write);
    return;
  }
  sound_property_write_apply(*property, write);
}

static std::shared_ptr<aud::AnimateableProperty> sound_entry_pitch(const AUD_SequenceEntry &entry)
{
  return std::shared_ptr<aud::AnimateableProperty>(entry, entry->getAnimProperty(aud::AP_PITCH));
}

static void sound_property_copy(aud::AnimateableProperty &src, aud::AnimateableProperty &dst)
{
  std::lock_guard<aud::ILockable> lock(src);
  const int frames = src.isAnimated() ? src.getBufferLength() : 1;
  Array<float> values(int64_t(frames) * src.getCount());
  src.readFrames(src.getBufferPosition(), frames, values.data());
  /* Frames before the buffer have the value of its first frame. */
  dst.write(values.data());
  if (src.isAnimated()) {
    dst.write(values.data(), src.getBufferPosition(), frames);
  }
}

/* Like #aud::SequenceEntry::getSoundRange, for the state of the entry after the evaluated frame,
 * which readers might not use yet. */
static bool sound_entry_sound_range(const Scene *scene,
                                    const AUD_SequenceEntry &entry,
                                    const double begin,
                                    const double end,
                                    double &r_sound_begin,
                                    double &r_sound_end)
{
  const std::shared_ptr<bke::SoundRecording> recording = sound_scene_recording(scene);
  if (!recording) {
    return entry->getSoundRange(begin, end, r_sound_begin, r_sound_end);
  }
  std::lock_guard lock(recording->state_mutex);
  const std::shared_ptr<aud::AnimateableProperty> pitch = sound_entry_pitch(entry);
  const bke::SoundEntryState *state = nullptr;
  Vector<const Vector<bke::SoundPropertyWrite> *> pitch_writes;
  for (const bke::SoundRecordedChanges &changes : recording->changes) {
    for (const auto &[sequence, changed_entry, changed_state] : changes.entries) {
      if (changed_entry == entry) {
        state = &changed_state;
      }
    }
    for (const auto &[property, writes] : changes.properties) {
      if (property == pitch) {
        pitch_writes.append(&writes);
      }
    }
  }
  const bke::SoundRecordedEntry *recorded = recording->entries.lookup_ptr(entry);
  if (recorded && recorded->recorded != recorded->state) {
    state = &recorded->state;
  }
  if (const bke::SoundRecordedProperty *property = recording->properties.lookup_ptr(pitch)) {
    /* Writes equal to the recorded ones are pending above or already applied. */
    if (!property->writes.is_empty() && property->writes != property->recorded) {
      pitch_writes.append(&property->writes);
    }
  }
  if (state == nullptr && pitch_writes.is_empty()) {
    return entry->getSoundRange(begin, end, r_sound_begin, r_sound_end);
  }
  if (state == nullptr && recorded) {
    state = &recorded->state;
  }
  if (state && state->removed) {
    return false;
  }
  if (state == nullptr || !state->bounds) {
    /* Unchanged since the recording started. Strips get their time range whenever their
     * retiming is written. */
    return entry->getSoundRange(begin, end, r_sound_begin, r_sound_end);
  }
  /* Use an entry with the latest time range and retiming, which readers don't use yet. */
  aud::Sequence &sound_scene = *scene->runtime->audio.sound_scene;
  aud::Sequence sequence(sound_scene.getSpecs(), sound_scene.getFPS(), false);
  const AUD_SequenceEntry latest = sequence.add(
      nullptr, state->bounds->begin, state->bounds->end, state->bounds->skip);
  aud::AnimateableProperty &latest_pitch = *latest->getAnimProperty(aud::AP_PITCH);
  sound_property_copy(*pitch, latest_pitch);
  for (const Vector<bke::SoundPropertyWrite> *writes : pitch_writes) {
    for (const bke::SoundPropertyWrite &write : *writes) {
      sound_property_write_apply(latest_pitch, write);
    }
  }
  return latest->getSoundRange(begin, end, r_sound_begin, r_sound_end);
}

/* Record the changes of the evaluated frame. Returns entries that were added since the last
 * recorded frame. */
static Vector<AUD_SequenceEntry> sound_recording_record_changes(bke::SoundRecording &recording,
                                                                const int frame)
{
  std::lock_guard lock(recording.state_mutex);
  bke::SoundRecordedChanges changes;
  changes.frame = frame;
  Vector<AUD_SequenceEntry> added;
  recording.entries.remove_if([&](const auto &item) {
    bke::SoundRecordedEntry &entry = item.value;
    if (entry.recorded == entry.state) {
      return false;
    }
    if (!entry.recorded) {
      added.append(item.key);
    }
    changes.entries.append({entry.sequence, item.key, entry.state});
    entry.recorded = entry.state;
    return entry.state.removed;
  });
  for (const auto item : recording.sequences.items()) {
    if (item.value.recorded != item.value.state) {
      changes.sequences.append({item.key, item.value.state});
      item.value.recorded = item.value.state;
    }
  }
  /* Properties that are not written anymore are not kept, e.g. of removed entries. */
  recording.properties.remove_if([&](const auto &item) {
    bke::SoundRecordedProperty &property = item.value;
    if (property.writes.is_empty()) {
      return true;
    }
    /* Writing the same values again doesn't change the property. */
    if (property.writes != property.recorded) {
      changes.properties.append({item.key, property.writes});
      property.recorded = std::move(property.writes);
    }
    property.writes.clear();
    return false;
  });
  if (!changes.entries.is_empty() || !changes.sequences.is_empty() ||
      !changes.properties.is_empty())
  {
    recording.changes.append(std::move(changes));
  }
  return added;
}

/* Apply the state recorded up to the frame, before reading audio that belongs to it. */
static void sound_recording_apply(bke::SoundRecording &recording, const int frame)
{
  std::lock_guard lock(recording.state_mutex);
  int64_t applied = 0;
  for (const bke::SoundRecordedChanges &frame_changes : recording.changes) {
    if (frame_changes.frame > frame) {
      break;
    }
    for (const auto &[property, writes] : frame_changes.properties) {
      for (const bke::SoundPropertyWrite &write : writes) {
        sound_property_write_apply(*property, write);
      }
    }
    for (const auto &[sequence, entry, state] : frame_changes.entries) {
      sound_entry_state_apply(sequence, entry, state);
    }
    for (const auto &[sequence, state] : frame_changes.sequences) {
      sound_sequence_state_apply(*sequence, state);
    }
    applied++;
  }
  /* Entries, sounds and properties that readers don't use anymore are freed. */
  recording.changes.remove(0, applied);

  for (const auto item : recording.reference_properties.items()) {
    const bke::SoundReferencePropertyKey &key = item.key;
    bke::SoundRecordedReferenceProperty &recorded = item.value;
    int64_t replaced = 0;
    for (auto &[version_frame, version] : recorded.versions) {
      if (version_frame > frame) {
        break;
      }
      /* Readers read either the earlier or the new values. */
      recorded.sequence->setReferenceAnimProperty(
          std::vector<int>(key.reference.begin(), key.reference.end()),
          key.entry_id,
          key.type,
          std::move(version));
      replaced++;
    }
    recorded.versions.remove(0, replaced);
  }
}

/**
 * The property that values of a reference to a Scene used by Scene strips are written to for the
 * frames that start during the evaluated frame, see
 * #sound_foreach_nested_scene_evaluated_value. Readers of the recording read earlier frames of the
 * recording's Scene later. When the reference plays frames again that the recording wrote values
 * for, like when a handler moves its Scene strip, the values are written to a new version of the
 * property instead, which replaces the property that readers read once they reach the frame, see
 * #sound_recording_apply.
 */
static aud::AnimateableProperty &sound_recording_reference_property(
    bke::SoundRecording &recording,
    const AUD_Sequence &sequence,
    const Span<int> reference,
    const SoundPropertyValue &value,
    aud::AnimateableProperty &property,
    const Bounds<int> frames,
    const int frame)
{
  bke::SoundRecordedReferenceProperty &recorded = recording.reference_properties.lookup_or_add_cb(
      {sequence.get(), Vector<int>(reference), value.entry_id, value.type},
      [&]() { return bke::SoundRecordedReferenceProperty{sequence}; });
  /* The reference plays written frames again, also when it plays an earlier frame again without
   * starting a frame. */
  const bool again = frames.min <= frames.max ? frames.min <= recorded.last_frame :
                                                frames.max < recorded.last_frame;
  if (again) {
    recorded.versions.append({frame, std::make_unique<aud::AnimateableProperty>(1, value.value)});
    /* Readers read the earlier values before this frame, which don't change anymore. Only the
     * frames that the reference plays from here on are waited for. */
    if (bke::SoundRecordedReference *played = recording.references.lookup_ptr(
            {sequence.get(), Vector<int>(reference)}))
    {
      played->read_frame = std::max(played->read_frame, frame);
      played->started_frames.clear();
    }
  }
  if (frames.min <= frames.max || again || recorded.last_frame == INT_MIN) {
    recorded.last_frame = frames.max;
  }
  return recorded.versions.is_empty() ? property : *recorded.versions.last().second;
}

/* Hold the value of a property that is written as framed value since its creation for the frames
 * before \a frame, which were not recorded. */
static void sound_hold_recorded_frame(aud::AnimateableProperty *property,
                                      const int frame,
                                      const int frames_before)
{
  std::lock_guard<aud::ILockable> lock(*property);
  if (!property->isAnimated()) {
    return;
  }
  float value[4];
  property->read(frame, value);
  for (int held_frame = std::max(frame - frames_before, 0); held_frame < frame; held_frame++) {
    property->write(value, held_frame, 1);
  }
}

static void sound_hold_recorded_speaker_frame(const AUD_SequenceEntry &handle,
                                              const int frame,
                                              const int frames_before)
{
  for (const aud::AnimateablePropertyType type :
       {aud::AP_VOLUME, aud::AP_PITCH, aud::AP_LOCATION, aud::AP_ORIENTATION})
  {
    sound_hold_recorded_frame(handle->getAnimProperty(type), frame, frames_before);
  }
}

/* Call \a fn for each Scene used by Scene strips of the Scene, directly or indirectly, once. */
static void sound_foreach_nested_scene(const Scene *scene_eval,
                                       Set<const Scene *> &visited_scenes,
                                       const FunctionRef<void(Scene &scene)> fn)
{
  if (scene_eval->ed == nullptr) {
    return;
  }
  seq::foreach_strip(&scene_eval->ed->seqbase, [&](Strip *strip) {
    if (strip->type == STRIP_TYPE_SCENE && strip->scene != nullptr &&
        DEG_is_evaluated(strip->scene) && visited_scenes.add(strip->scene))
    {
      fn(*strip->scene);
      sound_foreach_nested_scene(strip->scene, visited_scenes, fn);
    }
    return true;
  });
}

/* Scenes used by Scene strips are mixed into the recorded audio. */
static void sound_recording_attach(const std::shared_ptr<bke::SoundRecording> &recording,
                                   const Scene *scene_eval)
{
  Set<const Scene *> visited_scenes = {scene_eval};
  sound_foreach_nested_scene(scene_eval, visited_scenes, [&](Scene &scene) {
    std::weak_ptr<bke::SoundRecording> &scene_recording = scene.runtime->audio.recording;
    if (scene_recording.lock() != recording) {
      scene_recording = recording;
    }
  });
}

/**
 * Free the values of single frames that the mixdown of the recording doesn't read anymore, so that
 * their memory doesn't grow with the length of the render. The mixdown only reads forward, and
 * also reads the frame before its position, which the audio of that frame is interpolated with.
 * Pitch is kept, seeking integrates it from the start of the entry. An Explicit cache keeps all
 * its frames.
 */
static void sound_recording_discard_read_frames(const bke::SoundRecording &recording,
                                                const Scene *scene_eval)
{
  const bke::SceneAudioRuntime &audio = scene_eval->runtime->audio;
  if (recording.mixdown_readers == 0 ||
      audio.animation_cache_type == bke::AudioAnimationCacheType::Explicit)
  {
    return;
  }
  /* One more frame covers the single precision frames of the audio reader. */
  const int frame = int(std::floor(double(recording.mixdown_sample) /
                                   recording.mixdown_specs.rate *
                                   scene_eval->frames_per_second())) -
                    2;
  for (const aud::AnimateablePropertyType type :
       {aud::AP_VOLUME, aud::AP_LOCATION, aud::AP_ORIENTATION})
  {
    audio.sound_scene->getAnimProperty(type)->discardBefore(frame);
    for (const AUD_SequenceEntry &handle : audio.speaker_handles) {
      handle->getAnimProperty(type)->discardBefore(frame);
    }
  }
  if (scene_eval->ed != nullptr) {
    seq::foreach_strip(&scene_eval->ed->seqbase, [&](Strip *strip) {
      if (strip->runtime != nullptr && strip->runtime->scene_sound != nullptr) {
        strip->runtime->scene_sound->getAnimProperty(aud::AP_VOLUME)->discardBefore(frame);
        strip->runtime->scene_sound->getAnimProperty(aud::AP_PANNING)->discardBefore(frame);
      }
      return true;
    });
  }
}

static void sound_recording_record_reference(bke::SoundRecording &recording,
                                             const aud::Sequence *sequence,
                                             const Span<int> reference,
                                             const Bounds<int> frames,
                                             const int frame)
{
  bke::SoundRecordedReference &recorded = recording.references.lookup_or_add_cb(
      {sequence, Vector<int>(reference)},
      [&]() { return bke::SoundRecordedReference{frame, frame}; });
  recorded.last_frame = frame;
  if (frames.min <= frames.max &&
      (recorded.started_frames.is_empty() || recorded.started_frames.last().second != frame))
  {
    recorded.started_frames.append({frames, frame});
  }
}

/**
 * Update #bke::SoundRecording::references_recorded_time after recording the evaluated frame.
 * Reading a frame of a played Scene also reads the next frames, see #SOUND_ANIMATION_FRAMES_AHEAD.
 * The frames before the last recorded ones are read with recorded values from the start of the
 * frame of the recording's Scene during which the first of them started.
 */
static void sound_recording_update_references(bke::SoundRecording &recording,
                                              const Scene *scene_eval)
{
  const int frame = scene_eval->r.cfra;
  recording.references_recorded_time = std::numeric_limits<double>::infinity();
  recording.references.remove_if([&](const auto &item) {
    bke::SoundRecordedReference &recorded = item.value;
    /* All values of references that don't play anymore are recorded. */
    if (recorded.last_frame != frame) {
      return true;
    }
    Vector<std::pair<Bounds<int>, int>> &started = recorded.started_frames;
    if (!started.is_empty()) {
      const int read_until = started.last().first.max - (SOUND_ANIMATION_FRAMES_AHEAD - 1);
      const int64_t index = std::find_if(started.begin(),
                                         started.end(),
                                         [&](const std::pair<Bounds<int>, int> &item) {
                                           return item.first.max >= read_until;
                                         }) -
                            started.begin();
      /* Frames before the first started one are read from the start of the reference. Frames that
       * were read with recorded values stay so when the played frames jump. */
      if (index > 0 || read_until >= started[index].first.min) {
        recorded.read_frame = std::max(recorded.read_frame, started[index].second);
        started.remove(0, index);
      }
    }
    recording.references_recorded_time = std::min(
        recording.references_recorded_time, recorded.read_frame / scene_eval->frames_per_second());
    return false;
  });
}

static void sound_recording_record_frame(const std::shared_ptr<bke::SoundRecording> &recording,
                                         Scene *scene_eval)
{
  const int frame = scene_eval->r.cfra;
  const bke::SceneAudioRuntime &audio = scene_eval->runtime->audio;
  if (!recording->has_recorded_frame &&
      audio.animation_cache_type != bke::AudioAnimationCacheType::Explicit)
  {
    /* Speakers and the listener are written as framed values since their creation. The frame
     * before the first recorded one is not evaluated, it holds the first values instead of their
     * defaults, which audio of the first frame would be interpolated with. */
    sound_hold_recorded_frame(audio.sound_scene->getAnimProperty(aud::AP_LOCATION), frame, 1);
    sound_hold_recorded_frame(audio.sound_scene->getAnimProperty(aud::AP_ORIENTATION), frame, 1);
    for (const AUD_SequenceEntry &handle : audio.speaker_handles) {
      sound_hold_recorded_speaker_frame(handle, frame, 1);
    }
    /* The Scene volume and the volume and pan of strips can keep a value of an earlier recording
     * or playback before the first recorded frame. */
    sound_hold_recorded_frame(audio.sound_scene->getAnimProperty(aud::AP_VOLUME), frame, 1);
    if (scene_eval->ed != nullptr) {
      seq::foreach_strip(&scene_eval->ed->seqbase, [&](Strip *strip) {
        if (strip->runtime != nullptr && strip->runtime->scene_sound != nullptr) {
          const AUD_SequenceEntry &handle = strip->runtime->scene_sound;
          sound_hold_recorded_frame(handle->getAnimProperty(aud::AP_VOLUME), frame, 1);
          sound_hold_recorded_frame(handle->getAnimProperty(aud::AP_PANNING), frame, 1);
        }
        return true;
      });
    }
  }
  sound_recording_attach(recording, scene_eval);
  const Vector<AUD_SequenceEntry> added = sound_recording_record_changes(*recording, frame);
  if (recording->has_recorded_frame) {
    for (const AUD_SequenceEntry &handle : added) {
      if (audio.speaker_handles.contains(handle)) {
        /* Speaker entries are created again when their Object is copied. Readers use the new
         * entry from this frame on, like at the first recorded frame. */
        sound_hold_recorded_speaker_frame(handle, frame, 1);
      }
    }
  }
  recording->has_recorded_frame = true;
  sound_recording_discard_read_frames(*recording, scene_eval);
}

/* Playback prepares this many seconds of audio animation ahead of the current frame. This covers
 * the audio device buffer, interpolation and short delays of the playback steps, so that audio
 * rarely falls back to the current values. */
#  define SOUND_ANIMATION_PLAYBACK_SECONDS 1.0
/* Work that one playback step spends on preparing audio animation, see
 * #sound_sample_automatic_scene_audio_animation. Takes a few milliseconds. */
#  define SOUND_ANIMATION_PLAYBACK_WORK 16384

#endif

void BKE_sound_ensure_playback_audio_animation(Main *bmain,
                                               Scene *scene_orig,
                                               Depsgraph *depsgraph)
{
#ifndef WITH_AUDASPACE
  UNUSED_VARS(bmain, scene_orig, depsgraph);
#else
  const std::optional<uint64_t> frame_change_handlers_hash = sound_frame_change_handlers_hash();

  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
  Scene *scene_eval = DEG_get_evaluated(depsgraph, scene_orig);
  if (!DEG_is_evaluated(scene_eval)) {
    return;
  }
  bke::SceneAudioRuntime &audio = scene_eval->runtime->audio;
  const int frame_start = scene_orig->playback_start();
  const int frame_end = scene_orig->playback_end();
  if (sound_scene_audio_animation_cache_is_compatible(
          audio, frame_change_handlers_hash, {Bounds<int>(frame_start, frame_end)}) ||
      sound_scene_audio_animation_is_read(audio))
  {
    return;
  }
  if (audio.animation_cache_type == bke::AudioAnimationCacheType::Explicit) {
    /* Explicit caches are only used when they cover the whole playback range. */
    audio.animation_cache_dirty = true;
  }

  const int frame = std::clamp(scene_orig->r.cfra, frame_start, frame_end);
  const int frames_ahead = int(std::ceil(scene_eval->frames_per_second() *
                                         SOUND_ANIMATION_PLAYBACK_SECONDS)) +
                           SOUND_ANIMATION_FRAMES_AHEAD;
  /* Extend the prepared frames once less than the look-ahead remains, then by about as many
   * frames, so that preparation doesn't run at every step. */
  if (sound_scene_audio_animation_cache_is_compatible(
          audio,
          frame_change_handlers_hash,
          {Bounds<int>(frame, std::min(frame_end, frame + frames_ahead))}))
  {
    return;
  }
  sound_prepare_scene_audio_animation(
      depsgraph,
      scene_eval,
      {frame, int(std::min(int64_t(frame_end), int64_t(frame) + 2 * frames_ahead))},
      true,
      SOUND_ANIMATION_PLAYBACK_WORK);
#endif
}

void BKE_sound_discard_outdated_audio_animation(Main *bmain, Depsgraph *depsgraph)
{
#ifndef WITH_AUDASPACE
  UNUSED_VARS(bmain, depsgraph);
#else
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  bke::SceneAudioRuntime &audio = scene_eval->runtime->audio;
  /* Renders don't scrub. Other readers still use the stored values, and forward playback prepares
   * the caches at its next step. */
  if (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER || sound_scene_audio_animation_is_read(audio) ||
      sound_animation_is_playing(bmain))
  {
    return;
  }
  /* Changes of the Scenes used by Scene strips also make the cache of the graph's Scene
   * outdated. The caches of those Scenes are kept while it is not, even when they are marked as
   * dirty, since its cache only covers frames that include their animation. */
  if (!audio.animation_cache_dirty &&
      (audio.animation_cache_type == bke::AudioAnimationCacheType::None ||
       audio.animation_cache_frame_change_handlers_hash == sound_frame_change_handlers_hash()))
  {
    return;
  }
  Vector<Scene *> scenes = {scene_eval};
  Set<const Scene *> visited_scenes = {scene_eval};
  sound_foreach_nested_scene(
      scene_eval, visited_scenes, [&](Scene &nested_scene) { scenes.append(&nested_scene); });
  /* A device can still mix audio of an earlier frame change, like a scrubbed frame. It hears
   * either the outdated or the current values. The writes below don't lock the device
   * themselves. */
  BKE_sound_lock();
  Vector<Scene *> discarded_scenes;
  for (Scene *cache_scene_eval : scenes) {
    bke::SceneAudioRuntime &cache = cache_scene_eval->runtime->audio;
    if ((cache_scene_eval == scene_eval || cache.animation_cache_dirty) &&
        (cache.animation_cache_type != bke::AudioAnimationCacheType::None ||
         sound_scene_audio_animation_cache_has_data(cache)))
    {
      sound_reset_scene_audio_animation_no_lock(cache_scene_eval);
      sound_clear_scene_audio_animation_cache_state(cache);
      discarded_scenes.append(cache_scene_eval);
    }
  }
  for (Scene *cache_scene_eval : discarded_scenes) {
    /* Without a cache, the evaluated values of nested Scenes are written again. */
    sound_sync_scene_audio_current_frame(depsgraph, cache_scene_eval);
  }
  BKE_sound_unlock();
#endif
}

std::shared_ptr<bke::SoundAnimationUse> BKE_sound_audio_animation_use(const Scene *scene)
{
#ifdef WITH_AUDASPACE
  sound_verify_evaluated_id(&scene->id);
  std::weak_ptr<bke::SoundAnimationUse> &animation_use = scene->runtime->audio.animation_use;
  std::shared_ptr<bke::SoundAnimationUse> use = animation_use.lock();
  if (!use) {
    use = std::make_shared<bke::SoundAnimationUse>();
    animation_use = use;
  }
  return use;
#else
  UNUSED_VARS(scene);
  return nullptr;
#endif
}

void BKE_sound_record_scene_audio_animation(Depsgraph *depsgraph)
{
#ifdef WITH_AUDASPACE
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  if (scene_eval->runtime->audio.sound_scene == nullptr) {
    return;
  }
  /* Speakers and the listener are always written as framed values. */
  SoundSceneAudioProperties properties;
  properties.scene_volume = true;
  if (scene_eval->ed != nullptr) {
    seq::foreach_strip(&scene_eval->ed->seqbase, [&](Strip *strip) {
      if (strip->runtime != nullptr && strip->runtime->scene_sound != nullptr) {
        properties.strip_volumes.add(strip->runtime->scene_sound);
        properties.strip_pans.add(strip->runtime->scene_sound);
      }
      return true;
    });
  }
  bke::SceneAudioRuntime &audio = scene_eval->runtime->audio;
  const bke::AudioAnimationWriteState old_write_state = audio.animation_write_state;
  audio.animation_write_state = bke::AudioAnimationWriteState::Bake;
  sound_write_scene_audio_properties_at_frame(scene_eval, properties, scene_eval->r.cfra);
  audio.animation_write_state = old_write_state;

  /* Scenes used by Scene strips are not evaluated at their own frames, prepare the frames that
   * are played during this frame. */
  sound_prepare_scene_audio_animation(
      depsgraph, scene_eval, {scene_eval->r.cfra, scene_eval->r.cfra}, false, INT64_MAX);
  const std::shared_ptr<bke::SoundRecording> recording = audio.recording.lock();
  sound_foreach_nested_scene_evaluated_value(
      depsgraph,
      [&](Scene &nested_scene_eval,
          const Span<int> reference,
          const SoundPropertyValue &value,
          const Bounds<int> frames) {
        aud::AnimateableProperty *property = &sound_reference_property(
            nested_scene_eval, reference, value.entry_id, value.type, value.value);
        if (recording) {
          property = &sound_recording_reference_property(
              *recording,
              nested_scene_eval.runtime->audio.sound_scene,
              reference,
              value,
              *property,
              frames,
              scene_eval->r.cfra);
        }
        if (!property->isAnimated()) {
          /* Frames before the first recorded one have its value, also the frame that the
           * reference plays when no frame starts during the evaluated frame. */
          property->write(&value.value);
          if (frames.min > frames.max) {
            property->write(&value.value, frames.max, 1);
          }
        }
        if (frames.min <= frames.max) {
          const Array<float> values(frames.max - frames.min + 1, value.value);
          property->write(values.data(), frames.min, int(values.size()));
          /* Like the Automatic cache, keep only the frames that audio of a strip reads. */
          if (nested_scene_eval.runtime->audio.animation_cache_type !=
              bke::AudioAnimationCacheType::Explicit)
          {
            property->discardBefore(value.read_start);
          }
        }
        if (recording) {
          sound_recording_record_reference(*recording,
                                           nested_scene_eval.runtime->audio.sound_scene.get(),
                                           reference,
                                           frames,
                                           scene_eval->r.cfra);
        }
      });
  if (recording) {
    sound_recording_update_references(*recording, scene_eval);
    sound_recording_record_frame(recording, scene_eval);
  }
#else
  UNUSED_VARS(depsgraph);
#endif
}

void BKE_sound_end_scene_audio_recording(Depsgraph *depsgraph)
{
#ifdef WITH_AUDASPACE
  /* An Explicit cache already holds the recorded values, other recorded values are discarded. */
  Scene *scene_eval = DEG_get_evaluated_scene(depsgraph);
  bke::SceneAudioRuntime &audio = scene_eval->runtime->audio;
  if (const std::shared_ptr<bke::SoundRecording> recording = audio.recording.lock()) {
    /* Also apply changes that were not recorded for a frame. */
    sound_recording_record_changes(*recording, INT_MAX);
    sound_recording_apply(*recording, INT_MAX);
  }
  audio.recording.reset();
  Set<const Scene *> visited_scenes = {scene_eval};
  sound_foreach_nested_scene(scene_eval, visited_scenes, [&](Scene &scene) {
    bke::SceneAudioRuntime &nested_audio = scene.runtime->audio;
    if (!nested_audio.recording.expired() &&
        nested_audio.animation_cache_type != bke::AudioAnimationCacheType::Explicit)
    {
      /* Values of frames before changes of the Scene were kept for the recording. */
      nested_audio.animation_cache_dirty = true;
      /* Values that depend on the evaluation were recorded per reference for the rendered frames,
       * references read the current values again. */
      if (nested_audio.sound_scene) {
        nested_audio.sound_scene->clearReferenceAnimProperties();
      }
    }
    nested_audio.recording.reset();
  });
  if (audio.animation_cache_type != bke::AudioAnimationCacheType::Explicit) {
    sound_discard_scene_audio_animation_cache(depsgraph, scene_eval);
  }
#else
  UNUSED_VARS(depsgraph);
#endif
}

std::shared_ptr<bke::SoundRecording> BKE_sound_recording_ensure(const Scene *scene)
{
#ifdef WITH_AUDASPACE
  sound_verify_evaluated_id(&scene->id);
  bke::SceneAudioRuntime &audio = scene->runtime->audio;
  std::shared_ptr<bke::SoundRecording> recording = audio.recording.lock();
  if (!recording) {
    recording = std::make_shared<bke::SoundRecording>();
    audio.recording = recording;
  }
  sound_recording_attach(recording, scene);
  return recording;
#else
  UNUSED_VARS(scene);
  return nullptr;
#endif
}

std::unique_lock<std::mutex> BKE_sound_recording_lock(bke::SoundRecording &recording)
{
#ifdef WITH_AUDASPACE
  return std::unique_lock(recording.mutex);
#else
  UNUSED_VARS(recording);
  return {};
#endif
}

bool BKE_sound_recording_references_recorded(const bke::SoundRecording &recording,
                                             const double time)
{
#ifdef WITH_AUDASPACE
  return time <= recording.references_recorded_time;
#else
  UNUSED_VARS(recording, time);
  return true;
#endif
}

bool BKE_sound_bake_animation(Main *bmain,
                              Scene *scene_orig,
                              Depsgraph *depsgraph,
                              const int frame_start,
                              const int frame_end)
{
#ifndef WITH_AUDASPACE
  UNUSED_VARS(bmain, scene_orig, depsgraph, frame_start, frame_end);
  return true;
#else
  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);
  Scene *scene_eval = DEG_get_evaluated(depsgraph, scene_orig);

  /* Discard all reachable caches, including those of Scenes that are no longer played. */
  Set<const Scene *> visited_scenes;
  Vector<Scene *> scenes;
  sound_collect_explicit_bake_scenes(scene_eval, visited_scenes, scenes);
  for (Scene *scene_eval_to_bake : scenes) {
    if (sound_scene_audio_animation_is_read(scene_eval_to_bake->runtime->audio)) {
      /* Readers that don't lock the device read the animation that would be replaced. */
      return false;
    }
  }
  const bool playback_was_playing = sound_animation_pause_playback(scene_eval);
  for (Scene *scene_eval_to_bake : scenes) {
    sound_reset_scene_audio_animation(scene_eval_to_bake);
    bke::SceneAudioRuntime &audio = scene_eval_to_bake->runtime->audio;
    sound_clear_scene_audio_animation_cache_state(audio);
  }

  /* Scenes used by Scene strips are evaluated at the frames of this graph's Scene. Their values
   * that depend on the evaluation are recorded for the frames that the strips play. */
  Map<std::pair<std::shared_ptr<aud::AnimateableProperty>, Vector<int>>, SoundNestedBakeValues>
      nested_values;
  const int old_frame = scene_orig->r.cfra;
  for (int frame = frame_start > 0 ? frame_start - 1 : 0; frame <= frame_end + 1; frame++) {
    scene_orig->r.cfra = frame;
    /* Make sure the camera is updated, since it will affect stereo/surround output. */
    BKE_scene_camera_switch_update(scene_orig);
    BKE_scene_graph_update_for_newframe(depsgraph);
    sound_foreach_nested_scene_evaluated_value(
        depsgraph,
        [&](Scene &nested_scene_eval,
            const Span<int> reference,
            const SoundPropertyValue &value,
            const Bounds<int> nested_frames) {
          SoundNestedBakeValues &values = nested_values.lookup_or_add_cb(
              {value.property, Vector<int>(reference)}, [&]() {
                return SoundNestedBakeValues{sound_original_scene(&nested_scene_eval),
                                             Vector<int>(reference),
                                             value.entry_id,
                                             value.type,
                                             value.value};
              });
          for (int nested_frame = nested_frames.min; nested_frame <= nested_frames.max;
               nested_frame++)
          {
            values.values.add_overwrite(nested_frame, value.value);
          }
        });
  }

  scene_orig->r.cfra = old_frame;
  BKE_scene_graph_update_for_newframe(depsgraph);

  scene_eval = DEG_get_evaluated(depsgraph, scene_orig);
  if (!DEG_is_evaluated(scene_eval)) {
    sound_animation_resume_playback(scene_eval, playback_was_playing);
    return true;
  }
  /* The full evaluation only advances this graph's Scene. Scenes used by Scene strips get the
   * directly evaluable animation of the frames that the strips play, as for playback and
   * rendering. */
  sound_prepare_scene_audio_animation(
      depsgraph, scene_eval, {frame_start, frame_end}, false, INT64_MAX);
  sound_write_nested_scene_bake_values(depsgraph, nested_values);

  const std::optional<uint64_t> frame_change_handlers_hash = sound_frame_change_handlers_hash();
  sound_store_scene_audio_animation_cache(scene_eval,
                                          {frame_start, frame_end},
                                          false,
                                          frame_change_handlers_hash,
                                          bke::AudioAnimationCacheType::Explicit);
  sound_animation_resume_playback(scene_eval, playback_was_playing);
  return true;
#endif
}

static void sound_free_audio(bSound *sound)
{
#ifdef WITH_AUDASPACE
  bke::SoundRuntime *runtime = sound->runtime;
  sound_runtime_generation_replace(sound, nullptr);
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
  sound_runtime_generation_replace(sound, nullptr);
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
      runtime->handle = AUD_Sound(new aud::File((uchar *)pf->data, pf->size, sound->stream_index));
    }
    else {
      /* or else load it from disk */
      runtime->handle = AUD_Sound(new aud::File(fullpath, sound->stream_index));
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
    catch (aud::Exception &ex) {
      (void)ex;
    }
  }

  if (runtime->cache) {
    runtime->playback_handle = runtime->cache;
  }
  else {
    runtime->playback_handle = runtime->handle;
  }

  auto generation = std::make_shared<bke::SoundRuntimeGeneration>();
  generation->handle = runtime->handle;
  generation->reader_cache = std::make_shared<bke::SoundReaderCache>(generation->handle);
  sound_runtime_generation_replace(sound, std::move(generation));
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

void BKE_sound_recording_mixdown_open(bke::SoundRecording &recording,
                                      const Scene *scene,
                                      const aud::DeviceSpecs &specs,
                                      const int start,
                                      const float volume,
                                      const int64_t sample)
{
  std::lock_guard lock(recording.mutex);
  if (recording.mixdown_readers == 0) {
    recording.mixdown = BKE_sound_mixdown(scene, specs, start, volume);
    recording.mixdown_specs = specs;
    recording.mixdown_sample = sample;
  }
  recording.mixdown_readers++;
}

void BKE_sound_recording_mixdown_read(bke::SoundRecording &recording,
                                      const int64_t sample,
                                      const int frame,
                                      uchar *buffer,
                                      const int length)
{
  const int64_t size = int64_t(length) * AUD_DEVICE_SAMPLE_SIZE(recording.mixdown_specs);
  for (const int64_t i : recording.mixed_blocks.index_range()) {
    bke::SoundMixedBlock &block = recording.mixed_blocks[i];
    if (block.sample == sample && block.data.size() == size) {
      memcpy(buffer, block.data.data(), size);
      if (--block.readers == 0) {
        recording.mixed_blocks.remove(i);
      }
      return;
    }
  }
  BLI_assert(sample == recording.mixdown_sample);
  sound_recording_apply(recording, frame);
  bke::sound_device_read(recording.mixdown, buffer, length);
  recording.mixdown_sample += length;
  if (recording.mixdown_readers > 1) {
    recording.mixed_blocks.append(
        {sample, recording.mixdown_readers - 1, Array<uint8_t>(Span<uint8_t>(buffer, size))});
  }
}

void BKE_sound_recording_mixdown_close(bke::SoundRecording &recording, const int64_t sample)
{
  std::lock_guard lock(recording.mutex);
  recording.mixdown_readers--;
  recording.mixed_blocks.remove_if(
      [&](bke::SoundMixedBlock &block) { return block.sample >= sample && --block.readers == 0; });
  if (recording.mixdown_readers == 0) {
    recording.mixdown.reset();
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
  audio.sound_scene = std::make_shared<aud::Sequence>(
      specs, scene->frames_per_second(), scene->audio.flag & AUDIO_MUTE);
  audio.sound_scene->setSpeedOfSound(scene->audio.speed_of_sound);
  audio.sound_scene->setDopplerFactor(scene->audio.doppler_factor);
  audio.sound_scene->setDistanceModel(aud::DistanceModel(scene->audio.distance_model));
  audio.playback_handle = nullptr;
  audio.sound_scrub_handle = nullptr;
  audio.speaker_handles.clear();
  audio.animation_write_state = bke::AudioAnimationWriteState::Normal;
  sound_clear_scene_audio_animation_cache_state(audio);
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
  audio.animation_write_state = bke::AudioAnimationWriteState::Normal;
  sound_clear_scene_audio_animation_cache_state(audio);
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
    sound_sequence_update(scene, [&](bke::SoundSequenceState &state) { state.muted = muted; });
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

  sound_sequence_update(scene, [&](bke::SoundSequenceState &state) {
    state.listener = bke::SoundListener{scene->audio.speed_of_sound,
                                        scene->audio.doppler_factor,
                                        aud::DistanceModel(scene->audio.distance_model)};
  });
}

AUD_SequenceEntry BKE_sound_scene_add_scene_sound(Scene *scene, Strip *strip)
{
  sound_verify_evaluated_id(&scene->id);
  if (strip->scene && scene != strip->scene) {
    int startframe = strip->left_handle();
    int endframe = strip->right_handle(scene);
    int frameskip = strip->startofs + strip->anim_startofs;
    const double fps = scene->frames_per_second();
    return sound_entry_add(scene,
                           strip->scene->runtime->audio.sound_scene,
                           {startframe / fps, endframe / fps, frameskip / fps});
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
    return sound_entry_add(scene,
                           strip->sound->runtime->playback_handle,
                           {startframe / fps + offset_time, endframe / fps, 0.0f});
  }
  return sound_entry_add(scene,
                         strip->sound->runtime->playback_handle,
                         {startframe / fps, endframe / fps, -offset_time});
}

void BKE_sound_remove_scene_sound(Scene *scene, AUD_SequenceEntry handle)
{
  sound_entry_update(scene, handle, [](bke::SoundEntryState &state) { state.removed = true; });
}

void BKE_sound_mute_scene_sound(const Scene *scene, AUD_SequenceEntry handle, bool mute)
{
  sound_entry_update(scene, handle, [&](bke::SoundEntryState &state) { state.muted = mute; });
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
  bke::SoundEntryBounds bounds{startframe / fps, endframe / fps, -offset_time};
  if (offset_time >= 0.0f) {
    bounds = {startframe / fps + offset_time, endframe / fps, 0.0};
  }
  sound_entry_update(scene, handle, [&](bke::SoundEntryState &state) { state.bounds = bounds; });
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

void BKE_sound_update_scene_sound(const Scene *scene, AUD_SequenceEntry handle, bSound *sound)
{
  BKE_sound_update_sequence_handle(scene, handle, sound->runtime->playback_handle);
}

#endif /* WITH_AUDASPACE */

void BKE_sound_update_sequence_handle(const Scene *scene,
                                      AUD_SequenceEntry handle,
                                      AUD_Sound sound_handle)
{
#ifdef WITH_AUDASPACE
  sound_entry_update(
      scene, handle, [&](bke::SoundEntryState &state) { state.sound = sound_handle; });
#else
  UNUSED_VARS(scene, handle, sound_handle);
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
  /* Keep framed values of an audio animation cache. Check and write atomically. */
  std::lock_guard<aud::ILockable> lock(*prop);
  if (frame >= 0 && (animated || prop->isAnimated())) {
    prop->write(value, frame, 1);
  }
  else if (!prop->isAnimated()) {
    /* Also animated values that can't be written for a frame, unless framed values are kept. */
    prop->write(value);
  }
}

void BKE_sound_set_scene_volume(Scene *scene, float volume)
{
  BKE_sound_set_scene_volume_at_frame(scene, scene->r.cfra, volume);
}

void BKE_sound_set_scene_volume_at_frame(Scene *scene, const int frame, float volume)
{
  sound_verify_evaluated_id(&scene->id);
  if (scene->runtime->audio.sound_scene == nullptr) {
    return;
  }
  const bool animated = (scene->audio.flag & AUDIO_VOLUME_ANIMATED) != 0 ||
                        scene->runtime->audio.animation_write_state ==
                            bke::AudioAnimationWriteState::Bake;
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

void BKE_sound_set_scene_sound_pitch_at_frame(const Scene *scene,
                                              AUD_SequenceEntry handle,
                                              const int frame,
                                              float pitch)
{
  if (frame >= 0) {
    sound_property_write(scene, sound_entry_pitch(handle), {frame, frame + 1, pitch, false});
  }
}

void BKE_sound_set_scene_sound_pitch_constant_range(
    const Scene *scene, AUD_SequenceEntry handle, int frame_start, int frame_end, float pitch)
{
  frame_start = max_ii(0, frame_start);
  frame_end = max_ii(0, frame_end);
  sound_property_write(scene, sound_entry_pitch(handle), {frame_start, frame_end, pitch, true});
}

void BKE_sound_set_scene_sound_pan_at_frame(AUD_SequenceEntry handle,
                                            const int frame,
                                            float pan,
                                            bool animated)
{
  set_audaspace_anim_property(handle, aud::AP_PANNING, frame, &pan, animated);
}

static void sound_reset_scene_audio_animation_no_lock(Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

  const float default_location[3] = {0.0f, 0.0f, 0.0f};
  const float default_orientation[4] = {1.0f, 0.0f, 0.0f, 0.0f};
  const float default_value = 1.0f;
  /* #AnimateableProperty::reset() keeps the first value. Restore Audaspace's defaults before
   * rewriting the properties for the current scene. */

  /* Values recorded per reference to the Scenes used by Scene strips depend on the evaluation of
   * this Scene, see #sound_foreach_nested_scene_evaluated_value. */
  Set<const Scene *> visited_scenes = {scene};
  sound_foreach_nested_scene(scene, visited_scenes, [](Scene &nested_scene) {
    bke::SceneAudioRuntime &nested_audio = nested_scene.runtime->audio;
    if (nested_audio.sound_scene) {
      nested_audio.sound_scene->clearReferenceAnimProperties();
    }
    if (nested_audio.animation_cache_type == bke::AudioAnimationCacheType::Explicit) {
      nested_audio.animation_cache_dirty = true;
    }
  });

  if (scene->runtime->audio.sound_scene != nullptr) {
    scene->runtime->audio.sound_scene->clearReferenceAnimProperties();
    aud::AnimateableProperty *volume = scene->runtime->audio.sound_scene->getAnimProperty(
        aud::AP_VOLUME);
    volume->reset();
    aud::AnimateableProperty *location = scene->runtime->audio.sound_scene->getAnimProperty(
        aud::AP_LOCATION);
    location->reset();
    location->write(default_location);
    aud::AnimateableProperty *orientation = scene->runtime->audio.sound_scene->getAnimProperty(
        aud::AP_ORIENTATION);
    orientation->reset();
    orientation->write(default_orientation);
  }

  for (const AUD_SequenceEntry &handle : scene->runtime->audio.speaker_handles) {
    aud::AnimateableProperty *volume = handle->getAnimProperty(aud::AP_VOLUME);
    volume->reset();
    volume->write(&default_value);
    aud::AnimateableProperty *pitch = handle->getAnimProperty(aud::AP_PITCH);
    pitch->reset();
    pitch->write(&default_value);
    aud::AnimateableProperty *location = handle->getAnimProperty(aud::AP_LOCATION);
    location->reset();
    location->write(default_location);
    aud::AnimateableProperty *orientation = handle->getAnimProperty(aud::AP_ORIENTATION);
    orientation->reset();
    orientation->write(default_orientation);
  }

  if (scene->ed != nullptr) {
    seq::foreach_strip(&scene->ed->seqbase, [&](Strip *strip) {
      if (strip->runtime == nullptr || strip->runtime->scene_sound == nullptr) {
        return true;
      }

      /* The pitch holds the retiming, which is not cached. */
      AUD_SequenceEntry handle = strip->runtime->scene_sound;
      handle->getAnimProperty(aud::AP_VOLUME)->reset();
      handle->getAnimProperty(aud::AP_PANNING)->reset();
      return true;
    });
  }
}

static void sound_reset_scene_audio_animation(Scene *scene)
{
  BKE_sound_lock();
  sound_reset_scene_audio_animation_no_lock(scene);
  BKE_sound_unlock();
}

static void sound_discard_scene_audio_animation_cache(Depsgraph *depsgraph, Scene *scene)
{
  sound_reset_scene_audio_animation(scene);

  bke::SceneAudioRuntime &audio = scene->runtime->audio;
  sound_clear_scene_audio_animation_cache_state(audio);
  /* Without a cache, the evaluated values of nested Scenes are written again. */
  sound_sync_scene_audio_current_frame(depsgraph, scene);
}

void BKE_sound_update_sequencer(Main *main, bSound *sound)
{
  BLI_assert_msg(0, "is not supposed to be used, is weird function.");

  Scene *scene;

  for (scene = main->scenes.first(); scene; scene = static_cast<Scene *>(scene->id.next)) {
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
  bool do_audio_scrub = scene->audio.flag & AUDIO_SCRUB && !sound_animation_is_playing(bmain);

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
    AUD_Sound sound, float *buffer, int length, int samples_per_second, const bool *interrupt)
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

    if (*interrupt) {
      return 0;
    }

    aBuffer.assureSize(len * AUD_SAMPLE_SIZE(specs));
    buf = aBuffer.getBuffer();

    reader->read(len, eos, buf);

    max = min = *buf;
    power = *buf * *buf;
    for (int j = 1; j < len; j++) {
      if (buf[j] < min) {
        min = buf[j];
      }
      if (buf[j] > max) {
        max = buf[j];
      }
      power += buf[j] * buf[j];
    }

    buffer[i * 3] = min;
    buffer[i * 3 + 1] = max;
    buffer[i * 3 + 2] = std::sqrt(power / len);

    if (overallmax < max) {
      overallmax = max;
    }
    if (overallmax < -min) {
      overallmax = -min;
    }

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
  if (*stop) {
    BKE_sound_runtime_clear_waveform_loading_tag(sound);
    return;
  }

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
    MEM_delete(waveform);
    MEM_SAFE_DELETE(runtime->waveform);
    BLI_spin_lock(&runtime->spinlock);
    runtime->tags &= ~bke::SoundTags::WaveformLoading;
    BLI_spin_unlock(&runtime->spinlock);
    if (need_close_audio_handles) {
      sound_free_audio(sound);
    }
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

      if ((strip_runtime.speaker_handle != nullptr) &&
          scene->runtime->audio.speaker_handles.remove(strip_runtime.speaker_handle))
      {
        if (speaker->sound) {
          sound_entry_update(
              scene, strip_runtime.speaker_handle, [&](bke::SoundEntryState &state) {
                state.bounds = bke::SoundEntryBounds{
                    double(strip.start) / scene->frames_per_second(), FLT_MAX, 0.0};
              });
        }
        else {
          BKE_sound_remove_scene_sound(scene, strip_runtime.speaker_handle);
          strip_runtime.speaker_handle = nullptr;
        }
      }
      else {
        if (speaker->sound) {
          strip_runtime.speaker_handle = sound_entry_add(
              scene,
              speaker->sound->runtime->playback_handle,
              {double(strip.start) / scene->frames_per_second(), FLT_MAX, 0.0});
          strip_runtime.speaker_handle->setRelative(false);
        }
      }

      if (strip_runtime.speaker_handle) {
        new_set.add(strip_runtime.speaker_handle);

        mat4_to_quat(quat, object->object_to_world().ptr());
        float3 location = object->object_to_world().location();
        set_audaspace_anim_property(
            strip_runtime.speaker_handle, aud::AP_LOCATION, scene->r.cfra, location, true);
        set_audaspace_anim_property(
            strip_runtime.speaker_handle, aud::AP_ORIENTATION, scene->r.cfra, quat, true);
        set_audaspace_anim_property(
            strip_runtime.speaker_handle, aud::AP_VOLUME, scene->r.cfra, &speaker->volume, true);
        set_audaspace_anim_property(
            strip_runtime.speaker_handle, aud::AP_PITCH, scene->r.cfra, &speaker->pitch, true);
        sound_entry_update(scene, strip_runtime.speaker_handle, [&](bke::SoundEntryState &state) {
          state.sound = speaker->sound->runtime->playback_handle;
          state.muted = (strip.flag & NLASTRIP_FLAG_MUTED) || (speaker->flag & SPK_MUTED);
          state.speaker = std::array<float, 8>{speaker->volume_max,
                                               speaker->volume_min,
                                               speaker->distance_max,
                                               speaker->distance_reference,
                                               speaker->attenuation,
                                               speaker->cone_angle_outer,
                                               speaker->cone_angle_inner,
                                               speaker->cone_volume_outer};
        });
      }
    }
  }
}

void BKE_sound_update_scene(Depsgraph *depsgraph, Scene *scene)
{
  sound_verify_evaluated_id(&scene->id);

#  ifdef WITH_PYTHON
  if (scene->runtime->audio.animation_write_state == bke::AudioAnimationWriteState::Normal) {
    const std::optional<uint64_t> frame_change_handlers_hash =
        BPY_app_handlers_frame_change_identity();
    if (scene->runtime->audio.animation_cache_frame_change_handlers_hash !=
        frame_change_handlers_hash)
    {
      /* Handler lists are not depsgraph inputs. Detect policy changes at the normal audio update
       * boundary so the next consumer update can discard an incompatible cache. */
      scene->runtime->audio.animation_cache_dirty = true;
    }
  }
#  endif

  /* Audio of Scenes used by Scene strips is read with their own FPS, which
   * #BKE_scene_update_sound only updates for this Scene. */
  Set<const Scene *> visited_scenes = {scene};
  sound_foreach_nested_scene(scene, visited_scenes, [](Scene &nested_scene) {
    if (nested_scene.runtime->audio.sound_scene) {
      sound_sequence_update(&nested_scene, [&](bke::SoundSequenceState &state) {
        state.fps = float(nested_scene.frames_per_second());
      });
    }
  });
  sound_update_scene_objects(depsgraph, scene);
}

/* Speakers and the listener of the Scene. */
static void sound_update_scene_objects(Depsgraph *depsgraph, Scene *scene)
{
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
    BKE_sound_remove_scene_sound(scene, handle);
  }
  audio.speaker_handles.clear();

  if (scene->camera) {
    mat4_to_quat(quat, scene->camera->object_to_world().ptr());
    float3 location = scene->camera->object_to_world().location();
    set_audaspace_anim_property(
        audio.sound_scene, aud::AP_LOCATION, scene->r.cfra, location, true);
    set_audaspace_anim_property(audio.sound_scene, aud::AP_ORIENTATION, scene->r.cfra, quat, true);
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
      const size_t name_size = sizeof(char) * (name.length() + 1);
      names[i] = (char *)malloc(name_size);
      memcpy(names[i], name.c_str(), name_size);
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

int BKE_sound_stream_count(Main *main, const char *filepath)
{
  char filepath_abs[FILE_MAX];
  STRNCPY(filepath_abs, filepath);
  BLI_path_abs(filepath_abs, BKE_main_blendfile_path(main));

  std::vector<aud::StreamInfo> streams;
  try {
    streams = aud::FileManager::queryStreams(filepath_abs);
    return streams.size();
  }
  catch (aud::Exception &) {
    return 0;
  }
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

void BKE_sound_set_scene_sound_time_stretch_at_frame(const Scene *scene,
                                                     AUD_Sound handle,
                                                     int frame,
                                                     float time_stretch)
{
  if (frame >= 0) {
    sound_property_write(
        scene,
        std::dynamic_pointer_cast<aud::AnimateableTimeStretchPitchScale>(handle)->getAnimProperty(
            aud::AP_TIME_STRETCH),
        {frame, frame + 1, time_stretch, false});
  }
}

void BKE_sound_set_scene_sound_time_stretch_constant_range(
    const Scene *scene, AUD_Sound handle, int frame_start, int frame_end, float time_stretch)
{
  frame_start = max_ii(0, frame_start);
  frame_end = max_ii(0, frame_end);
  sound_property_write(
      scene,
      std::dynamic_pointer_cast<aud::AnimateableTimeStretchPitchScale>(handle)->getAnimProperty(
          aud::AP_TIME_STRETCH),
      {frame_start, frame_end, time_stretch, true});
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
    if (reader) {
      aud::Specs specs = reader->getSpecs();
      res.specs.channels = eSoundChannels(specs.channels);
      res.specs.samplerate = specs.rate;
      res.length = reader->getLength() / float(specs.rate);
    }
  }
  catch (aud::Exception &ex) {
    (void)ex;
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
  catch (Exception &ex) {
    (void)ex;
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
  catch (aud::Exception &ex) {
    (void)ex;
  }
  return nullptr;
}

bool bke::sound_device_read(AUD_Device device, uchar *buffer, int length)
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
  catch (aud::Exception &ex) {
    (void)ex;
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

    if (high < rate) {
      sound = AUD_Sound(new Lowpass(sound, high));
    }
    if (low > 0) {
      sound = AUD_Sound(new Highpass(sound, low));
    }

    sound = AUD_Sound(new Envelope(sound, attack, release, threshold, 0.1f));
    sound = AUD_Sound(new LinearResample(sound, specs));

    if (square) {
      sound = AUD_Sound(new Threshold(sound, sthreshold));
    }

    if (accumulate) {
      sound = AUD_Sound(new Accumulator(sound, additive));
    }
    else if (additive) {
      sound = AUD_Sound(new Sum(sound));
    }

    reader = sound->createReader();

    if (!reader) {
      return nullptr;
    }

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
                        uint start,
                        uint length,
                        uint buffersize,
                        const char *filename,
                        const aud::DeviceSpecs &specs,
                        aud::Container format,
                        aud::Codec codec,
                        uint bitrate,
                        bool split_channels,
                        std::string &r_error,
                        bool (*progress_callback)(float, void *),
                        void *progress_data)
{
  using namespace aud;
  try {
    sequence->setSpecs(specs.specs);
    std::shared_ptr<IReader> reader = sequence->createQualityReader(ResampleQuality::MEDIUM);
    reader->seek(start);

    if (!split_channels) {
      std::shared_ptr<IWriter> writer = FileWriter::createWriter(
          filename, specs, format, codec, bitrate);
      FileWriter::writeReader(
          reader, writer, length, buffersize, progress_callback, progress_data);
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
      FileWriter::writeReader(
          reader, writers, length, buffersize, progress_callback, progress_data);
    }
    return true;
  }
  catch (Exception &e) {
    r_error = e.getMessage();
    return false;
  }
}

#else /* WITH_AUDASPACE */

#  include "BLI_utildefines.hh"

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
void BKE_sound_mute_scene_sound(const Scene * /*scene*/,
                                AUD_SequenceEntry /*handle*/,
                                bool /*mute*/)
{
}
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
void BKE_sound_update_scene_sound(const Scene * /*scene*/,
                                  AUD_SequenceEntry /*handle*/,
                                  bSound * /*sound*/)
{
}
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
void BKE_sound_set_scene_volume_at_frame(Scene * /*scene*/, int /*frame*/, float /*volume*/) {}
void BKE_sound_set_scene_sound_pitch_at_frame(const Scene * /*scene*/,
                                              AUD_SequenceEntry /*handle*/,
                                              int /*frame*/,
                                              float /*pitch*/)
{
}
void BKE_sound_set_scene_sound_pitch_constant_range(const Scene * /*scene*/,
                                                    AUD_SequenceEntry /*handle*/,
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

int BKE_sound_stream_count(Main * /*main*/, const char * /*filepath*/)
{
  return 0;
}

#endif /* WITH_AUDASPACE */

#if !defined(WITH_AUDASPACE) || !defined(WITH_RUBBERBAND)
AUD_Sound BKE_sound_ensure_time_stretch_effect(const Strip * /*strip*/, float /*fps*/)
{
  return nullptr;
}

void BKE_sound_set_scene_sound_time_stretch_at_frame(const Scene * /*scene*/,
                                                     AUD_Sound /*handle*/,
                                                     int /*frame*/,
                                                     float /*time_stretch*/)
{
}
void BKE_sound_set_scene_sound_time_stretch_constant_range(const Scene * /*scene*/,
                                                           AUD_Sound /*handle*/,
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

namespace bke {

std::shared_ptr<const bSoundFrequencySampler> bSoundFrequencySampler::get_cached(
    const bSound &sound, const Key &key)
{
#ifdef WITH_AUDASPACE
  const std::shared_ptr<SoundRuntimeGeneration> generation = sound_runtime_generation_get(sound);
  if (!generation) {
    return nullptr;
  }
  {
    /* Fast common case when the sampler has been created already. */
    bSoundFrequencySamplerMap::ConstAccessor accessor;
    if (generation->samplers.lookup(accessor, key)) {
      return accessor->second;
    }
  }
  AUD_Sound sound_handle = generation->handle;
  if (!sound_handle) {
    /* Maybe try to load the sound in this case instead of relying on cache. */
    return nullptr;
  }
  /* Slower case when the sampler is newly created. */
  bSoundFrequencySamplerMap::MutableAccessor accessor;
  if (generation->samplers.add(accessor, key)) {
    if (key.channel.has_value()) {
      const SoundInfo info = sound_info_get(sound_handle);
      const int channel = *key.channel;
      if (channel < 0 || channel >= info.specs.channels) {
        return nullptr;
      }
    }
    accessor->second = std::shared_ptr<bSoundFrequencySampler>(
        new bSoundFrequencySampler(sound_handle, generation->reader_cache, key));
  }
  return accessor->second;
#else
  UNUSED_VARS(sound, key);
  return nullptr;
#endif
}

#ifdef WITH_AUDASPACE
static bSoundFrequencySampler::WindowWeights compute_window_function_weights(
    const bSoundFrequencySampler::WindowFunction window, const int size)
{
  Array<float> weights(size);
  switch (window) {
    case bSoundFrequencySampler::WindowFunction::Hann: {
      for (const int i : IndexRange(size)) {
        weights[i] = 0.5f - 0.5f * math::cos((2.0f * std::numbers::pi * i) / (size - 1));
      }
      break;
    }
    case bSoundFrequencySampler::WindowFunction::Hamming: {
      for (const int i : IndexRange(size)) {
        weights[i] = 0.54f - 0.46f * math::cos((2.0f * std::numbers::pi * i) / (size - 1));
      }
      break;
    }
    case bSoundFrequencySampler::WindowFunction::Blackman: {
      for (const int i : IndexRange(size)) {
        weights[i] = 0.42f - 0.5f * math::cos((2.0f * std::numbers::pi * i) / (size - 1)) +
                     0.08f * math::cos((4.0f * std::numbers::pi * i) / (size - 1));
      }
      break;
    }
    case bSoundFrequencySampler::WindowFunction::Rectangular: {
      weights.fill(1.0f);
      break;
    }
  }
  const float sum = std::accumulate(weights.begin(), weights.end(), 0.0f);
  return bSoundFrequencySampler::WindowWeights{std::move(weights), sum};
}

/** Caches the window function weights so that each combination is only computed once. */
static const bSoundFrequencySampler::WindowWeights &get_window_function_weights(
    const bSoundFrequencySampler::WindowFunction window, const int size)
{
  static Mutex mutex;
  static Map<std::pair<bSoundFrequencySampler::WindowFunction, int>,
             std::unique_ptr<bSoundFrequencySampler::WindowWeights>>
      map;
  std::lock_guard lock{mutex};
  return *map.lookup_or_add_cb({window, size}, [&]() {
    return std::make_unique<bSoundFrequencySampler::WindowWeights>(
        compute_window_function_weights(window, size));
  });
}
#endif

#ifdef WITH_AUDASPACE
bSoundFrequencySampler::bSoundFrequencySampler(AUD_Sound sound,
                                               std::shared_ptr<SoundReaderCache> reader_cache,
                                               const Key &key)
    : key_(key),
      reader_cache_(std::move(reader_cache)),
      window_weights_(get_window_function_weights(key.window_function, key.fft_size))
{
  const SoundInfo info = bke::sound_info_get(sound);
  samples_per_second_ = info.specs.samplerate;
  /* This could be a parameter but a single fixed value seems fine for now and makes caching much
   * simpler. */
  window_cache_stride_ = key.fft_size / 8;
  /* The length is negative for sounds of unknown duration. */
  const int window_caches_num = std::max<int>(
      std::ceil(info.length * info.specs.samplerate / window_cache_stride_), 0);
  window_caches_.reinitialize(window_caches_num);
}
#endif

std::optional<Array<float>> bSoundFrequencySampler::compute_fft(const int start_sample) const
{
  /* Since the result of the dft algorithm is symmetric in this case, only the first half is
   * computed. */
  const int frequencies_num = key_.fft_size / 2;

#if defined(WITH_AUDASPACE) && defined(WITH_FFTW3)
  /* Read some extra samples before the ones we are actually interested in here. This is done
   * because the #read function may sometimes give invalid data for the first samples. */
  const int warmup_samples = std::min(2000, start_sample);

  int channels_num;
  Array<float> read_buffer_extra;
  bool is_end_of_stream = false;
  int length = key_.fft_size + warmup_samples;
  {
    SoundReaderLease reader = reader_cache_->acquire();
    if (!reader) {
      return std::nullopt;
    }
    channels_num = reader->getSpecs().channels;
    read_buffer_extra.reinitialize((key_.fft_size + warmup_samples) * channels_num);
    try {
      reader->seek(std::max(start_sample - warmup_samples, 0));
      reader->read(length, is_end_of_stream, read_buffer_extra.data());
    }
    catch (...) {
      reader.discard();
      throw;
    }
  }
  const Span<float> read_buffer = read_buffer_extra
                                      .as_span()
                                      /* Limit to actually read values. */
                                      .take_front(length * channels_num)
                                      /* Drop the warmup samples. */
                                      .drop_front(warmup_samples * channels_num);
  const int read_length = read_buffer.size() / channels_num;

  /* Pull out the samples for the requested channel(s). */
  Array<float> buffer(key_.fft_size, 0.0f);
  if (key_.channel.has_value()) {
    const int channel = *key_.channel;
    if (channel < 0 || channel >= channels_num) {
      return std::nullopt;
    }
    for (const int i : IndexRange(read_length)) {
      buffer[i] = read_buffer[i * channels_num + channel];
    }
  }
  else {
    for (const int i : IndexRange(read_length)) {
      for (const int c : IndexRange(channels_num)) {
        buffer[i] += read_buffer[i * channels_num + c];
      }
      buffer[i] /= channels_num;
    }
  }

  /* Apply window function which avoids spectral leakage (depending on the function). */
  for (const int i : IndexRange(read_length)) {
    buffer[i] *= window_weights_.weights[i];
  }

  /* Set up the fftw plan. */
  fftwf_complex *fftwf_buffer = static_cast<fftwf_complex *>(
      fftwf_malloc(sizeof(fftwf_complex) * (frequencies_num + 1)));
  fftwf_plan plan = fftwf_plan_dft_r2c_1d(
      key_.fft_size, buffer.data(), fftwf_buffer, FFTW_ESTIMATE);
  BLI_SCOPED_DEFER([&]() {
    fftwf_destroy_plan(plan);
    fftwf_free(fftwf_buffer);
  });

  /* Actually perform the fourier transform. */
  fftwf_execute(plan);

  /* Compute the amplitudes of the frequencies. */
  Array<float> frequency_amplitudes(frequencies_num);
  /* The scaling factor is applied so that changing the fft size or window function does not affect
   * the magnitude of the result. */
  const float scaling_factor = 1.0f / window_weights_.weights_sum;
  for (const int i : IndexRange(frequencies_num)) {
    const fftwf_complex &c = fftwf_buffer[i];
    /* Take real and imaginary parts into account which correspond to the sin and cos component of
     * the frequency. */
    frequency_amplitudes[i] = sqrt(pow2f(c[0]) + pow2f(c[1])) * scaling_factor;
  }

  return frequency_amplitudes;
#else
  UNUSED_VARS(start_sample);
  return Array<float>(frequencies_num, 0.0f);
#endif
}

static float catmul_rom_interpolation(
    const float p0, const float p1, const float p2, const float p3, const float t)
{
  const float t2 = t * t;
  const float t3 = t2 * t;

  const float value = 0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 +
                             (-p0 + 3 * p1 - 3 * p2 + p3) * t3);
  return value;
}

static float bspline_interpolation(
    const float p0, const float p1, const float p2, const float p3, const float t)
{
  const float t2 = t * t;
  const float t3 = t2 * t;

  const float b0 = (1.0f - t) * (1.0f - t) * (1.0f - t) / 6.0f;
  const float b1 = (3.0f * t3 - 6.0f * t2 + 4.0f) / 6.0f;
  const float b2 = (-3.0f * t3 + 3.0f * t2 + 3.0f * t + 1.0f) / 6.0f;
  const float b3 = t3 / 6.0f;

  return (b0 * p0 + b1 * p1 + b2 * p2 + b3 * p3);
}

float bSoundFrequencySampler::sample(const float time,
                                     const float low,
                                     const float high,
                                     const InterpolationMethod time_interpolation,
                                     const InterpolationMethod frequency_interpolation) const
{
  if (low >= high) {
    return 0.0f;
  }
  const float i_float = std::max(
      0.0f, (time * samples_per_second_ - key_.fft_size / 2) / window_cache_stride_);
  const int i_pre = floorf(i_float);
  const int i_post = i_pre + 1;
  const float t = fractf(i_float);

  switch (time_interpolation) {
    case InterpolationMethod::Linear: {
      const float v_prev = this->sample_frequency_range_in_window(
          i_pre, low, high, frequency_interpolation);
      const float v_next = this->sample_frequency_range_in_window(
          i_post, low, high, frequency_interpolation);

      return math::interpolate(v_prev, v_next, t);
    }
    case InterpolationMethod::CatmullRom: {
      const int i_pre2 = i_pre - 1;
      const int i_post2 = i_post + 1;

      const float p0 = this->sample_frequency_range_in_window(
          i_pre2, low, high, frequency_interpolation);
      const float p1 = this->sample_frequency_range_in_window(
          i_pre, low, high, frequency_interpolation);
      const float p2 = this->sample_frequency_range_in_window(
          i_post, low, high, frequency_interpolation);
      const float p3 = this->sample_frequency_range_in_window(
          i_post2, low, high, frequency_interpolation);

      return catmul_rom_interpolation(p0, p1, p2, p3, t);
    }
    default:
    case InterpolationMethod::BSpline: {
      const int i_pre2 = i_pre - 1;
      const int i_post2 = i_post + 1;

      const float p0 = this->sample_frequency_range_in_window(
          i_pre2, low, high, frequency_interpolation);
      const float p1 = this->sample_frequency_range_in_window(
          i_pre, low, high, frequency_interpolation);
      const float p2 = this->sample_frequency_range_in_window(
          i_post, low, high, frequency_interpolation);
      const float p3 = this->sample_frequency_range_in_window(
          i_post2, low, high, frequency_interpolation);

      return bspline_interpolation(p0, p1, p2, p3, t);
    }
  }
}

float bSoundFrequencySampler::sample_frequency_range_in_window(int window_i,
                                                               float low,
                                                               float high,
                                                               InterpolationMethod method) const
{
  const std::optional<Span<float>> window_opt = this->ensure_window_cache(window_i);
  if (!window_opt.has_value()) {
    return 0.0f;
  }
  const Span<float> window_values = *window_opt;
  const float cumulative_low = this->sample_cumulative_frequency(window_values, low, method);
  const float cumulative_high = this->sample_cumulative_frequency(window_values, high, method);
  return cumulative_high - cumulative_low;
}

float bSoundFrequencySampler::sample_cumulative_frequency(const Span<float> window_values,
                                                          const float frequency,
                                                          const InterpolationMethod method) const
{
  const int max_i = window_values.size() - 1;
  const float i_float = frequency * key_.fft_size / samples_per_second_;
  const int i_pre = std::clamp<int>(std::floor(i_float), 0, max_i);
  const int i_post = std::min(i_pre + 1, max_i);
  const float t = fractf(i_float);

  switch (method) {
    case InterpolationMethod::Linear: {
      return math::interpolate(window_values[i_pre], window_values[i_post], t);
    }
    case InterpolationMethod::CatmullRom: {
      const int i_pre2 = std::max(i_pre - 1, 0);
      const int i_post2 = std::min(i_post + 1, max_i);

      const float p0 = window_values[i_pre2];
      const float p1 = window_values[i_pre];
      const float p2 = window_values[i_post];
      const float p3 = window_values[i_post2];

      return catmul_rom_interpolation(p0, p1, p2, p3, t);
    }
    case InterpolationMethod::BSpline: {
      const int i_pre2 = std::max(i_pre - 1, 0);
      const int i_post2 = std::min(i_post + 1, max_i);

      const float p0 = window_values[i_pre2];
      const float p1 = window_values[i_pre];
      const float p2 = window_values[i_post];
      const float p3 = window_values[i_post2];

      return bspline_interpolation(p0, p1, p2, p3, t);
    }
  }
  return 0.0f;
}

std::optional<Span<float>> bSoundFrequencySampler::ensure_window_cache(int window_i) const
{
  if (window_caches_.is_empty()) {
    return std::nullopt;
  }

  /* Clamp window so that too low or high indices get mapped to the closest valid index. */
  window_i = std::clamp<int>(window_i, 0, window_caches_.size() - 1);

  const WindowCache &window = window_caches_[window_i];
  /* Compute the FFT of that window if that wasn't done already. */
  window.mutex.ensure([&]() {
    std::optional<Array<float>> fft_array = this->compute_fft(window_i * window_cache_stride_);
    if (!fft_array.has_value()) {
      return;
    }
    /* Compute prefix sum for the fft values. */
    const Span<float> fft_values = fft_array->as_span();
    window.cumulative_amplitudes.emplace(fft_values.size() + 1);
    float sum = 0.0f;
    for (const int i : fft_array->index_range()) {
      const float value = std::abs(fft_values[i]);
      (*window.cumulative_amplitudes)[i] = sum;
      sum += value;
    }
    window.cumulative_amplitudes->last() = sum;
  });
  return window.cumulative_amplitudes;
}

}  // namespace bke

}  // namespace blender
