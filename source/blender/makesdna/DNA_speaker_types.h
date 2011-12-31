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
 * Contributor(s): Jörg Müller.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_speaker_types.h
 *  \ingroup DNA
 */

#ifndef DNA_SPEAKER_TYPES_H
#define DNA_SPEAKER_TYPES_H

#include "DNA_ID.h"

struct AnimData;
struct bSound;

typedef struct Speaker {
	ID id;
	struct AnimData *adt;	/* animation data (must be immediately after id for utilities to use it) */ 

	struct bSound *sound;

	// not animatable properties
	float volume_max;
	float volume_min;
	float distance_max;
	float distance_reference;
	float attenuation;
	float cone_angle_outer;
	float cone_angle_inner;
	float cone_volume_outer;

	// animatable properties
	float volume;
	float pitch;

	// flag
	short flag;
	short pad1[3];
} Speaker;

/* **************** SPEAKER ********************* */

/* flag */
#define SPK_DS_EXPAND   (1<<0)
#define SPK_MUTED       (1<<1)
#define SPK_RELATIVE    (1<<2)

#endif /* DNA_SPEAKER_TYPES_H */

