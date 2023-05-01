/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

#define SOUND_WAVE_SAMPLES_PER_SECOND 250

#if defined(WITH_AUDASPACE)
#  include <AUD_Device.h>
#endif

struct Depsgraph;
struct Main;
struct Sequence;
struct bSound;
struct SoundInfo;

typedef struct SoundWaveform {
  int length;
  float *data;
} SoundWaveform;

void BKE_sound_init_once(void);
void BKE_sound_exit_once(void);

void *BKE_sound_get_device(void);

void BKE_sound_init(struct Main *main);

void BKE_sound_init_main(struct Main *bmain);

void BKE_sound_exit(void);

void BKE_sound_force_device(const char *device);

struct bSound *BKE_sound_new_file(struct Main *main, const char *filepath);
struct bSound *BKE_sound_new_file_exists_ex(struct Main *bmain,
                                            const char *filepath,
                                            bool *r_exists);
struct bSound *BKE_sound_new_file_exists(struct Main *bmain, const char *filepath);

#if 0 /* UNUSED */
struct bSound *BKE_sound_new_buffer(struct Main *bmain, struct bSound *source);

struct bSound *BKE_sound_new_limiter(struct Main *bmain,
                                     struct bSound *source,
                                     float start,
                                     float end);
#endif

void BKE_sound_cache(struct bSound *sound);

void BKE_sound_delete_cache(struct bSound *sound);

void BKE_sound_reset_runtime(struct bSound *sound);
void BKE_sound_load(struct Main *main, struct bSound *sound);
void BKE_sound_ensure_loaded(struct Main *bmain, struct bSound *sound);

/* Matches AUD_Channels. */
typedef enum eSoundChannels {
  SOUND_CHANNELS_INVALID = 0,
  SOUND_CHANNELS_MONO = 1,
  SOUND_CHANNELS_STEREO = 2,
  SOUND_CHANNELS_STEREO_LFE = 3,
  SOUND_CHANNELS_SURROUND4 = 4,
  SOUND_CHANNELS_SURROUND5 = 5,
  SOUND_CHANNELS_SURROUND51 = 6,
  SOUND_CHANNELS_SURROUND61 = 7,
  SOUND_CHANNELS_SURROUND71 = 8,
} eSoundChannels;

typedef struct SoundInfo {
  struct {
    eSoundChannels channels;
    int samplerate;
  } specs;
  float length;
} SoundInfo;

typedef struct SoundStreamInfo {
  double duration;
  double start;
} SoundStreamInfo;

/* Get information about given sound. Returns truth on success., false if sound can not be loaded
 * or if the codes is not supported. */
bool BKE_sound_info_get(struct Main *main, struct bSound *sound, SoundInfo *sound_info);

/* Get information about given sound. Returns truth on success., false if sound can not be loaded
 * or if the codes is not supported. */
bool BKE_sound_stream_info_get(struct Main *main,
                               const char *filepath,
                               int stream,
                               SoundStreamInfo *sound_info);

#if defined(WITH_AUDASPACE)
AUD_Device *BKE_sound_mixdown(const struct Scene *scene,
                              AUD_DeviceSpecs specs,
                              int start,
                              float volume);
#endif

void BKE_sound_reset_scene_runtime(struct Scene *scene);
void BKE_sound_create_scene(struct Scene *scene);
void BKE_sound_ensure_scene(struct Scene *scene);

void BKE_sound_destroy_scene(struct Scene *scene);

void BKE_sound_lock(void);
void BKE_sound_unlock(void);

void BKE_sound_reset_scene_specs(struct Scene *scene);

void BKE_sound_mute_scene(struct Scene *scene, int muted);

void BKE_sound_update_fps(struct Main *bmain, struct Scene *scene);

void BKE_sound_update_scene_listener(struct Scene *scene);

void *BKE_sound_scene_add_scene_sound(
    struct Scene *scene, struct Sequence *sequence, int startframe, int endframe, int frameskip);
void *BKE_sound_scene_add_scene_sound_defaults(struct Scene *scene, struct Sequence *sequence);

void *BKE_sound_add_scene_sound(
    struct Scene *scene, struct Sequence *sequence, int startframe, int endframe, int frameskip);
void *BKE_sound_add_scene_sound_defaults(struct Scene *scene, struct Sequence *sequence);

void BKE_sound_remove_scene_sound(struct Scene *scene, void *handle);

void BKE_sound_mute_scene_sound(void *handle, bool mute);

void BKE_sound_move_scene_sound(const struct Scene *scene,
                                void *handle,
                                int startframe,
                                int endframe,
                                int frameskip,
                                double audio_offset);
void BKE_sound_move_scene_sound_defaults(struct Scene *scene, struct Sequence *sequence);

void BKE_sound_update_scene_sound(void *handle, struct bSound *sound);

void BKE_sound_set_cfra(int cfra);

void BKE_sound_set_scene_volume(struct Scene *scene, float volume);

void BKE_sound_set_scene_sound_volume(void *handle, float volume, char animated);

void BKE_sound_set_scene_sound_pitch(void *handle, float pitch, char animated);

void BKE_sound_set_scene_sound_pitch_constant_range(void *handle,
                                                    int frame_start,
                                                    int frame_end,
                                                    float pitch);

void BKE_sound_set_scene_sound_pan(void *handle, float pan, char animated);

void BKE_sound_update_sequencer(struct Main *main, struct bSound *sound);

void BKE_sound_play_scene(struct Scene *scene);

void BKE_sound_stop_scene(struct Scene *scene);

void BKE_sound_seek_scene(struct Main *bmain, struct Scene *scene);

double BKE_sound_sync_scene(struct Scene *scene);

int BKE_sound_scene_playing(struct Scene *scene);

void BKE_sound_free_waveform(struct bSound *sound);

void BKE_sound_read_waveform(struct Main *bmain, struct bSound *sound, bool *stop);

void BKE_sound_update_scene(struct Depsgraph *depsgraph, struct Scene *scene);

void *BKE_sound_get_factory(void *sound);

float BKE_sound_get_length(struct Main *bmain, struct bSound *sound);

char **BKE_sound_get_device_names(void);

typedef void (*SoundJackSyncCallback)(struct Main *bmain, int mode, double time);

void BKE_sound_jack_sync_callback_set(SoundJackSyncCallback callback);
void BKE_sound_jack_scene_update(struct Scene *scene, int mode, double time);

/* Dependency graph evaluation. */

struct Depsgraph;

void BKE_sound_evaluate(struct Depsgraph *depsgraph, struct Main *bmain, struct bSound *sound);

#ifdef __cplusplus
}
#endif
