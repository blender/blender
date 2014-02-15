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

struct ListBase;
struct ID;
struct Main;
struct Library;
struct wmWindowManager;
struct bContext;
struct PointerRNA;
struct PropertyRNA;

void *BKE_libblock_alloc(struct Main *bmain, short type, const char *name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void *BKE_libblock_copy_ex(struct Main *bmain, struct ID *id) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void *BKE_libblock_copy(struct ID *id) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void  BKE_libblock_copy_data(struct ID *id, const struct ID *id_from, const bool do_action);

void BKE_id_lib_local_paths(struct Main *bmain, struct Library *lib, struct ID *id);
void id_lib_extern(struct ID *id);
void BKE_library_filepath_set(struct Library *lib, const char *filepath);
void id_us_ensure_real(struct ID *id);
void id_us_plus(struct ID *id);
void id_us_min(struct ID *id);

bool id_make_local(struct ID *id, bool test);
bool id_single_user(struct bContext *C, struct ID *id, struct PointerRNA *ptr, struct PropertyRNA *prop);
bool id_copy(struct ID *id, struct ID **newid, bool test);
bool id_unlink(struct ID *id, int test);
void id_sort_by_name(struct ListBase *lb, struct ID *id);

bool new_id(struct ListBase *lb, struct ID *id, const char *name);
void id_clear_lib_data(struct Main *bmain, struct ID *id);

struct ListBase *which_libbase(struct Main *mainlib, short type);

#define MAX_LIBARRAY    41
int set_listbasepointers(struct Main *main, struct ListBase **lb);

void BKE_libblock_free(struct Main *bmain, void *idv);
void BKE_libblock_free_ex(struct Main *bmain, void *idv, bool do_id_user);
void BKE_libblock_free_us(struct Main *bmain, void *idv);
void BKE_libblock_free_data(struct ID *id);


/* Main API */
struct Main *BKE_main_new(void);
void BKE_main_free(struct Main *mainvar);

void BKE_main_id_tag_idcode(struct Main *mainvar, const short type, const bool tag);
void BKE_main_id_tag_listbase(struct ListBase *lb, const bool tag);
void BKE_main_id_tag_all(struct Main *mainvar, const bool tag);

void BKE_main_id_flag_listbase(ListBase *lb, const short flag, const bool value);
void BKE_main_id_flag_all(struct Main *bmain, const short flag, const bool value);

void BKE_main_id_clear_newpoins(struct Main *bmain);

void BKE_main_lib_objects_recalc_all(struct Main *bmain);

void rename_id(struct ID *id, const char *name);
void name_uiprefix_id(char *name, const struct ID *id);
void test_idbutton(char *name);

void BKE_library_make_local(struct Main *bmain, struct Library *lib, bool untagged_only);

struct ID *BKE_libblock_find_name(const short type, const char *name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

void set_free_windowmanager_cb(void (*func)(struct bContext *, struct wmWindowManager *) );
void set_free_notifier_reference_cb(void (*func)(const void *) );

/* use when "" is given to new_id() */
#define ID_FALLBACK_NAME N_("Untitled")

#define IS_TAGGED(_id) ((_id) && (((ID *)_id)->flag & LIB_DOIT))

#ifdef __cplusplus
}
#endif

#endif  /* __BKE_LIBRARY_H__ */
