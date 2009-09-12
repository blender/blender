/**
 * blenlib/DNA_sound_types.h (mar-2001 nzc)
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#ifndef DNA_SOUND_TYPES_H
#define DNA_SOUND_TYPES_H

#include "DNA_listBase.h"
#include "DNA_ID.h"

/* stupid... could easily be solved */
#include "DNA_view2d_types.h"

struct Ipo;
struct PackedFile;
struct SpaceLink;

// runtime only - no saving
typedef struct SoundHandle {
	struct SoundHandle *next, *prev;
	struct bSound *source;
	void *handle;
	int state;
	int startframe;
	int endframe;
	int frameskip;
	int mute;
	int changed;
	float volume;
	float pad;
} SoundHandle;

typedef struct Sound3D
{
	float min_gain;
	float max_gain;
	float reference_distance;
	float max_distance;
	float rolloff_factor;
	float cone_inner_angle;
	float cone_outer_angle;
	float cone_outer_gain;
} Sound3D;

typedef struct bSound {
	ID id;

	/**
	 * The path to the sound file.
	 */
	char name[160];

	/**
	 * The packed file.
	 */
	struct PackedFile *packedfile;

	/**
	 * The handle for audaspace.
	 */
	void *handle;

	/**
	 * Deprecated; used for loading pre 2.5 files.
	 */
	struct PackedFile *newpackedfile;
	struct Ipo *ipo;
	float volume;
	float attenuation;
	float pitch;
	float min_gain;
	float max_gain;
	float distance;
	int flags;

/**	currently	int type;
	struct bSound *child_sound;*/

	/**
	 * Whether the sound has been changed and must be restarted if playing.
	 */
	int changed;

	/**
	 * The audaspace handle for cache.
	 */
	void *cache;

/**	XXX unused currently	// SOUND_TYPE_LIMITER
	float start, end;*/
} bSound;

/* XXX unused currently
typedef enum eSound_Type {
	SOUND_TYPE_INVALID = -1,
	SOUND_TYPE_FILE = 0,
	SOUND_TYPE_BUFFER,
	SOUND_TYPE_LIMITER
} eSound_Type;*/

/* spacesound->flag */
#define SND_DRAWFRAMES	1
#define SND_CFRA_NUM	2

typedef struct SpaceSound {
	struct SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;
	struct ScrArea *area;
	
	View2D v2d;
	
	bSound *sound;
	short mode, sndnr;
	short xof, yof;
	short flag, lock;
	int pad2;
} SpaceSound;

#define SOUND_FLAGS_3D					(1 << 3)

/* to DNA_sound_types.h*/

#endif

