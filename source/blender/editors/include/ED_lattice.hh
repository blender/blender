/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct Base;
struct KeyBlock;
struct Lattice;
struct Object;
struct SelectPick_Params;
struct UndoType;
struct wmKeyConfig;

/* `lattice_ops.cc` */

void ED_operatortypes_lattice();
void ED_keymap_lattice(wmKeyConfig *keyconf);

KeyBlock *ED_lattice_get_edit_shape_key(const Lattice *latt);

/* `editlattice_select.cc` */

bool ED_lattice_flags_set(Object *obedit, int flag);
/**
 * \return True when pick finds an element or the selection changed.
 */
bool ED_lattice_select_pick(bContext *C, const int mval[2], const SelectPick_Params *params);

bool ED_lattice_deselect_all_multi(bContext *C);

/* `editlattice_undo.cc` */

/** Export for ED_undo_sys. */
void ED_lattice_undosys_type(UndoType *ut);
