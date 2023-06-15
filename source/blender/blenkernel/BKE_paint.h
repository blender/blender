/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2009 by Nicholas Bishop. All rights reserved. */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_bitmap.h"
#include "BLI_compiler_compat.h"
#include "BLI_utildefines.h"

#include "DNA_brush_enums.h"
#include "DNA_object_enums.h"

#include "BKE_attribute.h"
#include "BKE_pbvh.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BMFace;
struct BMesh;
struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct Brush;
struct CurveMapping;
struct Depsgraph;
struct EdgeSet;
struct EnumPropertyItem;
struct GHash;
struct GridPaintMask;
struct Image;
struct ImagePool;
struct ImageUser;
struct ListBase;
struct MLoopTri;
struct Main;
struct Mesh;
struct MeshElemMap;
struct Object;
struct PBVH;
struct Paint;
struct PaintCurve;
struct PaintModeSettings;
struct Palette;
struct PaletteColor;
struct RegionView3D;
struct Scene;
struct StrokeCache;
struct Sculpt;
struct SculptSession;
struct SubdivCCG;
struct Tex;
struct ToolSettings;
struct UnifiedPaintSettings;
struct View3D;
struct ViewLayer;
struct bContext;
struct bToolRef;
struct tPaletteColorHSV;

extern const uchar PAINT_CURSOR_SCULPT[3];
extern const uchar PAINT_CURSOR_VERTEX_PAINT[3];
extern const uchar PAINT_CURSOR_WEIGHT_PAINT[3];
extern const uchar PAINT_CURSOR_TEXTURE_PAINT[3];
extern const uchar PAINT_CURSOR_SCULPT_CURVES[3];

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
  /** Curves. */
  PAINT_MODE_SCULPT_CURVES = 10,

  /** Keep last. */
  PAINT_MODE_INVALID = 11,
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
ENUM_OPERATORS(ePaintOverlayControlFlags, PAINT_OVERLAY_OVERRIDE_SECONDARY);

#define PAINT_OVERRIDE_MASK \
  (PAINT_OVERLAY_OVERRIDE_SECONDARY | PAINT_OVERLAY_OVERRIDE_PRIMARY | \
   PAINT_OVERLAY_OVERRIDE_CURSOR)

/**
 * Defines 8 areas resulting of splitting the object space by the XYZ axis planes. This is used to
 * flip or mirror transform values depending on where the vertex is and where the transform
 * operation started to support XYZ symmetry on those operations in a predictable way.
 */
#define PAINT_SYMM_AREA_DEFAULT 0

typedef enum ePaintSymmetryAreas {
  PAINT_SYMM_AREA_X = (1 << 0),
  PAINT_SYMM_AREA_Y = (1 << 1),
  PAINT_SYMM_AREA_Z = (1 << 2),
} ePaintSymmetryAreas;
ENUM_OPERATORS(ePaintSymmetryAreas, PAINT_SYMM_AREA_Z);

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

/* Palettes. */

struct Palette *BKE_palette_add(struct Main *bmain, const char *name);
struct PaletteColor *BKE_palette_color_add(struct Palette *palette);
bool BKE_palette_is_empty(const struct Palette *palette);
/**
 * Remove color from palette. Must be certain color is inside the palette!
 */
void BKE_palette_color_remove(struct Palette *palette, struct PaletteColor *color);
void BKE_palette_clear(struct Palette *palette);

void BKE_palette_sort_hsv(struct tPaletteColorHSV *color_array, int totcol);
void BKE_palette_sort_svh(struct tPaletteColorHSV *color_array, int totcol);
void BKE_palette_sort_vhs(struct tPaletteColorHSV *color_array, int totcol);
void BKE_palette_sort_luminance(struct tPaletteColorHSV *color_array, int totcol);
bool BKE_palette_from_hash(struct Main *bmain,
                           struct GHash *color_table,
                           const char *name,
                           bool linear);

/* Paint curves. */

struct PaintCurve *BKE_paint_curve_add(struct Main *bmain, const char *name);

/**
 * Call when entering each respective paint mode.
 */
bool BKE_paint_ensure(struct ToolSettings *ts, struct Paint **r_paint);
void BKE_paint_init(struct Main *bmain, struct Scene *sce, ePaintMode mode, const uchar col[3]);
void BKE_paint_free(struct Paint *p);
/**
 * Called when copying scene settings, so even if 'src' and 'tar' are the same still do a
 * #id_us_plus(), rather than if we were copying between 2 existing scenes where a matching
 * value should decrease the existing user count as with #paint_brush_set()
 */
void BKE_paint_copy(struct Paint *src, struct Paint *tar, int flag);

void BKE_paint_runtime_init(const struct ToolSettings *ts, struct Paint *paint);

void BKE_paint_cavity_curve_preset(struct Paint *p, int preset);

eObjectMode BKE_paint_object_mode_from_paintmode(ePaintMode mode);
bool BKE_paint_ensure_from_paintmode(struct Scene *sce, ePaintMode mode);
struct Paint *BKE_paint_get_active_from_paintmode(struct Scene *sce, ePaintMode mode);
const struct EnumPropertyItem *BKE_paint_get_tool_enum_from_paintmode(ePaintMode mode);
const char *BKE_paint_get_tool_enum_translation_context_from_paintmode(ePaintMode mode);
const char *BKE_paint_get_tool_prop_id_from_paintmode(ePaintMode mode);
uint BKE_paint_get_brush_tool_offset_from_paintmode(ePaintMode mode);
struct Paint *BKE_paint_get_active(struct Scene *sce, struct ViewLayer *view_layer);
struct Paint *BKE_paint_get_active_from_context(const struct bContext *C);
ePaintMode BKE_paintmode_get_active_from_context(const struct bContext *C);
ePaintMode BKE_paintmode_get_from_tool(const struct bToolRef *tref);
struct Brush *BKE_paint_brush(struct Paint *paint);
const struct Brush *BKE_paint_brush_for_read(const struct Paint *p);
void BKE_paint_brush_set(struct Paint *paint, struct Brush *br);
struct Palette *BKE_paint_palette(struct Paint *paint);
void BKE_paint_palette_set(struct Paint *p, struct Palette *palette);
void BKE_paint_curve_set(struct Brush *br, struct PaintCurve *pc);
void BKE_paint_curve_clamp_endpoint_add_index(struct PaintCurve *pc, int add_index);

/**
 * Return true when in vertex/weight/texture paint + face-select mode?
 */
bool BKE_paint_select_face_test(struct Object *ob);
/**
 * Return true when in vertex/weight paint + vertex-select mode?
 */
bool BKE_paint_select_vert_test(struct Object *ob);
/**
 * used to check if selection is possible
 * (when we don't care if its face or vert)
 */
bool BKE_paint_select_elem_test(struct Object *ob);
/**
 * Checks if face/vertex hiding is always applied in the current mode.
 * Returns true in vertex/weight paint.
 */
bool BKE_paint_always_hide_test(struct Object *ob);

/* Partial visibility. */

/**
 * Returns non-zero if any of the face's vertices are hidden, zero otherwise.
 */
bool paint_is_face_hidden(const int *looptri_polys, const bool *hide_poly, int tri_index);
/**
 * Returns non-zero if any of the corners of the grid
 * face whose inner corner is at (x, y) are hidden, zero otherwise.
 */
bool paint_is_grid_face_hidden(const unsigned int *grid_hidden, int gridsize, int x, int y);
/**
 * Return true if all vertices in the face are visible, false otherwise.
 */
bool paint_is_bmesh_face_hidden(struct BMFace *f);

/* Paint masks. */

float paint_grid_paint_mask(const struct GridPaintMask *gpm, uint level, uint x, uint y);

void BKE_paint_face_set_overlay_color_get(int face_set, int seed, uchar r_color[4]);

/* Stroke related. */

bool paint_calculate_rake_rotation(struct UnifiedPaintSettings *ups,
                                   struct Brush *brush,
                                   const float mouse_pos[2],
                                   ePaintMode paint_mode);
void paint_update_brush_rake_rotation(struct UnifiedPaintSettings *ups,
                                      struct Brush *brush,
                                      float rotation);

void BKE_paint_stroke_get_average(struct Scene *scene, struct Object *ob, float stroke[3]);

/* Tool slot API. */

void BKE_paint_toolslots_init_from_main(struct Main *bmain);
void BKE_paint_toolslots_len_ensure(struct Paint *paint, int len);
void BKE_paint_toolslots_brush_update_ex(struct Paint *paint, struct Brush *brush);
void BKE_paint_toolslots_brush_update(struct Paint *paint);
/**
 * Run this to ensure brush types are set for each slot on entering modes
 * (for new scenes for example).
 */
void BKE_paint_toolslots_brush_validate(struct Main *bmain, struct Paint *paint);
struct Brush *BKE_paint_toolslots_brush_get(struct Paint *paint, int slot_index);

/* .blend I/O */

void BKE_paint_blend_write(struct BlendWriter *writer, struct Paint *paint);
void BKE_paint_blend_read_data(struct BlendDataReader *reader,
                               const struct Scene *scene,
                               struct Paint *paint);
void BKE_paint_blend_read_lib(struct BlendLibReader *reader,
                              struct Scene *scene,
                              struct Paint *paint);

#define SCULPT_FACE_SET_NONE 0

/** Used for both vertex color and weight paint. */
struct SculptVertexPaintGeomMap {
  int *vert_map_mem;
  struct MeshElemMap *vert_to_loop;
  int *poly_map_mem;
  struct MeshElemMap *vert_to_poly;
};

/** Pose Brush IK Chain. */
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

/* Cloth Simulation. */
typedef enum eSculptClothNodeSimState {
  /* Constraints were not built for this node, so it can't be simulated. */
  SCULPT_CLOTH_NODE_UNINITIALIZED,

  /* There are constraints for the geometry in this node, but it should not be simulated. */
  SCULPT_CLOTH_NODE_INACTIVE,

  /* There are constraints for this node and they should be used by the solver. */
  SCULPT_CLOTH_NODE_ACTIVE,
} eSculptClothNodeSimState;

typedef enum eSculptClothConstraintType {
  /* Constraint that creates the structure of the cloth. */
  SCULPT_CLOTH_CONSTRAINT_STRUCTURAL = 0,
  /* Constraint that references the position of a vertex and a position in deformation_pos which
   * can be deformed by the tools. */
  SCULPT_CLOTH_CONSTRAINT_DEFORMATION = 1,
  /* Constraint that references the vertex position and a editable soft-body position for
   * plasticity. */
  SCULPT_CLOTH_CONSTRAINT_SOFTBODY = 2,
  /* Constraint that references the vertex position and its initial position. */
  SCULPT_CLOTH_CONSTRAINT_PIN = 3,
} eSculptClothConstraintType;

typedef struct SculptClothLengthConstraint {
  /* Elements that are affected by the constraint. */
  /* Element a should always be a mesh vertex with the index stored in elem_index_a as it is always
   * deformed. Element b could be another vertex of the same mesh or any other position (arbitrary
   * point, position for a previous state). In that case, elem_index_a and elem_index_b should be
   * the same to avoid affecting two different vertices when solving the constraints.
   * *elem_position points to the position which is owned by the element. */
  int elem_index_a;
  float *elem_position_a;

  int elem_index_b;
  float *elem_position_b;

  float length;
  float strength;

  /* Index in #SculptClothSimulation.node_state of the node from where this constraint was created.
   * This constraints will only be used by the solver if the state is active. */
  int node;

  eSculptClothConstraintType type;
} SculptClothLengthConstraint;

typedef struct SculptClothSimulation {
  SculptClothLengthConstraint *length_constraints;
  int tot_length_constraints;
  struct EdgeSet *created_length_constraints;
  int capacity_length_constraints;
  float *length_constraint_tweak;

  /* Position anchors for deformation brushes. These positions are modified by the brush and the
   * final positions of the simulated vertices are updated with constraints that use these points
   * as targets. */
  float (*deformation_pos)[3];
  float *deformation_strength;

  float mass;
  float damping;
  float softbody_strength;

  float (*acceleration)[3];
  float (*pos)[3];
  float (*init_pos)[3];
  float (*init_no)[3];
  float (*softbody_pos)[3];
  float (*prev_pos)[3];
  float (*last_iteration_pos)[3];

  struct ListBase *collider_list;

  int totnode;
  /** #PBVHNode pointer as a key, index in #SculptClothSimulation.node_state as value. */
  struct GHash *node_state_index;
  eSculptClothNodeSimState *node_state;
} SculptClothSimulation;

typedef struct SculptPersistentBase {
  float co[3];
  float no[3];
  float disp;
} SculptPersistentBase;

typedef struct SculptVertexInfo {
  /* Indexed by base mesh vertex index, stores if that vertex is a boundary. */
  BLI_bitmap *boundary;
} SculptVertexInfo;

typedef struct SculptBoundaryEditInfo {
  /* Vertex index from where the topology propagation reached this vertex. */
  int original_vertex_i;

  /* How many steps were needed to reach this vertex from the boundary. */
  int propagation_steps_num;

  /* Strength that is used to deform this vertex. */
  float strength_factor;
} SculptBoundaryEditInfo;

/* Edge for drawing the boundary preview in the cursor. */
typedef struct SculptBoundaryPreviewEdge {
  PBVHVertRef v1;
  PBVHVertRef v2;
} SculptBoundaryPreviewEdge;

typedef struct SculptBoundary {
  /* Vertex indices of the active boundary. */
  PBVHVertRef *verts;
  int verts_capacity;
  int verts_num;

  /* Distance from a vertex in the boundary to initial vertex indexed by vertex index, taking into
   * account the length of all edges between them. Any vertex that is not in the boundary will have
   * a distance of 0. */
  float *distance;

  /* Data for drawing the preview. */
  SculptBoundaryPreviewEdge *edges;
  int edges_capacity;
  int edges_num;

  /* True if the boundary loops into itself. */
  bool forms_loop;

  /* Initial vertex in the boundary which is closest to the current sculpt active vertex. */
  PBVHVertRef initial_vertex;
  int initial_vertex_i;

  /* Vertex that at max_propagation_steps from the boundary and closest to the original active
   * vertex that was used to initialize the boundary. This is used as a reference to check how much
   * the deformation will go into the mesh and to calculate the strength of the brushes. */
  PBVHVertRef pivot_vertex;

  /* Stores the initial positions of the pivot and boundary initial vertex as they may be deformed
   * during the brush action. This allows to use them as a reference positions and vectors for some
   * brush effects. */
  float initial_vertex_position[3];
  float initial_pivot_position[3];

  /* Maximum number of topology steps that were calculated from the boundary. */
  int max_propagation_steps;

  /* Indexed by vertex index, contains the topology information needed for boundary deformations.
   */
  struct SculptBoundaryEditInfo *edit_info;

  /* Bend Deform type. */
  struct {
    float (*pivot_rotation_axis)[3];
    float (*pivot_positions)[3];
  } bend;

  /* Slide Deform type. */
  struct {
    float (*directions)[3];
  } slide;

  /* Twist Deform type. */
  struct {
    float rotation_axis[3];
    float pivot_position[3];
  } twist;
} SculptBoundary;

typedef struct SculptFakeNeighbors {
  bool use_fake_neighbors;

  /* Max distance used to calculate neighborhood information. */
  float current_max_distance;

  /* Indexed by vertex, stores the vertex index of its fake neighbor if available. */
  int *fake_neighbor_index;

} SculptFakeNeighbors;

/* Session data (mode-specific) */

/* Custom Temporary Attributes */

typedef struct SculptAttributeParams {
  /* Allocate a flat array outside the CustomData system.  Cannot be combined with permanent. */
  int simple_array : 1;

  /* Do not mark CustomData layer as temporary.  Cannot be combined with simple_array.  Doesn't
   * work with PBVH_GRIDS.
   */
  int permanent : 1;   /* Cannot be combined with simple_array. */
  int stroke_only : 1; /* Release layer at end of struct */
} SculptAttributeParams;

typedef struct SculptAttribute {
  /* Domain, data type and name */
  eAttrDomain domain;
  eCustomDataType proptype;
  char name[MAX_CUSTOMDATA_LAYER_NAME];

  /* Source layer on mesh/bmesh, if any. */
  struct CustomDataLayer *layer;

  /* Data stored as flat array. */
  void *data;
  int elem_size, elem_num;
  bool data_for_bmesh; /* Temporary data store as array outside of bmesh. */

  /* Data is a flat array outside the CustomData system.
   * This will be true if simple_array is requested in
   * SculptAttributeParams, or the PBVH type is PBVH_GRIDS or PBVH_BMESH.
   */
  bool simple_array;
  /* Data stored per BMesh element. */
  int bmesh_cd_offset;

  /* Sculpt usage */
  SculptAttributeParams params;

  /**
   * Used to keep track of which pre-allocated SculptAttribute instances
   * inside of SculptSession.temp_attribute are used.
   */
  bool used;
} SculptAttribute;

#define SCULPT_MAX_ATTRIBUTES 64

/* Get a standard attribute name.  Key must match up with a member
 * of SculptAttributePointers.
 */

#define SCULPT_ATTRIBUTE_NAME(key) \
  (offsetof(SculptAttributePointers, key) >= 0 ? /* Spellcheck name. */ \
       (".sculpt_" #key)                         /* Make name. */ \
       : \
       "You misspelled the layer name key")

/* Convenience pointers for standard sculpt attributes. */

typedef struct SculptAttributePointers {
  /* Persistent base. */
  SculptAttribute *persistent_co;
  SculptAttribute *persistent_no;
  SculptAttribute *persistent_disp;

  /* Precomputed auto-mask factor indexed by vertex, owned by the auto-masking system and
   * initialized in #SCULPT_automasking_cache_init when needed. */
  SculptAttribute *automasking_factor;
  SculptAttribute *automasking_occlusion; /* CD_PROP_INT8. */
  SculptAttribute *automasking_stroke_id;
  SculptAttribute *automasking_cavity;

  SculptAttribute *topology_island_key; /* CD_PROP_INT8 */

  /* BMesh */
  SculptAttribute *dyntopo_node_id_vertex;
  SculptAttribute *dyntopo_node_id_face;
} SculptAttributePointers;

#ifdef __cplusplus

typedef struct SculptSession {
  /* Mesh data (not copied) can come either directly from a Mesh, or from a MultiresDM */
  struct { /* Special handling for multires meshes */
    bool active;
    struct MultiresModifierData *modifier;
    int level;
  } multires;

  /* Depsgraph for the Cloth Brush solver to get the colliders. */
  struct Depsgraph *depsgraph;

  /* These are always assigned to base mesh data when using PBVH_FACES and PBVH_GRIDS. */
  float (*vert_positions)[3];
  blender::OffsetIndices<int> polys;
  blender::Span<int> corner_verts;

  /* These contain the vertex and poly counts of the final mesh. */
  int totvert, totpoly;

  struct KeyBlock *shapekey_active;
  struct MPropCol *vcol;
  struct MLoopCol *mcol;

  eAttrDomain vcol_domain;
  eCustomDataType vcol_type;

  float *vmask;

  /* Mesh connectivity maps. */
  /* Vertices to adjacent polys. */
  struct MeshElemMap *pmap;
  int *pmap_mem;

  /* Edges to adjacent polys. */
  struct MeshElemMap *epmap;
  int *epmap_mem;

  /* Vertices to adjacent edges. */
  struct MeshElemMap *vemap;
  int *vemap_mem;

  /* Mesh Face Sets */
  /* Total number of polys of the base mesh. */
  int totfaces;

  /* The 0 ID is not used by the tools or the visibility system, it is just used when creating new
   * geometry (the trim tool, for example) to detect which geometry was just added, so it can be
   * assigned a valid Face Set after creation. Tools are not intended to run with Face Sets IDs set
   * to 0. */
  int *face_sets;
  /**
   * A reference to the ".hide_poly" attribute, to store whether (base) polygons are hidden.
   * May be null.
   */
  bool *hide_poly;

  /* BMesh for dynamic topology sculpting */
  struct BMesh *bm;
  bool bm_smooth_shading;
  /* Undo/redo log for dynamic topology sculpting */
  struct BMLog *bm_log;

  /* Limit surface/grids. */
  struct SubdivCCG *subdiv_ccg;

  /* PBVH acceleration structure */
  struct PBVH *pbvh;

  /* Painting on deformed mesh */
  bool deform_modifiers_active; /* Object is deformed with some modifiers. */
  float (*orig_cos)[3];         /* Coords of un-deformed mesh. */
  float (*deform_cos)[3];       /* Coords of deformed mesh but without stroke displacement. */
  float (*deform_imats)[3][3];  /* Crazy-space deformation matrices. */

  /* Pool for texture evaluations. */
  struct ImagePool *tex_pool;

  struct StrokeCache *cache;
  struct FilterCache *filter_cache;
  struct ExpandCache *expand_cache;

  /* Cursor data and active vertex for tools */
  PBVHVertRef active_vertex;

  int active_face_index;
  int active_grid_index;

  /* When active, the cursor draws with faded colors, indicating that there is an action
   * enabled.
   */
  bool draw_faded_cursor;
  float cursor_radius;
  float cursor_location[3];
  float cursor_normal[3];
  float cursor_sampled_normal[3];
  float cursor_view_normal[3];

  /* For Sculpt trimming gesture tools, initial ray-cast data from the position of the mouse
   * when
   * the gesture starts (intersection with the surface and if they ray hit the surface or not).
   */
  float gesture_initial_location[3];
  float gesture_initial_normal[3];
  bool gesture_initial_hit;

  /* TODO(jbakker): Replace rv3d and v3d with ViewContext */
  struct RegionView3D *rv3d;
  struct View3D *v3d;
  struct Scene *scene;

  /* Dynamic mesh preview */
  PBVHVertRef *preview_vert_list;
  int preview_vert_count;

  /* Pose Brush Preview */
  float pose_origin[3];
  SculptPoseIKChain *pose_ik_chain_preview;

  /* Boundary Brush Preview */
  SculptBoundary *boundary_preview;

  SculptVertexInfo vertex_info;
  SculptFakeNeighbors fake_neighbors;

  /* Transform operator */
  float pivot_pos[3];
  float pivot_rot[4];
  float pivot_scale[3];

  float init_pivot_pos[3];
  float init_pivot_rot[4];
  float init_pivot_scale[3];

  float prev_pivot_pos[3];
  float prev_pivot_rot[4];
  float prev_pivot_scale[3];

  union {
    struct {
      struct SculptVertexPaintGeomMap gmap;
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

  /* This is a fixed-size array so we can pass pointers to its elements
   * to client code. This is important to keep bmesh offsets up to date.
   */
  struct SculptAttribute temp_attributes[SCULPT_MAX_ATTRIBUTES];

  /* Convenience #SculptAttribute pointers. */
  SculptAttributePointers attrs;

  /**
   * Some tools follows the shading chosen by the last used tool canvas.
   * When not set the viewport shading color would be used.
   *
   * NOTE: This setting is temporarily until paint mode is added.
   */
  bool sticky_shading_color;

  uchar stroke_id;

  /**
   * Last used painting canvas key.
   */
  char *last_paint_canvas_key;
  float last_normal[3];

  int last_automasking_settings_hash;
  uchar last_automask_stroke_id;
  bool islands_valid; /* Is attrs.topology_island_key valid? */
} SculptSession;

#endif

void BKE_sculptsession_free(struct Object *ob);
void BKE_sculptsession_free_deformMats(struct SculptSession *ss);
void BKE_sculptsession_free_vwpaint_data(struct SculptSession *ss);
void BKE_sculptsession_bm_to_me(struct Object *ob, bool reorder);
void BKE_sculptsession_bm_to_me_for_render(struct Object *object);
int BKE_sculptsession_vertex_count(const struct SculptSession *ss);

/* Ensure an attribute layer exists. */
SculptAttribute *BKE_sculpt_attribute_ensure(struct Object *ob,
                                             eAttrDomain domain,
                                             eCustomDataType proptype,
                                             const char *name,
                                             const SculptAttributeParams *params);

/* Returns nullptr if attribute does not exist. */
SculptAttribute *BKE_sculpt_attribute_get(struct Object *ob,
                                          eAttrDomain domain,
                                          eCustomDataType proptype,
                                          const char *name);

bool BKE_sculpt_attribute_exists(struct Object *ob,
                                 eAttrDomain domain,
                                 eCustomDataType proptype,
                                 const char *name);

bool BKE_sculpt_attribute_destroy(struct Object *ob, SculptAttribute *attr);

/* Destroy all attributes and pseudo-attributes created by sculpt mode. */
void BKE_sculpt_attribute_destroy_temporary_all(struct Object *ob);

/* Destroy attributes that were marked as stroke only in SculptAttributeParams. */
void BKE_sculpt_attributes_destroy_temporary_stroke(struct Object *ob);

BLI_INLINE void *BKE_sculpt_vertex_attr_get(const PBVHVertRef vertex, const SculptAttribute *attr)
{
  if (attr->data) {
    char *p = (char *)attr->data;
    int idx = (int)vertex.i;

    if (attr->data_for_bmesh) {
      BMElem *v = (BMElem *)vertex.i;
      idx = v->head.index;
    }

    return p + attr->elem_size * (int)idx;
  }
  else {
    BMElem *v = (BMElem *)vertex.i;
    return BM_ELEM_CD_GET_VOID_P(v, attr->bmesh_cd_offset);
  }

  return NULL;
}

BLI_INLINE void *BKE_sculpt_face_attr_get(const PBVHFaceRef vertex, const SculptAttribute *attr)
{
  if (attr->data) {
    char *p = (char *)attr->data;
    int idx = (int)vertex.i;

    if (attr->data_for_bmesh) {
      BMElem *v = (BMElem *)vertex.i;
      idx = v->head.index;
    }

    return p + attr->elem_size * (int)idx;
  }
  else {
    BMElem *v = (BMElem *)vertex.i;
    return BM_ELEM_CD_GET_VOID_P(v, attr->bmesh_cd_offset);
  }

  return NULL;
}

/**
 * Create new color layer on object if it doesn't have one and if experimental feature set has
 * sculpt vertex color enabled. Returns truth if new layer has been added, false otherwise.
 */
void BKE_sculpt_color_layer_create_if_needed(struct Object *object);

/**
 * \warning Expects a fully evaluated depsgraph.
 */
void BKE_sculpt_update_object_for_edit(struct Depsgraph *depsgraph,
                                       struct Object *ob_orig,
                                       bool need_pmap,
                                       bool need_mask,
                                       bool is_paint_tool);
void BKE_sculpt_update_object_before_eval(struct Object *ob_eval);
void BKE_sculpt_update_object_after_eval(struct Depsgraph *depsgraph, struct Object *ob_eval);

/**
 * Sculpt mode handles multi-res differently from regular meshes, but only if
 * it's the last modifier on the stack and it is not on the first level.
 */
struct MultiresModifierData *BKE_sculpt_multires_active(const struct Scene *scene,
                                                        struct Object *ob);
int *BKE_sculpt_face_sets_ensure(struct Object *ob);
/**
 * Create the attribute used to store face visibility and retrieve its data.
 * Note that changes to the face visibility have to be propagated to other domains
 * (see #SCULPT_visibility_sync_all_from_faces).
 */
bool *BKE_sculpt_hide_poly_ensure(struct Mesh *mesh);

/**
 * Ensures a mask layer exists. If depsgraph and bmain are non-null,
 * a mask doesn't exist and the object has a multi-resolution modifier
 * then the scene depsgraph will be evaluated to update the runtime
 * subdivision data.
 *
 * \note always call *before* #BKE_sculpt_update_object_for_edit.
 */
int BKE_sculpt_mask_layers_ensure(struct Depsgraph *depsgraph,
                                  struct Main *bmain,
                                  struct Object *ob,
                                  struct MultiresModifierData *mmd);
void BKE_sculpt_toolsettings_data_ensure(struct Scene *scene);

struct PBVH *BKE_sculpt_object_pbvh_ensure(struct Depsgraph *depsgraph, struct Object *ob);

void BKE_sculpt_bvh_update_from_ccg(struct PBVH *pbvh, struct SubdivCCG *subdiv_ccg);

void BKE_sculpt_ensure_orig_mesh_data(struct Scene *scene, struct Object *object);
void BKE_sculpt_sync_face_visibility_to_grids(struct Mesh *mesh, struct SubdivCCG *subdiv_ccg);

/**
 * Test if PBVH can be used directly for drawing, which is faster than
 * drawing the mesh and all updates that come with it.
 */
bool BKE_sculptsession_use_pbvh_draw(const struct Object *ob, const struct RegionView3D *rv3d);

enum {
  SCULPT_MASK_LAYER_CALC_VERT = (1 << 0),
  SCULPT_MASK_LAYER_CALC_LOOP = (1 << 1),
};

/* paint_vertex.cc */

/**
 * Fills the object's active color attribute layer with the fill color.
 *
 * \param[in] ob: The object.
 * \param[in] fill_color: The fill color.
 * \param[in] only_selected: Limit the fill to selected faces or vertices.
 *
 * \return #true if successful.
 */
bool BKE_object_attributes_active_color_fill(struct Object *ob,
                                             const float fill_color[4],
                                             bool only_selected);

/** C accessor for #Object::sculpt::pbvh. */
struct PBVH *BKE_object_sculpt_pbvh_get(struct Object *object);
bool BKE_object_sculpt_use_dyntopo(const struct Object *object);
void BKE_object_sculpt_dyntopo_smooth_shading_set(struct Object *object, bool value);

/* paint_canvas.cc */

/**
 * Create a key that can be used to compare with previous ones to identify changes.
 * The resulting 'string' is owned by the caller.
 */
char *BKE_paint_canvas_key_get(struct PaintModeSettings *settings, struct Object *ob);

bool BKE_paint_canvas_image_get(struct PaintModeSettings *settings,
                                struct Object *ob,
                                struct Image **r_image,
                                struct ImageUser **r_image_user);
int BKE_paint_canvas_uvmap_layer_index_get(const struct PaintModeSettings *settings,
                                           struct Object *ob);
void BKE_sculpt_check_cavity_curves(struct Sculpt *sd);
struct CurveMapping *BKE_sculpt_default_cavity_curve(void);

#ifdef __cplusplus
}
#endif
