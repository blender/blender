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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup editors
 */

#ifndef __ED_CURVE_H__
#define __ED_CURVE_H__

#ifdef __cplusplus
extern "C" {
#endif

struct BPoint;
struct Base;
struct BezTriple;
struct Curve;
struct EditNurb;
struct Main;
struct Nurb;
struct Object;
struct Text;
struct UndoType;
struct View3D;
struct bContext;
struct wmKeyConfig;
struct wmOperator;

/* curve_ops.c */
void ED_operatortypes_curve(void);
void ED_operatormacros_curve(void);
void ED_keymap_curve(struct wmKeyConfig *keyconf);

/* editcurve.c */
struct ListBase *object_editcurve_get(struct Object *ob);

void ED_curve_editnurb_load(struct Main *bmain, struct Object *obedit);
void ED_curve_editnurb_make(struct Object *obedit);
void ED_curve_editnurb_free(struct Object *obedit);

bool ED_curve_editnurb_select_pick(
    struct bContext *C, const int mval[2], bool extend, bool deselect, bool toggle);

struct Nurb *ED_curve_add_nurbs_primitive(
    struct bContext *C, struct Object *obedit, float mat[4][4], int type, int newob);

bool ED_curve_nurb_select_check(struct View3D *v3d, struct Nurb *nu);
int ED_curve_nurb_select_count(struct View3D *v3d, struct Nurb *nu);
bool ED_curve_nurb_select_all(const struct Nurb *nu);
bool ED_curve_nurb_deselect_all(const struct Nurb *nu);

int ED_curve_join_objects_exec(struct bContext *C, struct wmOperator *op);

/* editcurve_select.c */
bool ED_curve_select_check(struct View3D *v3d, struct EditNurb *editnurb);
bool ED_curve_deselect_all(struct EditNurb *editnurb);
bool ED_curve_deselect_all_multi_ex(struct Base **bases, int bases_len);
bool ED_curve_deselect_all_multi(struct bContext *C);
bool ED_curve_select_all(struct EditNurb *editnurb);
bool ED_curve_select_swap(struct EditNurb *editnurb, bool hide_handles);
int ED_curve_select_count(struct View3D *v3d, struct EditNurb *editnurb);

/* editcurve_undo.c */
void ED_curve_undosys_type(struct UndoType *ut);

/* editfont.c */
void ED_curve_editfont_load(struct Object *obedit);
void ED_curve_editfont_make(struct Object *obedit);
void ED_curve_editfont_free(struct Object *obedit);

void ED_text_to_object(struct bContext *C, struct Text *text, const bool split_lines);

void ED_curve_beztcpy(struct EditNurb *editnurb,
                      struct BezTriple *dst,
                      struct BezTriple *src,
                      int count);
void ED_curve_bpcpy(struct EditNurb *editnurb, struct BPoint *dst, struct BPoint *src, int count);

int ED_curve_updateAnimPaths(struct Main *bmain, struct Curve *cu);

bool ED_curve_active_center(struct Curve *cu, float center[3]);

bool ED_curve_editfont_select_pick(
    struct bContext *C, const int mval[2], bool extend, bool deselect, bool toggle);

/* editfont_undo.c */
void ED_font_undosys_type(struct UndoType *ut);

#if 0
/* debug only */
void printknots(struct Object *obedit);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __ED_CURVE_H__ */
