/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_SOUND_H__
#define __BKE_SOUND_H__

/** \file BKE_sound.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */

#define SOUND_WAVE_SAMPLES_PER_SECOND 250

struct PackedFile;
struct bSound;
struct ListBase;
struct Main;
struct Sequence;

typedef struct SoundWaveform {
	int length;
	float *data;
} SoundWaveform;

void sound_init_once(void);
void sound_exit_once(void);

void sound_init(struct Main *main);

void sound_init_main(struct Main *bmain);

void sound_exit(void);

void sound_force_device(int device);
int sound_define_from_str(const char *str);

struct bSound *sound_new_file(struct Main *main, const char *filename);

// XXX unused currently
#if 0
struct bSound *sound_new_buffer(struct Main *bmain, struct bSound *source);

struct bSound *sound_new_limiter(struct Main *bmain, struct bSound *source, float start, float end);
#endif

void sound_delete(struct Main *bmain, struct bSound *sound);

void sound_cache(struct bSound *sound);

void sound_cache_notifying(struct Main *main, struct bSound *sound);

void sound_delete_cache(struct bSound *sound);

void sound_load(struct Main *main, struct bSound *sound);

void BKE_sound_free(struct bSound *sound);

#ifdef __AUD_C_API_H__
AUD_Device *sound_mixdown(struct Scene *scene, AUD_DeviceSpecs specs, int start, float volume);
#endif

void sound_create_scene(struct Scene *scene);

void sound_destroy_scene(struct Scene *scene);

void sound_mute_scene(struct Scene *scene, int muted);

void sound_update_fps(struct Scene *scene);

void sound_update_scene_listener(struct Scene *scene);

void *sound_scene_add_scene_sound(struct Scene *scene, struct Sequence *sequence, int startframe, int endframe, int frameskip);
void *sound_scene_add_scene_sound_defaults(struct Scene *scene, struct Sequence *sequence);

void *sound_add_scene_sound(struct Scene *scene, struct Sequence *sequence, int startframe, int endframe, int frameskip);
void *sound_add_scene_sound_defaults(struct Scene *scene, struct Sequence *sequence);

void sound_remove_scene_sound(struct Scene *scene, void *handle);

void sound_mute_scene_sound(void *handle, char mute);

void sound_move_scene_sound(struct Scene *scene, void *handle, int startframe, int endframe, int frameskip);
void sound_move_scene_sound_defaults(struct Scene *scene, struct Sequence *sequence);

void sound_update_scene_sound(void *handle, struct bSound *sound);

void sound_set_cfra(int cfra);

void sound_set_scene_volume(struct Scene *scene, float volume);

void sound_set_scene_sound_volume(void *handle, float volume, char animated);

void sound_set_scene_sound_pitch(void *handle, float pitch, char animated);

void sound_set_scene_sound_pan(void *handle, float pan, char animated);

void sound_update_sequencer(struct Main *main, struct bSound *sound);

void sound_play_scene(struct Scene *scene);

void sound_stop_scene(struct Scene *scene);

void sound_seek_scene(struct Main *bmain, struct Scene *scene);

float sound_sync_scene(struct Scene *scene);

int sound_scene_playing(struct Scene *scene);

void sound_free_waveform(struct bSound *sound);

void sound_read_waveform(struct bSound *sound);

void sound_update_scene(struct Main *bmain, struct Scene *scene);

void *sound_get_factory(void *sound);

float sound_get_length(struct bSound *sound);

int sound_is_jack_supported(void);

#endif
