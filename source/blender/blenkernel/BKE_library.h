/**
 * blenlib/BKE_library.h (mar-2001 nzc)
 *	
 * Library
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
#ifndef BKE_LIBRARY_TYPES_H
#define BKE_LIBRARY_TYPES_H

struct ListBase;
struct ID;
struct Main;
struct Library;
struct wmWindowManager;
struct bContext;

void *alloc_libblock(struct ListBase *lb, short type, const char *name);
void *copy_libblock(void *rt);

void id_lib_extern(struct ID *id);
void id_us_plus(struct ID *id);
void id_us_min(struct ID *id);
int id_make_local(struct ID *id, int test);
int id_copy(struct ID *id, struct ID **newid, int test);
int id_unlink(struct ID *id, int test);

int check_for_dupid(struct ListBase *lb, struct ID *id, char *name);
int new_id(struct ListBase *lb, struct ID *id, const char *name);

struct ListBase *wich_libbase(struct Main *mainlib, short type);

#define MAX_LIBARRAY	40
int set_listbasepointers(struct Main *main, struct ListBase **lb);

void free_libblock(struct ListBase *lb, void *idv);
void free_libblock_us(struct ListBase *lb, void *idv);
void free_main(struct Main *mainvar);
void tag_main(struct Main *mainvar, int tag);

void splitIDname(char *name, char *left, int *nr);
void rename_id(struct ID *id, char *name);
void test_idbutton(char *name);
void text_idbutton(struct ID *id, char *text);
void all_local(struct Library *lib, int untagged_only);
struct ID *find_id(char *type, char *name);
void clear_id_newpoins(void);

void IDnames_to_pupstring(char **str, char *title, char *extraops, struct ListBase *lb,struct ID* link, short *nr);
void IMAnames_to_pupstring(char **str, char *title, char *extraops, struct ListBase *lb, struct ID *link, short *nr);
void IPOnames_to_pupstring(char **str, char *title, char *extraops, struct ListBase *lb, struct ID* link, short *nr, int blocktype);

void flag_listbase_ids(ListBase *lb, short flag, short value);
void flag_all_listbases_ids(short flag, short value);
void recalc_all_library_objects(struct Main *main);

void set_free_windowmanager_cb(void (*func)(struct bContext *, struct wmWindowManager *) );

#endif

