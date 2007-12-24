/**
 * $Id: BIF_editsound.h 12320 2007-10-21 15:42:08Z schlaile $
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef BIF_EDITSOUND_H
#define BIF_EDITSOUND_H

struct bSound;
struct bSample;
struct ListBase;
struct PackedFile;
struct hdaudio;

void sound_init_audio(void);
void sound_initialize_sounds(void);
void sound_exit_audio(void);
int sound_get_mixrate(void);

void* sound_get_audiodevice(void);
void* sound_get_listener(void);

int sound_set_sample(struct bSound* sound, struct bSample* sample);
int sound_sample_is_null(struct bSound* sound);
int sound_load_sample(struct bSound* sound);

struct bSample* sound_find_sample(struct bSound* sound);
struct bSample* sound_new_sample(struct bSound* sound);

struct bSound* sound_new_sound(char *name);
struct bSound* sound_make_copy(struct bSound* originalsound);
void sound_end_all_sounds(void);

void sound_initialize_sample(struct bSound * sound);
void sound_load_samples(void);

void sound_play_sound(struct bSound *sound);
void sound_stop_all_sounds(void);

void sound_set_position(void *object,
			struct bSound *sound,
			float obmatrix[4][4]);

struct hdaudio * sound_open_hdaudio(char * name);
struct hdaudio * sound_copy_hdaudio(struct hdaudio * c);

long sound_hdaudio_get_duration(struct hdaudio * hdaudio, double frame_rate);
void sound_hdaudio_extract(struct hdaudio * hdaudio, 
			   short * target_buffer,
			   int sample_position /* units of target_rate */,
			   int target_rate,
			   int target_channels,
			   int nb_samples /* in target */);
			   
void sound_close_hdaudio(struct hdaudio * hdaudio);



#endif

