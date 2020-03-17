/*
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
 */
#ifndef __BKE_MAIN_H__
#define __BKE_MAIN_H__

/** \file
 * \ingroup bke
 * \section aboutmain Main struct
 * Main is the root of the 'data-base' of a Blender context. All data is put into lists, and all
 * these lists are stored here.
 *
 * \note A Blender file is not much more than a binary dump of these lists. This list of lists is
 * not serialized itself.
 *
 * \note `BKE_main` files are for operations over the Main database itself, or generating extra
 * temp data to help working with it. Those should typically not affect the data-blocks themselves.
 *
 * \section Function Names
 *
 * - `BKE_main_` should be used for functions in that file.
 */

#include "DNA_listBase.h"

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BLI_mempool;
struct BlendThumbnail;
struct GHash;
struct GSet;
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
  /* WARNING! for user_to_used,
   * that pointer is really an ID** one, but for used_to_user, it’s only an ID* one! */
  struct ID **id_pointer;
  int usage_flag; /* Using IDWALK_ enums, in BKE_lib_query.h */
} MainIDRelationsEntry;

typedef struct MainIDRelations {
  struct GHash *id_user_to_used;
  struct GHash *id_used_to_user;

  short flag;

  /* Private... */
  struct BLI_mempool *entry_pool;
} MainIDRelations;

enum {
  /* Those bmain relations include pointers/usages from editors. */
  MAINIDRELATIONS_INCLUDE_UI = 1 << 0,
};

typedef struct Main {
  struct Main *next, *prev;
  char name[1024];                   /* 1024 = FILE_MAX */
  short versionfile, subversionfile; /* see BLENDER_VERSION, BLENDER_SUBVERSION */
  short minversionfile, minsubversionfile;
  uint64_t build_commit_timestamp; /* commit's timestamp from buildinfo */
  char build_hash[16];             /* hash from buildinfo */
  char recovered;                  /* indicate the main->name (file) is the recovered one */
  /** All current ID's exist in the last memfile undo step. */
  char is_memfile_undo_written;
  /**
   * An ID needs it's data to be flushed back.
   * use "needs_flush_to_id" in edit data to flag data which needs updating.
   */
  char is_memfile_undo_flush_needed;
  /**
   * Indicates that next memfile undo step should not allow to re-use old bmain when re-read, but
   * instead do a complete full re-read/update from stored memfile.
   */
  char use_memfile_full_barrier;

  BlendThumbnail *blen_thumb;

  struct Library *curlib;
  ListBase scenes;
  ListBase libraries;
  ListBase objects;
  ListBase meshes;
  ListBase curves;
  ListBase metaballs;
  ListBase materials;
  ListBase textures;
  ListBase images;
  ListBase lattices;
  ListBase lights;
  ListBase cameras;
  ListBase ipo; /* Deprecated (only for versioning). */
  ListBase shapekeys;
  ListBase worlds;
  ListBase screens;
  ListBase fonts;
  ListBase texts;
  ListBase speakers;
  ListBase lightprobes;
  ListBase sounds;
  ListBase collections;
  ListBase armatures;
  ListBase actions;
  ListBase nodetrees;
  ListBase brushes;
  ListBase particles;
  ListBase palettes;
  ListBase paintcurves;
  ListBase wm; /* Singleton (exception). */
  ListBase gpencils;
  ListBase movieclips;
  ListBase masks;
  ListBase linestyles;
  ListBase cachefiles;
  ListBase workspaces;
  ListBase hairs;
  ListBase pointclouds;
  ListBase volumes;

  /**
   * Must be generated, used and freed by same code - never assume this is valid data unless you
   * know when, who and how it was created.
   * Used by code doing a lot of remapping etc. at once to speed things up.
   */
  struct MainIDRelations *relations;

  struct MainLock *lock;
} Main;

struct Main *BKE_main_new(void);
void BKE_main_free(struct Main *mainvar);

void BKE_main_lock(struct Main *bmain);
void BKE_main_unlock(struct Main *bmain);

void BKE_main_relations_create(struct Main *bmain, const short flag);
void BKE_main_relations_free(struct Main *bmain);

struct GSet *BKE_main_gset_create(struct Main *bmain, struct GSet *gset);

/* *** Generic utils to loop over whole Main database. *** */

#define FOREACH_MAIN_LISTBASE_ID_BEGIN(_lb, _id) \
  { \
    ID *_id_next = (_lb)->first; \
    for ((_id) = _id_next; (_id) != NULL; (_id) = _id_next) { \
      _id_next = (_id)->next;

#define FOREACH_MAIN_LISTBASE_ID_END \
  } \
  } \
  ((void)0)

#define FOREACH_MAIN_LISTBASE_BEGIN(_bmain, _lb) \
  { \
    ListBase *_lbarray[MAX_LIBARRAY]; \
    int _i = set_listbasepointers((_bmain), _lbarray); \
    while (_i--) { \
      (_lb) = _lbarray[_i];

#define FOREACH_MAIN_LISTBASE_END \
  } \
  } \
  ((void)0)

/**
 * DO NOT use break statement with that macro,
 * use #FOREACH_MAIN_LISTBASE and #FOREACH_MAIN_LISTBASE_ID instead
 * if you need that kind of control flow. */
#define FOREACH_MAIN_ID_BEGIN(_bmain, _id) \
  { \
    ListBase *_lb; \
    FOREACH_MAIN_LISTBASE_BEGIN ((_bmain), _lb) { \
      FOREACH_MAIN_LISTBASE_ID_BEGIN (_lb, (_id))

#define FOREACH_MAIN_ID_END \
  FOREACH_MAIN_LISTBASE_ID_END; \
  } \
  FOREACH_MAIN_LISTBASE_END; \
  } \
  ((void)0)

struct BlendThumbnail *BKE_main_thumbnail_from_imbuf(struct Main *bmain, struct ImBuf *img);
struct ImBuf *BKE_main_thumbnail_to_imbuf(struct Main *bmain, struct BlendThumbnail *data);
void BKE_main_thumbnail_create(struct Main *bmain);

const char *BKE_main_blendfile_path(const struct Main *bmain) ATTR_NONNULL();
const char *BKE_main_blendfile_path_from_global(void);

struct ListBase *which_libbase(struct Main *mainlib, short type);

#define MAX_LIBARRAY 40
int set_listbasepointers(struct Main *main, struct ListBase *lb[MAX_LIBARRAY]);

#define MAIN_VERSION_ATLEAST(main, ver, subver) \
  ((main)->versionfile > (ver) || \
   (main->versionfile == (ver) && (main)->subversionfile >= (subver)))

#define MAIN_VERSION_OLDER(main, ver, subver) \
  ((main)->versionfile < (ver) || \
   (main->versionfile == (ver) && (main)->subversionfile < (subver)))

#define BLEN_THUMB_SIZE 128

#define BLEN_THUMB_MEMSIZE(_x, _y) \
  (sizeof(BlendThumbnail) + ((size_t)(_x) * (size_t)(_y)) * sizeof(int))
/** Protect against buffer overflow vulnerability & negative sizes. */
#define BLEN_THUMB_MEMSIZE_IS_VALID(_x, _y) \
  (((_x) > 0 && (_y) > 0) && ((uint64_t)(_x) * (uint64_t)(_y) < (SIZE_MAX / (sizeof(int) * 4))))

#ifdef __cplusplus
}
#endif

#endif /* __BKE_MAIN_H__ */
