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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_curve.h
 *  \ingroup editors
 */

#ifndef __ED_CURVE_H__
#define __ED_CURVE_H__

struct Base;
struct bContext;
struct Nurb;
struct Object;
struct Scene;
struct Text;
struct View3D;
struct wmOperator;
struct wmKeyConfig;
struct Curve;
struct EditNurb;
struct BezTriple;
struct BPoint;

/* curve_ops.c */
void    ED_operatortypes_curve(void);
void    ED_operatormacros_curve(void);
void    ED_keymap_curve(struct wmKeyConfig *keyconf);

/* editcurve.c */
void ED_curve_deselect_all(struct EditNurb *editnurb);
void ED_curve_select_all(struct EditNurb *editnurb);
void ED_curve_select_swap(struct EditNurb *editnurb, bool hide_handles);


void    undo_push_curve(struct bContext *C, const char *name);
ListBase *object_editcurve_get(struct Object *ob);

void    load_editNurb(struct Object *obedit);
void    make_editNurb(struct Object *obedit);
void    free_editNurb(struct Object *obedit);

bool    mouse_nurb(struct bContext *C, const int mval[2], bool extend, bool deselect, bool toggle);

struct Nurb *add_nurbs_primitive(struct bContext *C, struct Object *obedit, float mat[4][4], int type, int newob);

int     isNurbsel(struct Nurb *nu);

int     join_curve_exec(struct bContext *C, struct wmOperator *op);

/* editfont.h */
void    undo_push_font(struct bContext *C, const char *name);
void    make_editText(struct Object *obedit);
void    load_editText(struct Object *obedit);
void    free_editText(struct Object *obedit);

void    ED_text_to_object(struct bContext *C, struct Text *text, const bool split_lines);

bool ED_curve_select_nth(struct Curve *cu, int nth);

void ED_curve_beztcpy(struct EditNurb *editnurb, struct BezTriple *dst, struct BezTriple *src, int count);
void ED_curve_bpcpy(struct EditNurb *editnurb, struct BPoint *dst, struct BPoint *src, int count);

int ED_curve_updateAnimPaths(struct Curve *cu);

bool ED_curve_active_center(struct Curve *cu, float center[3]);

bool    mouse_font(struct bContext *C, const int mval[2], bool extend, bool deselect, bool toggle);

/* debug only */
void printknots(struct Object *obedit);

#endif /* __ED_CURVE_H__ */
