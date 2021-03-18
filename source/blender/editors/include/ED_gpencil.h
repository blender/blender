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
  float x, y;
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
struct bGPdata **ED_gpencil_data_get_pointers(const struct bContext *C, struct PointerRNA *r_ptr);

struct bGPdata *ED_gpencil_data_get_active(const struct bContext *C);
struct bGPdata *ED_gpencil_data_get_active_evaluated(const struct bContext *C);

/* Context independent (i.e. each required part is passed in instead) */
struct bGPdata **ED_gpencil_data_get_pointers_direct(struct ScrArea *area,
                                                     struct Object *ob,
                                                     struct PointerRNA *r_ptr);
struct bGPdata *ED_gpencil_data_get_active_direct(struct ScrArea *area, struct Object *ob);

struct bGPdata *ED_annotation_data_get_active(const struct bContext *C);
struct bGPdata **ED_annotation_data_get_pointers(const struct bContext *C,
                                                 struct PointerRNA *r_ptr);
struct bGPdata **ED_annotation_data_get_pointers_direct(struct ID *screen_id,
                                                        struct ScrArea *area,
                                                        struct Scene *scene,
                                                        struct PointerRNA *r_ptr);
struct bGPdata *ED_annotation_data_get_active_direct(struct ID *screen_id,
                                                     struct ScrArea *area,
                                                     struct Scene *scene);

bool ED_gpencil_data_owner_is_annotation(struct PointerRNA *owner_ptr);

/* 3D View */
bool ED_gpencil_has_keyframe_v3d(struct Scene *scene, struct Object *ob, int cfra);

/* ----------- Stroke Editing Utilities ---------------- */

bool ED_gpencil_stroke_can_use_direct(const struct ScrArea *area, const struct bGPDstroke *gps);
bool ED_gpencil_stroke_can_use(const struct bContext *C, const struct bGPDstroke *gps);
bool ED_gpencil_stroke_material_editable(struct Object *ob,
                                         const struct bGPDlayer *gpl,
                                         const struct bGPDstroke *gps);

/* ----------- Grease Pencil Operators ----------------- */

void ED_keymap_gpencil(struct wmKeyConfig *keyconf);

void ED_operatortypes_gpencil(void);
void ED_operatormacros_gpencil(void);

/* ------------- Copy-Paste Buffers -------------------- */

/* Strokes copybuf */
void ED_gpencil_strokes_copybuf_free(void);

/* ------------ Grease-Pencil Drawing API ------------------ */
/* drawgpencil.c */

void ED_annotation_draw_2dimage(const struct bContext *C);
void ED_annotation_draw_view2d(const struct bContext *C, bool onlyv2d);
void ED_annotation_draw_view3d(struct Scene *scene,
                               struct Depsgraph *depsgraph,
                               struct View3D *v3d,
                               struct ARegion *region,
                               bool only3d);
void ED_annotation_draw_ex(struct Scene *scene,
                           struct bGPdata *gpd,
                           int winx,
                           int winy,
                           const int cfra,
                           const char spacetype);

/* ----------- Grease-Pencil AnimEdit API ------------------ */
bool ED_gpencil_layer_frames_looper(struct bGPDlayer *gpl,
                                    struct Scene *scene,
                                    bool (*gpf_cb)(struct bGPDframe *, struct Scene *));
void ED_gpencil_layer_make_cfra_list(struct bGPDlayer *gpl, ListBase *elems, bool onlysel);

bool ED_gpencil_layer_frame_select_check(struct bGPDlayer *gpl);
void ED_gpencil_layer_frame_select_set(struct bGPDlayer *gpl, short mode);
void ED_gpencil_layer_frames_select_box(struct bGPDlayer *gpl,
                                        float min,
                                        float max,
                                        short select_mode);
void ED_gpencil_layer_frames_select_region(struct KeyframeEditData *ked,
                                           struct bGPDlayer *gpl,
                                           short tool,
                                           short select_mode);
void ED_gpencil_select_frames(struct bGPDlayer *gpl, short select_mode);
void ED_gpencil_select_frame(struct bGPDlayer *gpl, int selx, short select_mode);

bool ED_gpencil_layer_frames_delete(struct bGPDlayer *gpl);
void ED_gpencil_layer_frames_duplicate(struct bGPDlayer *gpl);

void ED_gpencil_layer_frames_keytype_set(struct bGPDlayer *gpl, short type);

void ED_gpencil_layer_snap_frames(struct bGPDlayer *gpl, struct Scene *scene, short mode);
void ED_gpencil_layer_mirror_frames(struct bGPDlayer *gpl, struct Scene *scene, short mode);

void ED_gpencil_anim_copybuf_free(void);
bool ED_gpencil_anim_copybuf_copy(struct bAnimContext *ac);
bool ED_gpencil_anim_copybuf_paste(struct bAnimContext *ac, const short copy_mode);

/* ------------ Grease-Pencil Undo System ------------------ */
int ED_gpencil_session_active(void);
int ED_undo_gpencil_step(struct bContext *C, const int step); /* eUndoStepDir. */

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

/* Add Lattice modifier using Parent operator */
bool ED_gpencil_add_lattice_modifier(const struct bContext *C,
                                     struct ReportList *reports,
                                     struct Object *ob,
                                     struct Object *ob_latt);

/* keep this aligned with gpencil_armature enum */
#define GP_PAR_ARMATURE_NAME 0
#define GP_PAR_ARMATURE_AUTO 1

/* ------------ Transformation Utilities ------------ */

/* reset parent matrix for all layers */
void ED_gpencil_reset_layers_parent(struct Depsgraph *depsgraph,
                                    struct Object *obact,
                                    struct bGPdata *gpd);

/* cursor utilities */
void ED_gpencil_brush_draw_eraser(struct Brush *brush, int x, int y);

/* ----------- Add Primitive Utilities -------------- */

void ED_gpencil_create_monkey(struct bContext *C, struct Object *ob, float mat[4][4]);
void ED_gpencil_create_stroke(struct bContext *C, struct Object *ob, float mat[4][4]);
void ED_gpencil_create_lineart(struct bContext *C, struct Object *ob);

/* ------------ Object Utilities ------------ */
struct Object *ED_gpencil_add_object(struct bContext *C,
                                     const float loc[3],
                                     unsigned short local_view_bits);
void ED_gpencil_add_defaults(struct bContext *C, struct Object *ob);
/* set object modes */
void ED_gpencil_setup_modes(struct bContext *C, struct bGPdata *gpd, int newmode);
bool ED_object_gpencil_exit(struct Main *bmain, struct Object *ob);

void ED_gpencil_project_stroke_to_plane(const struct Scene *scene,
                                        const struct Object *ob,
                                        const struct RegionView3D *rv3d,
                                        struct bGPDlayer *gpl,
                                        struct bGPDstroke *gps,
                                        const float origin[3],
                                        const int axis);
void ED_gpencil_project_point_to_plane(const struct Scene *scene,
                                       const struct Object *ob,
                                       struct bGPDlayer *gpl,
                                       const struct RegionView3D *rv3d,
                                       const float origin[3],
                                       const int axis,
                                       struct bGPDspoint *pt);
void ED_gpencil_drawing_reference_get(const struct Scene *scene,
                                      const struct Object *ob,
                                      char align_flag,
                                      float vec[3]);
void ED_gpencil_project_stroke_to_view(struct bContext *C,
                                       struct bGPDlayer *gpl,
                                       struct bGPDstroke *gps);

void ED_gpencil_stroke_reproject(struct Depsgraph *depsgraph,
                                 const struct GP_SpaceConversion *gsc,
                                 struct SnapObjectContext *sctx,
                                 struct bGPDlayer *gpl,
                                 struct bGPDframe *gpf,
                                 struct bGPDstroke *gps,
                                 const eGP_ReprojectModes mode,
                                 const bool keep_original);

/* set sculpt cursor */
void ED_gpencil_toggle_brush_cursor(struct bContext *C, bool enable, void *customdata);

/* vertex groups */
void ED_gpencil_vgroup_assign(struct bContext *C, struct Object *ob, float weight);
void ED_gpencil_vgroup_remove(struct bContext *C, struct Object *ob);
void ED_gpencil_vgroup_select(struct bContext *C, struct Object *ob);
void ED_gpencil_vgroup_deselect(struct bContext *C, struct Object *ob);

/* join objects */
int ED_gpencil_join_objects_exec(struct bContext *C, struct wmOperator *op);

/* texture coordinate utilities */
void ED_gpencil_tpoint_to_point(struct ARegion *region,
                                float origin[3],
                                const struct tGPspoint *tpt,
                                struct bGPDspoint *pt);
void ED_gpencil_update_color_uv(struct Main *bmain, struct Material *mat);

/* extend selection to stroke intersections
 * returns:
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
                                     const float scale,
                                     float r_hita[3],
                                     float r_hitb[3]);

void ED_gpencil_select_toggle_all(struct bContext *C, int action);
void ED_gpencil_select_curve_toggle_all(struct bContext *C, int action);

/* Ensure stroke sbuffer size is enough */
struct tGPspoint *ED_gpencil_sbuffer_ensure(struct tGPspoint *buffer_array,
                                            int *buffer_size,
                                            int *buffer_used,
                                            const bool clear);
void ED_gpencil_sbuffer_update_eval(struct bGPdata *gpd, struct Object *ob_eval);

/* Tag all scene grease pencil object to update. */
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

bool ED_gpencil_stroke_check_collision(struct GP_SpaceConversion *gsc,
                                       struct bGPDstroke *gps,
                                       const float mouse[2],
                                       const int radius,
                                       const float diff_mat[4][4]);
bool ED_gpencil_stroke_point_is_inside(struct bGPDstroke *gps,
                                       struct GP_SpaceConversion *gsc,
                                       int mouse[2],
                                       const float diff_mat[4][4]);
void ED_gpencil_projected_2d_bound_box(struct GP_SpaceConversion *gsc,
                                       struct bGPDstroke *gps,
                                       const float diff_mat[4][4],
                                       float r_min[2],
                                       float r_max[2]);

struct bGPDstroke *ED_gpencil_stroke_nearest_to_ends(struct bContext *C,
                                                     struct GP_SpaceConversion *gsc,
                                                     struct bGPDlayer *gpl,
                                                     struct bGPDframe *gpf,
                                                     struct bGPDstroke *gps,
                                                     const float radius,
                                                     int *r_index);

struct bGPDstroke *ED_gpencil_stroke_join_and_trim(struct bGPdata *gpd,
                                                   struct bGPDframe *gpf,
                                                   struct bGPDstroke *gps,
                                                   struct bGPDstroke *gps_dst,
                                                   const int pt_index);

void ED_gpencil_stroke_close_by_distance(struct bGPDstroke *gps, const float threshold);

#ifdef __cplusplus
}
#endif
