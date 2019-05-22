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
#ifndef __BKE_LIBRARY_H__
#define __BKE_LIBRARY_H__

/** \file BKE_library.h
 *  \ingroup bke
 *  \since March 2001
 *  \author nzc
 */
#ifdef __cplusplus
extern "C" {
#endif

#include "BLI_compiler_attrs.h"

struct BlendThumbnail;
struct GHash;
struct ID;
struct ImBuf;
struct Library;
struct ListBase;
struct Main;
struct PointerRNA;
struct PropertyRNA;
struct bContext;
struct wmWindowManager;

size_t BKE_libblock_get_alloc_info(short type, const char **name);
void *BKE_libblock_alloc_notest(short type) ATTR_WARN_UNUSED_RESULT;
void *BKE_libblock_alloc(struct Main *bmain, short type, const char *name, const int flag) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
void  BKE_libblock_init_empty(struct ID *id) ATTR_NONNULL(1);

/**
 * New ID creation/copying options.
 */
enum {
	/* *** Generic options (should be handled by all ID types copying, ID creation, etc.). *** */
	/* Create datablock outside of any main database - similar to 'localize' functions of materials etc. */
	LIB_ID_CREATE_NO_MAIN            = 1 << 0,
	/* Do not affect user refcount of datablocks used by new one (which also gets zero usercount then).
	 * Implies LIB_ID_CREATE_NO_MAIN. */
	LIB_ID_CREATE_NO_USER_REFCOUNT   = 1 << 1,
	/* Assume given 'newid' already points to allocated memory for whole datablock (ID + data) - USE WITH CAUTION!
	 * Implies LIB_ID_CREATE_NO_MAIN. */
	LIB_ID_CREATE_NO_ALLOCATE        = 1 << 2,

	LIB_ID_CREATE_NO_DEG_TAG         = 1 << 8,  /* Do not tag new ID for update in depsgraph. */

	/* Specific options to some ID types or usages, may be ignored by unrelated ID copying functions. */
	LIB_ID_COPY_NO_PROXY_CLEAR     = 1 << 16,  /* Object only, needed by make_local code. */
	LIB_ID_COPY_NO_PREVIEW         = 1 << 17,  /* Do not copy preview data, when supported. */
	LIB_ID_COPY_CACHES             = 1 << 18,  /* Copy runtime data caches. */
	/* XXX TODO Do we want to keep that? would rather try to get rid of it... */
	LIB_ID_COPY_ACTIONS            = 1 << 19,  /* EXCEPTION! Deep-copy actions used by animdata of copied ID. */
	LIB_ID_COPY_KEEP_LIB           = 1 << 20,  /* Keep the library pointer when copying datablock outside of bmain. */
};

void BKE_libblock_copy_ex(struct Main *bmain, const struct ID *id, struct ID **r_newid, const int flag);
void *BKE_libblock_copy(struct Main *bmain, const struct ID *id) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/* "Deprecated" old API. */
void *BKE_libblock_copy_nolib(const struct ID *id, const bool do_action) ATTR_NONNULL();

void  BKE_libblock_rename(struct Main *bmain, struct ID *id, const char *name) ATTR_NONNULL();
void  BLI_libblock_ensure_unique_name(struct Main *bmain, const char *name) ATTR_NONNULL();

struct ID *BKE_libblock_find_name(struct Main *bmain, const short type, const char *name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/* library_remap.c (keep here since they're general functions) */
/**
 * New freeing logic options.
 */
enum {
	/* *** Generic options (should be handled by all ID types freeing). *** */
	/* Do not try to remove freed ID from given Main (passed Main may be NULL). */
	LIB_ID_FREE_NO_MAIN            = 1 << 0,
	/* Do not affect user refcount of datablocks used by freed one.
	 * Implies LIB_ID_FREE_NO_MAIN. */
	LIB_ID_FREE_NO_USER_REFCOUNT   = 1 << 1,
	/* Assume freed ID datablock memory is managed elsewhere, do not free it
	 * (still calls relevant ID type's freeing function though) - USE WITH CAUTION!
	 * Implies LIB_ID_FREE_NO_MAIN. */
	LIB_ID_FREE_NOT_ALLOCATED      = 1 << 2,

	LIB_ID_FREE_NO_DEG_TAG         = 1 << 8,  /* Do not tag freed ID for update in depsgraph. */
	LIB_ID_FREE_NO_UI_USER         = 1 << 9,  /* Do not attempt to remove freed ID from UI data/notifiers/... */
};

void BKE_id_free_ex(struct Main *bmain, void *idv, int flag, const bool use_flag_from_idtag);
void BKE_id_free(struct Main *bmain, void *idv);
/* Those three naming are bad actually, should be BKE_id_free... (since it goes beyond mere datablock). */
/* "Deprecated" old API */
void  BKE_libblock_free_ex(struct Main *bmain, void *idv, const bool do_id_user, const bool do_ui_user) ATTR_NONNULL();
void  BKE_libblock_free(struct Main *bmain, void *idv) ATTR_NONNULL();
void  BKE_libblock_free_us(struct Main *bmain, void *idv) ATTR_NONNULL();

void BKE_libblock_management_main_add(struct Main *bmain, void *idv);
void BKE_libblock_management_main_remove(struct Main *bmain, void *idv);

void BKE_libblock_management_usercounts_set(struct Main *bmain, void *idv);
void BKE_libblock_management_usercounts_clear(struct Main *bmain, void *idv);

/* TODO should be named "BKE_id_delete()". */
void  BKE_libblock_delete(struct Main *bmain, void *idv) ATTR_NONNULL();

void  BKE_libblock_free_datablock(struct ID *id, const int flag) ATTR_NONNULL();
void  BKE_libblock_free_data(struct ID *id, const bool do_id_user) ATTR_NONNULL();

void BKE_id_lib_local_paths(struct Main *bmain, struct Library *lib, struct ID *id);
void id_lib_extern(struct ID *id);
void BKE_library_filepath_set(struct Main *bmain, struct Library *lib, const char *filepath);
void id_us_ensure_real(struct ID *id);
void id_us_clear_real(struct ID *id);
void id_us_plus_no_lib(struct ID *id);
void id_us_plus(struct ID *id);
void id_us_min(struct ID *id);
void id_fake_user_set(struct ID *id);
void id_fake_user_clear(struct ID *id);
void BKE_id_clear_newpoin(struct ID *id);

void BKE_id_make_local_generic(struct Main *bmain, struct ID *id, const bool id_in_mainlist, const bool lib_local);
bool id_make_local(struct Main *bmain, struct ID *id, const bool test, const bool force_local);
bool id_single_user(struct bContext *C, struct ID *id, struct PointerRNA *ptr, struct PropertyRNA *prop);
bool id_copy(struct Main *bmain, const struct ID *id, struct ID **newid, bool test);
bool BKE_id_copy_ex(struct Main *bmain, const struct ID *id, struct ID **r_newid, const int flag, const bool test);
void id_sort_by_name(struct ListBase *lb, struct ID *id);
void BKE_id_expand_local(struct Main *bmain, struct ID *id);
void BKE_id_copy_ensure_local(struct Main *bmain, const struct ID *old_id, struct ID *new_id);

bool new_id(struct ListBase *lb, struct ID *id, const char *name) ATTR_NONNULL(1, 2);
void id_clear_lib_data(struct Main *bmain, struct ID *id);
void id_clear_lib_data_ex(struct Main *bmain, struct ID *id, const bool id_in_mainlist);

struct ListBase *which_libbase(struct Main *mainlib, short type);

#define MAX_LIBARRAY    35
int set_listbasepointers(struct Main *main, struct ListBase *lb[MAX_LIBARRAY]);

/* Main API */
struct Main *BKE_main_new(void);
void BKE_main_free(struct Main *mainvar);

void BKE_main_lock(struct Main *bmain);
void BKE_main_unlock(struct Main *bmain);

void BKE_main_relations_create(struct Main *bmain);
void BKE_main_relations_free(struct Main *bmain);

struct BlendThumbnail *BKE_main_thumbnail_from_imbuf(struct Main *bmain, struct ImBuf *img);
struct ImBuf *BKE_main_thumbnail_to_imbuf(struct Main *bmain, struct BlendThumbnail *data);
void BKE_main_thumbnail_create(struct Main *bmain);

const char *BKE_main_blendfile_path(const struct Main *bmain)
	ATTR_NONNULL(1) ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;
const char *BKE_main_blendfile_path_from_global(void)
	ATTR_WARN_UNUSED_RESULT ATTR_RETURNS_NONNULL;

void BKE_main_id_tag_idcode(struct Main *mainvar, const short type, const int tag, const bool value);
void BKE_main_id_tag_listbase(struct ListBase *lb, const int tag, const bool value);
void BKE_main_id_tag_all(struct Main *mainvar, const int tag, const bool value);

void BKE_main_id_flag_listbase(struct ListBase *lb, const int flag, const bool value);
void BKE_main_id_flag_all(struct Main *bmain, const int flag, const bool value);

void BKE_main_id_clear_newpoins(struct Main *bmain);

void BLE_main_id_refcount_recompute(struct Main *bmain, const bool do_linked_only);

void BKE_main_lib_objects_recalc_all(struct Main *bmain);

/* (MAX_ID_NAME - 2) + 3 */
void BKE_id_ui_prefix(char name[66 + 1], const struct ID *id);

void BKE_library_free(struct Library *lib);

void BKE_library_make_local(
        struct Main *bmain, const struct Library *lib, struct GHash *old_to_new_ids,
        const bool untagged_only, const bool set_fake);

void BKE_id_tag_set_atomic(struct ID *id, int tag);
void BKE_id_tag_clear_atomic(struct ID *id, int tag);

bool BKE_id_is_in_gobal_main(struct ID *id);

/* use when "" is given to new_id() */
#define ID_FALLBACK_NAME N_("Untitled")

#define IS_TAGGED(_id) ((_id) && (((ID *)_id)->tag & LIB_TAG_DOIT))

#ifdef __cplusplus
}
#endif

#endif  /* __BKE_LIBRARY_H__ */
