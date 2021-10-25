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

/** \file ED_util.h
 *  \ingroup editors
 */

#ifndef __ED_UTIL_H__
#define __ED_UTIL_H__

struct bContext;
struct SpaceLink;
struct wmOperator;
struct wmOperatorType;

/* ed_util.c */

void    ED_editors_init(struct bContext *C);
void    ED_editors_exit(struct bContext *C);

bool    ED_editors_flush_edits(const struct bContext *C, bool for_render);

void    ED_spacedata_id_remap(struct ScrArea *sa, struct SpaceLink *sl, struct ID *old_id, struct ID *new_id);

void    ED_OT_flush_edits(struct wmOperatorType *ot);

/* ************** Undo ************************ */

/* undo.c */
void    ED_undo_push(struct bContext *C, const char *str);
void    ED_undo_push_op(struct bContext *C, struct wmOperator *op);
void    ED_undo_grouped_push(struct bContext *C, const char *str);
void    ED_undo_grouped_push_op(struct bContext *C, struct wmOperator *op);
void    ED_undo_pop_op(struct bContext *C, struct wmOperator *op);
void    ED_undo_pop(struct bContext *C);
void    ED_undo_redo(struct bContext *C);
void    ED_OT_undo(struct wmOperatorType *ot);
void    ED_OT_undo_push(struct wmOperatorType *ot);
void    ED_OT_redo(struct wmOperatorType *ot);
void    ED_OT_undo_redo(struct wmOperatorType *ot);
void    ED_OT_undo_history(struct wmOperatorType *ot);

int     ED_undo_operator_repeat(struct bContext *C, struct wmOperator *op);
/* convenience since UI callbacks use this mostly*/
void    ED_undo_operator_repeat_cb(struct bContext *C, void *arg_op, void *arg_unused);
void    ED_undo_operator_repeat_cb_evt(struct bContext *C, void *arg_op, int arg_unused);

bool    ED_undo_is_valid(const struct bContext *C, const char *undoname);

/* undo_editmode.c */
void undo_editmode_push(struct bContext *C, const char *name, 
                        void * (*getdata)(struct bContext *C),
                        void (*freedata)(void *),
                        void (*to_editmode)(void *, void *, void *),
                        void *(*from_editmode)(void *, void *),
                        int (*validate_undo)(void *, void *));


void    undo_editmode_clear(void);

/* ************** XXX OLD CRUFT WARNING ************* */

void apply_keyb_grid(int shift, int ctrl, float *val, float fac1, float fac2, float fac3, int invert);

/* where else to go ? */
void unpack_menu(struct bContext *C, const char *opname, const char *id_name, const char *abs_name, const char *folder, struct PackedFile *pf);

#endif /* __ED_UTIL_H__ */

