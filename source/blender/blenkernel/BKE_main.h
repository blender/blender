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
#ifndef __BKE_MAIN_H__
#define __BKE_MAIN_H__

/** \file BKE_main.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 *  \section aboutmain Main struct
 * Main is the root of the 'database' of a Blender context. All data
 * is stuffed into lists, and all these lists are knotted to here. A
 * Blender file is not much more but a binary dump of these
 * lists. This list of lists is not serialized itself.
 *
 * Oops... this should be a _types.h file.
 *
 */
#include "DNA_listBase.h"

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BlendThumbnail;
struct BLI_mempool;
struct Depsgraph;
struct GHash;
struct ImBuf;
struct Library;
struct MainLock;

/* Blender thumbnail, as written on file (width, height, and data as char RGBA). */
/* We pack pixel data after that struct. */
typedef struct BlendThumbnail {
	int width, height;
	char rect[0];
} BlendThumbnail;

/* Structs caching relations between data-blocks in a given Main. */
typedef struct MainIDRelationsEntry {
	struct MainIDRelationsEntry *next;
	/* WARNING! for user_to_used, that pointer is really an ID** one, but for used_to_user, it’s only an ID* one! */
	struct ID **id_pointer;
	int usage_flag;  /* Using IDWALK_ enums, in BKE_library_query.h */
} MainIDRelationsEntry;

typedef struct MainIDRelations {
	struct GHash *id_user_to_used;
	struct GHash *id_used_to_user;

	/* Private... */
	struct BLI_mempool *entry_pool;
} MainIDRelations;

typedef struct Main {
	struct Main *next, *prev;
	char name[1024]; /* 1024 = FILE_MAX */
	short versionfile, subversionfile;  /* see BLENDER_VERSION, BLENDER_SUBVERSION */
	short minversionfile, minsubversionfile;
	uint64_t build_commit_timestamp; /* commit's timestamp from buildinfo */
	char build_hash[16];  /* hash from buildinfo */
	char recovered;	/* indicate the main->name (file) is the recovered one */
	/** All current ID's exist in the last memfile undo step. */
	char is_memfile_undo_written;

	BlendThumbnail *blen_thumb;

	struct Library *curlib;
	ListBase scene;
	ListBase library;
	ListBase object;
	ListBase mesh;
	ListBase curve;
	ListBase mball;
	ListBase mat;
	ListBase tex;
	ListBase image;
	ListBase latt;
	ListBase lamp;
	ListBase camera;
	ListBase ipo;   // XXX deprecated
	ListBase key;
	ListBase world;
	ListBase screen;
	ListBase vfont;
	ListBase text;
	ListBase speaker;
	ListBase lightprobe;
	ListBase sound;
	ListBase collection;
	ListBase armature;
	ListBase action;
	ListBase nodetree;
	ListBase brush;
	ListBase particle;
	ListBase palettes;
	ListBase paintcurves;
	ListBase wm;
	ListBase gpencil;
	ListBase movieclip;
	ListBase mask;
	ListBase linestyle;
	ListBase cachefiles;
	ListBase workspaces;

	/* Must be generated, used and freed by same code - never assume this is valid data unless you know
	 * when, who and how it was created.
	 * Used by code doing a lot of remapping etc. at once to speed things up. */
	struct MainIDRelations *relations;

	struct MainLock *lock;
} Main;

struct Main *BKE_main_new(void);
void BKE_main_free(struct Main *mainvar);

void BKE_main_lock(struct Main *bmain);
void BKE_main_unlock(struct Main *bmain);

void BKE_main_relations_create(struct Main *bmain);
void BKE_main_relations_free(struct Main *bmain);

struct BlendThumbnail *BKE_main_thumbnail_from_imbuf(struct Main *bmain, struct ImBuf *img);
struct ImBuf *BKE_main_thumbnail_to_imbuf(struct Main *bmain, struct BlendThumbnail *data);
void BKE_main_thumbnail_create(struct Main *bmain);

const char *BKE_main_blendfile_path(const struct Main *bmain) ATTR_NONNULL();
const char *BKE_main_blendfile_path_from_global(void);

struct ListBase *which_libbase(struct Main *mainlib, short type);

#define MAX_LIBARRAY    37
int set_listbasepointers(struct Main *main, struct ListBase *lb[MAX_LIBARRAY]);

#define MAIN_VERSION_ATLEAST(main, ver, subver) \
	((main)->versionfile > (ver) || (main->versionfile == (ver) && (main)->subversionfile >= (subver)))

#define MAIN_VERSION_OLDER(main, ver, subver) \
	((main)->versionfile < (ver) || (main->versionfile == (ver) && (main)->subversionfile < (subver)))

#define BLEN_THUMB_SIZE 128

#define BLEN_THUMB_MEMSIZE(_x, _y) (sizeof(BlendThumbnail) + ((size_t)(_x) * (size_t)(_y)) * sizeof(int))
#define BLEN_THUMB_SAFE_MEMSIZE(_x, _y) ((uint64_t)_x * (uint64_t)_y < (SIZE_MAX / (sizeof(int) * 4)))

#ifdef __cplusplus
}
#endif

#endif  /* __BKE_MAIN_H__ */
