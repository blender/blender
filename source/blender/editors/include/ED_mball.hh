/* SPDX-FileCopyrightText: 2008 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct Base;
struct MetaElem;
struct Object;
struct SelectPick_Params;
struct UndoType;
struct bContext;
struct wmKeyConfig;

void ED_operatortypes_metaball();
void ED_operatormacros_metaball();
void ED_keymap_metaball(wmKeyConfig *keyconf);

/**
 * Add meta-element primitive to meta-ball object (which is in edit mode).
 */
MetaElem *ED_mball_add_primitive(
    bContext *C, Object *obedit, bool obedit_is_new, float mat[4][4], float dia, int type);

Base *ED_mball_base_and_elem_from_select_buffer(Base **bases,
                                                uint bases_len,
                                                const uint select_id,
                                                MetaElem **r_ml);

/**
 * Select meta-element with mouse click (user can select radius circle or stiffness circle).
 *
 * \return True when pick finds an element or the selection changed.
 */
bool ED_mball_select_pick(bContext *C, const int mval[2], const SelectPick_Params *params);

bool ED_mball_deselect_all_multi_ex(Base **bases, uint bases_len);
bool ED_mball_deselect_all_multi(bContext *C);

/**
 * This function is used to free all MetaElems from MetaBall.
 */
void ED_mball_editmball_free(Object *obedit);
/**
 * This function is called, when MetaBall Object is switched from object mode to edit mode.
 */
void ED_mball_editmball_make(Object *obedit);
/**
 * This function is called, when MetaBall Object switched from edit mode to object mode.
 * List of MetaElements is copied from object->data->edit_elems to object->data->elems.
 */
void ED_mball_editmball_load(Object *obedit);

/* `editmball_undo.cc` */

/** Export for ED_undo_sys. */
void ED_mball_undosys_type(UndoType *ut);

#define MBALLSEL_STIFF (1u << 30)
#define MBALLSEL_RADIUS (1u << 31)
#define MBALLSEL_ANY (MBALLSEL_STIFF | MBALLSEL_RADIUS)
