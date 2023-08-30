/* SPDX-FileCopyrightText: 2009 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_array.hh"
#include "BLI_bitmap.h"
#include "BLI_compiler_compat.h"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_utildefines.h"

#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_utildefines.h"
#include "DNA_brush_types.h"

#include "DNA_brush_enums.h"
#include "DNA_customdata_types.h"
#include "DNA_object_enums.h"
#include "DNA_scene_enums.h"

#include "BKE_attribute.h"
#include "BKE_pbvh.h"

#include "bmesh.h"
#include "bmesh_log.h"

#include <type_traits>

struct SculptAttribute;
struct BMFace;
struct BMesh;
struct BMIdMap;
struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct Brush;
struct CurveMapping;
struct Depsgraph;
struct EdgeSet;
struct EnumPropertyItem;
struct ExpandCache;
struct FilterCache;
struct GHash;
struct GridPaintMask;
struct Image;
struct ImagePool;
struct ImageUser;
struct KeyBlock;
struct ListBase;
struct MLoopTri;
struct Main;
struct Mesh;
struct MDeformVert;
struct MLoopCol;
struct MPropCol;
struct MultiresModifierData;
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

enum ePaintMode {
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
};

#define PAINT_MODE_HAS_BRUSH(mode) !ELEM(mode, PAINT_MODE_SCULPT_UV)

/* overlay invalidation */
enum ePaintOverlayControlFlags {
  PAINT_OVERLAY_INVALID_TEXTURE_PRIMARY = 1,
  PAINT_OVERLAY_INVALID_TEXTURE_SECONDARY = (1 << 2),
  PAINT_OVERLAY_INVALID_CURVE = (1 << 3),
  PAINT_OVERLAY_OVERRIDE_CURSOR = (1 << 4),
  PAINT_OVERLAY_OVERRIDE_PRIMARY = (1 << 5),
  PAINT_OVERLAY_OVERRIDE_SECONDARY = (1 << 6),
};
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

enum ePaintSymmetryAreas {
  PAINT_SYMM_AREA_X = (1 << 0),
  PAINT_SYMM_AREA_Y = (1 << 1),
  PAINT_SYMM_AREA_Z = (1 << 2),
};
ENUM_OPERATORS(ePaintSymmetryAreas, PAINT_SYMM_AREA_Z);

#define PAINT_SYMM_AREAS 8

void BKE_paint_invalidate_overlay_tex(Scene *scene, ViewLayer *view_layer, const Tex *tex);
void BKE_paint_invalidate_cursor_overlay(Scene *scene, ViewLayer *view_layer, CurveMapping *curve);
void BKE_paint_invalidate_overlay_all();
ePaintOverlayControlFlags BKE_paint_get_overlay_flags();
void BKE_paint_reset_overlay_invalid(ePaintOverlayControlFlags flag);
void BKE_paint_set_overlay_override(enum eOverlayFlags flag);

/* Palettes. */

Palette *BKE_palette_add(Main *bmain, const char *name);
PaletteColor *BKE_palette_color_add(Palette *palette);
bool BKE_palette_is_empty(const Palette *palette);
/**
 * Remove color from palette. Must be certain color is inside the palette!
 */
void BKE_palette_color_remove(Palette *palette, PaletteColor *color);
void BKE_palette_clear(Palette *palette);

void BKE_palette_sort_hsv(tPaletteColorHSV *color_array, int totcol);
void BKE_palette_sort_svh(tPaletteColorHSV *color_array, int totcol);
void BKE_palette_sort_vhs(tPaletteColorHSV *color_array, int totcol);
void BKE_palette_sort_luminance(tPaletteColorHSV *color_array, int totcol);
bool BKE_palette_from_hash(Main *bmain, GHash *color_table, const char *name, bool linear);

/* Paint curves. */

PaintCurve *BKE_paint_curve_add(Main *bmain, const char *name);

/**
 * Call when entering each respective paint mode.
 */
bool BKE_paint_ensure(ToolSettings *ts, Paint **r_paint);
void BKE_paint_init(Main *bmain, Scene *sce, ePaintMode mode, const uchar col[3]);
void BKE_paint_free(Paint *p);
/**
 * Called when copying scene settings, so even if 'src' and 'tar' are the same still do a
 * #id_us_plus(), rather than if we were copying between 2 existing scenes where a matching
 * value should decrease the existing user count as with #paint_brush_set()
 */
void BKE_paint_copy(Paint *src, Paint *tar, int flag);

void BKE_paint_runtime_init(const ToolSettings *ts, Paint *paint);

void BKE_paint_cavity_curve_preset(Paint *p, int preset);

eObjectMode BKE_paint_object_mode_from_paintmode(ePaintMode mode);
bool BKE_paint_ensure_from_paintmode(Scene *sce, ePaintMode mode);
Paint *BKE_paint_get_active_from_paintmode(Scene *sce, ePaintMode mode);
const EnumPropertyItem *BKE_paint_get_tool_enum_from_paintmode(ePaintMode mode);
const char *BKE_paint_get_tool_enum_translation_context_from_paintmode(ePaintMode mode);
const char *BKE_paint_get_tool_prop_id_from_paintmode(ePaintMode mode);
uint BKE_paint_get_brush_tool_offset_from_paintmode(ePaintMode mode);
Paint *BKE_paint_get_active(Scene *sce, ViewLayer *view_layer);
Paint *BKE_paint_get_active_from_context(const bContext *C);
ePaintMode BKE_paintmode_get_active_from_context(const bContext *C);
ePaintMode BKE_paintmode_get_from_tool(const bToolRef *tref);
Brush *BKE_paint_brush(Paint *paint);
const Brush *BKE_paint_brush_for_read(const Paint *p);
void BKE_paint_brush_set(Paint *paint, Brush *br);
Palette *BKE_paint_palette(Paint *paint);
void BKE_paint_palette_set(Paint *p, Palette *palette);
void BKE_paint_curve_set(Brush *br, PaintCurve *pc);
void BKE_paint_curve_clamp_endpoint_add_index(PaintCurve *pc, int add_index);

/**
 * Return true when in vertex/weight/texture paint + face-select mode?
 */
bool BKE_paint_select_face_test(Object *ob);
/**
 * Return true when in vertex/weight paint + vertex-select mode?
 */
bool BKE_paint_select_vert_test(Object *ob);
/**
 * used to check if selection is possible
 * (when we don't care if its face or vert)
 */
bool BKE_paint_select_elem_test(Object *ob);
/**
 * Checks if face/vertex hiding is always applied in the current mode.
 * Returns true in vertex/weight paint.
 */
bool BKE_paint_always_hide_test(Object *ob);

/* Partial visibility. */

/**
 * Returns non-zero if any of the corners of the grid
 * face whose inner corner is at (x, y) are hidden, zero otherwise.
 */
bool paint_is_grid_face_hidden(const unsigned int *grid_hidden, int gridsize, int x, int y);
/**
 * Return true if face is visible.
 */
bool paint_is_bmesh_face_hidden(BMFace *f);

/* Paint masks. */

float paint_grid_paint_mask(const GridPaintMask *gpm, uint level, uint x, uint y);

void BKE_paint_face_set_overlay_color_get(int face_set, int seed, uchar r_color[4]);

/* Stroke related. */

bool paint_calculate_rake_rotation(UnifiedPaintSettings *ups,
                                   Brush *brush,
                                   const float mouse_pos[2],
                                   const float initial_mouse_pos[2],
                                   ePaintMode paint_mode,
                                   bool stroke_has_started);
void paint_update_brush_rake_rotation(UnifiedPaintSettings *ups, Brush *brush, float rotation);

void BKE_paint_stroke_get_average(Scene *scene, Object *ob, float stroke[3]);

/* Tool slot API. */

void BKE_paint_toolslots_init_from_main(Main *bmain);
void BKE_paint_toolslots_len_ensure(Paint *paint, int len);
void BKE_paint_toolslots_brush_update_ex(Paint *paint, Brush *brush);
void BKE_paint_toolslots_brush_update(Paint *paint);
/**
 * Run this to ensure brush types are set for each slot on entering modes
 * (for new scenes for example).
 */
void BKE_paint_toolslots_brush_validate(Main *bmain, Paint *paint);
Brush *BKE_paint_toolslots_brush_get(Paint *paint, int slot_index);

/* .blend I/O */

void BKE_paint_blend_write(BlendWriter *writer, Paint *paint);
void BKE_paint_blend_read_data(BlendDataReader *reader, const Scene *scene, Paint *paint);

#define SCULPT_FACE_SET_NONE 0

/** Used for both vertex color and weight paint. */
struct SculptVertexPaintGeomMap {
  blender::Array<int> vert_to_loop_offsets;
  blender::Array<int> vert_to_loop_indices;
  blender::GroupedSpan<int> vert_to_loop;

  blender::Array<int> vert_to_face_offsets;
  blender::Array<int> vert_to_face_indices;
  blender::GroupedSpan<int> vert_to_face;
};

/** Pose Brush IK Chain. */
struct SculptPoseIKChainSegment {
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
};

struct SculptPoseIKChain {
  SculptPoseIKChainSegment *segments;
  int tot_segments;
  float grab_delta_offset[3];
};

/* Cloth Brush */

/* Cloth Simulation. */
enum eSculptClothNodeSimState {
  /* Constraints were not built for this node, so it can't be simulated. */
  SCULPT_CLOTH_NODE_UNINITIALIZED,

  /* There are constraints for the geometry in this node, but it should not be simulated. */
  SCULPT_CLOTH_NODE_INACTIVE,

  /* There are constraints for this node and they should be used by the solver. */
  SCULPT_CLOTH_NODE_ACTIVE,
};

enum eSculptClothConstraintType {
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
};

struct SculptClothLengthConstraint {
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
};

struct SculptClothSimulation {
  SculptClothLengthConstraint *length_constraints;
  int tot_length_constraints;
  EdgeSet *created_length_constraints;
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

  ListBase *collider_list;

  int totnode;
  /** #PBVHNode pointer as a key, index in #SculptClothSimulation.node_state as value. */
  GHash *node_state_index;
  eSculptClothNodeSimState *node_state;
};

struct SculptVertexInfo {
  /* Indexed by base mesh vertex index, stores if that vertex is a boundary. */
  BLI_bitmap *boundary;
};

struct SculptBoundaryEditInfo {
  /* Vertex index from where the topology propagation reached this vertex. */
  int original_vertex_i;

  /* How many steps were needed to reach this vertex from the boundary. */
  int propagation_steps_num;

  /* Strength that is used to deform this vertex. */
  float strength_factor;
};

/* Edge for drawing the boundary preview in the cursor. */
struct SculptBoundaryPreviewEdge {
  PBVHVertRef v1;
  PBVHVertRef v2;
};

struct SculptBoundary {
  /* Vertex indices of the active boundary. */
  PBVHVertRef *verts;
  int verts_capacity;
  int verts_num;

  /* Distance from a vertex in the boundary to initial vertex indexed by vertex index, taking into
   * account the length of all edges between them. Any vertex that is not in the boundary will have
   * a distance of 0. */
  float *distance;

  float (*smoothco)[3];
  float *boundary_dist;  // distances from verts to boundary
  float (*boundary_tangents)[3];

  PBVHVertRef *boundary_closest;
  int sculpt_totvert;

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
  SculptBoundaryEditInfo *edit_info;

  /* Bend Deform type. */
  struct {
    float (*pivot_rotation_axis)[3];
    float (*pivot_positions)[4];
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

  /* Cicrle Deform type. */
  struct {
    float (*origin)[3];
    float *radius;
  } circle;

  int deform_target;
};

struct SculptFakeNeighbors {
  bool use_fake_neighbors;

  /* Max distance used to calculate neighborhood information. */
  float current_max_distance;

  /* Indexed by vertex, stores the vertex index of its fake neighbor if available. */
  PBVHVertRef *fake_neighbor_index;
};

/* Session data (mode-specific) */

/* Custom Temporary Attributes */

struct SculptAttributeParams {
  /* Allocate a flat array outside the CustomData system.  Cannot be combined with permanent. */
  int simple_array : 1;

  /* Do not mark CustomData layer as temporary.  Cannot be combined with simple_array.  Doesn't
   * work with PBVH_GRIDS.
   */
  int permanent : 1;   /* Cannot be combined with simple_array. */
  int stroke_only : 1; /* Release layer at end of struct */
  int nointerp : 1;
  int nocopy : 1;
};

struct SculptAttribute {
  /* Domain, data type and name */
  eAttrDomain domain;
  eCustomDataType proptype;
  char name[MAX_CUSTOMDATA_LAYER_NAME];

  /* Source layer on mesh/bmesh, if any. */
  CustomDataLayer *layer;

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
  bool is_empty() const
  {
    return !used;
  }
};

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

struct SculptAttributePointers {
  SculptAttribute *face_set;

  /* Persistent base. */
  SculptAttribute *persistent_co;
  SculptAttribute *persistent_no;
  SculptAttribute *persistent_disp;

  /* Layer brush. */
  SculptAttribute *layer_displayment;

  /* Precomputed auto-mask factor indexed by vertex, owned by the auto-masking system and
   * initialized in #SCULPT_automasking_cache_init when needed. */
  SculptAttribute *automasking_factor;    /* Stroke only. */
  SculptAttribute *automasking_occlusion; /* CD_PROP_INT8, stroke only */
  SculptAttribute *automasking_stroke_id;
  SculptAttribute *automasking_cavity; /* Stroke only. */

  SculptAttribute *topology_island_key; /* CD_PROP_INT8 */

  /* BMesh */
  SculptAttribute *dyntopo_node_id_vertex;
  SculptAttribute *dyntopo_node_id_face;
  SculptAttribute *rake_temp;

  SculptAttribute *face_areas;

  SculptAttribute *smooth_bdist;
  SculptAttribute *smooth_vel;

  /* Sculpt utility attributes. */
  SculptAttribute *stroke_id;
  SculptAttribute *boundary_flags;      /* CD_PROP_INT32, vert */
  SculptAttribute *edge_boundary_flags; /* CD_PROP_INT32, vert */
  SculptAttribute *valence;             /* CD_PROP_INT32, vert */
  SculptAttribute *flags;               /* CD_PROP_INT8,  vert */

  SculptAttribute *orig_co, *orig_no; /* CD_PROP_FLOAT3, vert */
  SculptAttribute *orig_fsets;        /* CD_PROP_INT32,  face */
  SculptAttribute *orig_color;        /* CD_PROP_FLOAT4, vert */
  SculptAttribute *orig_mask;         /* CD_PROP_FLOAT   vert */

  SculptAttribute *curvature_dir; /* Curvature direction vectors, CD_PROP_FLOAT3 */

  SculptAttribute *smear_previous;
  SculptAttribute *hide_poly;
  SculptAttribute *limit_surface;

  SculptAttribute *layer_disp;
  SculptAttribute *layer_id;

  SculptAttribute *prefairing_co;
  SculptAttribute *fairing_fade;
  SculptAttribute *fairing_mask;

  /* Stores the displacement produced by the laplacian step of HC smooth */
  SculptAttribute *laplacian_disp;

  /* Enhance Details */
  SculptAttribute *detail_directions; /* Stroke only. */
};

struct SculptSession {
  /* Mesh data (not copied) can come either directly from a Mesh, or from a MultiresDM */
  struct { /* Special handling for multires meshes */
    bool active;
    MultiresModifierData *modifier;
    int level;
  } multires;

  /* Depsgraph for the Cloth Brush solver to get the colliders. */
  Depsgraph *depsgraph;

  /* These are always assigned to base mesh data when using PBVH_FACES and PBVH_GRIDS. */
  blender::MutableSpan<blender::float3> vert_positions;
  blender::Span<blender::int2> edges;
  blender::OffsetIndices<int> faces;
  blender::Span<int> corner_verts;
  blender::Span<int> corner_edges;

  const int *material_index;

  CustomData *vdata, *edata, *ldata, *pdata;

  /* These contain the vertex and poly counts of the final mesh. */
  int totvert, faces_num;

  KeyBlock *shapekey_active;
  MPropCol *vcol;
  MLoopCol *mcol;

  eAttrDomain vcol_domain;
  eCustomDataType vcol_type;

  float *vmask;

  /* Mesh connectivity maps. */
  /* Vertices to adjacent faces. */
  blender::Array<int> vert_to_face_offsets;
  blender::Array<int> vert_to_face_indices;
  blender::GroupedSpan<int> pmap;

  /* Edges to adjacent faces. */
  blender::Array<int> edge_to_face_offsets;
  blender::Array<int> edge_to_face_indices;
  blender::GroupedSpan<int> epmap;

  /* Vertices to adjacent edges. */
  blender::Array<int> vert_to_edge_offsets;
  blender::Array<int> vert_to_edge_indices;
  blender::GroupedSpan<int> vemap;

  /* Mesh Face Sets */
  /* Total number of faces of the base mesh. */
  int totedges, totloops, totfaces;

  /* The 0 ID is not used by the tools or the visibility system, it is just used when creating new
   * geometry (the trim tool, for example) to detect which geometry was just added, so it can be
   * assigned a valid Face Set after creation. Tools are not intended to run with Face Sets IDs set
   * to 0.
   */
  int *face_sets;
  /**
   * A reference to the ".hide_poly" attribute, to store whether (base) faces are hidden.
   * May be null.
   */
  bool *hide_poly;

  bool *select_poly;

  /* BMesh for dynamic topology sculpting */
  BMesh *bm;
  BMIdMap *bm_idmap;

  /* TODO: get rid of these cd_ members and use
   * .attrs.XXX.bmesh_cd_offset directly.
   */
  int cd_vert_node_offset;
  int cd_face_node_offset;
  int cd_vcol_offset;
  int cd_vert_mask_offset;
  int cd_faceset_offset;
  int cd_face_areas;

  int totuv;

  /* Reproject customdata during smooth. */
  eAttrCorrectMode distort_correction_mode;

  /* Undo/redo log for dynamic topology sculpting */
  BMLog *bm_log;

  /* Limit surface/grids. */
  SubdivCCG *subdiv_ccg;

  /* PBVH acceleration structure */
  PBVH *pbvh;
  PBVH *last_pbvh;

  /* Setting this to true allows a PBVH rebuild when evaluating the object even if the stroke or
   * filter caches are active. */
  bool needs_pbvh_rebuild;

  /* Painting on deformed mesh */
  bool deform_modifiers_active; /* Object is deformed with some modifiers. */
  float (*orig_cos)[3];         /* Coords of un-deformed mesh. */
  float (*deform_cos)[3];       /* Coords of deformed mesh but without stroke displacement. */
  float (*deform_imats)[3][3];  /* Crazy-space deformation matrices. */
  float *face_areas;            /* cached face areas for PBVH_FACES and PBVH_GRIDS */

  /* Pool for texture evaluations. */
  ImagePool *tex_pool;

  StrokeCache *cache;
  FilterCache *filter_cache;
  ExpandCache *expand_cache;

  /* Cursor data and active vertex for tools */
  PBVHVertRef active_vertex;
  PBVHFaceRef active_face;

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
  float gesture_initial_back_location[3];
  float gesture_initial_location[3];
  float gesture_initial_normal[3];
  bool gesture_initial_hit;

  /* TODO(jbakker): Replace rv3d and v3d with ViewContext */
  RegionView3D *rv3d;
  View3D *v3d;
  Scene *scene;
  int cd_origvcol_offset;
  int cd_origco_offset;
  int cd_origno_offset;

  /* Face Sets by topology. */
  int face_set_last_created;
  PBVHFaceRef face_set_last_poly;
  PBVHEdgeRef face_set_last_edge;

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

  float prev_pivot_pos[3];
  float prev_pivot_rot[4];
  float prev_pivot_scale[3];

  float init_pivot_pos[3];
  float init_pivot_rot[4];
  float init_pivot_scale[3];

  struct {
    struct {
      SculptVertexPaintGeomMap gmap;
    } vpaint;

    struct {
      SculptVertexPaintGeomMap gmap;
      /* Keep track of how much each vertex has been painted (non-airbrush only). */
      float *alpha_weight;

      /* Needed to continuously re-apply over the same weights (BRUSH_ACCUMULATE disabled).
       * Lazy initialize as needed (flag is set to 1 to tag it as uninitialized). */
      MDeformVert *dvert_prev;
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
  SculptAttribute temp_attributes[SCULPT_MAX_ATTRIBUTES];

  /* Convenience #SculptAttribute pointers. */
  SculptAttributePointers attrs;

  /**
   * Some tools follows the shading chosen by the last used tool canvas.
   * When not set the viewport shading color would be used.
   *
   * NOTE: This setting is temporarily until paint mode is added.
   */
  bool sticky_shading_color;

  ushort stroke_id;

  /**
   * Last used painting canvas key.
   */
  char *last_paint_canvas_key;
  float last_normal[3];

  /* Used to derive initial tip rotation. */
  float last_grab_delta[3];

  blender::Span<blender::float3> poly_normals;

  int last_automasking_settings_hash;
  uchar last_automask_stroke_id;
  bool *sharp_edge;
  bool *seam_edge;
  bool islands_valid; /* Is attrs.topology_island_key valid? */

  bool hard_edge_mode;
  DynTopoSettings cached_dyntopo;
  float sharp_angle_limit;
  eSculptBoundary smooth_boundary_flag;
};

void BKE_sculptsession_free(Object *ob);
void BKE_sculptsession_free_deformMats(SculptSession *ss);
void BKE_sculptsession_free_vwpaint_data(SculptSession *ss);

void BKE_sculptsession_bm_to_me(Object *ob, bool reorder);
void BKE_sculptsession_bm_to_me_for_render(Object *object);
int BKE_sculptsession_vertex_count(const SculptSession *ss);

void BKE_sculpt_ensure_idmap(Object *ob);

void BKE_sculpt_ensure_origco(Object *ob);
void BKE_sculpt_ensure_origmask(Object *ob);
void BKE_sculpt_ensure_origcolor(Object *ob);
void BKE_sculpt_ensure_origfset(Object *ob);
void BKE_sculpt_ensure_curvature_dir(Object *ob);

/* Ensures Sculpt_flags and sculpt_valence layers. */
void BKE_sculpt_ensure_sculpt_layers(Object *ob);

/* Ensure an attribute layer exists. */
SculptAttribute *BKE_sculpt_attribute_ensure(Object *ob,
                                             eAttrDomain domain,
                                             eCustomDataType proptype,
                                             const char *name,
                                             const SculptAttributeParams *params);

/* Returns nullptr if attribute does not exist. */
SculptAttribute *BKE_sculpt_attribute_get(Object *ob,
                                          eAttrDomain domain,
                                          eCustomDataType proptype,
                                          const char *name);

bool BKE_sculpt_attribute_exists(Object *ob,
                                 eAttrDomain domain,
                                 eCustomDataType proptype,
                                 const char *name);

bool BKE_sculpt_attribute_destroy(Object *ob, SculptAttribute *attr);

/* Destroy all attributes and pseudo-attributes created by sculpt mode. */
void BKE_sculpt_attribute_destroy_temporary_all(Object *ob);

/* Destroy attributes that were marked as stroke only in SculptAttributeParams. */
void BKE_sculpt_attributes_destroy_temporary_stroke(Object *ob);

/* Release a SculptAttribute ref without destroying the underlying attribute. */
void BKE_sculpt_attribute_release_ref(Object *ob, SculptAttribute *attr);

SculptAttribute BKE_sculpt_find_attribute(Object *ob, const char *name);

bool BKE_sculpt_init_flags_valence(Object *ob, PBVH *pbvh, int totvert, bool reset_flags);

BMesh *BKE_sculptsession_empty_bmesh_create(void);
void BKE_sculptsession_bmesh_attr_update_internal(Object *ob);

/* Ensures non-temporary attributes in me exist in the sculpt mesh, or vice
 * versa if load_to_mesh is true.
 */
void BKE_sculptsession_sync_attributes(Object *ob, Mesh *me, bool load_to_mesh);

void BKE_sculptsession_bmesh_add_layers(Object *ob);
void BKE_sculptsession_update_attr_refs(Object *ob);

int BKE_sculptsession_get_totvert(const SculptSession *ss);

void BKE_sculpt_distort_correction_set(Object *ob, eAttrCorrectMode value);
void BKE_sculptsession_free_attribute_refs(Object *ob);

/**
 * Create new color layer on object if it doesn't have one and if experimental feature set has
 * sculpt vertex color enabled. Returns truth if new layer has been added, false otherwise.
 */

void BKE_sculpt_color_layer_create_if_needed(Object *object);

/**
 * \warning Expects a fully evaluated depsgraph.
 */
void BKE_sculpt_update_object_for_edit(
    Depsgraph *depsgraph, Object *ob_orig, bool need_pmap, bool need_mask, bool is_paint_tool);
void BKE_sculpt_update_object_before_eval(Object *ob_eval);
void BKE_sculpt_update_object_after_eval(Depsgraph *depsgraph, Object *ob_eval);

/**
 * Sculpt mode handles multi-res differently from regular meshes, but only if
 * it's the last modifier on the stack and it is not on the first level.
 */
MultiresModifierData *BKE_sculpt_multires_active(const Scene *scene, Object *ob);
int *BKE_sculpt_face_sets_ensure(Object *ob);
/**
 * Create the attribute used to store face visibility and retrieve its data.
 * Note that changes to the face visibility have to be propagated to other domains
 * (see #SCULPT_visibility_sync_all_from_faces).
 */
bool *BKE_sculpt_hide_poly_ensure(Object *ob);

/**
 * Ensures a mask layer exists. If depsgraph and bmain are non-null,
 * a mask doesn't exist and the object has a multi-resolution modifier
 * then the scene depsgraph will be evaluated to update the runtime
 * subdivision data.
 *
 * \note always call *before* #BKE_sculpt_update_object_for_edit.
 */
int BKE_sculpt_mask_layers_ensure(Depsgraph *depsgraph,
                                  Main *bmain,
                                  Object *ob,
                                  MultiresModifierData *mmd);
void BKE_sculpt_toolsettings_data_ensure(Scene *scene);

PBVH *BKE_sculpt_object_pbvh_ensure(Depsgraph *depsgraph, Object *ob);

void BKE_sculpt_bvh_update_from_ccg(PBVH *pbvh, SubdivCCG *subdiv_ccg);

void BKE_sculpt_ensure_orig_mesh_data(Scene *scene, Object *object);
void BKE_sculpt_sync_face_visibility_to_grids(Mesh *mesh, SubdivCCG *subdiv_ccg);

/**
 * Test if PBVH can be used directly for drawing, which is faster than
 * drawing the mesh and all updates that come with it.
 */
bool BKE_sculptsession_use_pbvh_draw(const Object *ob, const RegionView3D *rv3d);

char BKE_get_fset_boundary_symflag(Object *object);

bool BKE_sculpt_has_persistent_base(SculptSession *ss);

/**
 * Sets ob->sculpt->bm.  The PBVH will be recreated if it exists
 * (if it's of type PBVH_BMESH) as will ob->sculpt->bm_idmap.
 *
 * Note: BMLog (ob->sculpt->bm_log) doesn't need to be reallocated.
 */
void BKE_sculpt_set_bmesh(Object *ob, BMesh *bm, bool free_existing = true);

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
bool BKE_object_attributes_active_color_fill(Object *ob,
                                             const float fill_color[4],
                                             bool only_selected);

/** C accessor for #Object::sculpt::pbvh. */
PBVH *BKE_object_sculpt_pbvh_get(Object *object);
bool BKE_object_sculpt_use_dyntopo(const Object *object);

/* paint_canvas.cc */

/**
 * Create a key that can be used to compare with previous ones to identify changes.
 * The resulting 'string' is owned by the caller.
 */
char *BKE_paint_canvas_key_get(PaintModeSettings *settings, Object *ob);

bool BKE_paint_canvas_image_get(PaintModeSettings *settings,
                                Object *ob,
                                Image **r_image,
                                ImageUser **r_image_user);
int BKE_paint_canvas_uvmap_layer_index_get(const PaintModeSettings *settings, Object *ob);
void BKE_sculpt_check_cavity_curves(Sculpt *sd);
CurveMapping *BKE_sculpt_default_cavity_curve(void);

namespace blender::bke::paint {

/* Base implementation for vertex_attr_*** and face_attr_*** methods.
 * Returns a pointer to the attribute data (as defined by attr) for elem.
 */
template<typename T, typename ElemRef = PBVHVertRef>
static T *elem_attr_ptr(const ElemRef elem, const SculptAttribute *attr)
{
  void *ptr = nullptr;

  if (attr->data) {
    char *p = (char *)attr->data;
    int idx = (int)elem.i;

    if (attr->data_for_bmesh) {
      BMElem *e = (BMElem *)elem.i;
      idx = e->head.index;
    }

    ptr = p + attr->elem_size * (int)idx;
  }
  else {
    BMElem *v = (BMElem *)elem.i;
    ptr = BM_ELEM_CD_GET_VOID_P(v, attr->bmesh_cd_offset);
  }

  return static_cast<T *>(ptr);
}

/*
 * Get a pointer to attribute data at vertex.
 *
 * Example: float *persistent_co = vertex_attr_ptr<float>(vertex, ss->attrs.persistent_co);
 */
template<typename T>
static T *vertex_attr_ptr(const PBVHVertRef vertex, const SculptAttribute *attr)
{
  return elem_attr_ptr<T, PBVHVertRef>(vertex, attr);
}

/*
 * Get attribute data at vertex.
 *
 * Example: float weight = vertex_attr_get<float>(vertex, ss->attrs.automasking_factor);
 */
template<typename T>
static T vertex_attr_get(const PBVHVertRef vertex, const SculptAttribute *attr)
{
  return *vertex_attr_ptr<T>(vertex, attr);
}

/*
 * Set attribute data at vertex.
 *
 * vertex_attr_set<float>(vertex, ss->attrs.automasking_factor, 1.0f);
 */
template<typename T>
static void vertex_attr_set(const PBVHVertRef vertex, const SculptAttribute *attr, T data)
{
  *vertex_attr_ptr<T>(vertex, attr) = data;
}

/*
 * Get a pointer to attribute data at vertex.
 *
 * Example: float *persistent_co = vertex_attr_ptr<float>(vertex, ss->attrs.persistent_co);
 */
template<typename T> static T *edge_attr_ptr(const PBVHEdgeRef edge, const SculptAttribute *attr)
{
  return elem_attr_ptr<T, PBVHEdgeRef>(edge, attr);
}
/*
 * Get attribute data at vertex.
 *
 * Example: float weight = vertex_attr_get<float>(vertex, ss->attrs.automasking_factor);
 */
template<typename T> static T edge_attr_get(const PBVHEdgeRef edge, const SculptAttribute *attr)
{
  return *edge_attr_ptr<T>(edge, attr);
}

/*
 * Set attribute data at vertex.
 *
 * vertex_attr_set<float>(vertex, ss->attrs.automasking_factor, 1.0f);
 */
template<typename T>
static void edge_attr_set(const PBVHEdgeRef edge, const SculptAttribute *attr, T data)
{
  *edge_attr_ptr<T>(edge, attr) = data;
}

template<typename T> static T *face_attr_ptr(const PBVHFaceRef face, const SculptAttribute *attr)
{
  return elem_attr_ptr<T, PBVHFaceRef>(face, attr);
}

template<typename T> static T face_attr_get(const PBVHFaceRef face, const SculptAttribute *attr)
{
  return *face_attr_ptr<T>(face, attr);
}

template<typename T>
static void face_attr_set(const PBVHFaceRef face, const SculptAttribute *attr, T data)
{
  *face_attr_ptr<T>(face, attr) = data;
}

bool get_original_vertex(SculptSession *ss,
                         PBVHVertRef vertex,
                         float **r_co,
                         float **r_no,
                         float **r_color,
                         float **r_mask);
void load_all_original(Object *ob);
}  // namespace blender::bke::paint

template<typename PBVHElemRef = PBVHVertRef>
inline void BKE_sculpt_boundary_flag_update(SculptSession *ss,
                                            PBVHElemRef elem,
                                            bool flag_vert_edges = false)
{
  int *flags;

  if constexpr (std::is_same_v<PBVHElemRef, PBVHVertRef>) {
    PBVHVertRef vertex = {elem.i};
    flags = blender::bke::paint::vertex_attr_ptr<int>(elem, ss->attrs.boundary_flags);

    if (flag_vert_edges) {
      switch (BKE_pbvh_type(ss->pbvh)) {
        case PBVH_BMESH: {
          BMVert *v = reinterpret_cast<BMVert *>(vertex.i);
          if (!v->e) {
            break;
          }

          BMEdge *e = v->e;
          do {
            PBVHEdgeRef edge = {reinterpret_cast<intptr_t>(e)};
            *blender::bke::paint::edge_attr_ptr<int>(
                edge, ss->attrs.edge_boundary_flags) |= SCULPT_BOUNDARY_NEEDS_UPDATE |
                                                        SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;
          } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

          break;
        }
        case PBVH_FACES:
          /* If we have a vertex to edge map use it. */
          if (!ss->vemap.is_empty()) {
            for (int edge_i : ss->vemap[vertex.i]) {
              *blender::bke::paint::edge_attr_ptr<int>(
                  {edge_i}, ss->attrs.edge_boundary_flags) |= SCULPT_BOUNDARY_NEEDS_UPDATE |
                                                              SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;
            }
          }
          else { /* Otherwise use vertex to poly map. */
            for (int poly_i : ss->pmap[vertex.i]) {
              for (int loop_i : ss->faces[poly_i]) {
                if (ss->corner_verts[loop_i] == vertex.i) {
                  int edge_i = ss->corner_edges[loop_i];
                  *blender::bke::paint::edge_attr_ptr<int>(
                      {edge_i},
                      ss->attrs.edge_boundary_flags) |= SCULPT_BOUNDARY_NEEDS_UPDATE |
                                                        SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;
                }
              }
            }
          }
          break;
        case PBVH_GRIDS:
          /* Not supported. */
          break;
      }
    }
  }
  else {
    flags = blender::bke::paint::edge_attr_ptr<int>(elem, ss->attrs.edge_boundary_flags);
  }

  *flags |= SCULPT_BOUNDARY_NEEDS_UPDATE | SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;
}

template<typename PBVHElemRef = PBVHVertRef>
inline void BKE_sculpt_boundary_flag_uv_update(SculptSession *ss, PBVHElemRef elem)
{
  int *flags;

  if constexpr (std::is_same_v<PBVHElemRef, PBVHVertRef>) {
    flags = blender::bke::paint::vertex_attr_ptr<int>(elem, ss->attrs.boundary_flags);
  }
  else {
    flags = blender::bke::paint::edge_attr_ptr<int>(elem, ss->attrs.edge_boundary_flags);
  }

  *flags |= SCULPT_BOUNDARY_UPDATE_UV;
}

template<typename PBVHElemRef = PBVHVertRef>
inline void BKE_sculpt_sharp_boundary_flag_update(SculptSession *ss,
                                                  PBVHElemRef elem,
                                                  bool update_ring = false)
{
  int *flags;

  if constexpr (std::is_same_v<PBVHElemRef, PBVHVertRef>) {
    flags = blender::bke::paint::vertex_attr_ptr<int>(elem, ss->attrs.boundary_flags);
  }
  else {
    flags = blender::bke::paint::edge_attr_ptr<int>(elem, ss->attrs.edge_boundary_flags);
  }

  *flags |= SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;

  if constexpr (std::is_same_v<PBVHElemRef, PBVHVertRef>) {
    if (update_ring && ss->bm) {
      BMVert *v = reinterpret_cast<BMVert *>(elem.i);
      if (!v->e) {
        return;
      }

      BMEdge *e = v->e;
      do {
        PBVHVertRef vertex2 = {reinterpret_cast<intptr_t>(BM_edge_other_vert(e, v))};

        int *flags2 = blender::bke::paint::vertex_attr_ptr<int>(vertex2, ss->attrs.boundary_flags);
        *flags2 |= SCULPT_BOUNDARY_UPDATE_SHARP_ANGLE;
      } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
    }
  }
}
