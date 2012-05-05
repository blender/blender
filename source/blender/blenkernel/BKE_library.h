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

struct ListBase;
struct ID;
struct Main;
struct Library;
struct wmWindowManager;
struct bContext;
struct PointerRNA;
struct PropertyRNA;

void *BKE_libblock_alloc(struct ListBase *lb, short type, const char *name);
void *BKE_libblock_copy(struct ID *id);
void  BKE_libblock_copy_data(struct ID *id, const struct ID *id_from, const short do_action);

void BKE_id_lib_local_paths(struct Main *bmain, struct Library *lib, struct ID *id);
void id_lib_extern(struct ID *id);
void BKE_library_filepath_set(struct Library *lib, const char *filepath);
void id_us_plus(struct ID *id);
void id_us_min(struct ID *id);

int id_make_local(struct ID *id, int test);
int id_single_user(struct bContext *C, struct ID *id, struct PointerRNA *ptr, struct PropertyRNA *prop);
int id_copy(struct ID *id, struct ID **newid, int test);
int id_unlink(struct ID *id, int test);
void id_sort_by_name(struct ListBase *lb, struct ID *id);

int new_id(struct ListBase *lb, struct ID *id, const char *name);
void id_clear_lib_data(struct Main *bmain, struct ID *id);

struct ListBase *which_libbase(struct Main *mainlib, short type);

#define MAX_LIBARRAY	40
int set_listbasepointers(struct Main *main, struct ListBase **lb);

void BKE_libblock_free(struct ListBase *lb, void *idv);
void BKE_libblock_free_us(struct ListBase *lb, void *idv);
void free_main(struct Main *mainvar);

void tag_main_idcode(struct Main *mainvar, const short type, const short tag);
void tag_main_lb(struct ListBase *lb, const short tag);
void tag_main(struct Main *mainvar, const short tag);

void rename_id(struct ID *id, const char *name);
void name_uiprefix_id(char *name, struct ID *id);
void test_idbutton(char *name);
void text_idbutton(struct ID *id, char *text);
void BKE_library_make_local(struct Main *bmain, struct Library *lib, int untagged_only);
struct ID *find_id(const char *type, const char *name);
void clear_id_newpoins(void);

void IDnames_to_pupstring(const char **str, const char *title, const char *extraops,
                          struct ListBase *lb, struct ID* link, short *nr);
void IMAnames_to_pupstring(const char **str, const char *title, const char *extraops,
                           struct ListBase *lb, struct ID *link, short *nr);

void flag_listbase_ids(ListBase *lb, short flag, short value);
void flag_all_listbases_ids(short flag, short value);
void recalc_all_library_objects(struct Main *main);

void set_free_windowmanager_cb(void (*func)(struct bContext *, struct wmWindowManager *) );

/* use when "" is given to new_id() */
#define ID_FALLBACK_NAME "Untitled"

#define IS_TAGGED(_id) ((_id) && (((ID *)_id)->flag & LIB_DOIT))

#ifdef __cplusplus
}
#endif

#endif
