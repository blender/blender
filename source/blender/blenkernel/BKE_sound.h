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
#ifndef __BKE_SOUND_H__
#define __BKE_SOUND_H__

/** \file
 * \ingroup bke
 */

#define SOUND_WAVE_SAMPLES_PER_SECOND 250

#if defined(WITH_AUDASPACE)
#  include <AUD_Device.h>
#endif

struct Main;
struct Sequence;
struct bSound;

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

// XXX unused currently
#if 0
struct bSound *BKE_sound_new_buffer(struct Main *bmain, struct bSound *source);

struct bSound *BKE_sound_new_limiter(struct Main *bmain,
                                     struct bSound *source,
                                     float start,
                                     float end);
#endif

void BKE_sound_cache(struct bSound *sound);

void BKE_sound_delete_cache(struct bSound *sound);

void BKE_sound_reset_pointers(struct bSound *sound);
void BKE_sound_load(struct Main *main, struct bSound *sound);
void BKE_sound_ensure_loaded(struct Main *bmain, struct bSound *sound);

void BKE_sound_free(struct bSound *sound);

void BKE_sound_copy_data(struct Main *bmain,
                         struct bSound *sound_dst,
                         const struct bSound *sound_src,
                         const int flag);

void BKE_sound_make_local(struct Main *bmain, struct bSound *sound, const bool lib_local);

#if defined(WITH_AUDASPACE)
AUD_Device *BKE_sound_mixdown(struct Scene *scene, AUD_DeviceSpecs specs, int start, float volume);
#endif

void BKE_sound_reset_scene_pointers(struct Scene *scene);
void BKE_sound_create_scene(struct Scene *scene);
void BKE_sound_ensure_scene(struct Scene *scene);

void BKE_sound_destroy_scene(struct Scene *scene);

void BKE_sound_reset_scene_specs(struct Scene *scene);

void BKE_sound_mute_scene(struct Scene *scene, int muted);

void BKE_sound_update_fps(struct Scene *scene);

void BKE_sound_update_scene_listener(struct Scene *scene);

void *BKE_sound_scene_add_scene_sound(
    struct Scene *scene, struct Sequence *sequence, int startframe, int endframe, int frameskip);
void *BKE_sound_scene_add_scene_sound_defaults(struct Scene *scene, struct Sequence *sequence);

void *BKE_sound_add_scene_sound(
    struct Scene *scene, struct Sequence *sequence, int startframe, int endframe, int frameskip);
void *BKE_sound_add_scene_sound_defaults(struct Scene *scene, struct Sequence *sequence);

void BKE_sound_remove_scene_sound(struct Scene *scene, void *handle);

void BKE_sound_mute_scene_sound(void *handle, char mute);

void BKE_sound_move_scene_sound(
    struct Scene *scene, void *handle, int startframe, int endframe, int frameskip);
void BKE_sound_move_scene_sound_defaults(struct Scene *scene, struct Sequence *sequence);

void BKE_sound_update_scene_sound(void *handle, struct bSound *sound);

void BKE_sound_set_cfra(int cfra);

void BKE_sound_set_scene_volume(struct Scene *scene, float volume);

void BKE_sound_set_scene_sound_volume(void *handle, float volume, char animated);

void BKE_sound_set_scene_sound_pitch(void *handle, float pitch, char animated);

void BKE_sound_set_scene_sound_pan(void *handle, float pan, char animated);

void BKE_sound_update_sequencer(struct Main *main, struct bSound *sound);

void BKE_sound_play_scene(struct Scene *scene);

void BKE_sound_stop_scene(struct Scene *scene);

void BKE_sound_seek_scene(struct Main *bmain, struct Scene *scene);

float BKE_sound_sync_scene(struct Scene *scene);

int BKE_sound_scene_playing(struct Scene *scene);

void BKE_sound_free_waveform(struct bSound *sound);

void BKE_sound_read_waveform(struct bSound *sound, short *stop);

void BKE_sound_update_scene(struct Main *bmain, struct Scene *scene);

void *BKE_sound_get_factory(void *sound);

float BKE_sound_get_length(struct bSound *sound);

char **BKE_sound_get_device_names(void);

#endif /* __BKE_SOUND_H__ */
