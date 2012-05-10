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

/** \file blender/editors/sculpt_paint/paint_intern.h
 *  \ingroup edsculpt
 */


#ifndef __PAINT_INTERN_H__
#define __PAINT_INTERN_H__

struct ARegion;
struct bContext;
struct bglMats;
struct Brush;
struct ListBase;
struct Mesh;
struct Object;
struct PaintStroke;
struct PointerRNA;
struct rcti;
struct Scene;
struct RegionView3D;
struct VPaint;
struct ViewContext;
struct wmEvent;
struct wmOperator;
struct wmOperatorType;

/* paint_stroke.c */
typedef int (*StrokeGetLocation)(struct bContext *C, float location[3], float mouse[2]);
typedef int (*StrokeTestStart)(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
typedef void (*StrokeUpdateStep)(struct bContext *C, struct PaintStroke *stroke, struct PointerRNA *itemptr);
typedef void (*StrokeDone)(const struct bContext *C, struct PaintStroke *stroke);

struct PaintStroke *paint_stroke_new(struct bContext *C,
					 StrokeGetLocation get_location, StrokeTestStart test_start,
					 StrokeUpdateStep update_step, StrokeDone done, int event_type);
void paint_stroke_data_free(struct wmOperator *op);

int paint_space_stroke_enabled(struct Brush *br);

struct wmKeyMap *paint_stroke_modal_keymap(struct wmKeyConfig *keyconf);
int paint_stroke_modal(struct bContext *C, struct wmOperator *op, struct wmEvent *event);
int paint_stroke_exec(struct bContext *C, struct wmOperator *op);
int paint_stroke_cancel(struct bContext *C, struct wmOperator *op);
struct ViewContext *paint_stroke_view_context(struct PaintStroke *stroke);
void *paint_stroke_mode_data(struct PaintStroke *stroke);
void paint_stroke_set_mode_data(struct PaintStroke *stroke, void *mode_data);
int paint_poll(struct bContext *C);
void paint_cursor_start(struct bContext *C, int (*poll)(struct bContext *C));

/* paint_vertex.c */
int weight_paint_poll(struct bContext *C);
int weight_paint_mode_poll(struct bContext *C);
int vertex_paint_poll(struct bContext *C);
int vertex_paint_mode_poll(struct bContext *C);

void vpaint_fill(struct Object *ob, unsigned int paintcol);
void wpaint_fill(struct VPaint *wp, struct Object *ob, float paintweight);

void PAINT_OT_weight_paint_toggle(struct wmOperatorType *ot);
void PAINT_OT_weight_paint(struct wmOperatorType *ot);
void PAINT_OT_weight_set(struct wmOperatorType *ot);
void PAINT_OT_weight_from_bones(struct wmOperatorType *ot);
void PAINT_OT_weight_sample(struct wmOperatorType *ot);
void PAINT_OT_weight_sample_group(struct wmOperatorType *ot);

void PAINT_OT_vertex_paint_toggle(struct wmOperatorType *ot);
void PAINT_OT_vertex_paint(struct wmOperatorType *ot);

unsigned int vpaint_get_current_col(struct VPaint *vp);

/* paint_image.c */
int image_texture_paint_poll(struct bContext *C);

void PAINT_OT_image_paint(struct wmOperatorType *ot);
void PAINT_OT_grab_clone(struct wmOperatorType *ot);
void PAINT_OT_sample_color(struct wmOperatorType *ot);
void PAINT_OT_clone_cursor_set(struct wmOperatorType *ot);
void PAINT_OT_texture_paint_toggle(struct wmOperatorType *ot);
void PAINT_OT_project_image(struct wmOperatorType *ot);
void PAINT_OT_image_from_view(struct wmOperatorType *ot);

/* uv sculpting */
int uv_sculpt_poll(struct bContext *C);

void SCULPT_OT_uv_sculpt_stroke(struct wmOperatorType *ot);

/* paint_utils.c */

/* Convert the object-space axis-aligned bounding box (expressed as
 * its minimum and maximum corners) into a screen-space rectangle,
 * returns zero if the result is empty */
int paint_convert_bb_to_rect(struct rcti *rect,
							 const float bb_min[3],
							 const float bb_max[3],
							 const struct ARegion *ar,
							 struct RegionView3D *rv3d,
							 struct Object *ob);

/* Get four planes in object-space that describe the projection of
 * screen_rect from screen into object-space (essentially converting a
 * 2D screens-space bounding box into four 3D planes) */
void paint_calc_redraw_planes(float planes[4][4],
							  const struct ARegion *ar,
							  struct RegionView3D *rv3d,
							  struct Object *ob,
							  const struct rcti *screen_rect);

void projectf(struct bglMats *mats, const float v[3], float p[2]);
float paint_calc_object_space_radius(struct ViewContext *vc, const float center[3], float pixel_radius);
float paint_get_tex_pixel(struct Brush* br, float u, float v);
int imapaint_pick_face(struct ViewContext *vc, const int mval[2], unsigned int *index, unsigned int totface);
void imapaint_pick_uv(struct Scene *scene, struct Object *ob, unsigned int faceindex, const int xy[2], float uv[2]);

void paint_sample_color(struct Scene *scene, struct ARegion *ar, int x, int y);
void BRUSH_OT_curve_preset(struct wmOperatorType *ot);

void PAINT_OT_face_select_linked(struct wmOperatorType *ot);
void PAINT_OT_face_select_linked_pick(struct wmOperatorType *ot);
void PAINT_OT_face_select_all(struct wmOperatorType *ot);
void PAINT_OT_face_select_inverse(struct wmOperatorType *ot);
void PAINT_OT_face_select_hide(struct wmOperatorType *ot);
void PAINT_OT_face_select_reveal(struct wmOperatorType *ot);

void PAINT_OT_vert_select_all(struct wmOperatorType *ot);
void PAINT_OT_vert_select_inverse(struct wmOperatorType *ot);
int vert_paint_poll(struct bContext *C);
int mask_paint_poll(struct bContext *C);

int facemask_paint_poll(struct bContext *C);

/* stroke operator */
typedef enum BrushStrokeMode {
	BRUSH_STROKE_NORMAL,
	BRUSH_STROKE_INVERT,
	BRUSH_STROKE_SMOOTH,
} BrushStrokeMode;

/* paint_undo.c */
typedef void (*UndoRestoreCb)(struct bContext *C, struct ListBase *lb);
typedef void (*UndoFreeCb)(struct ListBase *lb);

void undo_paint_push_begin(int type, const char *name, UndoRestoreCb restore, UndoFreeCb free);
struct ListBase *undo_paint_push_get_list(int type);
void undo_paint_push_count_alloc(int type, int size);
void undo_paint_push_end(int type);

/* paint_hide.c */

typedef enum {
	PARTIALVIS_HIDE,
	PARTIALVIS_SHOW
} PartialVisAction;

typedef enum {
	PARTIALVIS_INSIDE,
	PARTIALVIS_OUTSIDE,
	PARTIALVIS_ALL,
	PARTIALVIS_MASKED
} PartialVisArea;

void PAINT_OT_hide_show(struct wmOperatorType *ot);

/* paint_mask.c */

typedef enum {
	PAINT_MASK_FLOOD_VALUE,
	PAINT_MASK_INVERT
} PaintMaskFloodMode;

void PAINT_OT_mask_flood_fill(struct wmOperatorType *ot);

#endif /* __PAINT_INTERN_H__ */
