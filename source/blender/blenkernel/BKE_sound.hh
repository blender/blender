/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include <string>

#include "BKE_sound_types.hh"

#include "BLI_vector.hh"

#if defined(WITH_AUDASPACE)
#  include <file/IWriter.h>
#endif

#define SOUND_WAVE_SAMPLES_PER_SECOND 250

namespace blender {

struct Depsgraph;
struct Main;
struct ReportList;
struct Scene;
struct Strip;
struct bSound;

void BKE_sound_init_once();
void BKE_sound_exit_once();

void BKE_sound_init(Main *bmain);

void BKE_sound_refresh_callback_bmain(Main *bmain);

void BKE_sound_force_device(const char *device);

bSound *BKE_sound_new_file(Main *bmain, const char *filepath);
bSound *BKE_sound_new_file_exists(Main *bmain, const char *filepath);

void BKE_sound_load(Main *bmain, bSound *sound);

/**
 * High-level pack function.
 *
 * Packs sound data from its filepath.
 * Does nothing if sound is already packed.
 */
void BKE_sound_packfile_ensure(Main *bmain, bSound *sound, ReportList *reports);

/** Matches AUD_Channels. */
enum eSoundChannels {
  SOUND_CHANNELS_INVALID = 0,
  SOUND_CHANNELS_MONO = 1,
  SOUND_CHANNELS_STEREO = 2,
  SOUND_CHANNELS_STEREO_LFE = 3,
  SOUND_CHANNELS_SURROUND4 = 4,
  SOUND_CHANNELS_SURROUND5 = 5,
  SOUND_CHANNELS_SURROUND51 = 6,
  SOUND_CHANNELS_SURROUND61 = 7,
  SOUND_CHANNELS_SURROUND71 = 8,
};

struct SoundInfo {
  struct {
    eSoundChannels channels;
    int samplerate;
  } specs;
  float length;
};

struct SoundStreamInfo {
  double duration;
  double start;
};

/**
 * Get information about given sound.
 *
 * \return true on success, false if sound can not be loaded or if the codes is not supported.
 */
bool BKE_sound_info_get(Main *main, bSound *sound, SoundInfo *sound_info);

/**
 * Get information about given sound.
 *
 * \return on success, false if sound can not be loaded or if the codes is not supported.
 */
bool BKE_sound_stream_info_get(Main *main,
                               const char *filepath,
                               int stream,
                               SoundStreamInfo *sound_info);

#if defined(WITH_AUDASPACE)
AUD_Device BKE_sound_mixdown(const Scene *scene,
                             const aud::DeviceSpecs &specs,
                             int start,
                             float volume);
#endif

void BKE_sound_create_scene(Scene *scene);
void BKE_sound_ensure_scene(Scene *scene);

void BKE_sound_destroy_scene(Scene *scene);

void BKE_sound_lock();
void BKE_sound_unlock();

void BKE_sound_reset_scene_specs(Scene *scene);

void BKE_sound_mute_scene(Scene *scene, bool muted);

void BKE_sound_update_fps(Main *bmain, Scene *scene);

void BKE_sound_update_scene_listener(Scene *scene);

AUD_SequenceEntry BKE_sound_scene_add_scene_sound(Scene *scene, Strip *strip);
AUD_SequenceEntry BKE_sound_add_scene_sound(Scene *scene, Strip *strip);

void BKE_sound_remove_scene_sound(Scene *scene, AUD_SequenceEntry handle);

void BKE_sound_mute_scene_sound(AUD_SequenceEntry handle, bool mute);

void BKE_sound_move_scene_sound(const Scene *scene,
                                AUD_SequenceEntry handle,
                                int startframe,
                                int endframe,
                                int frameskip,
                                double audio_offset);
void BKE_sound_move_scene_sound_defaults(Scene *scene, Strip *strip);

/** Join the Sequence with the structure in Audaspace, the second parameter is a #bSound. */
void BKE_sound_update_scene_sound(AUD_SequenceEntry handle, bSound *sound);

/**
 * Join the Sequence with the structure in Audaspace,
 *
 * \param sound_handle: the `AUD_Sound` created in Audaspace previously.
 */
void BKE_sound_update_sequence_handle(AUD_SequenceEntry handle, AUD_Sound sound_handle);

void BKE_sound_set_scene_volume(Scene *scene, float volume);

void BKE_sound_set_scene_sound_volume_at_frame(AUD_SequenceEntry handle,
                                               int frame,
                                               float volume,
                                               bool animated);

void BKE_sound_set_scene_sound_pitch_at_frame(AUD_SequenceEntry handle,
                                              int frame,
                                              float pitch,
                                              bool animated);

void BKE_sound_set_scene_sound_pitch_constant_range(AUD_SequenceEntry handle,
                                                    int frame_start,
                                                    int frame_end,
                                                    float pitch);

void BKE_sound_set_scene_sound_time_stretch_at_frame(AUD_Sound handle,
                                                     int frame,
                                                     float time_stretch,
                                                     bool animated);

void BKE_sound_set_scene_sound_time_stretch_constant_range(AUD_Sound handle,
                                                           int frame_start,
                                                           int frame_end,
                                                           float time_stretch);

void BKE_sound_set_scene_sound_pan_at_frame(AUD_SequenceEntry handle,
                                            int frame,
                                            float pan,
                                            bool animated);

void BKE_sound_update_sequencer(Main *main, bSound *sound);

void BKE_sound_play_scene(Scene *scene);

void BKE_sound_stop_scene(Scene *scene);

void BKE_sound_seek_scene(Main *bmain, Scene *scene);

double BKE_sound_sync_scene(Scene *scene);

void BKE_sound_read_waveform(Main *bmain, bSound *sound, bool *stop);

void BKE_sound_update_scene(Depsgraph *depsgraph, Scene *scene);

AUD_Sound BKE_sound_get_factory(void *sound);

float BKE_sound_get_length(Main *bmain, bSound *sound);

char **BKE_sound_get_device_names();

typedef void (*SoundJackSyncCallback)(Main *bmain, int mode, double time);

void BKE_sound_jack_sync_callback_set(SoundJackSyncCallback callback);
void BKE_sound_jack_scene_update(Scene *scene, int mode, double time);

void BKE_sound_evaluate(Depsgraph *depsgraph, Main *bmain, bSound *sound);

AUD_Sound BKE_sound_ensure_time_stretch_effect(const Strip *strip, float fps);

void BKE_sound_runtime_state_get_and_clear(const bSound *sound,
                                           AUD_Sound *r_cache,
                                           AUD_Sound *r_playback_handle,
                                           Vector<float> **r_waveform);
void BKE_sound_runtime_state_set(const bSound *sound,
                                 AUD_Sound cache,
                                 AUD_Sound playback_handle,
                                 Vector<float> *waveform);

AUD_Sound BKE_sound_playback_handle_get(const bSound *sound);

void BKE_sound_runtime_clear_waveform_loading_tag(bSound *sound);
bool BKE_sound_runtime_start_waveform_loading(bSound *sound);
const Vector<float> *BKE_sound_runtime_get_waveform(const bSound *sound);

#if defined(WITH_AUDASPACE)
namespace bke {
void sound_system_initialize();
AUD_Device sound_device_init(const char *device,
                             const aud::DeviceSpecs &specs,
                             int buffersize,
                             const char *name);
void sound_device_exit();
AUD_Handle sound_device_play(AUD_Device device, AUD_Sound sound);
bool sound_device_read(AUD_Device device, unsigned char *buffer, int length);
AUD_Handle sound_pause_after(AUD_Handle handle, double seconds);
SoundInfo sound_info_get(AUD_Sound sound);
float *sound_read_file_buffer(const char *filename,
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
                              int *length);

bool sound_mixdown(AUD_Sequence sequence,
                   unsigned int start,
                   unsigned int length,
                   unsigned int buffersize,
                   const char *filename,
                   const aud::DeviceSpecs &specs,
                   aud::Container format,
                   aud::Codec codec,
                   unsigned int bitrate,
                   bool split_channels,
                   std::string &r_error);

}  // namespace bke
#endif  // #if defined(WITH_AUDASPACE)

}  // namespace blender
