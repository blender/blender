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

/** \file ED_mball.h
 *  \ingroup editors
 */

#ifndef __ED_MBALL_H__
#define __ED_MBALL_H__

struct bContext;
struct MetaBall;
struct Object;
struct wmKeyConfig;

void ED_operatortypes_metaball(void);
void ED_operatormacros_metaball(void);
void ED_keymap_metaball(struct wmKeyConfig *keyconf);

struct MetaElem *add_metaball_primitive(struct bContext *C, struct Object *obedit, float mat[4][4], float dia, int type);

bool mouse_mball(struct bContext *C, const int mval[2], bool extend, bool deselect, bool toggle);

void free_editMball(struct Object *obedit);
void make_editMball(struct Object *obedit);
void load_editMball(struct Object *obedit);

void undo_push_mball(struct bContext *C, const char *name);

void ED_mball_transform(struct MetaBall *mb, float mat[4][4]);

#endif  /* __ED_MBALL_H__ */
