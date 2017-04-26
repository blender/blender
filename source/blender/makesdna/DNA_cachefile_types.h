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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Kevin Dietrich.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_cachefile_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_CACHEFILE_TYPES_H__
#define __DNA_CACHEFILE_TYPES_H__

#include "DNA_ID.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CacheFile::flag */
enum {
	CACHEFILE_DS_EXPAND = (1 << 0),
	CACHEFILE_DIRTY     = (1 << 1),
};

/* CacheFile::draw_flag */
enum {
	CACHEFILE_KEYFRAME_DRAWN = (1 << 0),
};

/* Representation of an object's path inside the Alembic file.
 * Note that this is not a file path. */
typedef struct AlembicObjectPath {
	struct AlembicObjectPath *next, *prev;

	char path[4096];
} AlembicObjectPath;

typedef struct CacheFile {
	ID id;
	struct AnimData *adt;

	struct AbcArchiveHandle *handle;
	void *handle_mutex;

	/* Paths of the objects inside of the Alembic archive referenced by this
	 * CacheFile. */
	ListBase object_paths;

	char filepath[1024];  /* 1024 = FILE_MAX */

	char is_sequence;
	char forward_axis;
	char up_axis;
	char override_frame;

	float scale;
	float frame;  /* The frame/time to lookup in the cache file. */

	short flag;  /* Animation flag. */
	short draw_flag;
} CacheFile;

#ifdef __cplusplus
}
#endif

#endif  /* __DNA_CACHEFILE_TYPES_H__ */
