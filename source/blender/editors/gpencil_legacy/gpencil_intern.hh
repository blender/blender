/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 */

#pragma once

#include "DNA_vec_types.h"

#include "ED_numinput.hh"

#define DEPTH_INVALID 1.0f

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
struct ViewDepths;
struct wmOperatorType;
struct wmWindow;

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
 * code (in `drawgpencil.cc`).
 *
 * NOTE: All this is within the gpencil module, so nothing needs
 * to be exported to other modules.
 */

/* Internal Operator-State Data ------------------------ */

/** Random settings by stroke */
struct GpRandomSettings {
  /** Pressure used for evaluated curves. */
  float pen_press;

  float hsv[3];
  float pressure;
  float strength;
  float uv;
};

/* Temporary draw data (no draw manager mode) */
struct tGPDdraw {
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
};

/* Modal Operator Drawing Callbacks ------------------------ */

/**
 * Wrapper to draw strokes for filling operator.
 */
void ED_gpencil_draw_fill(tGPDdraw *tgpw);

/* ***************************************************** */
/* Internal API */

/* Stroke Coordinates API ------------------------------ */
/* `gpencil_utils.cc` */

struct GP_SpaceConversion {
  Scene *scene;
  Object *ob;
  bGPdata *gpd;
  bGPDlayer *gpl;

  ScrArea *area;
  ARegion *region;
  View2D *v2d;

  const rctf *subrect; /* for using the camera rect within the 3d view */
  rctf subrect_data;

  float mat[4][4]; /* transform matrix on the strokes (introduced in [b770964]) */
};

/* Temporary primitive operation data */
struct tGPDprimitive {
  /** main database pointer */
  Main *bmain;
  Depsgraph *depsgraph;
  /** window where painting originated */
  wmWindow *win;
  /** current scene from context */
  Scene *scene;
  /** current active gp object */
  Object *ob;
  /** current evaluated gp object */
  Object *ob_eval;
  /** area where painting originated */
  ScrArea *area;
  /** region where painting originated */
  RegionView3D *rv3d;
  /** view3d where painting originated */
  View3D *v3d;
  /** region where painting originated */
  ARegion *region;
  /** current GP datablock */
  bGPdata *gpd;
  /** current material */
  Material *material;
  /** current brush */
  Brush *brush;
  /** For operations that require occlusion testing. */
  ViewDepths *depths;

  /** Settings to pass to gp_points_to_xy(). */
  GP_SpaceConversion gsc;

  /** current frame number */
  int cframe;
  /** layer */
  bGPDlayer *gpl;
  /** frame */
  bGPDframe *gpf;
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
  /** number of subdivisions. */
  int subdiv;
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
  RNG *rng;

  /** numeric input */
  NumInput num;

  /** size in pixels for uv calculation */
  float totpixlen;

  /** Random settings by stroke */
  GpRandomSettings random_settings;
};

/**
 * Check whether a given stroke segment is inside a circular brush
 *
 * \param mval: The current screen-space coordinates (midpoint) of the brush
 * \param rad: The radius of the brush
 *
 * \param x0, y0: The screen-space x and y coordinates of the start of the stroke segment
 * \param x1, y1: The screen-space x and y coordinates of the end of the stroke segment
 */
bool gpencil_stroke_inside_circle(const float mval[2], int rad, int x0, int y0, int x1, int y1);

/**
 * Init settings for stroke point space conversions
 *
 * \param r_gsc: [out] The space conversion settings struct, populated with necessary params
 */
void gpencil_point_conversion_init(bContext *C, GP_SpaceConversion *r_gsc);

/**
 * Convert a Grease Pencil coordinate (i.e. can be 2D or 3D) to screen-space (2D)
 *
 * \param[out] r_x: The screen-space x-coordinate of the point
 * \param[out] r_y: The screen-space y-coordinate of the point
 *
 * \warning This assumes that the caller has already checked
 * whether the stroke in question can be drawn.
 */
void gpencil_point_to_xy(const GP_SpaceConversion *gsc,
                         const bGPDstroke *gps,
                         const bGPDspoint *pt,
                         int *r_x,
                         int *r_y);

/**
 * Convert a Grease Pencil coordinate (i.e. can be 2D or 3D) to screen-space (2D).
 *
 * Just like #gpencil_point_to_xy(), except the resulting coordinates are floats not ints.
 * Use this version to solve "stair-step" artifacts which may arise when
 * round-tripping the calculations.
 *
 * \param r_x: The screen-space x-coordinate of the point.
 * \param r_y: The screen-space y-coordinate of the point.
 *
 * \warning This assumes that the caller has already checked
 * whether the stroke in question can be drawn.
 */
void gpencil_point_to_xy_fl(const GP_SpaceConversion *gsc,
                            const bGPDstroke *gps,
                            const bGPDspoint *pt,
                            float *r_x,
                            float *r_y);

/**
 * Convert point to world space
 *
 * \param pt: Original point
 * \param diff_mat: Matrix with the transformation
 * \param[out] r_pt: Pointer to new point after apply matrix
 */
void gpencil_point_to_world_space(const bGPDspoint *pt,
                                  const float diff_mat[4][4],
                                  bGPDspoint *r_pt);
/**
 * Change points position relative to parent object
 */
/**
 * Change position relative to parent object
 */
void gpencil_world_to_object_space(Depsgraph *depsgraph,
                                   Object *obact,
                                   bGPDlayer *gpl,
                                   bGPDstroke *gps);
/**
 * Change point position relative to parent object
 */
/**
 * Change point position relative to parent object
 */
void gpencil_world_to_object_space_point(Depsgraph *depsgraph,
                                         Object *obact,
                                         bGPDlayer *gpl,
                                         bGPDspoint *pt);

/**
 * generic based on gpencil_point_to_xy_fl
 */
void gpencil_point_3d_to_xy(const GP_SpaceConversion *gsc,
                            short flag,
                            const float pt[3],
                            float xy[2]);

/**
 * Project screen-space coordinates to 3D-space
 *
 * For use with editing tools where it is easier to perform the operations in 2D,
 * and then later convert the transformed points back to 3D.
 *
 * \param screen_co: The screen-space 2D coordinates to convert to
 * \param r_out: The resulting 3D coordinates of the input point
 *
 * \note We include this as a utility function, since the standard method
 * involves quite a few steps, which are invariably always the same
 * for all GPencil operations. So, it's nicer to just centralize these.
 *
 * \warning Assumes that it is getting called in a 3D view only.
 */
bool gpencil_point_xy_to_3d(const GP_SpaceConversion *gsc,
                            Scene *scene,
                            const float screen_co[2],
                            float r_out[3]);

/* helper to convert 2d to 3d */

/**
 * Convert #tGPspoint (temporary 2D/screen-space point data used by GP modal operators)
 * to 3D coordinates.
 *
 * \param point2D: The screen-space 2D point data to convert.
 * \param depth: Depth array (via #ED_view3d_depth_read_cached()).
 * \param r_out: The resulting 2D point data.
 */
void gpencil_stroke_convertcoords_tpoint(Scene *scene,
                                         ARegion *region,
                                         Object *ob,
                                         const tGPspoint *point2D,
                                         float *depth,
                                         float r_out[3]);

/* Poll Callbacks ------------------------------------ */
/* `gpencil_utils.cc` */

/**
 * Poll callback for adding data/layers - special.
 */
bool gpencil_add_poll(bContext *C);
/**
 * Poll callback for checking if there is an active layer.
 */
bool gpencil_active_layer_poll(bContext *C);
/**
 * Poll callback for checking if there is an active brush.
 */
bool gpencil_active_brush_poll(bContext *C);
bool gpencil_brush_create_presets_poll(bContext *C);

int ED_gpencil_new_layer_dialog(bContext *C, wmOperator *op);

/* Copy/Paste Buffer --------------------------------- */
/* `gpencil_edit.cc` */

/**
 * list of #bGPDstroke instances
 *
 * \note is exposed within the editors/gpencil module so that other tools can use it too.
 */
extern ListBase gpencil_strokes_copypastebuf;

/* Build a map for converting between old color-names and destination-color-refs. */
/**
 * Ensure that destination datablock has all the colors the pasted strokes need.
 * Helper function for copy-pasting strokes
 */
GHash *gpencil_copybuf_validate_colormap(bContext *C);

/* Stroke Editing ------------------------------------ */

/**
 * Simple wrapper to external call.
 */
int gpencil_delete_selected_point_wrap(bContext *C);

/**
 * Subdivide a stroke once, by adding a point half way between each pair of existing points
 * \param gpd: Datablock
 * \param gps: Stroke data
 * \param subdivide: Number of times to subdivide
 */
void gpencil_subdivide_stroke(bGPdata *gpd, bGPDstroke *gps, int subdivide);

/* Layers Enums -------------------------------------- */

/**
 * Just existing layers.
 */
const EnumPropertyItem *ED_gpencil_layers_enum_itemf(bContext *C,
                                                     PointerRNA *ptr,
                                                     PropertyRNA *prop,
                                                     bool *r_free);
/**
 * Existing + Option to add/use new layer.
 */
const EnumPropertyItem *ED_gpencil_layers_with_new_enum_itemf(bContext *C,
                                                              PointerRNA *ptr,
                                                              PropertyRNA *prop,
                                                              bool *r_free);
/**
 * Just existing Materials.
 */
const EnumPropertyItem *ED_gpencil_material_enum_itemf(bContext *C,
                                                       PointerRNA *ptr,
                                                       PropertyRNA *prop,
                                                       bool *r_free);

/* ***************************************************** */
/* Operator Defines */

/* annotations ------ */

void GPENCIL_OT_annotate(wmOperatorType *ot);
void GPENCIL_OT_annotation_add(wmOperatorType *ot);
void GPENCIL_OT_data_unlink(wmOperatorType *ot);
void GPENCIL_OT_layer_annotation_add(wmOperatorType *ot);
void GPENCIL_OT_layer_annotation_remove(wmOperatorType *ot);
void GPENCIL_OT_layer_annotation_move(wmOperatorType *ot);
void GPENCIL_OT_blank_frame_add(wmOperatorType *ot);
void GPENCIL_OT_annotation_active_frame_delete(wmOperatorType *ot);

/* Paint Modes for operator */
enum eGPencil_PaintModes {
  GP_PAINTMODE_DRAW = 0,
  GP_PAINTMODE_ERASER,
  GP_PAINTMODE_DRAW_STRAIGHT,
  GP_PAINTMODE_DRAW_POLY,
  GP_PAINTMODE_SET_CP,
};

/* chunk size for gp-session buffer (the total size is a multiple of this number) */
#define GP_STROKE_BUFFER_CHUNK 2048

/* buttons editing --- */

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

/* undo stack ---------- */

void gpencil_undo_init(bGPdata *gpd);
void gpencil_undo_push(bGPdata *gpd);
void gpencil_undo_finish();

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
    GP_EditableStrokes_Iter gpstroke_iter = {{{0}}}; \
    Depsgraph *depsgraph_ = CTX_data_ensure_evaluated_depsgraph(C); \
    Object *obact_ = CTX_data_active_object(C); \
    bGPdata *gpd_ = CTX_data_gpencil_data(C); \
    const bool is_multiedit_ = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd_); \
    CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) { \
      bGPDframe *init_gpf_ = (is_multiedit_) ? (bGPDframe *)gpl->frames.first : gpl->actframe; \
      for (bGPDframe *gpf_ = init_gpf_; gpf_; gpf_ = gpf_->next) { \
        if ((gpf_ == gpl->actframe) || ((gpf_->flag & GP_FRAME_SELECT) && is_multiedit_)) { \
          BKE_gpencil_layer_transform_matrix_get( \
              depsgraph_, obact_, gpl, gpstroke_iter.diff_mat); \
          invert_m4_m4(gpstroke_iter.inverse_diff_mat, gpstroke_iter.diff_mat); \
          /* loop over strokes */ \
          bGPDstroke *gpsn_; \
          for (bGPDstroke *gps = (bGPDstroke *)gpf_->strokes.first; gps; gps = gpsn_) { \
            gpsn_ = gps->next; \
            /* skip strokes that are invalid for current view */ \
            if (ED_gpencil_stroke_can_use(C, gps) == false) { \
              continue; \
            } \
            /* check if the color is editable */ \
            if (ED_gpencil_stroke_material_editable(obact_, gpl, gps) == false) { \
              continue; \
            } \
    /* ... Do Stuff With Strokes ... */

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
 * Iterate over all editable edit-curves in the current context,
 * stopping on each usable layer + stroke + curve pair (i.e. `gpl`, `gps` and `gpc`)
 * to perform some operations on the curve.
 *
 * \param gpl: The identifier to use for the layer of the stroke being processed.
 *             Choose a suitable value to avoid name clashes.
 * \param gps: The identifier to use for current stroke being processed.
 *             Choose a suitable value to avoid name clashes.
 * \param gpc: The identifier to use for current editcurve being processed.
 *             Choose a suitable value to avoid name clashes.
 */
#define GP_EDITABLE_CURVES_BEGIN(gpstroke_iter, C, gpl, gps, gpc) \
  { \
    GP_EditableStrokes_Iter gpstroke_iter = {{{0}}}; \
    Depsgraph *depsgraph_ = CTX_data_ensure_evaluated_depsgraph(C); \
    Object *obact_ = CTX_data_active_object(C); \
    bGPdata *gpd_ = CTX_data_gpencil_data(C); \
    const bool is_multiedit_ = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd_); \
    CTX_DATA_BEGIN (C, bGPDlayer *, gpl, editable_gpencil_layers) { \
      bGPDframe *init_gpf_ = (is_multiedit_) ? (bGPDframe *)gpl->frames.first : gpl->actframe; \
      for (bGPDframe *gpf_ = init_gpf_; gpf_; gpf_ = gpf_->next) { \
        if ((gpf_ == gpl->actframe) || ((gpf_->flag & GP_FRAME_SELECT) && is_multiedit_)) { \
          BKE_gpencil_layer_transform_matrix_get( \
              depsgraph_, obact_, gpl, gpstroke_iter.diff_mat); \
          invert_m4_m4(gpstroke_iter.inverse_diff_mat, gpstroke_iter.diff_mat); \
          /* loop over strokes */ \
          bGPDstroke *gpsn_; \
          for (bGPDstroke *gps = (bGPDstroke *)gpf_->strokes.first; gps; gps = gpsn_) { \
            gpsn_ = gps->next; \
            /* skip strokes that are invalid for current view */ \
            if (ED_gpencil_stroke_can_use(C, gps) == false) \
              continue; \
            if (gps->editcurve == NULL) \
              continue; \
            bGPDcurve *gpc = gps->editcurve; \
    /* ... Do Stuff With Strokes ... */

#define GP_EDITABLE_CURVES_END(gpstroke_iter) \
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
    GP_EditableStrokes_Iter gpstroke_iter = {{{0}}}; \
    Depsgraph *depsgraph_ = CTX_data_ensure_evaluated_depsgraph(C); \
    Object *obact_ = CTX_data_active_object(C); \
    Object *ob_eval_ = (Object *)DEG_get_evaluated_id(depsgraph_, &obact_->id); \
    bGPdata *gpd_ = (bGPdata *)ob_eval_->data; \
    const bool is_multiedit_ = (bool)GPENCIL_MULTIEDIT_SESSIONS_ON(gpd_); \
    LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_->layers) { \
      if (BKE_gpencil_layer_is_editable(gpl)) { \
        bGPDframe *init_gpf_ = (is_multiedit_) ? (bGPDframe *)gpl->frames.first : gpl->actframe; \
        for (bGPDframe *gpf_ = init_gpf_; gpf_; gpf_ = gpf_->next) { \
          if ((gpf_ == gpl->actframe) || ((gpf_->flag & GP_FRAME_SELECT) && is_multiedit_)) { \
            BKE_gpencil_layer_transform_matrix_get( \
                depsgraph_, obact_, gpl, gpstroke_iter.diff_mat); \
            /* Undo layer transform. */ \
            mul_m4_m4m4(gpstroke_iter.diff_mat, gpstroke_iter.diff_mat, gpl->layer_invmat); \
            /* loop over strokes */ \
            LISTBASE_FOREACH (bGPDstroke *, gps, &gpf_->strokes) { \
              /* skip strokes that are invalid for current view */ \
              if (ED_gpencil_stroke_can_use(C, gps) == false) { \
                continue; \
              } \
              /* check if the color is editable */ \
              if (ED_gpencil_stroke_material_editable(obact_, gpl, gps) == false) { \
                continue; \
              } \
    /* ... Do Stuff With Strokes ... */

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
