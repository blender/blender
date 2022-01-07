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
 * The Original Code is Copyright (C) 2008, Blender Foundation
 * This is a new part of Blender
 */

/** \file
 * \ingroup editors
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct ListBase;
struct PointerRNA;

struct Brush;
struct GP_SpaceConversion;
struct GpRandomSettings;
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

#define GPENCIL_MINIMUM_JOIN_DIST 20.0f

/* Reproject stroke modes. */
typedef enum eGP_ReprojectModes {
  /* Axis */
  GP_REPROJECT_FRONT = 0,
  GP_REPROJECT_SIDE,
  GP_REPROJECT_TOP,
  /* On same plane, parallel to view-plane. */
  GP_REPROJECT_VIEW,
  /* Reprojected on to the scene geometry */
  GP_REPROJECT_SURFACE,
  /* Reprojected on 3D cursor orientation */
  GP_REPROJECT_CURSOR,
  /* Keep equals (used in some operators) */
  GP_REPROJECT_KEEP,
} eGP_ReprojectModes;

/* Target object modes. */
typedef enum eGP_TargetObjectMode {
  GP_TARGET_OB_NEW = 0,
  GP_TARGET_OB_SELECTED = 1,
} eGP_TargetObjectMode;

/* ------------- Grease-Pencil Runtime Data ---------------- */

/* Temporary 'Stroke Point' data (2D / screen-space)
 *
 * Used as part of the 'stroke cache' used during drawing of new strokes
 */
typedef struct tGPspoint {
  /** Coordinates x and y of cursor (in relative to area). */
  float m_xy[2];
  /** Pressure of tablet at this point. */
  float pressure;
  /** Pressure of tablet at this point for alpha factor. */
  float strength;
  /** Time relative to stroke start (used when converting to path). */
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
} tGPspoint;

/* ----------- Grease Pencil Tools/Context ------------- */

/* Context-dependent */

/**
 * Get pointer to active Grease Pencil data-block,
 * and an RNA-pointer to trace back to whatever owns it.
 */
struct bGPdata **ED_gpencil_data_get_pointers(const struct bContext *C, struct PointerRNA *r_ptr);

/**
 * Get the active Grease Pencil data-block
 */
struct bGPdata *ED_gpencil_data_get_active(const struct bContext *C);
/**
 * Get the evaluated copy of the active Grease Pencil data-block (where applicable)
 * - For the 3D View (i.e. "GP Objects"), this gives the evaluated copy of the GP data-block
 *   (i.e. a copy of the active GP data-block for the active object, where modifiers have been
 *   applied). This is needed to correctly work with "Copy-on-Write".
 * - For all other editors (i.e. "GP Annotations"), this just gives the active data-block
 *   like for #ED_gpencil_data_get_active()
 */
struct bGPdata *ED_gpencil_data_get_active_evaluated(const struct bContext *C);

/**
 * Context independent (i.e. each required part is passed in instead).
 *
 * Get pointer to active Grease Pencil data-block,
 * and an RNA-pointer to trace back to whatever owns it,
 * when context info is not available.
 */
struct bGPdata **ED_gpencil_data_get_pointers_direct(struct ScrArea *area,
                                                     struct Object *ob,
                                                     struct PointerRNA *r_ptr);
/* Get the active Grease Pencil data-block, when context is not available */
struct bGPdata *ED_gpencil_data_get_active_direct(struct ScrArea *area, struct Object *ob);

/**
 * Get the active Grease Pencil data-block
 * \note This is the original (#G.main) copy of the data-block, stored in files.
 * Do not use for reading evaluated copies of GP Objects data.
 */
struct bGPdata *ED_annotation_data_get_active(const struct bContext *C);
/**
 * Get pointer to active Grease Pencil data-block,
 * and an RNA-pointer to trace back to whatever owns it.
 */
struct bGPdata **ED_annotation_data_get_pointers(const struct bContext *C,
                                                 struct PointerRNA *r_ptr);
/**
 * Get pointer to active Grease Pencil data-block for annotations,
 * and an RNA-pointer to trace back to whatever owns it,
 * when context info is not available.
 */
struct bGPdata **ED_annotation_data_get_pointers_direct(struct ID *screen_id,
                                                        struct ScrArea *area,
                                                        struct Scene *scene,
                                                        struct PointerRNA *r_ptr);
/**
 * Get the active Grease Pencil data-block, when context is not available.
 */
struct bGPdata *ED_annotation_data_get_active_direct(struct ID *screen_id,
                                                     struct ScrArea *area,
                                                     struct Scene *scene);

/**
 * Utility to check whether the r_ptr output of ED_gpencil_data_get_pointers()
 * is for annotation usage.
 */
bool ED_gpencil_data_owner_is_annotation(struct PointerRNA *owner_ptr);

/* 3D View */

/**
 * Check whether there's an active GP keyframe on the current frame.
 */
bool ED_gpencil_has_keyframe_v3d(struct Scene *scene, struct Object *ob, int cfra);

/* ----------- Stroke Editing Utilities ---------------- */
bool ED_gpencil_frame_has_selected_stroke(const struct bGPDframe *gpf);
bool ED_gpencil_layer_has_selected_stroke(const struct bGPDlayer *gpl, bool is_multiedit);

/**
 * Check whether given stroke can be edited given the supplied context.
 * TODO: do we need additional flags for screen-space vs data-space?.
 */
bool ED_gpencil_stroke_can_use_direct(const struct ScrArea *area, const struct bGPDstroke *gps);
/* Check whether given stroke can be edited in the current context */
bool ED_gpencil_stroke_can_use(const struct bContext *C, const struct bGPDstroke *gps);
/* Check whether given stroke can be edited for the current color */
bool ED_gpencil_stroke_material_editable(struct Object *ob,
                                         const struct bGPDlayer *gpl,
                                         const struct bGPDstroke *gps);
/* Check whether given stroke is visible for the current material. */
bool ED_gpencil_stroke_material_visible(struct Object *ob, const struct bGPDstroke *gps);

/* ----------- Grease Pencil Operators ----------------- */

void ED_keymap_gpencil(struct wmKeyConfig *keyconf);

void ED_operatortypes_gpencil(void);
void ED_operatormacros_gpencil(void);

/* ------------- Copy-Paste Buffers -------------------- */

/* Strokes copybuf */

/**
 * Free copy/paste buffer data.
 */
void ED_gpencil_strokes_copybuf_free(void);

/* ------------ Grease-Pencil Drawing API ------------------ */
/* drawgpencil.c */

/**
 * Draw grease-pencil sketches to specified 2d-view that uses ibuf corrections.
 */
void ED_annotation_draw_2dimage(const struct bContext *C);
/**
 * Draw grease-pencil sketches to specified 2d-view
 * assuming that matrices are already set correctly.
 *
 * \note This gets called twice - first time with onlyv2d=true to draw 'canvas' strokes,
 * second time with onlyv2d=false for screen-aligned strokes.
 */
void ED_annotation_draw_view2d(const struct bContext *C, bool onlyv2d);
/**
 * Draw annotations sketches to specified 3d-view assuming that matrices are already set correctly.
 * NOTE: this gets called twice - first time with only3d=true to draw 3d-strokes,
 * second time with only3d=false for screen-aligned strokes.
 */
void ED_annotation_draw_view3d(struct Scene *scene,
                               struct Depsgraph *depsgraph,
                               struct View3D *v3d,
                               struct ARegion *region,
                               bool only3d);
void ED_annotation_draw_ex(
    struct Scene *scene, struct bGPdata *gpd, int winx, int winy, int cfra, char spacetype);

/* ----------- Grease-Pencil AnimEdit API ------------------ */
/**
 * Loops over the gp-frames for a gp-layer, and applies the given callback.
 */
bool ED_gpencil_layer_frames_looper(struct bGPDlayer *gpl,
                                    struct Scene *scene,
                                    bool (*gpf_cb)(struct bGPDframe *, struct Scene *));
/**
 * Make a listing all the gp-frames in a layer as cfraelems.
 */
void ED_gpencil_layer_make_cfra_list(struct bGPDlayer *gpl, ListBase *elems, bool onlysel);

/**
 * Check if one of the frames in this layer is selected.
 */
bool ED_gpencil_layer_frame_select_check(const struct bGPDlayer *gpl);
/**
 * Set all/none/invert select.
 */
void ED_gpencil_layer_frame_select_set(struct bGPDlayer *gpl, short mode);
/**
 * Select the frames in this layer that occur within the bounds specified.
 */
void ED_gpencil_layer_frames_select_box(struct bGPDlayer *gpl,
                                        float min,
                                        float max,
                                        short select_mode);
/**
 * Select the frames in this layer that occur within the lasso/circle region specified.
 */
void ED_gpencil_layer_frames_select_region(struct KeyframeEditData *ked,
                                           struct bGPDlayer *gpl,
                                           short tool,
                                           short select_mode);
/**
 * Set all/none/invert select (like above, but with SELECT_* modes).
 */
void ED_gpencil_select_frames(struct bGPDlayer *gpl, short select_mode);
/**
 * Select the frame in this layer that occurs on this frame (there should only be one at most).
 */
void ED_gpencil_select_frame(struct bGPDlayer *gpl, int selx, short select_mode);

/**
 * Delete selected frames.
 */
bool ED_gpencil_layer_frames_delete(struct bGPDlayer *gpl);
/**
 * Duplicate selected frames from given gp-layer.
 */
void ED_gpencil_layer_frames_duplicate(struct bGPDlayer *gpl);

/**
 * Merge two layers.
 */
void ED_gpencil_layer_merge(struct bGPdata *gpd,
                            struct bGPDlayer *gpl_src,
                            struct bGPDlayer *gpl_dst,
                            bool reverse);

/**
 * Set keyframe type for selected frames from given gp-layer
 *
 * \param type: The type of keyframe (#eBezTriple_KeyframeType) to set selected frames to.
 */
void ED_gpencil_layer_frames_keytype_set(struct bGPDlayer *gpl, short type);
/**
 * Snap selected frames to ....
 */
void ED_gpencil_layer_snap_frames(struct bGPDlayer *gpl, struct Scene *scene, short mode);

/**
 * Mirror selected gp-frames on...
 * TODO: mirror over a specific time.
 */
void ED_gpencil_layer_mirror_frames(struct bGPDlayer *gpl, struct Scene *scene, short mode);

/**
 * This function frees any MEM_calloc'ed copy/paste buffer data.
 */
void ED_gpencil_anim_copybuf_free(void);
/**
 * This function adds data to the copy/paste buffer, freeing existing data first
 * Only the selected GP-layers get their selected keyframes copied.
 *
 * Returns whether the copy operation was successful or not.
 */
bool ED_gpencil_anim_copybuf_copy(struct bAnimContext *ac);
/**
 * Pastes keyframes from buffer, and reports success.
 */
bool ED_gpencil_anim_copybuf_paste(struct bAnimContext *ac, short copy_mode);

/* ------------ Grease-Pencil Undo System ------------------ */
int ED_gpencil_session_active(void);
/**
 * \param step: eUndoStepDir.
 */
int ED_undo_gpencil_step(struct bContext *C, int step); /* eUndoStepDir. */

/* ------------ Grease-Pencil Armature ------------------ */
bool ED_gpencil_add_armature(const struct bContext *C,
                             struct ReportList *reports,
                             struct Object *ob,
                             struct Object *ob_arm);
bool ED_gpencil_add_armature_weights(const struct bContext *C,
                                     struct ReportList *reports,
                                     struct Object *ob,
                                     struct Object *ob_arm,
                                     int mode);

/**
 * Add Lattice modifier using Parent operator.
 * Parent GPencil object to Lattice.
 */
bool ED_gpencil_add_lattice_modifier(const struct bContext *C,
                                     struct ReportList *reports,
                                     struct Object *ob,
                                     struct Object *ob_latt);

/* keep this aligned with gpencil_armature enum */
#define GP_PAR_ARMATURE_NAME 0
#define GP_PAR_ARMATURE_AUTO 1

/* ------------ Transformation Utilities ------------ */

/**
 * Reset parent matrix for all layers.
 */
void ED_gpencil_reset_layers_parent(struct Depsgraph *depsgraph,
                                    struct Object *obact,
                                    struct bGPdata *gpd);

/* Cursor utilities. */

/**
 * Draw eraser cursor.
 */
void ED_gpencil_brush_draw_eraser(struct Brush *brush, int x, int y);

/* ----------- Add Primitive Utilities -------------- */

/** Number of values defining each point in the built-in data buffers for primitives. */
#define GP_PRIM_DATABUF_SIZE 5
/**
 * Populate stroke with point data from data buffers.
 * \param gps: Grease pencil stroke
 * \param array: Flat array of point data values. Each entry has #GP_PRIM_DATABUF_SIZE values.
 * \param totpoints: Total of points
 * \param mat: 4x4 transform matrix to transform points into the right coordinate space.
 */
void ED_gpencil_stroke_init_data(struct bGPDstroke *gps,
                                 const float *array,
                                 int totpoints,
                                 const float mat[4][4]);

/**
 * Add a Simple empty object with one layer and one color.
 */
void ED_gpencil_create_blank(struct bContext *C, struct Object *ob, float mat[4][4]);
/**
 * Add a 2D Suzanne (original model created by Matias Mendiola).
 */
void ED_gpencil_create_monkey(struct bContext *C, struct Object *ob, float mat[4][4]);
/**
 * Add a Simple stroke with colors
 * (original design created by Daniel M. Lara and Matias Mendiola).
 */
void ED_gpencil_create_stroke(struct bContext *C, struct Object *ob, float mat[4][4]);
/**
 * Add a Simple LineArt setup.
 */
void ED_gpencil_create_lineart(struct bContext *C, struct Object *ob);

/* ------------ Object Utilities ------------ */
/**
 * Helper function to create new #OB_GPENCIL Object.
 */
struct Object *ED_gpencil_add_object(struct bContext *C,
                                     const float loc[3],
                                     unsigned short local_view_bits);
/**
 * Helper function to create default colors and drawing brushes.
 */
void ED_gpencil_add_defaults(struct bContext *C, struct Object *ob);
/**
 * Set object modes.
 */
void ED_gpencil_setup_modes(struct bContext *C, struct bGPdata *gpd, int newmode);
bool ED_object_gpencil_exit(struct Main *bmain, struct Object *ob);

/**
 * Reproject all points of the stroke to a plane locked to axis to avoid stroke offset
 */
void ED_gpencil_project_stroke_to_plane(const struct Scene *scene,
                                        const struct Object *ob,
                                        const struct RegionView3D *rv3d,
                                        struct bGPDlayer *gpl,
                                        struct bGPDstroke *gps,
                                        const float origin[3],
                                        int axis);
/**
 * Reproject given point to a plane locked to axis to avoid stroke offset
 * \param pt: Point to affect (used for input & output).
 */
void ED_gpencil_project_point_to_plane(const struct Scene *scene,
                                       const struct Object *ob,
                                       struct bGPDlayer *gpl,
                                       const struct RegionView3D *rv3d,
                                       const float origin[3],
                                       int axis,
                                       struct bGPDspoint *pt);
/**
 * Get drawing reference point for conversion or projection of the stroke
 * \param r_vec: Reference point found
 */
void ED_gpencil_drawing_reference_get(const struct Scene *scene,
                                      const struct Object *ob,
                                      char align_flag,
                                      float r_vec[3]);
void ED_gpencil_project_stroke_to_view(struct bContext *C,
                                       struct bGPDlayer *gpl,
                                       struct bGPDstroke *gps);

/**
 * Reproject selected strokes.
 */
void ED_gpencil_stroke_reproject(struct Depsgraph *depsgraph,
                                 const struct GP_SpaceConversion *gsc,
                                 struct SnapObjectContext *sctx,
                                 struct bGPDlayer *gpl,
                                 struct bGPDframe *gpf,
                                 struct bGPDstroke *gps,
                                 eGP_ReprojectModes mode,
                                 bool keep_original);

/**
 * Turn brush cursor in on/off.
 */
void ED_gpencil_toggle_brush_cursor(struct bContext *C, bool enable, void *customdata);

/* vertex groups */

/**
 * Assign points to vertex group.
 */
void ED_gpencil_vgroup_assign(struct bContext *C, struct Object *ob, float weight);
/**
 * Remove points from vertex group.
 */
void ED_gpencil_vgroup_remove(struct bContext *C, struct Object *ob);
/**
 * Select points of vertex group.
 */
void ED_gpencil_vgroup_select(struct bContext *C, struct Object *ob);
/**
 * Un-select points of vertex group.
 */
void ED_gpencil_vgroup_deselect(struct bContext *C, struct Object *ob);

/* join objects */

/**
 * Join objects called from OBJECT_OT_join.
 */
int ED_gpencil_join_objects_exec(struct bContext *C, struct wmOperator *op);

/* texture coordinate utilities */

/**
 * Convert 2d #tGPspoint to 3d #bGPDspoint.
 */
void ED_gpencil_tpoint_to_point(struct ARegion *region,
                                float origin[3],
                                const struct tGPspoint *tpt,
                                struct bGPDspoint *pt);
/**
 * Recalculate UV for any stroke using the material.
 */
void ED_gpencil_update_color_uv(struct Main *bmain, struct Material *mat);

/**
 * Extend selection to stroke intersections:
 * \returns:
 * 0 - No hit
 * 1 - Hit in point A
 * 2 - Hit in point B
 * 3 - Hit in point A and B
 */
int ED_gpencil_select_stroke_segment(struct bGPdata *gpd,
                                     struct bGPDlayer *gpl,
                                     struct bGPDstroke *gps,
                                     struct bGPDspoint *pt,
                                     bool select,
                                     bool insert,
                                     float scale,
                                     float r_hita[3],
                                     float r_hitb[3]);

void ED_gpencil_select_toggle_all(struct bContext *C, int action);
void ED_gpencil_select_curve_toggle_all(struct bContext *C, int action);

/**
 * Ensure the #tGPspoint buffer (while drawing stroke)
 * size is enough to save all points of the stroke.
 */
struct tGPspoint *ED_gpencil_sbuffer_ensure(struct tGPspoint *buffer_array,
                                            int *buffer_size,
                                            int *buffer_used,
                                            bool clear);
void ED_gpencil_sbuffer_update_eval(struct bGPdata *gpd, struct Object *ob_eval);

/**
 * Tag all scene grease pencil object to update.
 */
void ED_gpencil_tag_scene_gpencil(struct Scene *scene);

/* Vertex color set. */

void ED_gpencil_fill_vertex_color_set(struct ToolSettings *ts,
                                      struct Brush *brush,
                                      struct bGPDstroke *gps);
void ED_gpencil_point_vertex_color_set(struct ToolSettings *ts,
                                       struct Brush *brush,
                                       struct bGPDspoint *pt,
                                       struct tGPspoint *tpt);
void ED_gpencil_sbuffer_vertex_color_set(struct Depsgraph *depsgraph,
                                         struct Object *ob,
                                         struct ToolSettings *ts,
                                         struct Brush *brush,
                                         struct Material *material,
                                         float random_color[3],
                                         float pen_pressure);
void ED_gpencil_init_random_settings(struct Brush *brush,
                                     const int mval[2],
                                     struct GpRandomSettings *random_settings);

/**
 * Check if the stroke collides with brush.
 */
bool ED_gpencil_stroke_check_collision(const struct GP_SpaceConversion *gsc,
                                       struct bGPDstroke *gps,
                                       const float mouse[2],
                                       int radius,
                                       const float diff_mat[4][4]);
/**
 * Check if a point is inside of the stroke.
 *
 * \param gps: Stroke to check.
 * \param gsc: Space conversion data.
 * \param mouse: Mouse position.
 * \param diff_mat: View matrix.
 * \return True if the point is inside.
 */
bool ED_gpencil_stroke_point_is_inside(const struct bGPDstroke *gps,
                                       const struct GP_SpaceConversion *gsc,
                                       const int mouse[2],
                                       const float diff_mat[4][4]);
/**
 * Get the bigger 2D bound box points.
 */
void ED_gpencil_projected_2d_bound_box(const struct GP_SpaceConversion *gsc,
                                       const struct bGPDstroke *gps,
                                       const float diff_mat[4][4],
                                       float r_min[2],
                                       float r_max[2]);

struct bGPDstroke *ED_gpencil_stroke_nearest_to_ends(struct bContext *C,
                                                     const struct GP_SpaceConversion *gsc,
                                                     struct bGPDlayer *gpl,
                                                     struct bGPDframe *gpf,
                                                     struct bGPDstroke *gps,
                                                     const float ctrl1[2],
                                                     const float ctrl2[2],
                                                     float radius,
                                                     int *r_index);
/**
 * Get extremes of stroke in 2D using current view.
 */
void ED_gpencil_stroke_extremes_to2d(const struct GP_SpaceConversion *gsc,
                                     const float diff_mat[4][4],
                                     struct bGPDstroke *gps,
                                     float r_ctrl1[2],
                                     float r_ctrl2[2]);

/**
 * Join two stroke using a contact point index and trimming the rest.
 */
struct bGPDstroke *ED_gpencil_stroke_join_and_trim(struct bGPdata *gpd,
                                                   struct bGPDframe *gpf,
                                                   struct bGPDstroke *gps,
                                                   struct bGPDstroke *gps_dst,
                                                   int pt_index);

/**
 * Close if the distance between extremes is below threshold.
 */
void ED_gpencil_stroke_close_by_distance(struct bGPDstroke *gps, float threshold);

#ifdef __cplusplus
}
#endif
