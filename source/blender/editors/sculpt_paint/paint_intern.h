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

#ifndef ED_PAINT_INTERN_H
#define ED_PAINT_INTERN_H

struct bContext;
struct Scene;
struct Object;
struct Mesh;
struct PaintStroke;
struct PointerRNA;
struct ViewContext;
struct wmEvent;
struct wmOperator;
struct wmOperatorType;
struct ARegion;

/* paint_stroke.c */
typedef int (*StrokeTestStart)(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
typedef void (*StrokeUpdateStep)(struct bContext *C, struct PaintStroke *stroke, struct PointerRNA *itemptr);
typedef void (*StrokeDone)(struct bContext *C, struct PaintStroke *stroke);

struct PaintStroke *paint_stroke_new(bContext *C, StrokeTestStart test_start,
				     StrokeUpdateStep update_step, StrokeDone done);
int paint_stroke_modal(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
int paint_stroke_exec(struct bContext *C, struct wmOperator *op);
struct ViewContext *paint_stroke_view_context(struct PaintStroke *stroke);
void *paint_stroke_mode_data(struct PaintStroke *stroke);
void paint_stroke_set_mode_data(struct PaintStroke *stroke, void *mode_data);
int paint_poll(bContext *C);
void paint_cursor_start(struct bContext *C, int (*poll)(struct bContext *C));

/* paint_vertex.c */
int weight_paint_poll(struct bContext *C);
int vertex_paint_poll(struct bContext *C);
int vertex_paint_mode_poll(struct bContext *C);

void clear_vpaint(Scene *scene, int selected);

void PAINT_OT_weight_paint_toggle(struct wmOperatorType *ot);
void PAINT_OT_weight_paint_radial_control(struct wmOperatorType *ot);
void PAINT_OT_weight_paint(struct wmOperatorType *ot);

void PAINT_OT_vertex_paint_radial_control(struct wmOperatorType *ot);
void PAINT_OT_vertex_paint_toggle(struct wmOperatorType *ot);
void PAINT_OT_vertex_paint(struct wmOperatorType *ot);

/* paint_image.c */
int image_texture_paint_poll(struct bContext *C);

void PAINT_OT_image_paint(struct wmOperatorType *ot);
void PAINT_OT_image_paint_radial_control(struct wmOperatorType *ot);
void PAINT_OT_grab_clone(struct wmOperatorType *ot);
void PAINT_OT_sample_color(struct wmOperatorType *ot);
void PAINT_OT_clone_cursor_set(struct wmOperatorType *ot);
void PAINT_OT_texture_paint_toggle(struct wmOperatorType *ot);
void PAINT_OT_texture_paint_radial_control(struct wmOperatorType *ot);

/* paint_utils.c */
int imapaint_pick_face(struct ViewContext *vc, struct Mesh *me, int *mval, unsigned int *index);
void imapaint_pick_uv(struct Scene *scene, struct Object *ob, struct Mesh *mesh, unsigned int faceindex, int *xy, float *uv);

void paint_sample_color(struct Scene *scene, struct ARegion *ar, int x, int y);
void BRUSH_OT_curve_preset(struct wmOperatorType *ot);

#endif /* ED_PAINT_INTERN_H */

