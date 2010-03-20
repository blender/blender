/**
 * sound.h (mar-2001 nzc)
 *
 * $Id$
 *
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
#ifndef BKE_SOUND_H
#define BKE_SOUND_H

struct PackedFile;
struct bSound;
struct bContext;
struct ListBase;
struct Main;
struct Sequence;

void sound_init(struct Main *main);

void sound_exit();

void sound_force_device(int device);
int sound_define_from_str(char *str);

struct bSound* sound_new_file(struct Main *main, char* filename);

// XXX unused currently
#if 0
struct bSound* sound_new_buffer(struct bContext *C, struct bSound *source);

struct bSound* sound_new_limiter(struct bContext *C, struct bSound *source, float start, float end);
#endif

void sound_delete(struct bContext *C, struct bSound* sound);

void sound_cache(struct bSound* sound, int ignore);

void sound_delete_cache(struct bSound* sound);

void sound_load(struct Main *main, struct bSound* sound);

void sound_free(struct bSound* sound);

#ifdef AUD_CAPI
AUD_Device* sound_mixdown(struct Scene *scene, AUD_DeviceSpecs specs, int start, float volume);
#endif

void sound_create_scene(struct Scene *scene);

void sound_destroy_scene(struct Scene *scene);

void* sound_scene_add_scene_sound(struct Scene *scene, struct Sequence* sequence, int startframe, int endframe, int frameskip);

void* sound_add_scene_sound(struct Scene *scene, struct Sequence* sequence, int startframe, int endframe, int frameskip);

void sound_remove_scene_sound(struct Scene *scene, void* handle);

void sound_mute_scene_sound(struct Scene *scene, void* handle, char mute);

void sound_move_scene_sound(struct Scene *scene, void* handle, int startframe, int endframe, int frameskip);

void sound_play_scene(struct Scene *scene);

void sound_stop_scene(struct Scene *scene);

void sound_seek_scene(struct bContext *C);

float sound_sync_scene(struct Scene *scene);

int sound_scene_playing(struct Scene *scene);

int sound_read_sound_buffer(struct bSound* sound, float* buffer, int length);

#endif
