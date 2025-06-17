/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

struct ID;
struct ListBase;
struct PointerRNA;

struct Brush;
struct GP_SpaceConversion;
struct bGPDframe;
struct bGPDlayer;
struct bGPDspoint;
struct bGPDstroke;
struct bGPdata;
struct tGPspoint;

struct ARegion;
struct Depsgraph;
struct Main;
struct RegionView3D;
struct ReportList;
struct Scene;
struct ScrArea;
struct SnapObjectContext;
struct ToolSettings;
struct View3D;
struct bContext;

struct Material;
struct Object;

struct KeyframeEditData;
struct bAnimContext;

struct wmKeyConfig;
struct wmOperator;

/* ------------- Grease-Pencil Runtime Data ---------------- */

/**
 * Temporary 'Stroke Point' data (2D / screen-space)
 *
 * Used as part of the 'stroke cache' used during drawing of new strokes
 */
struct tGPspoint {
  /** Coordinates x and y of cursor (in relative to area). */
  float m_xy[2];
  /** Pressure of tablet at this point. */
  float pressure;
  /** Pressure of tablet at this point for alpha factor. */
  float strength;
  /** Time relative to stroke start (used when converting to path & in build modifier). */
  float time;
  /** Factor of uv along the stroke. */
  float uv_fac;
  /** UV rotation for dot mode. */
  float uv_rot;
  /** Random value. */
  float rnd[3];
  /** Random flag. */
  bool rnd_dirty;
  /** Point vertex color. */
  float vert_color[4];
};

/* ----------- Grease Pencil Tools/Context ------------- */

/* Context-dependent */

/**
 * Get the active Grease Pencil data-block
 * \note This is the original (#G.main) copy of the data-block, stored in files.
 * Do not use for reading evaluated copies of GP Objects data.
 */
bGPdata *ED_annotation_data_get_active(const bContext *C);
/**
 * Get pointer to active Grease Pencil data-block,
 * and an RNA-pointer to trace back to whatever owns it.
 */
bGPdata **ED_annotation_data_get_pointers(const bContext *C, PointerRNA *r_ptr);
/**
 * Get pointer to active Grease Pencil data-block for annotations,
 * and an RNA-pointer to trace back to whatever owns it,
 * when context info is not available.
 */
bGPdata **ED_annotation_data_get_pointers_direct(ID *screen_id,
                                                 ScrArea *area,
                                                 Scene *scene,
                                                 PointerRNA *r_ptr);
/**
 * Get the active Grease Pencil data-block, when context is not available.
 */
bGPdata *ED_annotation_data_get_active_direct(ID *screen_id, ScrArea *area, Scene *scene);

/**
 * Check whether given stroke can be edited given the supplied context.
 * TODO: do we need additional flags for screen-space vs data-space?.
 */
bool ED_gpencil_stroke_can_use_direct(const ScrArea *area, const bGPDstroke *gps);

/* ----------- Grease Pencil Operators ----------------- */

void ED_keymap_gpencil_legacy(wmKeyConfig *keyconf);

void ED_operatortypes_gpencil_legacy();

/* ------------ Grease-Pencil Drawing API ------------------ */
/* `drawgpencil.cc` */

/**
 * Draw grease-pencil sketches to specified 2d-view that uses `ibuf` corrections.
 */
void ED_annotation_draw_2dimage(const bContext *C);
/**
 * Draw grease-pencil sketches to specified 2d-view
 * assuming that matrices are already set correctly.
 *
 * \note This gets called twice - first time with onlyv2d=true to draw 'canvas' strokes,
 * second time with onlyv2d=false for screen-aligned strokes.
 */
void ED_annotation_draw_view2d(const bContext *C, bool onlyv2d);
/**
 * Draw annotations sketches to specified 3d-view assuming that matrices are already set correctly.
 * NOTE: this gets called twice - first time with only3d=true to draw 3d-strokes,
 * second time with only3d=false for screen-aligned strokes.
 */
void ED_annotation_draw_view3d(
    Scene *scene, Depsgraph *depsgraph, View3D *v3d, ARegion *region, bool only3d);
void ED_annotation_draw_ex(
    Scene *scene, bGPdata *gpd, int winx, int winy, int cfra, char spacetype);

/* ----------- Grease-Pencil AnimEdit API ------------------ */
/**
 * Loops over the GP-frames for a GP-layer, and applies the given callback.
 */
bool ED_gpencil_layer_frames_looper(bGPDlayer *gpl,
                                    Scene *scene,
                                    bool (*gpf_cb)(bGPDframe *, Scene *));
/**
 * Make a listing all the gp-frames in a layer as cfraelems.
 */
void ED_gpencil_layer_make_cfra_list(bGPDlayer *gpl, ListBase *elems, bool onlysel);

/**
 * Check if one of the frames in this layer is selected.
 */
bool ED_gpencil_layer_frame_select_check(const bGPDlayer *gpl);
/**
 * Set all/none/invert select.
 */
void ED_gpencil_layer_frame_select_set(bGPDlayer *gpl, short mode);
/**
 * Select the frames in this layer that occur within the bounds specified.
 */
void ED_gpencil_layer_frames_select_box(bGPDlayer *gpl, float min, float max, short select_mode);
/**
 * Select the frames in this layer that occur within the lasso/circle region specified.
 */
void ED_gpencil_layer_frames_select_region(KeyframeEditData *ked,
                                           bGPDlayer *gpl,
                                           short tool,
                                           short select_mode);
/**
 * Set all/none/invert select (like above, but with SELECT_* modes).
 */
void ED_gpencil_select_frames(bGPDlayer *gpl, short select_mode);
/**
 * Select the frame in this layer that occurs on this frame (there should only be one at most).
 */
void ED_gpencil_select_frame(bGPDlayer *gpl, int selx, short select_mode);

/**
 * Set the layer's channel as active
 */
void ED_gpencil_set_active_channel(bGPdata *gpd, bGPDlayer *gpl);

/**
 * Delete selected frames.
 */
bool ED_gpencil_layer_frames_delete(bGPDlayer *gpl);
/**
 * Duplicate selected frames from given gp-layer.
 */
void ED_gpencil_layer_frames_duplicate(bGPDlayer *gpl);

/**
 * Set keyframe type for selected frames from given gp-layer
 *
 * \param type: The type of keyframe (#eBezTriple_KeyframeType) to set selected frames to.
 */
void ED_gpencil_layer_frames_keytype_set(bGPDlayer *gpl, short type);
/**
 * Snap selected frames to ....
 */
void ED_gpencil_layer_snap_frames(bGPDlayer *gpl, Scene *scene, short mode);

/**
 * Mirror selected gp-frames on...
 * TODO: mirror over a specific time.
 */
void ED_gpencil_layer_mirror_frames(bGPDlayer *gpl, Scene *scene, short mode);

/**
 * This function frees any MEM_calloc'ed copy/paste buffer data.
 */
void ED_gpencil_anim_copybuf_free();
/**
 * This function adds data to the copy/paste buffer, freeing existing data first
 * Only the selected GP-layers get their selected keyframes copied.
 *
 * Returns whether the copy operation was successful or not.
 */
bool ED_gpencil_anim_copybuf_copy(bAnimContext *ac);
/**
 * Pastes keyframes from buffer, and reports success.
 */
bool ED_gpencil_anim_copybuf_paste(bAnimContext *ac, short offset_mode);

/* ----------- Add Primitive Utilities -------------- */

/* texture coordinate utilities */

/**
 * Ensure the #tGPspoint buffer (while drawing stroke)
 * size is enough to save all points of the stroke.
 */
tGPspoint *ED_gpencil_sbuffer_ensure(tGPspoint *buffer_array,
                                     int *buffer_size,
                                     int *buffer_used,
                                     bool clear);
