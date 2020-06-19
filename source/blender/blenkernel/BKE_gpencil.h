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

#ifndef __BKE_GPENCIL_H__
#define __BKE_GPENCIL_H__

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct Brush;
struct CurveMapping;
struct Depsgraph;
struct GHash;
struct ListBase;
struct MDeformVert;
struct Main;
struct Material;
struct MaterialGPencilStyle;
struct Object;
struct Scene;
struct SpaceImage;
struct ToolSettings;
struct ViewLayer;
struct bDeformGroup;
struct bGPDframe;
struct bGPDlayer;
struct bGPDlayer_Mask;
struct bGPDspoint;
struct bGPDstroke;
struct bGPdata;

#define GPENCIL_SIMPLIFY(scene) ((scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_ENABLE))
#define GPENCIL_SIMPLIFY_ONPLAY(playing) \
  (((playing == true) && (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_ON_PLAY)) || \
   ((scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_ON_PLAY) == 0))
#define GPENCIL_SIMPLIFY_FILL(scene, playing) \
  ((GPENCIL_SIMPLIFY_ONPLAY(playing) && (GPENCIL_SIMPLIFY(scene)) && \
    (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_FILL)))
#define GPENCIL_SIMPLIFY_MODIF(scene) \
  ((GPENCIL_SIMPLIFY(scene) && (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_MODIFIER)))
#define GPENCIL_SIMPLIFY_FX(scene, playing) \
  ((GPENCIL_SIMPLIFY_ONPLAY(playing) && (GPENCIL_SIMPLIFY(scene)) && \
    (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_FX)))
#define GPENCIL_SIMPLIFY_TINT(scene) \
  ((GPENCIL_SIMPLIFY(scene)) && (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_TINT))
#define GPENCIL_SIMPLIFY_AA(scene) \
  ((GPENCIL_SIMPLIFY(scene)) && (scene->r.simplify_gpencil & SIMPLIFY_GPENCIL_AA))

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

void BKE_gpencil_free_point_weights(struct MDeformVert *dvert);
void BKE_gpencil_free_stroke_weights(struct bGPDstroke *gps);
void BKE_gpencil_free_stroke(struct bGPDstroke *gps);
bool BKE_gpencil_free_strokes(struct bGPDframe *gpf);
void BKE_gpencil_free_frames(struct bGPDlayer *gpl);
void BKE_gpencil_free_layers(struct ListBase *list);
void BKE_gpencil_free(struct bGPdata *gpd, bool free_all);
void BKE_gpencil_eval_delete(struct bGPdata *gpd_eval);
void BKE_gpencil_free_layer_masks(struct bGPDlayer *gpl);
void BKE_gpencil_tag(struct bGPdata *gpd);

void BKE_gpencil_batch_cache_dirty_tag(struct bGPdata *gpd);
void BKE_gpencil_batch_cache_free(struct bGPdata *gpd);

void BKE_gpencil_stroke_sync_selection(struct bGPDstroke *gps);

struct bGPDframe *BKE_gpencil_frame_addnew(struct bGPDlayer *gpl, int cframe);
struct bGPDframe *BKE_gpencil_frame_addcopy(struct bGPDlayer *gpl, int cframe);
struct bGPDlayer *BKE_gpencil_layer_addnew(struct bGPdata *gpd, const char *name, bool setactive);
struct bGPdata *BKE_gpencil_data_addnew(struct Main *bmain, const char name[]);

struct bGPDframe *BKE_gpencil_frame_duplicate(const struct bGPDframe *gpf_src);
struct bGPDlayer *BKE_gpencil_layer_duplicate(const struct bGPDlayer *gpl_src);
void BKE_gpencil_frame_copy_strokes(struct bGPDframe *gpf_src, struct bGPDframe *gpf_dst);
struct bGPDstroke *BKE_gpencil_stroke_duplicate(struct bGPDstroke *gps_src, const bool dup_points);

struct bGPdata *BKE_gpencil_copy(struct Main *bmain, const struct bGPdata *gpd);

struct bGPdata *BKE_gpencil_data_duplicate(struct Main *bmain,
                                           const struct bGPdata *gpd,
                                           bool internal_copy);

void BKE_gpencil_frame_delete_laststroke(struct bGPDlayer *gpl, struct bGPDframe *gpf);

/* materials */
void BKE_gpencil_material_index_reassign(struct bGPdata *gpd, int totcol, int index);
bool BKE_gpencil_material_index_used(struct bGPdata *gpd, int index);
void BKE_gpencil_material_remap(struct bGPdata *gpd,
                                const unsigned int *remap,
                                unsigned int remap_len);
bool BKE_gpencil_merge_materials_table_get(struct Object *ob,
                                           const float hue_threshold,
                                           const float sat_threshold,
                                           const float val_threshold,
                                           struct GHash *r_mat_table);

/* statistics functions */
void BKE_gpencil_stats_update(struct bGPdata *gpd);

/* Utilities for creating and populating GP strokes */
/* - Number of values defining each point in the built-in data
 *   buffers for primitives (e.g. 2D Monkey)
 */
#define GP_PRIM_DATABUF_SIZE 5

void BKE_gpencil_stroke_add_points(struct bGPDstroke *gps,
                                   const float *array,
                                   const int totpoints,
                                   const float mat[4][4]);

struct bGPDstroke *BKE_gpencil_stroke_new(int mat_idx, int totpoints, short thickness);
struct bGPDstroke *BKE_gpencil_stroke_add(
    struct bGPDframe *gpf, int mat_idx, int totpoints, short thickness, const bool insert_at_head);

struct bGPDstroke *BKE_gpencil_stroke_add_existing_style(struct bGPDframe *gpf,
                                                         struct bGPDstroke *existing,
                                                         int mat_idx,
                                                         int totpoints,
                                                         short thickness);

/* Stroke and Fill - Alpha Visibility Threshold */
#define GPENCIL_ALPHA_OPACITY_THRESH 0.001f
#define GPENCIL_STRENGTH_MIN 0.003f

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

struct bGPDframe *BKE_gpencil_layer_frame_get(struct bGPDlayer *gpl,
                                              int cframe,
                                              eGP_GetFrame_Mode addnew);
struct bGPDframe *BKE_gpencil_layer_frame_find(struct bGPDlayer *gpl, int cframe);
bool BKE_gpencil_layer_frame_delete(struct bGPDlayer *gpl, struct bGPDframe *gpf);

struct bGPDlayer *BKE_gpencil_layer_named_get(struct bGPdata *gpd, const char *name);
struct bGPDlayer *BKE_gpencil_layer_active_get(struct bGPdata *gpd);
void BKE_gpencil_layer_active_set(struct bGPdata *gpd, struct bGPDlayer *active);
void BKE_gpencil_layer_delete(struct bGPdata *gpd, struct bGPDlayer *gpl);
void BKE_gpencil_layer_autolock_set(struct bGPdata *gpd, const bool unlock);

struct bGPDlayer_Mask *BKE_gpencil_layer_mask_add(struct bGPDlayer *gpl, const char *name);
void BKE_gpencil_layer_mask_remove(struct bGPDlayer *gpl, struct bGPDlayer_Mask *mask);
void BKE_gpencil_layer_mask_remove_ref(struct bGPdata *gpd, const char *name);
struct bGPDlayer_Mask *BKE_gpencil_layer_mask_named_get(struct bGPDlayer *gpl, const char *name);
void BKE_gpencil_layer_mask_sort(struct bGPdata *gpd, struct bGPDlayer *gpl);
void BKE_gpencil_layer_mask_sort_all(struct bGPdata *gpd);
void BKE_gpencil_layer_frames_sort(struct bGPDlayer *gpl, bool *r_has_duplicate_frames);

/* Brush */
struct Material *BKE_gpencil_brush_material_get(struct Brush *brush);
void BKE_gpencil_brush_material_set(struct Brush *brush, struct Material *material);

/* Object */
struct Material *BKE_gpencil_object_material_ensure_active(struct Object *ob);
struct Material *BKE_gpencil_object_material_ensure_from_brush(struct Main *bmain,
                                                               struct Object *ob,
                                                               struct Brush *brush);
int BKE_gpencil_object_material_ensure(struct Main *bmain,
                                       struct Object *ob,
                                       struct Material *material);

struct Material *BKE_gpencil_object_material_new(struct Main *bmain,
                                                 struct Object *ob,
                                                 const char *name,
                                                 int *r_index);

int BKE_gpencil_object_material_index_get(struct Object *ob, struct Material *ma);

struct Material *BKE_gpencil_object_material_from_brush_get(struct Object *ob,
                                                            struct Brush *brush);
int BKE_gpencil_object_material_get_index_from_brush(struct Object *ob, struct Brush *brush);

struct Material *BKE_gpencil_object_material_ensure_from_active_input_toolsettings(
    struct Main *bmain, struct Object *ob, struct ToolSettings *ts);
struct Material *BKE_gpencil_object_material_ensure_from_active_input_brush(struct Main *bmain,
                                                                            struct Object *ob,
                                                                            struct Brush *brush);
struct Material *BKE_gpencil_object_material_ensure_from_active_input_material(struct Object *ob);

bool BKE_gpencil_stroke_select_check(const struct bGPDstroke *gps);

/* vertex groups */
void BKE_gpencil_dvert_ensure(struct bGPDstroke *gps);
void BKE_gpencil_vgroup_remove(struct Object *ob, struct bDeformGroup *defgroup);
void BKE_gpencil_stroke_weights_duplicate(struct bGPDstroke *gps_src, struct bGPDstroke *gps_dst);

/* Set active frame by layer. */
void BKE_gpencil_frame_active_set(struct Depsgraph *depsgraph, struct bGPdata *gpd);

void BKE_gpencil_frame_range_selected(struct bGPDlayer *gpl, int *r_initframe, int *r_endframe);
float BKE_gpencil_multiframe_falloff_calc(
    struct bGPDframe *gpf, int actnum, int f_init, int f_end, struct CurveMapping *cur_falloff);

void BKE_gpencil_palette_ensure(struct Main *bmain, struct Scene *scene);

bool BKE_gpencil_from_image(struct SpaceImage *sima,
                            struct bGPDframe *gpf,
                            const float size,
                            const bool mask);

/* Iterator */
/* frame & stroke are NULL if it is a layer callback. */
typedef void (*gpIterCb)(struct bGPDlayer *layer,
                         struct bGPDframe *frame,
                         struct bGPDstroke *stroke,
                         void *thunk);

void BKE_gpencil_visible_stroke_iter(struct ViewLayer *view_layer,
                                     struct Object *ob,
                                     gpIterCb layer_cb,
                                     gpIterCb stroke_cb,
                                     void *thunk,
                                     bool do_onion,
                                     int cfra);

extern void (*BKE_gpencil_batch_cache_dirty_tag_cb)(struct bGPdata *gpd);
extern void (*BKE_gpencil_batch_cache_free_cb)(struct bGPdata *gpd);

void BKE_gpencil_frame_original_pointers_update(const struct bGPDframe *gpf_orig,
                                                const struct bGPDframe *gpf_eval);
void BKE_gpencil_update_orig_pointers(const struct Object *ob_orig, const struct Object *ob_eval);

void BKE_gpencil_parent_matrix_get(const struct Depsgraph *depsgraph,
                                   struct Object *obact,
                                   struct bGPDlayer *gpl,
                                   float diff_mat[4][4]);

void BKE_gpencil_update_layer_parent(const struct Depsgraph *depsgraph, struct Object *ob);

#ifdef __cplusplus
}
#endif

#endif /*  __BKE_GPENCIL_H__ */
