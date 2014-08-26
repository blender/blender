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
struct ImagePool;
struct ColorSpace;
struct ColorManagedDisplay;
struct ListBase;
struct Material;
struct Mesh;
struct MTex;
struct Object;
struct PaintStroke;
struct Paint;
struct PointerRNA;
struct rcti;
struct Scene;
struct RegionView3D;
struct VPaint;
struct ViewContext;
struct wmEvent;
struct wmOperator;
struct wmOperatorType;
struct ImagePaintState;
struct wmWindowManager;
struct DMCoNo;
enum PaintMode;

/* paint_stroke.c */
typedef bool (*StrokeGetLocation)(struct bContext *C, float location[3], const float mouse[2]);
typedef bool (*StrokeTestStart)(struct bContext *C, struct wmOperator *op, const float mouse[2]);
typedef void (*StrokeUpdateStep)(struct bContext *C, struct PaintStroke *stroke, struct PointerRNA *itemptr);
typedef void (*StrokeRedraw)(const struct bContext *C, struct PaintStroke *stroke, bool final);
typedef void (*StrokeDone)(const struct bContext *C, struct PaintStroke *stroke);

struct PaintStroke *paint_stroke_new(struct bContext *C, struct wmOperator *op,
                                     StrokeGetLocation get_location, StrokeTestStart test_start,
                                     StrokeUpdateStep update_step, StrokeRedraw redraw,
                                     StrokeDone done, int event_type);
void paint_stroke_data_free(struct wmOperator *op);

bool paint_space_stroke_enabled(struct Brush *br, enum PaintMode mode);
bool paint_supports_dynamic_size(struct Brush *br, enum PaintMode mode);
bool paint_supports_dynamic_tex_coords(struct Brush *br, enum PaintMode mode);
bool paint_supports_smooth_stroke(struct Brush *br, enum PaintMode mode);
bool paint_supports_texture(enum PaintMode mode);
bool paint_supports_jitter(enum PaintMode mode);

struct wmKeyMap *paint_stroke_modal_keymap(struct wmKeyConfig *keyconf);
int paint_stroke_modal(struct bContext *C, struct wmOperator *op, const struct wmEvent *event);
int paint_stroke_exec(struct bContext *C, struct wmOperator *op);
void paint_stroke_cancel(struct bContext *C, struct wmOperator *op);
bool paint_stroke_flipped(struct PaintStroke *stroke);
struct ViewContext *paint_stroke_view_context(struct PaintStroke *stroke);
void *paint_stroke_mode_data(struct PaintStroke *stroke);
float paint_stroke_distance_get(struct PaintStroke *stroke);
void paint_stroke_set_mode_data(struct PaintStroke *stroke, void *mode_data);
int paint_poll(struct bContext *C);
void paint_cursor_start(struct bContext *C, int (*poll)(struct bContext *C));
void paint_cursor_start_explicit(struct Paint *p, struct wmWindowManager *wm, int (*poll)(struct bContext *C));
void paint_cursor_delete_textures(void);

/* paint_vertex.c */
int weight_paint_poll(struct bContext *C);
int weight_paint_mode_poll(struct bContext *C);
int vertex_paint_poll(struct bContext *C);
int vertex_paint_mode_poll(struct bContext *C);

bool ED_vpaint_fill(struct Object *ob, unsigned int paintcol);
bool ED_wpaint_fill(struct VPaint *wp, struct Object *ob, float paintweight);

bool ED_vpaint_smooth(struct Object *ob);

void PAINT_OT_weight_paint_toggle(struct wmOperatorType *ot);
void PAINT_OT_weight_paint(struct wmOperatorType *ot);
void PAINT_OT_weight_set(struct wmOperatorType *ot);
void PAINT_OT_weight_from_bones(struct wmOperatorType *ot);
void PAINT_OT_weight_sample(struct wmOperatorType *ot);
void PAINT_OT_weight_sample_group(struct wmOperatorType *ot);

enum {
	WPAINT_GRADIENT_TYPE_LINEAR,
	WPAINT_GRADIENT_TYPE_RADIAL
};
void PAINT_OT_weight_gradient(struct wmOperatorType *ot);

void PAINT_OT_vertex_paint_toggle(struct wmOperatorType *ot);
void PAINT_OT_vertex_paint(struct wmOperatorType *ot);

unsigned int vpaint_get_current_col(struct Scene *scene, struct VPaint *vp);


/* paint_vertex_proj.c */
struct VertProjHandle;
struct VertProjHandle *ED_vpaint_proj_handle_create(
        struct Scene *scene, struct Object *ob,
        struct DMCoNo **r_vcosnos);
void  ED_vpaint_proj_handle_update(
        struct VertProjHandle *vp_handle,
        /* runtime vars */
        struct ARegion *ar, const float mval_fl[2]);
void  ED_vpaint_proj_handle_free(
        struct VertProjHandle *vp_handle);


/* paint_image.c */
typedef struct ImagePaintPartialRedraw {
	int x1, y1, x2, y2;  /* XXX, could use 'rcti' */
	int enabled;
} ImagePaintPartialRedraw;

#define IMAPAINT_TILE_BITS          6
#define IMAPAINT_TILE_SIZE          (1 << IMAPAINT_TILE_BITS)
#define IMAPAINT_TILE_NUMBER(size)  (((size) + IMAPAINT_TILE_SIZE - 1) >> IMAPAINT_TILE_BITS)

int image_texture_paint_poll(struct bContext *C);
void *image_undo_find_tile(struct Image *ima, struct ImBuf *ibuf, int x_tile, int y_tile, unsigned short **mask, bool validate);
void *image_undo_push_tile(struct Image *ima, struct ImBuf *ibuf, struct ImBuf **tmpibuf, int x_tile, int y_tile,  unsigned short **, bool **valid, bool proj);
void image_undo_remove_masks(void);
void image_undo_init_locks(void);
void image_undo_end_locks(void);

void imapaint_image_update(struct SpaceImage *sima, struct Image *image, struct ImBuf *ibuf, short texpaint);
struct ImagePaintPartialRedraw *get_imapaintpartial(void);
void set_imapaintpartial(struct ImagePaintPartialRedraw *ippr);
void imapaint_region_tiles(struct ImBuf *ibuf, int x, int y, int w, int h, int *tx, int *ty, int *tw, int *th);
int get_imapaint_zoom(struct bContext *C, float *zoomx, float *zoomy);
void *paint_2d_new_stroke(struct bContext *, struct wmOperator *, int mode);
void paint_2d_redraw(const bContext *C, void *ps, bool final);
void paint_2d_stroke_done(void *ps);
void paint_2d_stroke(void *ps, const float prev_mval[2], const float mval[2], const bool eraser, float pressure, float distance, float size);
void paint_2d_bucket_fill(const struct bContext *C, const float color[3], struct Brush *br, const float mouse_init[2], void *ps);
void paint_2d_gradient_fill (const struct bContext *C, struct Brush *br, const float mouse_init[2], const float mouse_final[2], void *ps);
void *paint_proj_new_stroke(struct bContext *C, struct Object *ob, const float mouse[2], int mode);
void paint_proj_stroke(const struct bContext *C, void *ps, const float prevmval_i[2], const float mval_i[2], const bool eraser, float pressure, float distance, float size);
void paint_proj_redraw(const struct bContext *C, void *pps, bool final);
void paint_proj_stroke_done(void *ps);
void paint_proj_mesh_data_ensure(bContext *C, struct Object *ob, struct wmOperator *op);
bool proj_paint_add_slot(bContext *C, struct Material *ma, struct wmOperator *op);

void paint_brush_color_get(struct Scene *scene, struct Brush *br, bool color_correction, bool invert, float distance, float pressure, float color[3], struct ColorManagedDisplay *display);
bool paint_use_opacity_masking(struct Brush *brush);
void paint_brush_init_tex(struct Brush *brush);
void paint_brush_exit_tex(struct Brush *brush);

void PAINT_OT_grab_clone(struct wmOperatorType *ot);
void PAINT_OT_sample_color(struct wmOperatorType *ot);
void PAINT_OT_brush_colors_flip(struct wmOperatorType *ot);
void PAINT_OT_texture_paint_toggle(struct wmOperatorType *ot);
void PAINT_OT_project_image(struct wmOperatorType *ot);
void PAINT_OT_image_from_view(struct wmOperatorType *ot);
void PAINT_OT_add_texture_paint_slot(struct wmOperatorType *ot);
void PAINT_OT_delete_texture_paint_slot(struct wmOperatorType *ot);
void PAINT_OT_image_paint(struct wmOperatorType *ot);

/* uv sculpting */
int uv_sculpt_poll(struct bContext *C);

void SCULPT_OT_uv_sculpt_stroke(struct wmOperatorType *ot);

/* paint_utils.c */

/* Convert the object-space axis-aligned bounding box (expressed as
 * its minimum and maximum corners) into a screen-space rectangle,
 * returns zero if the result is empty */
bool paint_convert_bb_to_rect(struct rcti *rect,
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

float paint_calc_object_space_radius(struct ViewContext *vc, const float center[3], float pixel_radius);
float paint_get_tex_pixel(struct MTex *mtex, float u, float v, struct ImagePool *pool, int thread);
void paint_get_tex_pixel_col(struct MTex *mtex, float u, float v, float rgba[4], struct ImagePool *pool, int thread, bool convert, struct ColorSpace *colorspace);

void paint_sample_color(bContext *C, struct ARegion *ar, int x, int y, bool texpaint_proj, bool palette);
void BRUSH_OT_curve_preset(struct wmOperatorType *ot);

void PAINT_OT_face_select_linked(struct wmOperatorType *ot);
void PAINT_OT_face_select_linked_pick(struct wmOperatorType *ot);
void PAINT_OT_face_select_all(struct wmOperatorType *ot);
void PAINT_OT_face_select_hide(struct wmOperatorType *ot);
void PAINT_OT_face_select_reveal(struct wmOperatorType *ot);

void PAINT_OT_vert_select_all(struct wmOperatorType *ot);
void PAINT_OT_vert_select_ungrouped(struct wmOperatorType *ot);

int vert_paint_poll(struct bContext *C);
int mask_paint_poll(struct bContext *C);
int paint_curve_poll(struct bContext *C);

int facemask_paint_poll(struct bContext *C);
void flip_v3_v3(float out[3], const float in[3], const char symm);

/* stroke operator */
typedef enum BrushStrokeMode {
	BRUSH_STROKE_NORMAL,
	BRUSH_STROKE_INVERT,
	BRUSH_STROKE_SMOOTH
} BrushStrokeMode;

/* paint_undo.c */
struct ListBase *undo_paint_push_get_list(int type);
void undo_paint_push_count_alloc(int type, int size);

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
	PAINT_MASK_FLOOD_VALUE_INVERSE,
	PAINT_MASK_INVERT
} PaintMaskFloodMode;

void PAINT_OT_mask_flood_fill(struct wmOperatorType *ot);
void PAINT_OT_mask_lasso_gesture(struct wmOperatorType *ot);

/* paint_curve.c */
void PAINTCURVE_OT_new(struct wmOperatorType *ot);
void PAINTCURVE_OT_add_point(struct wmOperatorType *ot);
void PAINTCURVE_OT_delete_point(struct wmOperatorType *ot);
void PAINTCURVE_OT_select(struct wmOperatorType *ot);
void PAINTCURVE_OT_slide(struct wmOperatorType *ot);
void PAINTCURVE_OT_draw(struct wmOperatorType *ot);
void PAINTCURVE_OT_cursor(struct wmOperatorType *ot);

/* image painting blur kernel */
typedef struct {
	float *wdata; /* actual kernel */
	int side; /* kernel side */
	int side_squared; /* data side */
	float pixel_len; /* pixels around center that kernel is wide */
} BlurKernel;

enum BlurKernelType;
/* can be extended to other blur kernels later */
BlurKernel *paint_new_blur_kernel(struct Brush *br);
void paint_delete_blur_kernel(BlurKernel *);

/* paint curve defines */
#define PAINT_CURVE_NUM_SEGMENTS 40

#endif /* __PAINT_INTERN_H__ */
