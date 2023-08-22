/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BlendDataReader;
struct Brush;
struct CurveMapping;
struct Depsgraph;
struct GHash;
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
#define GPENCIL_SIMPLIFY_MODIF(scene) \
  ((GPENCIL_SIMPLIFY(scene) && (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_MODIFIER)))
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
#define GPENCIL_USE_VERTEX_COLOR_STROKE(toolsettings, brush) \
  ((GPENCIL_USE_VERTEX_COLOR(toolsettings) && \
    (((brush)->gpencil_settings->vertex_mode == GPPAINT_MODE_STROKE) || \
     ((brush)->gpencil_settings->vertex_mode == GPPAINT_MODE_BOTH))))
#define GPENCIL_USE_VERTEX_COLOR_FILL(toolsettings, brush) \
  ((GPENCIL_USE_VERTEX_COLOR(toolsettings) && \
    (((brush)->gpencil_settings->vertex_mode == GPPAINT_MODE_FILL) || \
     ((brush)->gpencil_settings->vertex_mode == GPPAINT_MODE_BOTH))))
#define GPENCIL_TINT_VERTEX_COLOR_STROKE(brush) \
  (((brush)->gpencil_settings->vertex_mode == GPPAINT_MODE_STROKE) || \
   ((brush)->gpencil_settings->vertex_mode == GPPAINT_MODE_BOTH))
#define GPENCIL_TINT_VERTEX_COLOR_FILL(brush) \
  (((brush)->gpencil_settings->vertex_mode == GPPAINT_MODE_FILL) || \
   ((brush)->gpencil_settings->vertex_mode == GPPAINT_MODE_BOTH))

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
/** Free (or release) any data used by this grease pencil (does not free the gpencil itself). */
void BKE_gpencil_free_data(struct bGPdata *gpd, bool free_all);
/**
 * Delete grease pencil evaluated data
 * \param gpd_eval: Grease pencil data-block
 */
void BKE_gpencil_eval_delete(struct bGPdata *gpd_eval);
void BKE_gpencil_free_layer_masks(struct bGPDlayer *gpl);
/**
 * Tag data-block for depsgraph update.
 * Wrapper to avoid include Depsgraph tag functions in other modules.
 * \param gpd: Grease pencil data-block.
 */
void BKE_gpencil_tag(struct bGPdata *gpd);

void BKE_gpencil_batch_cache_dirty_tag(struct bGPdata *gpd);
void BKE_gpencil_batch_cache_free(struct bGPdata *gpd);

/**
 * Ensure selection status of stroke is in sync with its points.
 * \param gps: Grease pencil stroke
 */
void BKE_gpencil_stroke_sync_selection(struct bGPdata *gpd, struct bGPDstroke *gps);
void BKE_gpencil_curve_sync_selection(struct bGPdata *gpd, struct bGPDstroke *gps);
/** Assign unique stroke ID for selection. */
void BKE_gpencil_stroke_select_index_set(struct bGPdata *gpd, struct bGPDstroke *gps);
/** Reset unique stroke ID for selection. */
void BKE_gpencil_stroke_select_index_reset(struct bGPDstroke *gps);

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
 * Make a copy of a given gpencil data settings.
 */
void BKE_gpencil_data_copy_settings(const struct bGPdata *gpd_src, struct bGPdata *gpd_dst);

/**
 * Make a copy of a given gpencil layer settings.
 */
void BKE_gpencil_layer_copy_settings(const struct bGPDlayer *gpl_src, struct bGPDlayer *gpl_dst);

/**
 * Make a copy of a given gpencil frame settings.
 */
void BKE_gpencil_frame_copy_settings(const struct bGPDframe *gpf_src, struct bGPDframe *gpf_dst);

/**
 * Make a copy of a given gpencil stroke settings.
 */
void BKE_gpencil_stroke_copy_settings(const struct bGPDstroke *gps_src,
                                      struct bGPDstroke *gps_dst);

/**
 * Make a copy of strokes between gpencil frames.
 * \param gpf_src: Source grease pencil frame
 * \param gpf_dst: Destination grease pencil frame
 */
void BKE_gpencil_frame_copy_strokes(struct bGPDframe *gpf_src, struct bGPDframe *gpf_dst);
/* Create a hash with the list of selected frame number. */
void BKE_gpencil_frame_selected_hash(struct bGPdata *gpd, struct GHash *r_list);

/* Make a copy of a given gpencil stroke editcurve */
struct bGPDcurve *BKE_gpencil_stroke_curve_duplicate(struct bGPDcurve *gpc_src);
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

/**
 * Delete the last stroke of the given frame.
 * \param gpl: Grease pencil layer
 * \param gpf: Grease pencil frame
 */
void BKE_gpencil_frame_delete_laststroke(struct bGPDlayer *gpl, struct bGPDframe *gpf);

/* materials */
/**
 * Reassign strokes using a material.
 * \param gpd: Grease pencil data-block
 * \param totcol: Total materials
 * \param index: Index of the material
 */
void BKE_gpencil_material_index_reassign(struct bGPdata *gpd, int totcol, int index);
/**
 * Remove strokes using a material.
 * \param gpd: Grease pencil data-block
 * \param index: Index of the material
 * \return True if removed
 */
bool BKE_gpencil_material_index_used(struct bGPdata *gpd, int index);
/**
 * Remap material
 * \param gpd: Grease pencil data-block
 * \param remap: Remap index
 * \param remap_len: Remap length
 */
void BKE_gpencil_material_remap(struct bGPdata *gpd,
                                const unsigned int *remap,
                                unsigned int remap_len);
/**
 * Load a table with material conversion index for merged materials.
 * \param ob: Grease pencil object.
 * \param hue_threshold: Threshold for Hue.
 * \param sat_threshold: Threshold for Saturation.
 * \param val_threshold: Threshold for Value.
 * \param r_mat_table: return material table.
 * \return True if done.
 */
bool BKE_gpencil_merge_materials_table_get(struct Object *ob,
                                           float hue_threshold,
                                           float sat_threshold,
                                           float val_threshold,
                                           struct GHash *r_mat_table);
/**
 * Merge similar materials
 * \param ob: Grease pencil object
 * \param hue_threshold: Threshold for Hue
 * \param sat_threshold: Threshold for Saturation
 * \param val_threshold: Threshold for Value
 * \param r_removed: Number of materials removed
 * \return True if done
 */
bool BKE_gpencil_merge_materials(struct Object *ob,
                                 float hue_threshold,
                                 float sat_threshold,
                                 float val_threshold,
                                 int *r_removed);

/* statistics functions */
/**
 * Calc grease pencil statistics functions.
 * \param gpd: Grease pencil data-block
 */
void BKE_gpencil_stats_update(struct bGPdata *gpd);

/**
 * Create a new stroke, with pre-allocated data buffers.
 * \param mat_idx: Index of the material
 * \param totpoints: Total points
 * \param thickness: Stroke thickness
 * \return Pointer to new stroke
 */
struct bGPDstroke *BKE_gpencil_stroke_new(int mat_idx, int totpoints, short thickness);
/**
 * Create a new stroke and add to frame.
 * \param gpf: Grease pencil frame
 * \param mat_idx: Material index
 * \param totpoints: Total points
 * \param thickness: Stroke thickness
 * \param insert_at_head: Add to the head of the strokes list
 * \return Pointer to new stroke
 */
struct bGPDstroke *BKE_gpencil_stroke_add(
    struct bGPDframe *gpf, int mat_idx, int totpoints, short thickness, bool insert_at_head);

/**
 * Add a stroke and copy the temporary drawing color value
 * from one of the existing stroke.
 * \param gpf: Grease pencil frame
 * \param existing: Stroke with the style to copy
 * \param mat_idx: Material index
 * \param totpoints: Total points
 * \param thickness: Stroke thickness
 * \return Pointer to new stroke
 */
struct bGPDstroke *BKE_gpencil_stroke_add_existing_style(struct bGPDframe *gpf,
                                                         struct bGPDstroke *existing,
                                                         int mat_idx,
                                                         int totpoints,
                                                         short thickness);

struct bGPDcurve *BKE_gpencil_stroke_editcurve_new(int tot_curve_points);

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
 * Set locked layers for autolock mode.
 * \param gpd: Grease pencil data-block
 * \param unlock: Unlock flag
 */
void BKE_gpencil_layer_autolock_set(struct bGPdata *gpd, bool unlock);

/**
 * Add grease pencil mask layer.
 * \param gpl: Grease pencil layer
 * \param name: Name of the mask
 * \return Pointer to new mask layer
 */
struct bGPDlayer_Mask *BKE_gpencil_layer_mask_add(struct bGPDlayer *gpl, const char *name);
/**
 * Remove grease pencil mask layer.
 * \param gpl: Grease pencil layer
 * \param mask: Grease pencil mask layer
 */
void BKE_gpencil_layer_mask_remove(struct bGPDlayer *gpl, struct bGPDlayer_Mask *mask);
/**
 * Remove any reference to mask layer.
 * \param gpd: Grease pencil data-block
 * \param name: Name of the mask layer
 */
void BKE_gpencil_layer_mask_remove_ref(struct bGPdata *gpd, const char *name);
/**
 * Get mask layer by name.
 * \param gpl: Grease pencil layer
 * \param name: Mask name
 * \return Pointer to mask layer
 */
struct bGPDlayer_Mask *BKE_gpencil_layer_mask_named_get(struct bGPDlayer *gpl, const char *name);
/**
 * Sort grease pencil mask layers.
 * \param gpd: Grease pencil data-block
 * \param gpl: Grease pencil layer
 */
void BKE_gpencil_layer_mask_sort(struct bGPdata *gpd, struct bGPDlayer *gpl);
/**
 * Sort all grease pencil mask layer.
 * \param gpd: Grease pencil data-block
 */
void BKE_gpencil_layer_mask_sort_all(struct bGPdata *gpd);
/**
 * Make a copy of a given gpencil mask layers.
 */
void BKE_gpencil_layer_mask_copy(const struct bGPDlayer *gpl_src, struct bGPDlayer *gpl_dst);
/**
 * Clean any invalid mask layer.
 */
void BKE_gpencil_layer_mask_cleanup(struct bGPdata *gpd, struct bGPDlayer *gpl);
/**
 * Clean any invalid mask layer for all layers.
 */
void BKE_gpencil_layer_mask_cleanup_all_layers(struct bGPdata *gpd);

/**
 * Sort grease pencil frames.
 * \param gpl: Grease pencil layer
 * \param r_has_duplicate_frames: Duplicated frames flag
 */
void BKE_gpencil_layer_frames_sort(struct bGPDlayer *gpl, bool *r_has_duplicate_frames);

struct bGPDlayer *BKE_gpencil_layer_get_by_name(struct bGPdata *gpd,
                                                const char *name,
                                                int first_if_not_found);

/* Brush */
/**
 * Get grease pencil material from brush.
 * \param brush: Brush
 * \return Pointer to material
 */
struct Material *BKE_gpencil_brush_material_get(struct Brush *brush);
/**
 * Set grease pencil brush material.
 * \param brush: Brush
 * \param material: Material
 */
void BKE_gpencil_brush_material_set(struct Brush *brush, struct Material *material);

/* Object */
/**
 * Get active color, and add all default settings if we don't find anything.
 * \param ob: Grease pencil object
 * \return Material pointer
 */
struct Material *BKE_gpencil_object_material_ensure_active(struct Object *ob);
/**
 * Adds the pinned material to the object if necessary.
 * \param bmain: Main pointer
 * \param ob: Grease pencil object
 * \param brush: Brush
 * \return Pointer to material
 */
struct Material *BKE_gpencil_object_material_ensure_from_brush(struct Main *bmain,
                                                               struct Object *ob,
                                                               struct Brush *brush);
/**
 * Assigns the material to object (if not already present) and returns its index (mat_nr).
 * \param bmain: Main pointer
 * \param ob: Grease pencil object
 * \param material: Material
 * \return Index of the material
 */
int BKE_gpencil_object_material_ensure(struct Main *bmain,
                                       struct Object *ob,
                                       struct Material *material);
struct Material *BKE_gpencil_object_material_ensure_by_name(struct Main *bmain,
                                                            struct Object *ob,
                                                            const char *name,
                                                            int *r_index);

/**
 * Creates a new grease-pencil material and assigns it to object.
 * \param bmain: Main pointer
 * \param ob: Grease pencil object
 * \param name: Material name
 * \param r_index: value is set to zero based index of the new material if \a r_index is not NULL.
 * \return Material pointer.
 */
struct Material *BKE_gpencil_object_material_new(struct Main *bmain,
                                                 struct Object *ob,
                                                 const char *name,
                                                 int *r_index);

/**
 * Get material index (0-based like mat_nr not #Object::actcol).
 * \param ob: Grease pencil object
 * \param ma: Material
 * \return Index of the material
 */
int BKE_gpencil_object_material_index_get(struct Object *ob, struct Material *ma);
int BKE_gpencil_object_material_index_get_by_name(struct Object *ob, const char *name);

/**
 * Returns the material for a brush with respect to its pinned state.
 * \param ob: Grease pencil object
 * \param brush: Brush
 * \return Material pointer
 */
struct Material *BKE_gpencil_object_material_from_brush_get(struct Object *ob,
                                                            struct Brush *brush);
/**
 * Returns the material index for a brush with respect to its pinned state.
 * \param ob: Grease pencil object
 * \param brush: Brush
 * \return Material index.
 */
int BKE_gpencil_object_material_get_index_from_brush(struct Object *ob, struct Brush *brush);

/**
 * Guaranteed to return a material assigned to object. Returns never NULL.
 * \param bmain: Main pointer
 * \param ob: Grease pencil object
 * \return Material pointer.
 */
struct Material *BKE_gpencil_object_material_ensure_from_active_input_toolsettings(
    struct Main *bmain, struct Object *ob, struct ToolSettings *ts);
/**
 * Guaranteed to return a material assigned to object. Returns never NULL.
 * \param bmain: Main pointer
 * \param ob: Grease pencil object.
 * \param brush: Brush
 * \return Material pointer
 */
struct Material *BKE_gpencil_object_material_ensure_from_active_input_brush(struct Main *bmain,
                                                                            struct Object *ob,
                                                                            struct Brush *brush);
/**
 * Guaranteed to return a material assigned to object. Returns never NULL.
 * Only use this for materials unrelated to user input.
 * \param ob: Grease pencil object
 * \return Material pointer
 */
struct Material *BKE_gpencil_object_material_ensure_from_active_input_material(struct Object *ob);

/**
 * Check if stroke has any point selected
 * \param gps: Grease pencil stroke
 * \return True if selected
 */
bool BKE_gpencil_stroke_select_check(const struct bGPDstroke *gps);

/* vertex groups */
/**
 * Ensure stroke has vertex group.
 * \param gps: Grease pencil stroke
 */
void BKE_gpencil_dvert_ensure(struct bGPDstroke *gps);
/**
 * Remove a vertex group.
 * \param ob: Grease pencil object
 * \param defgroup: deform group
 */
void BKE_gpencil_vgroup_remove(struct Object *ob, struct bDeformGroup *defgroup);
/**
 * Make a copy of a given gpencil weights.
 * \param gps_src: Source grease pencil stroke
 * \param gps_dst: Destination grease pencil stroke
 */
void BKE_gpencil_stroke_weights_duplicate(struct bGPDstroke *gps_src, struct bGPDstroke *gps_dst);

/* Set active frame by layer. */
/**
 * Set current grease pencil active frame.
 * \param depsgraph: Current depsgraph
 * \param gpd: Grease pencil data-block.
 */
void BKE_gpencil_frame_active_set(struct Depsgraph *depsgraph, struct bGPdata *gpd);

/**
 * Get range of selected frames in layer.
 * Always the active frame is considered as selected, so if no more selected the range
 * will be equal to the current active frame.
 * \param gpl: Layer.
 * \param r_initframe: Number of first selected frame.
 * \param r_endframe: Number of last selected frame.
 */
void BKE_gpencil_frame_range_selected(struct bGPDlayer *gpl, int *r_initframe, int *r_endframe);
/**
 * Get Falloff factor base on frame range
 * \param gpf: Frame.
 * \param actnum: Number of active frame in layer.
 * \param f_init: Number of first selected frame.
 * \param f_end: Number of last selected frame.
 * \param cur_falloff: Curve with falloff factors.
 */
float BKE_gpencil_multiframe_falloff_calc(
    struct bGPDframe *gpf, int actnum, int f_init, int f_end, struct CurveMapping *cur_falloff);

/**
 * Create a default palette.
 * \param bmain: Main pointer
 * \param scene: Scene
 */
void BKE_gpencil_palette_ensure(struct Main *bmain, struct Scene *scene);

/**
 * Create grease pencil strokes from image
 * \param sima: Image
 * \param gpd: Grease pencil data-block
 * \param gpf: Grease pencil frame
 * \param size: Size
 * \param mask: Mask
 * \return  True if done
 */
bool BKE_gpencil_from_image(
    struct SpaceImage *sima, struct bGPdata *gpd, struct bGPDframe *gpf, float size, bool mask);

/* Iterators */
/**
 * Frame & stroke are NULL if it is a layer callback.
 */
typedef void (*gpIterCb)(struct bGPDlayer *layer,
                         struct bGPDframe *frame,
                         struct bGPDstroke *stroke,
                         void *thunk);

void BKE_gpencil_visible_stroke_iter(struct bGPdata *gpd,
                                     gpIterCb layer_cb,
                                     gpIterCb stroke_cb,
                                     void *thunk);

void BKE_gpencil_visible_stroke_advanced_iter(struct ViewLayer *view_layer,
                                              struct Object *ob,
                                              gpIterCb layer_cb,
                                              gpIterCb stroke_cb,
                                              void *thunk,
                                              bool do_onion,
                                              int cfra);

extern void (*BKE_gpencil_batch_cache_dirty_tag_cb)(struct bGPdata *gpd);
extern void (*BKE_gpencil_batch_cache_free_cb)(struct bGPdata *gpd);

/**
 * Update original pointers in evaluated frame.
 * \param gpf_orig: Original grease-pencil frame.
 * \param gpf_eval: Evaluated grease pencil frame.
 */
void BKE_gpencil_frame_original_pointers_update(const struct bGPDframe *gpf_orig,
                                                const struct bGPDframe *gpf_eval);

/**
 * Update original pointers in evaluated layer.
 * \param gpl_orig: Original grease-pencil layer.
 * \param gpl_eval: Evaluated grease pencil layer.
 */
void BKE_gpencil_layer_original_pointers_update(const struct bGPDlayer *gpl_orig,
                                                const struct bGPDlayer *gpl_eval);
/**
 * Update pointers of eval data to original data to keep references.
 * \param ob_orig: Original grease pencil object
 * \param ob_eval: Evaluated grease pencil object
 */
void BKE_gpencil_update_orig_pointers(const struct Object *ob_orig, const struct Object *ob_eval);

/**
 * Update pointers of eval data to original data to keep references.
 * \param gpd_orig: Original grease pencil data
 * \param gpd_eval: Evaluated grease pencil data
 */
void BKE_gpencil_data_update_orig_pointers(const struct bGPdata *gpd_orig,
                                           const struct bGPdata *gpd_eval);

/**
 * Get parent matrix, including layer parenting.
 * \param depsgraph: Depsgraph
 * \param obact: Grease pencil object
 * \param gpl: Grease pencil layer
 * \param diff_mat: Result parent matrix
 */
void BKE_gpencil_layer_transform_matrix_get(const struct Depsgraph *depsgraph,
                                            struct Object *obact,
                                            struct bGPDlayer *gpl,
                                            float diff_mat[4][4]);

/**
 * Update parent matrix and local transforms.
 * \param depsgraph: Depsgraph
 * \param ob: Grease pencil object
 */
void BKE_gpencil_update_layer_transforms(const struct Depsgraph *depsgraph, struct Object *ob);

/**
 * Find material by name prefix.
 * \param ob: Object pointer
 * \param name_prefix: Prefix name of the material
 * \return  Index
 */
int BKE_gpencil_material_find_index_by_name_prefix(struct Object *ob, const char *name_prefix);

void BKE_gpencil_blend_read_data(struct BlendDataReader *reader, struct bGPdata *gpd);

bool BKE_gpencil_can_avoid_full_copy_on_write(const struct Depsgraph *depsgraph,
                                              struct bGPdata *gpd);

/**
 * Update the geometry of the evaluated bGPdata.
 * This function will:
 *    1) Copy the original data over to the evaluated object.
 *    2) Update the original pointers in the runtime structs.
 */
void BKE_gpencil_update_on_write(struct bGPdata *gpd_orig, struct bGPdata *gpd_eval);

#ifdef __cplusplus
}
#endif
