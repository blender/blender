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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/util/util_intern.h
 *  \ingroup edutil
 */


#ifndef __UTIL_INTERN_H__
#define __UTIL_INTERN_H__

/* internal exports only */

/* editmode_undo.c */
void        undo_editmode_name(struct bContext *C, const char *undoname);
bool        undo_editmode_is_valid(const char *undoname);
const char *undo_editmode_get_name(struct bContext *C, int nr, bool *r_active);
void       *undo_editmode_get_prev(struct Object *ob);
void        undo_editmode_step(struct bContext *C, int step);
void        undo_editmode_number(struct bContext *C, int nr);

#endif /* __UTIL_INTERN_H__ */

