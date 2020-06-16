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
 * \ingroup edgpencil
 */

#ifndef __GPENCIL_INTERN_H__
#define __GPENCIL_INTERN_H__

#include "DNA_vec_types.h"

#include "ED_numinput.h"

/* internal exports only */
struct Material;
struct bGPDspoint;
struct bGPDstroke;
struct bGPdata;
struct tGPspoint;

struct GHash;
struct RNG;

struct ARegion;
struct Brush;
struct Scene;
struct View2D;
struct View3D;
struct wmOperatorType;

struct Depsgraph;

struct EnumPropertyItem;
struct PointerRNA;
struct PropertyRNA;

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

/** Random settings by stroke */
typedef struct GpRandomSettings {
  /** Pressure used for evaluated curves. */
  float pen_press;

  float hsv[3];
  float pressure;
  float strength;
  float uv;
} GpRandomSettings;

/* Temporary draw data (no draw manager mode) */
typedef struct tGPDdraw {
  struct RegionView3D *rv3d;   /* region to draw */
  struct Depsgraph *depsgraph; /* depsgraph */
  struct Object *ob;           /* GP object */
  struct bGPdata *gpd;         /* current GP datablock */
  struct bGPDlayer *gpl;       /* layer */
  struct bGPDframe *gpf;       /* frame */
  struct bGPDframe *t_gpf;     /* temporal frame */
  struct bGPDstroke *gps;      /* stroke */
  int disable_fill;            /* disable fill */
  int offsx;                   /* windows offset x */
  int offsy;                   /* windows offset y */
  int winx;                    /* windows width */
  int winy;                    /* windows height */
  int dflag;                   /* flags datablock */
  short lthick;                /* layer thickness */
  float opacity;               /* opacity */
  float tintcolor[4];          /* tint color */
  bool onion;                  /* onion flag */
  bool custonion;              /* use custom onion colors */
  bool is_fill_stroke;         /* use fill tool */
  float diff_mat[4][4];        /* matrix */
} tGPDdraw;

/* Temporary interpolate operation data */
typedef struct tGPDinterpolate_layer {
  struct tGPDinterpolate_layer *next, *prev;

  /** layer */
  struct bGPDlayer *gpl;
  /** frame before current frame (interpolate-from) */
  struct bGPDframe *prevFrame;
  /** frame after current frame (interpolate-to) */
  struct bGPDframe *nextFrame;
  /** interpolated frame */
  struct bGPDframe *interFrame;
  /** interpolate factor */
  float factor;

} tGPDinterpolate_layer;

typedef struct tGPDinterpolate {
  /** Current depsgraph from context */
  struct Depsgraph *depsgraph;
  /** current scene from context */
  struct Scene *scene;
  /** area where painting originated */
  struct ScrArea *area;
  /** region where painting originated */
  struct ARegion *region;
  /** current GP datablock */
  struct bGPdata *gpd;
  /** current material */
  struct Material *mat;

  /** current frame number */
  int cframe;
  /** (tGPDinterpolate_layer) layers to be interpolated */
  ListBase ilayers;
  /** value for determining the displacement influence */
  float shift;
  /** initial interpolation factor for active layer */
  float init_factor;
  /** shift low limit (-100%) */
  float low_limit;
  /** shift upper limit (200%) */
  float high_limit;
  /** flag from toolsettings */
  int flag;

  NumInput num; /* numeric input */
} tGPDinterpolate;

/* Temporary primitive operation data */
typedef struct tGPDprimitive {
  /** main database pointer */
  struct Main *bmain;
  struct Depsgraph *depsgraph;
  /** window where painting originated */
  struct wmWindow *win;
  /** current scene from context */
  struct Scene *scene;
  /** current active gp object */
  struct Object *ob;
  /** current evaluated gp object */
  struct Object *ob_eval;
  /** area where painting originated */
  struct ScrArea *area;
  /** region where painting originated */
  struct RegionView3D *rv3d;
  /** view3d where painting originated */
  struct View3D *v3d;
  /** region where painting originated */
  struct ARegion *region;
  /** current GP datablock */
  struct bGPdata *gpd;
  /** current material */
  struct Material *material;
  /** current brush */
  struct Brush *brush;

  /** current frame number */
  int cframe;
  /** layer */
  struct bGPDlayer *gpl;
  /** frame */
  struct bGPDframe *gpf;
  /** type of primitive */
  int type;
  /** original type of primitive */
  int orign_type;
  /** type of primitive is a curve */
  bool curve;
  /** brush size */
  int brush_size;
  /** brush strength */
  float brush_strength;
  /** flip option */
  short flip;
  /** array of data-points for stroke */
  tGPspoint *points;
  /** number of edges allocated */
  int point_count;
  /** stored number of polygon edges */
  int tot_stored_edges;
  /** number of polygon edges */
  int tot_edges;
  /** move distance */
  float move[2];
  /** initial box corner */
  float origin[2];
  /** first box corner */
  float start[2];
  /** last box corner */
  float end[2];
  /** midpoint box corner */
  float midpoint[2];
  /** first control point */
  float cp1[2];
  /** second control point */
  float cp2[2];
  /** flag to determine control point is selected */
  int sel_cp;
  /** flag to determine operations in progress */
  int flag;
  /** flag to determine operations previous mode */
  int prev_flag;
  /** recorded mouse-position */
  float mval[2];
  /** previous recorded mouse-position */
  float mvalo[2];

  /** lock to viewport axis */
  int lock_axis;
  struct RNG *rng;

  /** numeric input */
  NumInput num;

  /** size in pixels for uv calculation */
  float totpixlen;

  /** Random settings by stroke */
  GpRandomSettings random_settings;

} tGPDprimitive;

/* Modal Operator Drawing Callbacks ------------------------ */
void ED_gp_draw_fill(struct tGPDdraw *tgpw);

/* ***************************************************** */
/* Internal API */

/* Stroke Coordinates API ------------------------------ */
/* gpencil_utils.c */

typedef struct GP_SpaceConversion {
  struct Scene *scene;
  struct Object *ob;
  struct bGPdata *gpd;
  struct bGPDlayer *gpl;

  struct ScrArea *area;
  struct ARegion *region;
  struct View2D *v2d;

  rctf *subrect; /* for using the camera rect within the 3d view */
  rctf subrect_data;

  float mat[4][4]; /* transform matrix on the strokes (introduced in [b770964]) */
} GP_SpaceConversion;

bool gp_stroke_inside_circle(const float mval[2], int rad, int x0, int y0, int x1, int y1);

void gp_point_conversion_init(struct bContext *C, GP_SpaceConversion *r_gsc);

void gp_point_to_xy(const GP_SpaceConversion *gsc,
                    const struct bGPDstroke *gps,
                    const struct bGPDspoint *pt,
                    int *r_x,
                    int *r_y);

void gp_point_to_xy_fl(const GP_SpaceConversion *gsc,
                       const bGPDstroke *gps,
                       const bGPDspoint *pt,
                       float *r_x,
                       float *r_y);

void gp_point_to_parent_space(const bGPDspoint *pt, const float diff_mat[4][4], bGPDspoint *r_pt);
/**
 * Change points position relative to parent object
 */
void gp_apply_parent(struct Depsgraph *depsgraph,
                     struct Object *obact,
                     bGPDlayer *gpl,
                     bGPDstroke *gps);
/**
 * Change point position relative to parent object
 */
void gp_apply_parent_point(struct Depsgraph *depsgraph,
                           struct Object *obact,
                           bGPDlayer *gpl,
                           bGPDspoint *pt);

void gp_point_3d_to_xy(const GP_SpaceConversion *gsc,
                       const short flag,
                       const float pt[3],
                       float xy[2]);

bool gp_point_xy_to_3d(const GP_SpaceConversion *gsc,
                       struct Scene *scene,
                       const float screen_co[2],
                       float r_out[3]);

/* helper to convert 2d to 3d */
void gp_stroke_convertcoords_tpoint(struct Scene *scene,
                                    struct ARegion *region,
                                    struct Object *ob,
                                    const struct tGPspoint *point2D,
                                    float *depth,
                                    float out[3]);

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

void gp_stroke_delete_tagged_points(bGPDframe *gpf,
                                    bGPDstroke *gps,
                                    bGPDstroke *next_stroke,
                                    int tag_flags,
                                    bool select,
                                    int limit);
int gp_delete_selected_point_wrap(bContext *C);

void gp_subdivide_stroke(bGPDstroke *gps, const int subdivide);

/* Layers Enums -------------------------------------- */

const struct EnumPropertyItem *ED_gpencil_layers_enum_itemf(struct bContext *C,
                                                            struct PointerRNA *ptr,
                                                            struct PropertyRNA *prop,
                                                            bool *r_free);
const struct EnumPropertyItem *ED_gpencil_layers_with_new_enum_itemf(struct bContext *C,
                                                                     struct PointerRNA *ptr,
                                                                     struct PropertyRNA *prop,
                                                                     bool *r_free);
const struct EnumPropertyItem *ED_gpencil_material_enum_itemf(struct bContext *C,
                                                              struct PointerRNA *ptr,
                                                              struct PropertyRNA *prop,
                                                              bool *r_free);

/* ***************************************************** */
/* Operator Defines */

/* annotations ------ */

void GPENCIL_OT_annotate(struct wmOperatorType *ot);

/* drawing ---------- */

void GPENCIL_OT_draw(struct wmOperatorType *ot);
void GPENCIL_OT_fill(struct wmOperatorType *ot);

/* Vertex Paint. */
void GPENCIL_OT_vertex_paint(struct wmOperatorType *ot);
void GPENCIL_OT_vertex_color_brightness_contrast(struct wmOperatorType *ot);
void GPENCIL_OT_vertex_color_hsv(struct wmOperatorType *ot);
void GPENCIL_OT_vertex_color_invert(struct wmOperatorType *ot);
void GPENCIL_OT_vertex_color_levels(struct wmOperatorType *ot);
void GPENCIL_OT_vertex_color_set(struct wmOperatorType *ot);

/* Guides ----------------------- */

void GPENCIL_OT_guide_rotate(struct wmOperatorType *ot);

/* Paint Modes for operator */
typedef enum eGPencil_PaintModes {
  GP_PAINTMODE_DRAW = 0,
  GP_PAINTMODE_ERASER,
  GP_PAINTMODE_DRAW_STRAIGHT,
  GP_PAINTMODE_DRAW_POLY,
  GP_PAINTMODE_SET_CP,
} eGPencil_PaintModes;

/* chunk size for gp-session buffer (the total size is a multiple of this number) */
#define GP_STROKE_BUFFER_CHUNK 2048

/* stroke editing ----- */

void GPENCIL_OT_editmode_toggle(struct wmOperatorType *ot);
void GPENCIL_OT_selectmode_toggle(struct wmOperatorType *ot);
void GPENCIL_OT_paintmode_toggle(struct wmOperatorType *ot);
void GPENCIL_OT_sculptmode_toggle(struct wmOperatorType *ot);
void GPENCIL_OT_weightmode_toggle(struct wmOperatorType *ot);
void GPENCIL_OT_vertexmode_toggle(struct wmOperatorType *ot);
void GPENCIL_OT_selection_opacity_toggle(struct wmOperatorType *ot);

void GPENCIL_OT_select(struct wmOperatorType *ot);
void GPENCIL_OT_select_all(struct wmOperatorType *ot);
void GPENCIL_OT_select_circle(struct wmOperatorType *ot);
void GPENCIL_OT_select_box(struct wmOperatorType *ot);
void GPENCIL_OT_select_lasso(struct wmOperatorType *ot);

void GPENCIL_OT_select_linked(struct wmOperatorType *ot);
void GPENCIL_OT_select_grouped(struct wmOperatorType *ot);
void GPENCIL_OT_select_more(struct wmOperatorType *ot);
void GPENCIL_OT_select_less(struct wmOperatorType *ot);
void GPENCIL_OT_select_first(struct wmOperatorType *ot);
void GPENCIL_OT_select_last(struct wmOperatorType *ot);
void GPENCIL_OT_select_alternate(struct wmOperatorType *ot);
void GPENCIL_OT_select_vertex_color(struct wmOperatorType *ot);

void GPENCIL_OT_duplicate(struct wmOperatorType *ot);
void GPENCIL_OT_delete(struct wmOperatorType *ot);
void GPENCIL_OT_dissolve(struct wmOperatorType *ot);
void GPENCIL_OT_copy(struct wmOperatorType *ot);
void GPENCIL_OT_paste(struct wmOperatorType *ot);
void GPENCIL_OT_extrude(struct wmOperatorType *ot);

void GPENCIL_OT_move_to_layer(struct wmOperatorType *ot);
void GPENCIL_OT_layer_change(struct wmOperatorType *ot);
void GPENCIL_OT_layer_active(struct wmOperatorType *ot);

void GPENCIL_OT_snap_to_grid(struct wmOperatorType *ot);
void GPENCIL_OT_snap_to_cursor(struct wmOperatorType *ot);
void GPENCIL_OT_snap_cursor_to_selected(struct wmOperatorType *ot);

void GPENCIL_OT_reproject(struct wmOperatorType *ot);
void GPENCIL_OT_recalc_geometry(struct wmOperatorType *ot);

/* stroke sculpting -- */

void GPENCIL_OT_sculpt_paint(struct wmOperatorType *ot);
void GPENCIL_OT_weight_paint(struct wmOperatorType *ot);

/* buttons editing --- */

void GPENCIL_OT_annotation_add(struct wmOperatorType *ot);
void GPENCIL_OT_data_unlink(struct wmOperatorType *ot);

void GPENCIL_OT_layer_add(struct wmOperatorType *ot);
void GPENCIL_OT_layer_remove(struct wmOperatorType *ot);
void GPENCIL_OT_layer_move(struct wmOperatorType *ot);
void GPENCIL_OT_layer_annotation_add(struct wmOperatorType *ot);
void GPENCIL_OT_layer_annotation_remove(struct wmOperatorType *ot);
void GPENCIL_OT_layer_annotation_move(struct wmOperatorType *ot);
void GPENCIL_OT_layer_duplicate(struct wmOperatorType *ot);
void GPENCIL_OT_layer_duplicate_object(struct wmOperatorType *ot);

void GPENCIL_OT_layer_mask_add(struct wmOperatorType *ot);
void GPENCIL_OT_layer_mask_remove(struct wmOperatorType *ot);

void GPENCIL_OT_hide(struct wmOperatorType *ot);
void GPENCIL_OT_reveal(struct wmOperatorType *ot);

void GPENCIL_OT_lock_all(struct wmOperatorType *ot);
void GPENCIL_OT_unlock_all(struct wmOperatorType *ot);

void GPENCIL_OT_layer_isolate(struct wmOperatorType *ot);
void GPENCIL_OT_layer_merge(struct wmOperatorType *ot);

void GPENCIL_OT_blank_frame_add(struct wmOperatorType *ot);

void GPENCIL_OT_active_frame_delete(struct wmOperatorType *ot);
void GPENCIL_OT_annotation_active_frame_delete(struct wmOperatorType *ot);
void GPENCIL_OT_active_frames_delete_all(struct wmOperatorType *ot);
void GPENCIL_OT_frame_duplicate(struct wmOperatorType *ot);
void GPENCIL_OT_frame_clean_fill(struct wmOperatorType *ot);
void GPENCIL_OT_frame_clean_loose(struct wmOperatorType *ot);

void GPENCIL_OT_convert(struct wmOperatorType *ot);
void GPENCIL_OT_bake_mesh_animation(struct wmOperatorType *ot);

void GPENCIL_OT_image_to_grease_pencil(struct wmOperatorType *ot);

enum {
  GP_STROKE_JOIN = -1,
  GP_STROKE_JOINCOPY = 1,
};

enum {
  GP_STROKE_BOX = -1,
  GP_STROKE_LINE = 1,
  GP_STROKE_CIRCLE = 2,
  GP_STROKE_ARC = 3,
  GP_STROKE_CURVE = 4,
  GP_STROKE_POLYLINE = 5,
};

enum {
  GP_MERGE_STROKE = -1,
  GP_MERGE_POINT = 1,
};

void GPENCIL_OT_stroke_arrange(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_change_color(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_apply_thickness(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_cyclical_set(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_caps_set(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_join(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_flip(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_subdivide(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_simplify(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_simplify_fixed(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_separate(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_split(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_smooth(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_sample(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_merge(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_cutter(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_trim(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_merge_by_distance(struct wmOperatorType *ot);
void GPENCIL_OT_stroke_merge_material(struct wmOperatorType *ot);

void GPENCIL_OT_material_to_vertex_color(struct wmOperatorType *ot);
void GPENCIL_OT_extract_palette_vertex(struct wmOperatorType *ot);

void GPENCIL_OT_transform_fill(struct wmOperatorType *ot);
void GPENCIL_OT_reset_transform_fill(struct wmOperatorType *ot);

void GPENCIL_OT_brush_reset(struct wmOperatorType *ot);
void GPENCIL_OT_brush_reset_all(struct wmOperatorType *ot);

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
void GPENCIL_OT_vertex_group_normalize(struct wmOperatorType *ot);
void GPENCIL_OT_vertex_group_normalize_all(struct wmOperatorType *ot);

/* color handle */
void GPENCIL_OT_lock_layer(struct wmOperatorType *ot);
void GPENCIL_OT_material_isolate(struct wmOperatorType *ot);
void GPENCIL_OT_material_hide(struct wmOperatorType *ot);
void GPENCIL_OT_material_reveal(struct wmOperatorType *ot);
void GPENCIL_OT_material_lock_all(struct wmOperatorType *ot);
void GPENCIL_OT_material_unlock_all(struct wmOperatorType *ot);
void GPENCIL_OT_material_lock_unused(struct wmOperatorType *ot);
void GPENCIL_OT_material_select(struct wmOperatorType *ot);
void GPENCIL_OT_material_set(struct wmOperatorType *ot);
void GPENCIL_OT_set_active_material(struct wmOperatorType *ot);

/* convert old 2.7 files to 2.8 */
void GPENCIL_OT_convert_old_files(struct wmOperatorType *ot);

/* armatures */
void GPENCIL_OT_generate_weights(struct wmOperatorType *ot);

/* ****************************************************** */
/* FILTERED ACTION DATA - TYPES  ---> XXX DEPRECATED OLD ANIM SYSTEM CODE! */

/* XXX - TODO: replace this with the modern bAnimListElem... */
/* This struct defines a structure used for quick access */
typedef struct bActListElem {
  struct bActListElem *next, *prev;

  void *data; /* source data this elem represents */
  int type;   /* one of the ACTTYPE_* values */
  int flag;   /* copy of elem's flags for quick access */
  int index;  /* copy of adrcode where applicable */

  void *key_data; /* motion data - ipo or ipo-curve */
  short datatype; /* type of motion data to expect */

  struct bActionGroup *grp; /* action group that owns the channel */

  void *owner;     /* will either be an action channel or fake ipo-channel (for keys) */
  short ownertype; /* type of owner */
} bActListElem;

/* ****************************************************** */
/* FILTER ACTION DATA - METHODS/TYPES */

/* filtering flags  - under what circumstances should a channel be added */
typedef enum ACTFILTER_FLAGS {
  ACTFILTER_VISIBLE = (1 << 0),    /* should channels be visible */
  ACTFILTER_SEL = (1 << 1),        /* should channels be selected */
  ACTFILTER_FOREDIT = (1 << 2),    /* does editable status matter */
  ACTFILTER_CHANNELS = (1 << 3),   /* do we only care that it is a channel */
  ACTFILTER_IPOKEYS = (1 << 4),    /* only channels referencing ipo's */
  ACTFILTER_ONLYICU = (1 << 5),    /* only reference ipo-curves */
  ACTFILTER_FORDRAWING = (1 << 6), /* make list for interface drawing */
  ACTFILTER_ACTGROUPED = (1 << 7), /* belongs to the active group */
} ACTFILTER_FLAGS;

/* Action Editor - Main Data types */
typedef enum ACTCONT_TYPES {
  ACTCONT_NONE = 0,
  ACTCONT_ACTION,
  ACTCONT_SHAPEKEY,
  ACTCONT_GPENCIL,
} ACTCONT_TYPES;

/* ****************************************************** */
/* Stroke Iteration Utilities */

struct GP_EditableStrokes_Iter {
  float diff_mat[4][4];
  float inverse_diff_mat[4][4];
};

/**
 * Iterate over all editable strokes in the current context,
 * stopping on each usable layer + stroke pair (i.e. gpl and gps)
 * to perform some operations on the stroke.
 *
 * \param gpl: The identifier to use for the layer of the stroke being processed.
 *                    Choose a suitable value to avoid name clashes.
 * \param gps: The identifier to use for current stroke being processed.
 *                    Choose a suitable value to avoid name clashes.
 */
#define GP_EDITABLE_STROKES_BEGIN(gpstroke_iter, C, gpl, gps) \
  { \
    struct GP_EditableStrokes_Iter gpstroke_iter = {{{0}}}; \
    Depsgraph *depsgraph_ = CTX_data_ensure_evaluated_depsgraph(C); \
    Object *obact_ = CTX_data_active_object(C); \
    bGPdata *gpd_ = CTX_data_gpencil_data(C); \
    const bool is_multiedit_ = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd_); \
    CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) { \
      bGPDframe *init_gpf_ = (is_multiedit_) ? gpl->frames.first : gpl->actframe; \
      for (bGPDframe *gpf_ = init_gpf_; gpf_; gpf_ = gpf_->next) { \
        if ((gpf_ == gpl->actframe) || ((gpf_->flag & GP_FRAME_SELECT) && is_multiedit_)) { \
          BKE_gpencil_parent_matrix_get(depsgraph_, obact_, gpl, gpstroke_iter.diff_mat); \
          invert_m4_m4(gpstroke_iter.inverse_diff_mat, gpstroke_iter.diff_mat); \
          /* loop over strokes */ \
          bGPDstroke *gpsn_; \
          for (bGPDstroke *gps = gpf_->strokes.first; gps; gps = gpsn_) { \
            gpsn_ = gps->next; \
            /* skip strokes that are invalid for current view */ \
            if (ED_gpencil_stroke_can_use(C, gps) == false) \
              continue; \
            /* check if the color is editable */ \
            if (ED_gpencil_stroke_color_use(obact_, gpl, gps) == false) \
              continue; \
    /* ... Do Stuff With Strokes ...  */

#define GP_EDITABLE_STROKES_END(gpstroke_iter) \
  } \
  } \
  if (!is_multiedit_) { \
    break; \
  } \
  } \
  } \
  CTX_DATA_END; \
  } \
  (void)0

/**
 * Iterate over all editable strokes using evaluated data in the current context,
 * stopping on each usable layer + stroke pair (i.e. gpl and gps)
 * to perform some operations on the stroke.
 *
 * \param gpl: The identifier to use for the layer of the stroke being processed.
 *                    Choose a suitable value to avoid name clashes.
 * \param gps: The identifier to use for current stroke being processed.
 *                    Choose a suitable value to avoid name clashes.
 */
#define GP_EVALUATED_STROKES_BEGIN(gpstroke_iter, C, gpl, gps) \
  { \
    struct GP_EditableStrokes_Iter gpstroke_iter = {{{0}}}; \
    Depsgraph *depsgraph_ = CTX_data_ensure_evaluated_depsgraph(C); \
    Object *obact_ = CTX_data_active_object(C); \
    Object *ob_eval_ = (Object *)DEG_get_evaluated_id(depsgraph_, &obact_->id); \
    bGPdata *gpd_ = (bGPdata *)ob_eval_->data; \
    const bool is_multiedit_ = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd_); \
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_->layers) { \
      if (BKE_gpencil_layer_is_editable(gpl)) { \
        bGPDframe *init_gpf_ = (is_multiedit_) ? gpl->frames.first : gpl->actframe; \
        for (bGPDframe *gpf_ = init_gpf_; gpf_; gpf_ = gpf_->next) { \
          if ((gpf_ == gpl->actframe) || ((gpf_->flag & GP_FRAME_SELECT) && is_multiedit_)) { \
            BKE_gpencil_parent_matrix_get(depsgraph_, obact_, gpl, gpstroke_iter.diff_mat); \
            invert_m4_m4(gpstroke_iter.inverse_diff_mat, gpstroke_iter.diff_mat); \
            /* loop over strokes */ \
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf_->strokes) { \
              /* skip strokes that are invalid for current view */ \
              if (ED_gpencil_stroke_can_use(C, gps) == false) \
                continue; \
              /* check if the color is editable */ \
              if (ED_gpencil_stroke_color_use(obact_, gpl, gps) == false) \
                continue; \
    /* ... Do Stuff With Strokes ...  */

#define GP_EVALUATED_STROKES_END(gpstroke_iter) \
  } \
  } \
  if (!is_multiedit_) { \
    break; \
  } \
  } \
  } \
  } \
  } \
  (void)0

/* ****************************************************** */

#endif /* __GPENCIL_INTERN_H__ */
