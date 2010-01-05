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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_CURVE_H
#define ED_CURVE_H

struct Base;
struct bContext;
struct Nurb;
struct Object;
struct Scene;
struct Text;
struct View3D;
struct wmOperator;
struct wmKeyConfig;

/* curve_ops.c */
void	ED_operatortypes_curve(void);
void	ED_keymap_curve	(struct wmKeyConfig *keyconf);

/* editcurve.c */
void CU_deselect_all(struct Object *obedit);
void CU_select_all(struct Object *obedit);
void CU_select_swap(struct Object *obedit);


void	undo_push_curve	(struct bContext *C, char *name);
ListBase *curve_get_editcurve(struct Object *ob);

void	load_editNurb	(struct Object *obedit);
void	make_editNurb	(struct Object *obedit);
void	free_editNurb	(struct Object *obedit);

int 	mouse_nurb		(struct bContext *C, short mval[2], int extend);

struct Nurb *add_nurbs_primitive(struct bContext *C, int type, int newname);

int		isNurbsel		(struct Nurb *nu);;

int		join_curve_exec	(struct bContext *C, struct wmOperator *op);

/* editfont.h */
void	undo_push_font	(struct bContext *C, char *name);
void	make_editText	(struct Object *obedit);
void	load_editText	(struct Object *obedit);
void	free_editText	(struct Object *obedit);

void	ED_text_to_object(struct bContext *C, struct Text *text, int split_lines);

#endif /* ED_CURVE_H */

