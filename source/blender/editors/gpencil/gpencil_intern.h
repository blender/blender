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

/** \file blender/editors/gpencil/gpencil_intern.h
 *  \ingroup edgpencil
 */

#ifndef __GPENCIL_INTERN_H__
#define __GPENCIL_INTERN_H__


#include "DNA_vec_types.h"

#include "ED_numinput.h"


/* internal exports only */
struct bGPdata;
struct bGPDstroke;
struct bGPDspoint;
struct tGPspoint;
struct Material;

struct GHash;
struct RNG;

struct Brush;
struct Scene;
struct ARegion;
struct View3D;
struct View2D;
struct wmOperatorType;

struct Depsgraph;

struct PointerRNA;
struct PropertyRNA;
struct EnumPropertyItem;


/* ***************************************************** */
/* Modal Operator Geometry Preview
 *
 * Several modal operators (Fill, Interpolate, Primitive)
 * need to run some drawing code to display previews, or
 * to perform screen-space/image-based analysis routines.
 * The following structs + function prototypes are used
 * by these operators so that the operator code
 * (in gpencil_<opname>.c) can communicate with the drawing
 * code (in drawgpencil.c).
 *
 * NOTE: All this is within the gpencil module, so nothing needs
 * to be exported to other modules.
 */

/* Internal Operator-State Data ------------------------ */

/* Temporary draw data (no draw manager mode) */
typedef struct tGPDdraw {
	struct RegionView3D *rv3d;          /* region to draw */
	struct Depsgraph *depsgraph;        /* depsgraph */
	struct Object *ob;                  /* GP object */
	struct bGPdata *gpd;                /* current GP datablock */
	struct bGPDlayer *gpl;              /* layer */
	struct bGPDframe *gpf;              /* frame */
	struct bGPDframe *t_gpf;            /* temporal frame */
	struct bGPDstroke *gps;             /* stroke */
	int disable_fill;                   /* disable fill */
	int offsx;                          /* windows offset x */
	int offsy;                          /* windows offset y */
	int winx;                           /* windows width */
	int winy;                           /* windows height */
	int dflag;                          /* flags datablock */
	short lthick;                       /* layer thickness */
	float opacity;                      /* opacity */
	float tintcolor[4];                 /* tint color */
	bool onion;                         /* onion flag */
	bool custonion;                     /* use custom onion colors */
	float diff_mat[4][4];               /* matrix */
} tGPDdraw;


/* Temporary interpolate operation data */
typedef struct tGPDinterpolate_layer {
	struct tGPDinterpolate_layer *next, *prev;

	struct bGPDlayer *gpl;            /* layer */
	struct bGPDframe *prevFrame;      /* frame before current frame (interpolate-from) */
	struct bGPDframe *nextFrame;      /* frame after current frame (interpolate-to) */
	struct bGPDframe *interFrame;     /* interpolated frame */
	float factor;                     /* interpolate factor */

} tGPDinterpolate_layer;

typedef struct tGPDinterpolate {
	struct Scene *scene;       /* current scene from context */
	struct ScrArea *sa;        /* area where painting originated */
	struct ARegion *ar;        /* region where painting originated */
	struct bGPdata *gpd;       /* current GP datablock */
	struct Material *mat;      /* current material */

	int cframe;                /* current frame number */
	ListBase ilayers;          /* (tGPDinterpolate_layer) layers to be interpolated */
	float shift;               /* value for determining the displacement influence */
	float init_factor;         /* initial interpolation factor for active layer */
	float low_limit;           /* shift low limit (-100%) */
	float high_limit;          /* shift upper limit (200%) */
	int flag;                  /* flag from toolsettings */

	NumInput num;              /* numeric input */
	void *draw_handle_3d;      /* handle for drawing strokes while operator is running 3d stuff */
	void *draw_handle_screen;  /* handle for drawing strokes while operator is running screen stuff */
} tGPDinterpolate;


/* Temporary primitive operation data */
typedef struct tGPDprimitive {
	struct Depsgraph *depsgraph;
	struct wmWindow *win;             /* window where painting originated */
	struct Scene *scene;              /* current scene from context */
	struct Object *ob;                /* current active gp object */
	struct ScrArea *sa;               /* area where painting originated */
	struct RegionView3D *rv3d;        /* region where painting originated */
	struct View3D *v3d;               /* view3d where painting originated */
	struct ARegion *ar;               /* region where painting originated */
	struct bGPdata *gpd;              /* current GP datablock */
	struct Material *mat;             /* current material */
	struct Brush *brush;              /* current brush */

	int cframe;                       /* current frame number */
	struct bGPDlayer *gpl;            /* layer */
	struct bGPDframe *gpf;            /* frame */
	int type;                         /* type of primitive */
	int tot_edges;                    /* number of polygon edges */
	int top[2];                       /* first box corner */
	int bottom[2];                    /* last box corner */
	int flag;                         /* flag to determine operations in progress */

	int lock_axis;                    /* lock to viewport axis */

	NumInput num;                     /* numeric input */
	void *draw_handle_3d;             /* handle for drawing strokes while operator is running 3d stuff */
} tGPDprimitive;


/* Modal Operator Drawing Callbacks ------------------------ */

void ED_gp_draw_interpolation(const struct bContext *C, struct tGPDinterpolate *tgpi, const int type);
void ED_gp_draw_primitives(const struct bContext *C, struct tGPDprimitive *tgpi, const int type);
void ED_gp_draw_fill(struct tGPDdraw *tgpw);

/* ***************************************************** */
/* Internal API */

/* Stroke Coordinates API ------------------------------ */
/* gpencil_utils.c */

typedef struct GP_SpaceConversion {
	struct bGPdata *gpd;
	struct bGPDlayer *gpl;

	struct ScrArea *sa;
	struct ARegion *ar;
	struct View2D *v2d;

	rctf *subrect;       /* for using the camera rect within the 3d view */
	rctf subrect_data;

	float mat[4][4];     /* transform matrix on the strokes (introduced in [b770964]) */
} GP_SpaceConversion;

bool gp_stroke_inside_circle(const int mval[2], const int UNUSED(mvalo[2]),
                             int rad, int x0, int y0, int x1, int y1);

void gp_point_conversion_init(struct bContext *C, GP_SpaceConversion *r_gsc);

void gp_point_to_xy(GP_SpaceConversion *settings, struct bGPDstroke *gps, struct bGPDspoint *pt,
                    int *r_x, int *r_y);

void gp_point_to_xy_fl(GP_SpaceConversion *gsc, bGPDstroke *gps, bGPDspoint *pt,
                       float *r_x, float *r_y);

void gp_point_to_parent_space(bGPDspoint *pt, float diff_mat[4][4], bGPDspoint *r_pt);
/**
 * Change points position relative to parent object
 */
void gp_apply_parent(struct Depsgraph *depsgraph, struct Object *obact, bGPdata *gpd, bGPDlayer *gpl, bGPDstroke *gps);
/**
 * Change point position relative to parent object
 */
void gp_apply_parent_point(struct Depsgraph *depsgraph, struct Object *obact, bGPdata *gpd, bGPDlayer *gpl, bGPDspoint *pt);

bool gp_point_xy_to_3d(GP_SpaceConversion *gsc, struct Scene *scene, const float screen_co[2], float r_out[3]);

/* helper to convert 2d to 3d */
void gp_stroke_convertcoords_tpoint(struct Scene *scene, struct ARegion *ar,
									struct View3D *v3d, struct Object *ob,
									bGPDlayer *gpl, const struct tGPspoint *point2D,
									float *depth, float out[3]);

/* Poll Callbacks ------------------------------------ */
/* gpencil_utils.c */

bool gp_add_poll(struct bContext *C);
bool gp_active_layer_poll(struct bContext *C);
bool gp_active_brush_poll(struct bContext *C);
bool gp_brush_crt_presets_poll(bContext *C);

/* Copy/Paste Buffer --------------------------------- */
/* gpencil_edit.c */

extern ListBase gp_strokes_copypastebuf;

/* Build a map for converting between old colornames and destination-color-refs */
struct GHash *gp_copybuf_validate_colormap(struct bContext *C);

/* Stroke Editing ------------------------------------ */

void gp_stroke_delete_tagged_points(bGPDframe *gpf, bGPDstroke *gps, bGPDstroke *next_stroke,
									int tag_flags, bool select);
int gp_delete_selected_point_wrap(bContext *C);

bool gp_smooth_stroke(bGPDstroke *gps, int i, float inf, bool affect_pressure);
bool gp_smooth_stroke_strength(bGPDstroke *gps, int i, float inf);
bool gp_smooth_stroke_thickness(bGPDstroke *gps, int i, float inf);
void gp_subdivide_stroke(bGPDstroke *gps, const int subdivide);
void gp_randomize_stroke(bGPDstroke *gps, Brush *brush, struct RNG *rng);

/* Layers Enums -------------------------------------- */

const struct EnumPropertyItem *ED_gpencil_layers_enum_itemf(
        struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop,
        bool *r_free);
const struct EnumPropertyItem *ED_gpencil_layers_with_new_enum_itemf(
        struct bContext *C, struct PointerRNA *ptr, struct PropertyRNA *prop,
        bool *r_free);

/* ***************************************************** */
/* Operator Defines */

/* annotations ------ */

void GPENCIL_OT_annotate(struct wmOperatorType *ot);


/* drawing ---------- */

void GPENCIL_OT_draw(struct wmOperatorType *ot);
void GPENCIL_OT_fill(struct wmOperatorType *ot);

/* Paint Modes for operator */
typedef enum eGPencil_PaintModes {
	GP_PAINTMODE_DRAW = 0,
	GP_PAINTMODE_ERASER,
	GP_PAINTMODE_DRAW_STRAIGHT,
	GP_PAINTMODE_DRAW_POLY
} eGPencil_PaintModes;

/* maximum sizes of gp-session buffer */
#define GP_STROKE_BUFFER_MAX    5000

/* stroke editing ----- */

void GPENCIL_OT_editmode_toggle(struct wmOperatorType *ot);
void GPENCIL_OT_paintmode_toggle(struct wmOperatorType *ot);
void GPENCIL_OT_sculptmode_toggle(struct wmOperatorType *ot);
void GPENCIL_OT_weightmode_toggle(struct wmOperatorType *ot);
void GPENCIL_OT_selection_opacity_toggle(struct wmOperatorType *ot);
void GPENCIL_OT_multiedit_toggle(struct wmOperatorType *ot);

void GPENCIL_OT_select(struct wmOperatorType *ot);
void GPENCIL_OT_select_all(struct wmOperatorType *ot);
void GPENCIL_OT_select_circle(struct wmOperatorType *ot);
void GPENCIL_OT_select_border(struct wmOperatorType *ot);
void GPENCIL_OT_select_lasso(struct wmOperatorType *ot);

void GPENCIL_OT_select_linked(struct wmOperatorType *ot);
void GPENCIL_OT_select_grouped(struct wmOperatorType *ot);
void GPENCIL_OT_select_more(struct wmOperatorType *ot);
void GPENCIL_OT_select_less(struct wmOperatorType *ot);
void GPENCIL_OT_select_first(struct wmOperatorType *ot);
void GPENCIL_OT_select_last(struct wmOperatorType *ot);
void GPENCIL_OT_select_alternate(struct wmOperatorType *ot);

void GPENCIL_OT_duplicate(struct wmOperatorType *ot);
void GPENCIL_OT_delete(struct wmOperatorType *ot);
void GPENCIL_OT_dissolve(struct wmOperatorType *ot);
void GPENCIL_OT_copy(struct wmOperatorType *ot);
void GPENCIL_OT_paste(struct wmOperatorType *ot);

void GPENCIL_OT_move_to_layer(struct wmOperatorType *ot);
void GPENCIL_OT_layer_change(struct wmOperatorType *ot);

void GPENCIL_OT_snap_to_grid(struct wmOperatorType *ot);
void GPENCIL_OT_snap_to_cursor(struct wmOperatorType *ot);
void GPENCIL_OT_snap_cursor_to_selected(struct wmOperatorType *ot);
void GPENCIL_OT_snap_cursor_to_center(struct wmOperatorType *ot);

void GPENCIL_OT_reproject(struct wmOperatorType *ot);

/* stroke sculpting -- */

void GPENCIL_OT_brush_paint(struct wmOperatorType *ot);

/* buttons editing --- */

void GPENCIL_OT_data_add(struct wmOperatorType *ot);
void GPENCIL_OT_data_unlink(struct wmOperatorType *ot);

void GPENCIL_OT_layer_add(struct wmOperatorType *ot);
void GPENCIL_OT_layer_remove(struct wmOperatorType *ot);
void GPENCIL_OT_layer_move(struct wmOperatorType *ot);
void GPENCIL_OT_layer_duplicate(struct wmOperatorType *ot);

void GPENCIL_OT_hide(struct wmOperatorType *ot);
void GPENCIL_OT_reveal(struct wmOperatorType *ot);

void GPENCIL_OT_lock_all(struct wmOperatorType *ot);
void GPENCIL_OT_unlock_all(struct wmOperatorType *ot);

void GPENCIL_OT_layer_isolate(struct wmOperatorType *ot);
void GPENCIL_OT_layer_merge(struct wmOperatorType *ot);

void GPENCIL_OT_blank_frame_add(struct wmOperatorType *ot);

void GPENCIL_OT_active_frame_delete(struct wmOperatorType *ot);
void GPENCIL_OT_active_frames_delete_all(struct wmOperatorType *ot);
void GPENCIL_OT_frame_duplicate(struct wmOperatorType *ot);
void GPENCIL_OT_frame_clean_fill(struct wmOperatorType *ot);

void GPENCIL_OT_convert(struct wmOperatorType *ot);

enum {
	GP_STROKE_JOIN = -1,
	GP_STROKE_JOINCOPY = 1
};

enum {
	GP_STROKE_BOX = -1,
	GP_STROKE_LINE = 1,
	GP_STROKE_CIRCLE = 2
};


void GPENCIL_OT_stroke_arrange(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_change_color(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_lock_color(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_apply_thickness(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_cyclical_set(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_join(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_flip(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_subdivide(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_simplify(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_simplify_fixed(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_separate(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_split(struct wmOperatorType *ot);

void GPENCIL_OT_brush_presets_create(struct wmOperatorType *ot);
void GPENCIL_OT_brush_select(struct wmOperatorType *ot);

void GPENCIL_OT_sculpt_select(struct wmOperatorType *ot);

/* undo stack ---------- */

void gpencil_undo_init(struct bGPdata *gpd);
void gpencil_undo_push(struct bGPdata *gpd);
void gpencil_undo_finish(void);

/* interpolation ---------- */

void GPENCIL_OT_interpolate(struct wmOperatorType *ot);
void GPENCIL_OT_interpolate_sequence(struct wmOperatorType *ot);
void GPENCIL_OT_interpolate_reverse(struct wmOperatorType *ot);

/* primitives ---------- */

void GPENCIL_OT_primitive(struct wmOperatorType *ot);

/* vertex groups ------------ */
void GPENCIL_OT_vertex_group_assign(struct wmOperatorType *ot);
void GPENCIL_OT_vertex_group_remove_from(struct wmOperatorType *ot);
void GPENCIL_OT_vertex_group_select(struct wmOperatorType *ot);
void GPENCIL_OT_vertex_group_deselect(struct wmOperatorType *ot);
void GPENCIL_OT_vertex_group_invert(struct wmOperatorType *ot);
void GPENCIL_OT_vertex_group_smooth(struct wmOperatorType *ot);

/* color handle */
void GPENCIL_OT_lock_layer(struct wmOperatorType *ot);
void GPENCIL_OT_color_isolate(struct wmOperatorType *ot);
void GPENCIL_OT_color_hide(struct wmOperatorType *ot);
void GPENCIL_OT_color_reveal(struct wmOperatorType *ot);
void GPENCIL_OT_color_lock_all(struct wmOperatorType *ot);
void GPENCIL_OT_color_unlock_all(struct wmOperatorType *ot);
void GPENCIL_OT_color_select(struct wmOperatorType *ot);

/* convert old 2.7 files to 2.8 */
void GPENCIL_OT_convert_old_files(struct wmOperatorType *ot);

/* ****************************************************** */
/* FILTERED ACTION DATA - TYPES  ---> XXX DEPRECEATED OLD ANIM SYSTEM CODE! */

/* XXX - TODO: replace this with the modern bAnimListElem... */
/* This struct defines a structure used for quick access */
typedef struct bActListElem {
	struct bActListElem *next, *prev;

	void *data;   /* source data this elem represents */
	int   type;   /* one of the ACTTYPE_* values */
	int   flag;   /* copy of elem's flags for quick access */
	int   index;  /* copy of adrcode where applicable */

	void  *key_data;  /* motion data - ipo or ipo-curve */
	short  datatype;  /* type of motion data to expect */

	struct bActionGroup *grp;   /* action group that owns the channel */

	void  *owner;      /* will either be an action channel or fake ipo-channel (for keys) */
	short  ownertype;  /* type of owner */
} bActListElem;

/* ****************************************************** */
/* FILTER ACTION DATA - METHODS/TYPES */

/* filtering flags  - under what circumstances should a channel be added */
typedef enum ACTFILTER_FLAGS {
	ACTFILTER_VISIBLE       = (1 << 0),   /* should channels be visible */
	ACTFILTER_SEL           = (1 << 1),   /* should channels be selected */
	ACTFILTER_FOREDIT       = (1 << 2),   /* does editable status matter */
	ACTFILTER_CHANNELS      = (1 << 3),   /* do we only care that it is a channel */
	ACTFILTER_IPOKEYS       = (1 << 4),   /* only channels referencing ipo's */
	ACTFILTER_ONLYICU       = (1 << 5),   /* only reference ipo-curves */
	ACTFILTER_FORDRAWING    = (1 << 6),   /* make list for interface drawing */
	ACTFILTER_ACTGROUPED    = (1 << 7)    /* belongs to the active group */
} ACTFILTER_FLAGS;

/* Action Editor - Main Data types */
typedef enum ACTCONT_TYPES {
	ACTCONT_NONE = 0,
	ACTCONT_ACTION,
	ACTCONT_SHAPEKEY,
	ACTCONT_GPENCIL
} ACTCONT_TYPES;

/* ****************************************************** */
/* Stroke Iteration Utilities */

/**
* Iterate over all editable strokes in the current context,
* stopping on each usable layer + stroke pair (i.e. gpl and gps)
* to perform some operations on the stroke.
*
* \param gpl  The identifier to use for the layer of the stroke being processed.
*                    Choose a suitable value to avoid name clashes.
* \param gps The identifier to use for current stroke being processed.
*                    Choose a suitable value to avoid name clashes.
*/
#define GP_EDITABLE_STROKES_BEGIN(C, gpl, gps)                                          \
{                                                                                       \
	Depsgraph *depsgraph_ = CTX_data_depsgraph(C);                                      \
	Object *obact_ = CTX_data_active_object(C);                                          \
	bGPdata *gpd_ = CTX_data_gpencil_data(C);                                            \
	bool is_multiedit = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd_);                       \
	CTX_DATA_BEGIN(C, bGPDlayer*, gpl, editable_gpencil_layers)                         \
	{                                                                                   \
		bGPDframe *init_gpf = gpl->actframe;                                                \
		if (is_multiedit) {                                                                 \
			init_gpf = gpl->frames.first;                                                   \
		}                                                                                   \
		for (bGPDframe *gpf = init_gpf; gpf; gpf = gpf->next) {                        \
			if ((gpf == gpl->actframe) || ((gpf->flag & GP_FRAME_SELECT) && (is_multiedit))) {  \
				/* calculate difference matrix */                                               \
				float diff_mat[4][4];                                                           \
				ED_gpencil_parent_location(depsgraph_, obact_, gpd_, gpl, diff_mat);            \
				/* loop over strokes */                                                         \
				for (bGPDstroke *gps = gpf->strokes.first; gps; gps = gps->next) {    \
					/* skip strokes that are invalid for current view */                        \
					if (ED_gpencil_stroke_can_use(C, gps) == false)                             \
						continue;                                                               \
					/* check if the color is editable */                                        \
					if (ED_gpencil_stroke_color_use(obact_, gpl, gps) == false)                         \
						continue;                                                               \
					/* ... Do Stuff With Strokes ...  */

#define GP_EDITABLE_STROKES_END    \
				}                  \
			}                      \
			if (!is_multiedit) {   \
				break;             \
			}                      \
		}                          \
	}                              \
	CTX_DATA_END;                  \
} (void)0

/* ****************************************************** */

#endif /* __GPENCIL_INTERN_H__ */
