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

#ifndef __ED_MBALL_H__
#define __ED_MBALL_H__

#ifdef __cplusplus
extern "C" {
#endif

struct Base;
struct Object;
struct UndoType;
struct bContext;
struct wmKeyConfig;

void ED_operatortypes_metaball(void);
void ED_operatormacros_metaball(void);
void ED_keymap_metaball(struct wmKeyConfig *keyconf);

struct MetaElem *ED_mball_add_primitive(struct bContext *C,
                                        struct Object *obedit,
                                        bool obedit_is_new,
                                        float mat[4][4],
                                        float dia,
                                        int type);

bool ED_mball_select_pick(
    struct bContext *C, const int mval[2], bool extend, bool deselect, bool toggle);

bool ED_mball_deselect_all_multi_ex(struct Base **bases, uint bases_len);
bool ED_mball_deselect_all_multi(struct bContext *C);

void ED_mball_editmball_free(struct Object *obedit);
void ED_mball_editmball_make(struct Object *obedit);
void ED_mball_editmball_load(struct Object *obedit);

/* editmball_undo.c */
void ED_mball_undosys_type(struct UndoType *ut);

#define MBALLSEL_STIFF (1u << 30)
#define MBALLSEL_RADIUS (1u << 31)
#define MBALLSEL_ANY (MBALLSEL_STIFF | MBALLSEL_RADIUS)

#ifdef __cplusplus
}
#endif

#endif /* __ED_MBALL_H__ */
