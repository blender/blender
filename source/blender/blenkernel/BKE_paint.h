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
 * The Original Code is Copyright (C) 2009 by Nicholas Bishop
 * All rights reserved.
 */

#ifndef __BKE_PAINT_H__
#define __BKE_PAINT_H__

/** \file
 * \ingroup bke
 */

#include "BLI_utildefines.h"
#include "DNA_object_enums.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BMFace;
struct BMesh;
struct Brush;
struct CurveMapping;
struct Depsgraph;
struct EnumPropertyItem;
struct GHash;
struct GridPaintMask;
struct ImagePool;
struct MLoop;
struct MLoopTri;
struct MVert;
struct Main;
struct Mesh;
struct MeshElemMap;
struct Object;
struct PBVH;
struct Paint;
struct PaintCurve;
struct Palette;
struct PaletteColor;
struct ReportList;
struct Scene;
struct StrokeCache;
struct SubdivCCG;
struct SubdivCCG;
struct Tex;
struct ToolSettings;
struct UnifiedPaintSettings;
struct View3D;
struct ViewLayer;
struct bContext;
struct bToolRef;
struct tPaletteColorHSV;

enum eOverlayFlags;

extern const char PAINT_CURSOR_SCULPT[3];
extern const char PAINT_CURSOR_VERTEX_PAINT[3];
extern const char PAINT_CURSOR_WEIGHT_PAINT[3];
extern const char PAINT_CURSOR_TEXTURE_PAINT[3];

typedef enum ePaintMode {
  PAINT_MODE_SCULPT = 0,
  /** Vertex color. */
  PAINT_MODE_VERTEX = 1,
  PAINT_MODE_WEIGHT = 2,
  /** 3D view (projection painting). */
  PAINT_MODE_TEXTURE_3D = 3,
  /** Image space (2D painting). */
  PAINT_MODE_TEXTURE_2D = 4,
  PAINT_MODE_SCULPT_UV = 5,
  PAINT_MODE_GPENCIL = 6,
  /* Grease Pencil Vertex Paint */
  PAINT_MODE_VERTEX_GPENCIL = 7,
  PAINT_MODE_SCULPT_GPENCIL = 8,
  PAINT_MODE_WEIGHT_GPENCIL = 9,

  /** Keep last. */
  PAINT_MODE_INVALID = 10,
} ePaintMode;

#define PAINT_MODE_HAS_BRUSH(mode) !ELEM(mode, PAINT_MODE_SCULPT_UV)

/* overlay invalidation */
typedef enum ePaintOverlayControlFlags {
  PAINT_OVERLAY_INVALID_TEXTURE_PRIMARY = 1,
  PAINT_OVERLAY_INVALID_TEXTURE_SECONDARY = (1 << 2),
  PAINT_OVERLAY_INVALID_CURVE = (1 << 3),
  PAINT_OVERLAY_OVERRIDE_CURSOR = (1 << 4),
  PAINT_OVERLAY_OVERRIDE_PRIMARY = (1 << 5),
  PAINT_OVERLAY_OVERRIDE_SECONDARY = (1 << 6),
} ePaintOverlayControlFlags;

#define PAINT_OVERRIDE_MASK \
  (PAINT_OVERLAY_OVERRIDE_SECONDARY | PAINT_OVERLAY_OVERRIDE_PRIMARY | \
   PAINT_OVERLAY_OVERRIDE_CURSOR)

/* Defines 8 areas resulting of splitting the object space by the XYZ axis planes. This is used to
 * flip or mirror transform values depending on where the vertex is and where the transform
 * operation started to support XYZ symmetry on those operations in a predictable way. */

#define PAINT_SYMM_AREA_DEFAULT 0

typedef enum ePaintSymmetryAreas {
  PAINT_SYMM_AREA_X = (1 << 0),
  PAINT_SYMM_AREA_Y = (1 << 1),
  PAINT_SYMM_AREA_Z = (1 << 2),
} ePaintSymmetryAreas;

#define PAINT_SYMM_AREAS 8

void BKE_paint_invalidate_overlay_tex(struct Scene *scene,
                                      struct ViewLayer *view_layer,
                                      const struct Tex *tex);
void BKE_paint_invalidate_cursor_overlay(struct Scene *scene,
                                         struct ViewLayer *view_layer,
                                         struct CurveMapping *curve);
void BKE_paint_invalidate_overlay_all(void);
ePaintOverlayControlFlags BKE_paint_get_overlay_flags(void);
void BKE_paint_reset_overlay_invalid(ePaintOverlayControlFlags flag);
void BKE_paint_set_overlay_override(enum eOverlayFlags flag);

/* palettes */
struct Palette *BKE_palette_add(struct Main *bmain, const char *name);
struct Palette *BKE_palette_copy(struct Main *bmain, const struct Palette *palette);
struct PaletteColor *BKE_palette_color_add(struct Palette *palette);
bool BKE_palette_is_empty(const struct Palette *palette);
void BKE_palette_color_remove(struct Palette *palette, struct PaletteColor *color);
void BKE_palette_clear(struct Palette *palette);

void BKE_palette_sort_hsv(struct tPaletteColorHSV *color_array, const int totcol);
void BKE_palette_sort_svh(struct tPaletteColorHSV *color_array, const int totcol);
void BKE_palette_sort_vhs(struct tPaletteColorHSV *color_array, const int totcol);
void BKE_palette_sort_luminance(struct tPaletteColorHSV *color_array, const int totcol);
bool BKE_palette_from_hash(struct Main *bmain,
                           struct GHash *color_table,
                           const char *name,
                           const bool linear);

/* paint curves */
struct PaintCurve *BKE_paint_curve_add(struct Main *bmain, const char *name);
struct PaintCurve *BKE_paint_curve_copy(struct Main *bmain, const struct PaintCurve *pc);

bool BKE_paint_ensure(struct ToolSettings *ts, struct Paint **r_paint);
void BKE_paint_init(struct Main *bmain, struct Scene *sce, ePaintMode mode, const char col[3]);
void BKE_paint_free(struct Paint *p);
void BKE_paint_copy(struct Paint *src, struct Paint *tar, const int flag);

void BKE_paint_runtime_init(const struct ToolSettings *ts, struct Paint *paint);

void BKE_paint_cavity_curve_preset(struct Paint *p, int preset);

eObjectMode BKE_paint_object_mode_from_paintmode(ePaintMode mode);
bool BKE_paint_ensure_from_paintmode(struct Scene *sce, ePaintMode mode);
struct Paint *BKE_paint_get_active_from_paintmode(struct Scene *sce, ePaintMode mode);
const struct EnumPropertyItem *BKE_paint_get_tool_enum_from_paintmode(ePaintMode mode);
const char *BKE_paint_get_tool_prop_id_from_paintmode(ePaintMode mode);
uint BKE_paint_get_brush_tool_offset_from_paintmode(const ePaintMode mode);
struct Paint *BKE_paint_get_active(struct Scene *sce, struct ViewLayer *view_layer);
struct Paint *BKE_paint_get_active_from_context(const struct bContext *C);
ePaintMode BKE_paintmode_get_active_from_context(const struct bContext *C);
ePaintMode BKE_paintmode_get_from_tool(const struct bToolRef *tref);
struct Brush *BKE_paint_brush(struct Paint *paint);
void BKE_paint_brush_set(struct Paint *paint, struct Brush *br);
struct Palette *BKE_paint_palette(struct Paint *paint);
void BKE_paint_palette_set(struct Paint *p, struct Palette *palette);
void BKE_paint_curve_set(struct Brush *br, struct PaintCurve *pc);
void BKE_paint_curve_clamp_endpoint_add_index(struct PaintCurve *pc, const int add_index);

void BKE_paint_data_warning(
    struct ReportList *reports, bool uvs, bool mat, bool tex, bool stencil);
bool BKE_paint_proj_mesh_data_check(
    struct Scene *scene, struct Object *ob, bool *uvs, bool *mat, bool *tex, bool *stencil);

/* testing face select mode
 * Texture paint could be removed since selected faces are not used
 * however hiding faces is useful */
bool BKE_paint_select_face_test(struct Object *ob);
bool BKE_paint_select_vert_test(struct Object *ob);
bool BKE_paint_select_elem_test(struct Object *ob);

/* partial visibility */
bool paint_is_face_hidden(const struct MLoopTri *lt,
                          const struct MVert *mvert,
                          const struct MLoop *mloop);
bool paint_is_grid_face_hidden(const unsigned int *grid_hidden, int gridsize, int x, int y);
bool paint_is_bmesh_face_hidden(struct BMFace *f);

/* paint masks */
float paint_grid_paint_mask(const struct GridPaintMask *gpm, uint level, uint x, uint y);

/* stroke related */
bool paint_calculate_rake_rotation(struct UnifiedPaintSettings *ups,
                                   struct Brush *brush,
                                   const float mouse_pos[2]);
void paint_update_brush_rake_rotation(struct UnifiedPaintSettings *ups,
                                      struct Brush *brush,
                                      float rotation);

void BKE_paint_stroke_get_average(struct Scene *scene, struct Object *ob, float stroke[3]);

/* Tool slot API. */
void BKE_paint_toolslots_init_from_main(struct Main *bmain);
void BKE_paint_toolslots_len_ensure(struct Paint *paint, int len);
void BKE_paint_toolslots_brush_update_ex(struct Paint *paint, struct Brush *brush);
void BKE_paint_toolslots_brush_update(struct Paint *paint);
void BKE_paint_toolslots_brush_validate(struct Main *bmain, struct Paint *paint);
struct Brush *BKE_paint_toolslots_brush_get(struct Paint *paint, int slot_index);

#define SCULPT_FACE_SET_NONE 0

/* Used for both vertex color and weight paint */
struct SculptVertexPaintGeomMap {
  int *vert_map_mem;
  struct MeshElemMap *vert_to_loop;
  int *poly_map_mem;
  struct MeshElemMap *vert_to_poly;
};

/* Pose Brush IK Chain */
typedef struct SculptPoseIKChainSegment {
  float orig[3];
  float head[3];

  float initial_orig[3];
  float initial_head[3];
  float len;
  float scale[3];
  float rot[4];
  float *weights;

  /* Store a 4x4 transform matrix for each of the possible combinations of enabled XYZ symmetry
   * axis. */
  float trans_mat[PAINT_SYMM_AREAS][4][4];
  float pivot_mat[PAINT_SYMM_AREAS][4][4];
  float pivot_mat_inv[PAINT_SYMM_AREAS][4][4];
} SculptPoseIKChainSegment;

typedef struct SculptPoseIKChain {
  SculptPoseIKChainSegment *segments;
  int tot_segments;
  float grab_delta_offset[3];
} SculptPoseIKChain;

/* Cloth Brush */

typedef struct SculptClothLengthConstraint {
  int v1;
  int v2;

  float length;
} SculptClothLengthConstraint;

typedef struct SculptClothSimulation {
  SculptClothLengthConstraint *length_constraints;
  int tot_length_constraints;
  int capacity_length_constraints;
  float *length_constraint_tweak;

  float mass;
  float damping;

  float (*acceleration)[3];
  float (*pos)[3];
  float (*init_pos)[3];
  float (*prev_pos)[3];

} SculptClothSimulation;

typedef struct SculptLayerPersistentBase {
  float co[3];
  float no[3];
  float disp;
} SculptLayerPersistentBase;

/* Session data (mode-specific) */

typedef struct SculptSession {
  /* Mesh data (not copied) can come either directly from a Mesh, or from a MultiresDM */
  struct { /* Special handling for multires meshes */
    bool active;
    struct MultiresModifierData *modifier;
    int level;
  } multires;

  /* These are always assigned to base mesh data when using PBVH_FACES and PBVH_GRIDS. */
  struct MVert *mvert;
  struct MPoly *mpoly;
  struct MLoop *mloop;

  /* These contain the vertex and poly counts of the final mesh. */
  int totvert, totpoly;

  struct KeyBlock *shapekey_active;
  float *vmask;

  /* Mesh connectivity */
  struct MeshElemMap *pmap;
  int *pmap_mem;

  /* Mesh Face Sets */
  /* Total number of polys of the base mesh. */
  int totfaces;
  /* Face sets store its visibility in the sign of the integer, using the absolute value as the
   * Face Set ID. Positive IDs are visible, negative IDs are hidden. */
  int *face_sets;

  /* BMesh for dynamic topology sculpting */
  struct BMesh *bm;
  int cd_vert_node_offset;
  int cd_face_node_offset;
  bool bm_smooth_shading;
  /* Undo/redo log for dynamic topology sculpting */
  struct BMLog *bm_log;

  /* Limit surface/grids. */
  struct SubdivCCG *subdiv_ccg;

  /* PBVH acceleration structure */
  struct PBVH *pbvh;
  bool show_mask;
  bool show_face_sets;

  /* Painting on deformed mesh */
  bool deform_modifiers_active; /* object is deformed with some modifiers */
  float (*orig_cos)[3];         /* coords of undeformed mesh */
  float (*deform_cos)[3];       /* coords of deformed mesh but without stroke displacement */
  float (*deform_imats)[3][3];  /* crazyspace deformation matrices */

  /* Used to cache the render of the active texture */
  unsigned int texcache_side, *texcache, texcache_actual;
  struct ImagePool *tex_pool;

  struct StrokeCache *cache;
  struct FilterCache *filter_cache;

  /* Cursor data and active vertex for tools */
  int active_vertex_index;

  int active_face_index;
  int active_grid_index;

  float cursor_radius;
  float cursor_location[3];
  float cursor_normal[3];
  float cursor_sampled_normal[3];
  float cursor_view_normal[3];

  /* TODO(jbakker): Replace rv3d adn v3d with ViewContext */
  struct RegionView3D *rv3d;
  struct View3D *v3d;

  /* Dynamic mesh preview */
  int *preview_vert_index_list;
  int preview_vert_index_count;

  /* Pose Brush Preview */
  float pose_origin[3];
  SculptPoseIKChain *pose_ik_chain_preview;

  /* Layer brush persistence between strokes */
  /* This is freed with the PBVH, so it is always in sync with the mesh. */
  SculptLayerPersistentBase *layer_base;

  /* Transform operator */
  float pivot_pos[3];
  float pivot_rot[4];
  float pivot_scale[3];

  float init_pivot_pos[3];
  float init_pivot_rot[4];
  float init_pivot_scale[3];

  union {
    struct {
      struct SculptVertexPaintGeomMap gmap;

      /* For non-airbrush painting to re-apply from the original (MLoop aligned). */
      unsigned int *previous_color;
    } vpaint;

    struct {
      struct SculptVertexPaintGeomMap gmap;
      /* Keep track of how much each vertex has been painted (non-airbrush only). */
      float *alpha_weight;

      /* Needed to continuously re-apply over the same weights (BRUSH_ACCUMULATE disabled).
       * Lazy initialize as needed (flag is set to 1 to tag it as uninitialized). */
      struct MDeformVert *dvert_prev;
    } wpaint;

    /* TODO: identify sculpt-only fields */
    // struct { ... } sculpt;
  } mode;
  eObjectMode mode_type;

  /* This flag prevents PBVH from being freed when creating the vp_handle for texture paint. */
  bool building_vp_handle;

  /**
   * ID data is older than sculpt-mode data.
   * Set #Main.is_memfile_undo_flush_needed when enabling.
   */
  char needs_flush_to_id;

} SculptSession;

void BKE_sculptsession_free(struct Object *ob);
void BKE_sculptsession_free_deformMats(struct SculptSession *ss);
void BKE_sculptsession_free_vwpaint_data(struct SculptSession *ss);
void BKE_sculptsession_bm_to_me(struct Object *ob, bool reorder);
void BKE_sculptsession_bm_to_me_for_render(struct Object *object);

void BKE_sculpt_update_object_for_edit(struct Depsgraph *depsgraph,
                                       struct Object *ob_orig,
                                       bool need_pmap,
                                       bool need_mask);
void BKE_sculpt_update_object_before_eval(struct Object *ob_eval);
void BKE_sculpt_update_object_after_eval(struct Depsgraph *depsgraph, struct Object *ob_eval);

struct MultiresModifierData *BKE_sculpt_multires_active(struct Scene *scene, struct Object *ob);
int BKE_sculpt_mask_layers_ensure(struct Object *ob, struct MultiresModifierData *mmd);
void BKE_sculpt_toolsettings_data_ensure(struct Scene *scene);

struct PBVH *BKE_sculpt_object_pbvh_ensure(struct Depsgraph *depsgraph, struct Object *ob);

void BKE_sculpt_bvh_update_from_ccg(struct PBVH *pbvh, struct SubdivCCG *subdiv_ccg);

bool BKE_sculptsession_use_pbvh_draw(const struct Object *ob, const struct View3D *v3d);

enum {
  SCULPT_MASK_LAYER_CALC_VERT = (1 << 0),
  SCULPT_MASK_LAYER_CALC_LOOP = (1 << 1),
};

#ifdef __cplusplus
}
#endif

#endif
