/**
 * $Id:
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_UTIL_H
#define ED_UTIL_H

struct Object;
struct bContext;
struct ARegion;
struct uiBlock;
struct wmOperator;
struct wmOperatorType;

/* ed_util.c */

void	ED_editors_exit			(struct bContext *C);

/* ************** Undo ************************ */

/* undo.c */
void	ED_undo_push			(struct bContext *C, char *str);
void	ED_undo_push_op			(struct bContext *C, struct wmOperator *op);
void	ED_undo_pop_op			(struct bContext *C, struct wmOperator *op);
void	ED_undo_pop				(struct bContext *C);
void	ED_undo_redo			(struct bContext *C);
void	ED_OT_undo				(struct wmOperatorType *ot);
void	ED_OT_redo				(struct wmOperatorType *ot);

/* undo_editmode.c */
void undo_editmode_push(struct bContext *C, char *name, 
						void * (*getdata)(struct bContext *C),
						void (*freedata)(void *), 
						void (*to_editmode)(void *, void *),  
						void *(*from_editmode)(void *),
						int (*validate_undo)(void *, void *));

						
void	*undo_editmode_get_prev		(struct Object *ob);
struct uiBlock *editmode_undohistorymenu(struct bContext *C, struct ARegion *ar, void *arg_unused);
void	undo_editmode_menu			(struct bContext *C);
void	undo_editmode_clear			(void);
void	undo_editmode_step			(struct bContext *C, int step);


/* ************** XXX OLD CRUFT WARNING ************* */

void apply_keyb_grid(int shift, int ctrl, float *val, float fac1, float fac2, float fac3, int invert);
int GetButStringLength(char *str);

#endif /* ED_UTIL_H */

