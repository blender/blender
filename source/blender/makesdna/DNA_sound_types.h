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

/* extern int noaudio; * defined in sound.c . also not very nice */
/*  extern ListBase *samples; don't do this in DNA, but in BKE_... instead */

struct bSample;
struct Ipo;
struct PackedFile;
struct SpaceLink;

/* should not be here! */
#
#
typedef struct bSample {
	ID id;
	void *data;
	void *snd_sample;
	short type, bits;
	short channels;
	int len, rate;
//	int buffer;
	int alindex;
	char fakedata[16];
	int flags;
	char name[160];
	struct PackedFile * packedfile;
	short us;
} bSample;



typedef struct bSound {
	ID id;
	char name[160];
	struct bSample *sample;
	void *stream;
	struct PackedFile *packedfile;
	struct PackedFile *newpackedfile;
	void *snd_sound;
	struct Ipo *ipo;
	float volume, panning;
	/**
	 * Sets the rollofffactor. The	rollofffactor is a per-Source parameter
	 * the application can use to increase or decrease	the range of a source
	 * by decreasing or increasing the attenuation, respectively. The default
	 * value is 1. The implementation is free to optimize for a rollofffactor
	 * value of 0, which indicates that the application does not wish any
	 * distance attenuation on the respective Source.
	 */
	float attenuation;
	float pitch;
	/**
	 * min_gain indicates the minimal gain which is always guaranteed for this sound
	 */
	float min_gain;
	/**
	 * max_gain indicates the maximal gain which is always guaranteed for this sound
	 */
	float max_gain;
	/**
	 * Sets the referencedistance at which the listener will experience gain.
	 */
	float distance;
	int flags;
	int streamlen;
	char channels;
	char highprio;
	char pad[10];
} bSound;

typedef struct bSoundListener {
	ID id;
	/**
	 * Overall gain
	 */
	float gain;
	/**
	 * Sets a scaling to exaggerate or deemphasize the Doppler (pitch) shift
	 * resulting from the calculation.
	 */
	float dopplerfactor;
	/**
	 * Sets the value of the propagation speed relative to which the source
	 * velocities are interpreted.
	 */
	float dopplervelocity;
	short numsoundsblender;
	short numsoundsgameengine;
	
} bSoundListener;

/* spacesound->flag */
#define SND_DRAWFRAMES	1
#define SND_CFRA_NUM	2

typedef struct SpaceSound {
	struct SpaceLink *next, *prev;
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


enum SAMPLE_FileTypes {
	SAMPLE_INVALID = -1,		// must be negative
	SAMPLE_UNKNOWN = 0,
	SAMPLE_RAW,
	SAMPLE_WAV,
	SAMPLE_MP2,
	SAMPLE_MP3,
	SAMPLE_OGG_VORBIS,
	SAMPLE_WMA,
	SAMPLE_ASF,
	SAMPLE_AIFF
};


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

#define SAMPLE_NEEDS_SAVE		(1 << 0)

/* to DNA_sound_types.h*/

#endif

