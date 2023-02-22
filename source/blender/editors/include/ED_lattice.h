/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct Base;
struct Object;
struct SelectPick_Params;
struct UndoType;
struct wmKeyConfig;

/* lattice_ops.c */

void ED_operatortypes_lattice(void);
void ED_keymap_lattice(struct wmKeyConfig *keyconf);

/* editlattice_select.c */

bool ED_lattice_flags_set(struct Object *obedit, int flag);
/**
 * \return True when pick finds an element or the selection changed.
 */
bool ED_lattice_select_pick(struct bContext *C,
                            const int mval[2],
                            const struct SelectPick_Params *params);

bool ED_lattice_deselect_all_multi_ex(struct Base **bases, uint bases_len);
bool ED_lattice_deselect_all_multi(struct bContext *C);

/* editlattice_undo.c */

/** Export for ED_undo_sys. */
void ED_lattice_undosys_type(struct UndoType *ut);

#ifdef __cplusplus
}
#endif
