/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

struct BlendDataReader;
struct Brush;
struct CurveMapping;
struct Depsgraph;
struct ListBase;
struct MDeformVert;
struct Main;
struct Material;
struct Object;
struct Scene;
struct SpaceImage;
struct ToolSettings;
struct ViewLayer;
struct bDeformGroup;
struct bGPDcurve;
struct bGPDframe;
struct bGPDlayer;
struct bGPDlayer_Mask;
struct bGPDstroke;
struct bGPdata;

#define GPENCIL_SIMPLIFY(scene) \
  ((scene->r.mode & R_SIMPLIFY) && (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_ENABLE))
#define GPENCIL_SIMPLIFY_ONPLAY(playing) \
  (((playing == true) && (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_ON_PLAY)) || \
   ((scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_ON_PLAY) == 0))
#define GPENCIL_SIMPLIFY_FILL(scene, playing) \
  ((GPENCIL_SIMPLIFY_ONPLAY(playing) && GPENCIL_SIMPLIFY(scene) && \
    (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_FILL)))
#define GPENCIL_SIMPLIFY_FX(scene, playing) \
  ((GPENCIL_SIMPLIFY_ONPLAY(playing) && GPENCIL_SIMPLIFY(scene) && \
    (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_FX)))
#define GPENCIL_SIMPLIFY_TINT(scene) \
  (GPENCIL_SIMPLIFY(scene) && (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_TINT))
#define GPENCIL_SIMPLIFY_AA(scene) \
  (GPENCIL_SIMPLIFY(scene) && (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_AA))

/* Vertex Color macros. */
#define GPENCIL_USE_VERTEX_COLOR(toolsettings) \
  (((toolsettings)->gp_paint->mode == GPPAINT_FLAG_USE_VERTEXCOLOR))

/* ------------ Grease-Pencil API ------------------ */

/* clean vertex groups weights */
void BKE_gpencil_free_point_weights(struct MDeformVert *dvert);
void BKE_gpencil_free_stroke_weights(struct bGPDstroke *gps);
void BKE_gpencil_free_stroke_editcurve(struct bGPDstroke *gps);
/** Free stroke, doesn't unlink from any #ListBase. */
void BKE_gpencil_free_stroke(struct bGPDstroke *gps);
/** Free strokes belonging to a gp-frame. */
bool BKE_gpencil_free_strokes(struct bGPDframe *gpf);
/** Free all of a gp-layer's frames. */
void BKE_gpencil_free_frames(struct bGPDlayer *gpl);
/** Free all of the gp-layers for a viewport (list should be `&gpd->layers` or so). */
void BKE_gpencil_free_layers(struct ListBase *list);
/** Free all of the palettes & colors (list should be `&gpd->palettes` or so). */
void BKE_gpencil_free_legacy_palette_data(struct ListBase *list);
/** Free (or release) any data used by this grease pencil (does not free the gpencil itself). */
void BKE_gpencil_free_data(struct bGPdata *gpd, bool free_all);
void BKE_gpencil_free_layer_masks(struct bGPDlayer *gpl);
/**
 * Tag data-block for depsgraph update.
 * Wrapper to avoid include Depsgraph tag functions in other modules.
 * \param gpd: Grease pencil data-block.
 */
void BKE_gpencil_tag(struct bGPdata *gpd);

/**
 * Add a new gp-frame to the given layer.
 * \param gpl: Grease pencil layer
 * \param cframe: Frame number
 * \return Pointer to new frame
 */
struct bGPDframe *BKE_gpencil_frame_addnew(struct bGPDlayer *gpl, int cframe);
/**
 * Add a copy of the active gp-frame to the given layer.
 * \param gpl: Grease pencil layer
 * \param cframe: Frame number
 * \return Pointer to new frame
 */
struct bGPDframe *BKE_gpencil_frame_addcopy(struct bGPDlayer *gpl, int cframe);
/**
 * Add a new gp-layer and make it the active layer.
 * \param gpd: Grease pencil data-block
 * \param name: Name of the layer
 * \param setactive: Set as active
 * \param add_to_header: Used to force the layer added at header
 * \return Pointer to new layer
 */
struct bGPDlayer *BKE_gpencil_layer_addnew(struct bGPdata *gpd,
                                           const char *name,
                                           bool setactive,
                                           bool add_to_header);
/**
 * Add a new grease pencil data-block.
 * \param bmain: Main pointer
 * \param name: Name of the datablock
 * \return Pointer to new data-block
 */
struct bGPdata *BKE_gpencil_data_addnew(struct Main *bmain, const char name[]);

/**
 * Make a copy of a given gpencil frame.
 * \param gpf_src: Source grease pencil frame
 * \return Pointer to new frame
 */
struct bGPDframe *BKE_gpencil_frame_duplicate(const struct bGPDframe *gpf_src, bool dup_strokes);
/**
 * Make a copy of a given gpencil layer.
 * \param gpl_src: Source grease pencil layer
 * \return Pointer to new layer
 */
struct bGPDlayer *BKE_gpencil_layer_duplicate(const struct bGPDlayer *gpl_src,
                                              bool dup_frames,
                                              bool dup_strokes);

/**
 * Make a copy of a given grease-pencil stroke.
 * \param gps_src: Source grease pencil strokes.
 * \param dup_points: Duplicate points data.
 * \param dup_curve: Duplicate curve data.
 * \return Pointer to new stroke.
 */
struct bGPDstroke *BKE_gpencil_stroke_duplicate(struct bGPDstroke *gps_src,
                                                bool dup_points,
                                                bool dup_curve);

/**
 * Make a copy of a given gpencil data-block.
 *
 * XXX: Should this be deprecated?
 */
struct bGPdata *BKE_gpencil_data_duplicate(struct Main *bmain,
                                           const struct bGPdata *gpd,
                                           bool internal_copy);

/* Stroke and Fill - Alpha Visibility Threshold */
#define GPENCIL_ALPHA_OPACITY_THRESH 0.001f
#define GPENCIL_STRENGTH_MIN 0.003f

/**
 * Check if the given layer is able to be edited or not.
 * \param gpl: Grease pencil layer
 * \return True if layer is editable
 */
bool BKE_gpencil_layer_is_editable(const struct bGPDlayer *gpl);

/* How gpencil_layer_getframe() should behave when there
 * is no existing GP-Frame on the frame requested.
 */
typedef enum eGP_GetFrame_Mode {
  /* Use the preceding gp-frame (i.e. don't add anything) */
  GP_GETFRAME_USE_PREV = 0,

  /* Add a new empty/blank frame */
  GP_GETFRAME_ADD_NEW = 1,
  /* Make a copy of the active frame */
  GP_GETFRAME_ADD_COPY = 2,
} eGP_GetFrame_Mode;

/**
 * Get the appropriate gp-frame from a given layer
 * - this sets the layer's `actframe` var (if allowed to)
 * - extension beyond range (if first gp-frame is after all frame in interest and cannot add)
 *
 * \param gpl: Grease pencil layer
 * \param cframe: Frame number
 * \param addnew: Add option
 * \return Pointer to new frame
 */
struct bGPDframe *BKE_gpencil_layer_frame_get(struct bGPDlayer *gpl,
                                              int cframe,
                                              eGP_GetFrame_Mode addnew);
/**
 * Look up the gp-frame on the requested frame number, but don't add a new one.
 * \param gpl: Grease pencil layer
 * \param cframe: Frame number
 * \return Pointer to frame
 */
struct bGPDframe *BKE_gpencil_layer_frame_find(struct bGPDlayer *gpl, int cframe);
/**
 * Delete the given frame from a layer.
 * \param gpl: Grease pencil layer
 * \param gpf: Grease pencil frame
 * \return True if delete was done
 */
bool BKE_gpencil_layer_frame_delete(struct bGPDlayer *gpl, struct bGPDframe *gpf);

/**
 * Get layer by name
 * \param gpd: Grease pencil data-block
 * \param name: Layer name
 * \return Pointer to layer
 */
struct bGPDlayer *BKE_gpencil_layer_named_get(struct bGPdata *gpd, const char *name);
/**
 * Get the active grease pencil layer for editing.
 * \param gpd: Grease pencil data-block
 * \return Pointer to layer
 */
struct bGPDlayer *BKE_gpencil_layer_active_get(struct bGPdata *gpd);
/**
 * Set active grease pencil layer.
 * \param gpd: Grease pencil data-block
 * \param active: Grease pencil layer to set as active
 */
void BKE_gpencil_layer_active_set(struct bGPdata *gpd, struct bGPDlayer *active);
/**
 * Delete grease pencil layer.
 * \param gpd: Grease pencil data-block
 * \param gpl: Grease pencil layer
 */
void BKE_gpencil_layer_delete(struct bGPdata *gpd, struct bGPDlayer *gpl);
/**
 * Sort grease pencil frames.
 * \param gpl: Grease pencil layer
 * \param r_has_duplicate_frames: Duplicated frames flag
 */
void BKE_gpencil_layer_frames_sort(struct bGPDlayer *gpl, bool *r_has_duplicate_frames);

/* Brush */
/**
 * Set grease pencil brush material.
 * \param brush: Brush
 * \param material: Material
 */
void BKE_gpencil_brush_material_set(struct Brush *brush, struct Material *material);

/* vertex groups */
/**
 * Make a copy of a given gpencil weights.
 * \param gps_src: Source grease pencil stroke
 * \param gps_dst: Destination grease pencil stroke
 */
void BKE_gpencil_stroke_weights_duplicate(struct bGPDstroke *gps_src, struct bGPDstroke *gps_dst);

/**
 * Create a default palette.
 * \param bmain: Main pointer
 * \param scene: Scene
 */
void BKE_gpencil_palette_ensure(struct Main *bmain, struct Scene *scene);

void BKE_gpencil_blend_read_data(struct BlendDataReader *reader, struct bGPdata *gpd);
