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
	char name[160];
	void *stream; // AUD_XXX deprecated
	struct PackedFile *packedfile;
	struct PackedFile *newpackedfile; // AUD_XXX deprecated
	void *snd_sound; // AUD_XXX used for AUD_Sound now
	struct Ipo *ipo; // AUD_XXX deprecated
	float volume, panning; // AUD_XXX deprecated
	/**
	 * Sets the rollofffactor. The	rollofffactor is a per-Source parameter
	 * the application can use to increase or decrease	the range of a source
	 * by decreasing or increasing the attenuation, respectively. The default
	 * value is 1. The implementation is free to optimize for a rollofffactor
	 * value of 0, which indicates that the application does not wish any
	 * distance attenuation on the respective Source.
	 */
	float attenuation; // AUD_XXX deprecated
	float pitch; // AUD_XXX deprecated
	/**
	 * min_gain indicates the minimal gain which is always guaranteed for this sound
	 */
	float min_gain; // AUD_XXX deprecated
	/**
	 * max_gain indicates the maximal gain which is always guaranteed for this sound
	 */
	float max_gain; // AUD_XXX deprecated
	/**
	 * Sets the referencedistance at which the listener will experience gain.
	 */
	float distance; // AUD_XXX deprecated
	int flags; // AUD_XXX deprecated
	int streamlen; // AUD_XXX deprecated
	char channels; // AUD_XXX deprecated
	char highprio; // AUD_XXX deprecated
	char pad[10]; // AUD_XXX deprecated

	// AUD_XXX NEW
	int type;
	int changed;
	struct bSound *child_sound;
	void *cache;

	// SOUND_TYPE_LIMITER
	float start, end;
} bSound;

typedef enum eSound_Type {
	SOUND_TYPE_INVALID = -1,
	SOUND_TYPE_FILE = 0,
	SOUND_TYPE_BUFFER,
	SOUND_TYPE_LIMITER
} eSound_Type;

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


#define SOUND_CHANNELS_STEREO	0
#define SOUND_CHANNELS_LEFT		1
#define SOUND_CHANNELS_RIGHT	2

#define SOUND_FLAGS_LOOP 				(1 << 0)
#define SOUND_FLAGS_FIXED_VOLUME 		(1 << 1)
#define SOUND_FLAGS_FIXED_PANNING 		(1 << 2)
#define SOUND_FLAGS_3D					(1 << 3)
#define SOUND_FLAGS_BIDIRECTIONAL_LOOP	(1 << 4)
#define SOUND_FLAGS_PRIORITY			(1 << 5)
#define SOUND_FLAGS_SEQUENCE			(1 << 6)

/* to DNA_sound_types.h*/

#endif

