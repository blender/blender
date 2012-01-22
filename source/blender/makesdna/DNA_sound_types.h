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
/** \file DNA_sound_types.h
 *  \ingroup DNA
 *  \since mar-2001
 *  \author nzc
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

typedef struct bSound {
	ID id;

	/**
	 * The path to the sound file.
	 */
	char name[1024];  /* 1024 = FILE_MAX */

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
	int pad;

/**	currently	int type;
	struct bSound *child_sound;*/

	/**
	 * The audaspace handle for cache.
	 */
	void *cache;

	/**
	 * Waveform display data.
	 */
	void *waveform;

	/**
	 * The audaspace handle that should actually be played back.
	 * Should be cache if cache != NULL; otherwise it's handle
	 */
	void *playback_handle;

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

#define SOUND_FLAGS_3D					(1 << 3) /* deprecated! used for sound actuator loading */
#define SOUND_FLAGS_CACHING				(1 << 4)
#define SOUND_FLAGS_MONO				(1 << 5)

/* to DNA_sound_types.h*/

#endif

