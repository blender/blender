/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "DNA_listBase.h"

namespace blender {

struct BlendDataReader;
struct Brush;
struct CurveMapping;
struct Depsgraph;
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
struct bGPDpalette;

/* ------------ Annotation API ------------------ */

/* clean vertex groups weights */
void BKE_annotations_free_point_weights(struct MDeformVert *dvert);
void BKE_annotations_free_stroke_weights(struct bGPDstroke *gps);
void BKE_annotations_free_stroke_editcurve(struct bGPDstroke *gps);
/** Free stroke, doesn't unlink from any #ListBase. */
void BKE_annotations_free_stroke(struct bGPDstroke *gps);
/** Free strokes belonging to an annotation frame. */
bool BKE_annotations_free_strokes(struct bGPDframe *gpf);
/** Free all of an annotation layer's frames. */
void BKE_annotations_free_frames(struct bGPDlayer *gpl);
/** Free all of the annotation layers for a viewport (list should be `&gpd->layers` or so). */
void BKE_annotations_free_layers(ListBaseT<bGPDlayer> *list);
/** Free all of the palettes & colors (list should be `&gpd->palettes` or so). */
void BKE_annotations_free_legacy_palette_data(ListBaseT<bGPDpalette> *list);
/** Free (or release) any data used by this annotation (does not free the annotation itself). */
void BKE_annotations_free_data(struct bGPdata *gpd, bool free_all);
void BKE_annotations_free_layer_masks(struct bGPDlayer *gpl);
/**
 * Tag data-block for depsgraph update.
 * Wrapper to avoid include Depsgraph tag functions in other modules.
 * \param gpd: Annotation data-block.
 */
void BKE_annotations_tag(struct bGPdata *gpd);

/**
 * Add a new gp-frame to the given layer.
 * \param gpl: Annotation layer
 * \param cframe: Frame number
 * \return Pointer to new frame
 */
struct bGPDframe *BKE_annotations_frame_addnew(struct bGPDlayer *gpl, int cframe);
/**
 * Add a copy of the active gp-frame to the given layer.
 * \param gpl: Annotation layer
 * \param cframe: Frame number
 * \return Pointer to new frame
 */
struct bGPDframe *BKE_annotations_frame_addcopy(struct bGPDlayer *gpl, int cframe);
/**
 * Add a new gp-layer and make it the active layer.
 * \param gpd: Annotation data-block
 * \param name: Name of the layer
 * \param setactive: Set as active
 * \param add_to_header: Used to force the layer added at header
 * \return Pointer to new layer
 */
struct bGPDlayer *BKE_annotations_layer_addnew(struct bGPdata *gpd,
                                               const char *name,
                                               bool setactive,
                                               bool add_to_header);
/**
 * Add a new annotation data-block.
 * \param bmain: Main pointer
 * \param name: Name of the datablock
 * \return Pointer to new data-block
 */
struct bGPdata *BKE_annotations_data_addnew(struct Main *bmain, const char name[]);

/**
 * Make a copy of a given annotation frame.
 * \param gpf_src: Source annotation frame
 * \return Pointer to new frame
 */
struct bGPDframe *BKE_annotations_frame_duplicate(const struct bGPDframe *gpf_src,
                                                  bool dup_strokes);
/**
 * Make a copy of a given annotation layer.
 * \param gpl_src: Source annotation layer
 * \return Pointer to new layer
 */
struct bGPDlayer *BKE_annotations_layer_duplicate(const struct bGPDlayer *gpl_src,
                                                  bool dup_frames,
                                                  bool dup_strokes);

/**
 * Make a copy of a given grease-pencil stroke.
 * \param gps_src: Source annotation strokes.
 * \param dup_points: Duplicate points data.
 * \param dup_curve: Duplicate curve data.
 * \return Pointer to new stroke.
 */
struct bGPDstroke *BKE_gpencil_stroke_duplicate(struct bGPDstroke *gps_src,
                                                bool dup_points,
                                                bool dup_curve);

/**
 * Make a copy of a given annotation data-block.
 *
 * XXX: Should this be deprecated?
 */
struct bGPdata *BKE_annotations_data_duplicate(struct Main *bmain,
                                               const struct bGPdata *gpd,
                                               bool internal_copy);

#define GPENCIL_STRENGTH_MIN 0.003f

/**
 * Check if the given layer is able to be edited or not.
 * \param gpl: Annotation layer
 * \return True if layer is editable
 */
bool BKE_annotations_layer_is_editable(const struct bGPDlayer *gpl);

/* How annotations_layer_getframe() should behave when there
 * is no existing GP-Frame on the frame requested.
 */
enum eGP_GetFrame_Mode {
  /* Use the preceding gp-frame (i.e. don't add anything) */
  GP_GETFRAME_USE_PREV = 0,

  /* Add a new empty/blank frame */
  GP_GETFRAME_ADD_NEW = 1,
  /* Make a copy of the active frame */
  GP_GETFRAME_ADD_COPY = 2,
};

/**
 * Get the appropriate gp-frame from a given layer
 * - this sets the layer's `actframe` var (if allowed to)
 * - extension beyond range (if first gp-frame is after all frame in interest and cannot add)
 *
 * \param gpl: Annotation layer
 * \param cframe: Frame number
 * \param addnew: Add option
 * \return Pointer to new frame
 */
struct bGPDframe *BKE_annotations_layer_frame_get(struct bGPDlayer *gpl,
                                                  int cframe,
                                                  eGP_GetFrame_Mode addnew);
/**
 * Look up the gp-frame on the requested frame number, but don't add a new one.
 * \param gpl: Annotation layer
 * \param cframe: Frame number
 * \return Pointer to frame
 */
struct bGPDframe *BKE_annotations_layer_frame_find(struct bGPDlayer *gpl, int cframe);
/**
 * Delete the given frame from a layer.
 * \param gpl: Annotation layer
 * \param gpf: Annotation frame
 * \return True if delete was done
 */
bool BKE_annotations_layer_frame_delete(struct bGPDlayer *gpl, struct bGPDframe *gpf);

/**
 * Get layer by name
 * \param gpd: Annotation data-block
 * \param name: Layer name
 * \return Pointer to layer
 */
struct bGPDlayer *BKE_annotations_layer_named_get(struct bGPdata *gpd, const char *name);
/**
 * Get the active annotation layer for editing.
 * \param gpd: Annotation data-block
 * \return Pointer to layer
 */
struct bGPDlayer *BKE_annotations_layer_active_get(struct bGPdata *gpd);
/**
 * Set active annotation layer.
 * \param gpd: Annotation data-block
 * \param active: Annotation layer to set as active
 */
void BKE_annotations_layer_active_set(struct bGPdata *gpd, struct bGPDlayer *active);
/**
 * Delete annotation layer.
 * \param gpd: Annotation data-block
 * \param gpl: Annotation layer
 */
void BKE_annotations_layer_delete(struct bGPdata *gpd, struct bGPDlayer *gpl);
/**
 * Sort annotation frames.
 * \param gpl: Annotation layer
 * \param r_has_duplicate_frames: Duplicated frames flag
 */
void BKE_annotations_layer_frames_sort(struct bGPDlayer *gpl, bool *r_has_duplicate_frames);

/* vertex groups */
/**
 * Make a copy of a given annotation weights.
 * \param gps_src: Source annotation stroke
 * \param gps_dst: Destination annotation stroke
 */
void BKE_gpencil_stroke_weights_duplicate(struct bGPDstroke *gps_src, struct bGPDstroke *gps_dst);

void BKE_annotations_blend_read_data(struct BlendDataReader *reader, struct bGPdata *gpd);

}  // namespace blender
