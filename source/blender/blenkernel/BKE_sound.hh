/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 * SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include <mutex>
#include <string>

#include "BKE_sound_types.hh"

#include "BLI_vector.hh"

#if defined(WITH_AUDASPACE)
#  include <file/IWriter.h>
#endif

#define SOUND_WAVE_SAMPLES_PER_SECOND 250

/**
 * Reading the audio of a #Scene at a frame also reads animated audio properties of later frames:
 * they are interpolated with a cubic spline that uses the next two frames, and 3D velocities are
 * computed from the property one frame later.
 */
#define SOUND_ANIMATION_FRAMES_AHEAD 3

namespace blender {

struct Depsgraph;
struct Main;
struct ReportList;
struct Scene;
struct Strip;
struct bSound;
namespace bke {
struct SoundAnimationUse;
struct SoundRecording;
}  // namespace bke

void BKE_sound_init_once();
void BKE_sound_exit_once();

void BKE_sound_init(Main *bmain);

void BKE_sound_refresh_callback_bmain(Main *bmain);

void BKE_sound_force_device(const char *device);

/**
 * Looks up an existing sound by `(filepath, stream_index)`.
 * Returns it if found, otherwise, creates a new one.
 */
bSound *BKE_sound_new_file_exists(Main *bmain, const char *filepath, short stream_index = 0);
bSound *BKE_sound_new_file(Main *bmain, const char *filepath, short stream_index = 0);

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

/**
 * Returns the number of audio streams in the given file.
 * Returns 0 if the file cannot be opened.
 */
int BKE_sound_stream_count(Main *main, const char *filepath);

#if defined(WITH_AUDASPACE)
AUD_Device BKE_sound_mixdown(const Scene *scene,
                             const aud::DeviceSpecs &specs,
                             int start,
                             float volume);
/**
 * Open the mixdown of the audio of a recording (see #BKE_sound_recording_ensure) for a reader
 * that starts at \a sample. The movie files of the views of a render write the same audio, their
 * readers share the mixdown.
 */
void BKE_sound_recording_mixdown_open(bke::SoundRecording &recording,
                                      const Scene *scene,
                                      const aud::DeviceSpecs &specs,
                                      int start,
                                      float volume,
                                      int64_t sample);
/**
 * Read the mixed audio that starts at \a sample, with the state recorded up to \a frame. Readers
 * hold the lock of #BKE_sound_recording_lock. Frames must not decrease.
 */
void BKE_sound_recording_mixdown_read(
    bke::SoundRecording &recording, int64_t sample, int frame, unsigned char *buffer, int length);
/** Close the mixdown for a reader that read the audio before \a sample. */
void BKE_sound_recording_mixdown_close(bke::SoundRecording &recording, int64_t sample);
#endif

void BKE_sound_create_scene(Scene *scene);
void BKE_sound_ensure_scene(Scene *scene);

/** Mark the evaluated #Scene's audio animation cache dirty. */
void BKE_sound_invalidate_scene_audio_animation_cache(Scene *scene);

/**
 * Explicitly generate framed audio automation with full dependency-graph evaluation. Scenes used
 * by Scene strips get the directly evaluable automation of the frames that the strips play. They
 * are evaluated at the frames of the graph's Scene, their other animated values (driven, and all
 * while frame-change handlers are registered) get the value evaluated at the frame of the graph's
 * Scene during which the strips play a frame, like in renders. These values are stored for each
 * Scene strip, strips can play the same frames during different frames of the graph's Scene.
 * Returns false without changes while a render records the audio of these Scenes or a Mixdown
 * reads it (see #BKE_sound_audio_animation_use), since they read the animation that is replaced.
 */
bool BKE_sound_bake_animation(
    Main *bmain, Scene *scene_orig, Depsgraph *depsgraph, int frame_start, int frame_end);

/**
 * Prepare directly evaluable framed audio automation for playback of the #Scene. Dependency-driven
 * values and frame-change handlers remain live and are not evaluated at future frames. Only a
 * limited number of frames ahead of the current frame is prepared, with a bounded amount of work
 * per call. Called for every playback step, audio of frames that are not prepared yet uses the
 * current values. Compatible Explicit caches remain usable. Caches that a render records into or
 * that readers of #BKE_sound_audio_animation_use hold are not prepared again.
 */
void BKE_sound_ensure_playback_audio_animation(Main *bmain,
                                               Scene *scene_orig,
                                               Depsgraph *depsgraph);

/**
 * Discard the audio animation caches of the evaluated #Scene and the Scenes used by its Scene
 * strips that are outdated, so that audio heard without a playback step, like when scrubbing, uses
 * the current values. Audio devices hear either the outdated or the current values. Forward
 * playback prepares them again. Renders that record into them and readers of
 * #BKE_sound_audio_animation_use keep them, they are only marked as outdated.
 */
void BKE_sound_discard_outdated_audio_animation(Main *bmain, Depsgraph *depsgraph);

/**
 * Keep the audio animation of the evaluated #Scene and the Scenes used by its Scene strips while
 * the returned value is held, for a reader that doesn't lock the audio device, like a Mixdown
 * written in the background. Changes still mark the caches as outdated, they are discarded or
 * prepared again once no reader holds them anymore.
 */
std::shared_ptr<bke::SoundAnimationUse> BKE_sound_audio_animation_use(const Scene *scene);

/**
 * Frame at which evaluated audio properties of the #Scene are written, or -1 if they must not be
 * written as framed values: Scenes used by Scene strips are not evaluated at their own frames, and
 * values between frames are not the values of a frame, which must not be written over an audio
 * animation cache. Properties without framed values get the current value instead.
 */
int BKE_sound_scene_audio_frame(const Depsgraph *depsgraph, const Scene *scene);

/**
 * Record the current audio properties of the graph's evaluated #Scene as values of its current
 * frame, so that audio of that frame can be read after later frames were evaluated. Also prepares
 * the automation of Scenes used by Scene strips during the frame, and records their other animated
 * values for each Scene strip, for the frames that it plays during the frame, see
 * #BKE_sound_bake_animation and #BKE_sound_recording_references_recorded.
 *
 * Other audio state that evaluating the frame changed, such as muting, time ranges or sounds of
 * the Sequence entries, is recorded for the frame in the recording of
 * #BKE_sound_recording_ensure. Values of frames that the mixdown of the recording doesn't read
 * anymore are discarded, unless the Scene has an Explicit cache.
 */
void BKE_sound_record_scene_audio_animation(Depsgraph *depsgraph);
/**
 * Recording of the audio state of the evaluated #Scene and the Scenes used by its Scene strips.
 * While it exists, evaluation doesn't change the state that is read when mixing, apart from
 * values of single frames. Its audio is read with #BKE_sound_recording_mixdown_read, which lags
 * behind the evaluated frame and uses the state recorded for the frames that it reads.
 */
std::shared_ptr<bke::SoundRecording> BKE_sound_recording_ensure(const Scene *scene);
/**
 * Readers on other threads hold the lock while reading, the evaluating thread while evaluating
 * and recording a frame, which writes framed values.
 */
std::unique_lock<std::mutex> BKE_sound_recording_lock(bke::SoundRecording &recording);
/**
 * Whether reading the audio of a recording before \a time (in seconds) only reads values that
 * were recorded for Scenes used by Scene strips, see #BKE_sound_record_scene_audio_animation.
 * Reading a frame of such a Scene also reads its next frames, which can start many frames of the
 * recorded Scene later. Readers hold the lock of #BKE_sound_recording_lock.
 */
bool BKE_sound_recording_references_recorded(const bke::SoundRecording &recording, double time);
/**
 * Apply all recorded state and discard the values recorded by
 * #BKE_sound_record_scene_audio_animation.
 */
void BKE_sound_end_scene_audio_recording(Depsgraph *depsgraph);

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

void BKE_sound_mute_scene_sound(const Scene *scene, AUD_SequenceEntry handle, bool mute);

void BKE_sound_move_scene_sound(const Scene *scene,
                                AUD_SequenceEntry handle,
                                int startframe,
                                int endframe,
                                int frameskip,
                                double audio_offset);
void BKE_sound_move_scene_sound_defaults(Scene *scene, Strip *strip);

/** Join the Sequence with the structure in Audaspace, the second parameter is a #bSound. */
void BKE_sound_update_scene_sound(const Scene *scene, AUD_SequenceEntry handle, bSound *sound);

/**
 * Join the Sequence with the structure in Audaspace,
 *
 * \param sound_handle: the `AUD_Sound` created in Audaspace previously.
 */
void BKE_sound_update_sequence_handle(const Scene *scene,
                                      AUD_SequenceEntry handle,
                                      AUD_Sound sound_handle);

void BKE_sound_set_scene_volume(Scene *scene, float volume);
void BKE_sound_set_scene_volume_at_frame(Scene *scene, int frame, float volume);

void BKE_sound_set_scene_sound_volume_at_frame(AUD_SequenceEntry handle,
                                               int frame,
                                               float volume,
                                               bool animated);

void BKE_sound_set_scene_sound_pitch_at_frame(const Scene *scene,
                                              AUD_SequenceEntry handle,
                                              int frame,
                                              float pitch);

void BKE_sound_set_scene_sound_pitch_constant_range(
    const Scene *scene, AUD_SequenceEntry handle, int frame_start, int frame_end, float pitch);

void BKE_sound_set_scene_sound_time_stretch_at_frame(const Scene *scene,
                                                     AUD_Sound handle,
                                                     int frame,
                                                     float time_stretch);

void BKE_sound_set_scene_sound_time_stretch_constant_range(
    const Scene *scene, AUD_Sound handle, int frame_start, int frame_end, float time_stretch);

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
                   std::string &r_error,
                   bool (*progress_callback)(float, void *),
                   void *progress_data);

}  // namespace bke
#endif  // #if defined(WITH_AUDASPACE)

}  // namespace blender
