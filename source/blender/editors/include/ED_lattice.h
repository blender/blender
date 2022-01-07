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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Base;
struct Object;
struct UndoType;
struct wmKeyConfig;

/* lattice_ops.c */

void ED_operatortypes_lattice(void);
void ED_keymap_lattice(struct wmKeyConfig *keyconf);

/* editlattice_select.c */

bool ED_lattice_flags_set(struct Object *obedit, int flag);
bool ED_lattice_select_pick(
    struct bContext *C, const int mval[2], bool extend, bool deselect, bool toggle);

bool ED_lattice_deselect_all_multi_ex(struct Base **bases, uint bases_len);
bool ED_lattice_deselect_all_multi(struct bContext *C);

/* editlattice_undo.c */

/** Export for ED_undo_sys. */
void ED_lattice_undosys_type(struct UndoType *ut);

#ifdef __cplusplus
}
#endif
