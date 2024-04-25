/* SPDX-FileCopyrightText: 2006 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include <queue>

#include "BKE_attribute.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"

#include "BLI_array.hh"
#include "BLI_bit_vector.hh"
#include "BLI_generic_array.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "DNA_brush_enums.h"

#include "ED_view3d.hh"

namespace blender::ed::sculpt_paint {
namespace auto_mask {
struct NodeData;
struct Cache;
}
namespace cloth {
struct SimulationData;
}
namespace undo {
struct Node;
}
}
struct BMLog;
struct Dial;
struct DistRayAABB_Precalc;
struct Image;
struct ImageUser;
struct KeyBlock;
struct Object;
struct SculptProjectVector;
struct bContext;
struct PaintModeSettings;
struct WeightPaintInfo;
struct WPaintData;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperator;
struct wmOperatorType;

/* -------------------------------------------------------------------- */
/** \name Sculpt Types
 * \{ */

enum SculptUpdateType {
  SCULPT_UPDATE_COORDS = 1 << 0,
  SCULPT_UPDATE_MASK = 1 << 1,
  SCULPT_UPDATE_VISIBILITY = 1 << 2,
  SCULPT_UPDATE_COLOR = 1 << 3,
  SCULPT_UPDATE_IMAGE = 1 << 4,
  SCULPT_UPDATE_FACE_SET = 1 << 5,
};

struct SculptCursorGeometryInfo {
  blender::float3 location;
  blender::float3 normal;
  blender::float3 active_vertex_co;
};

#define SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY 256

struct SculptVertexNeighborIter {
  /* Storage */
  PBVHVertRef *neighbors;
  int *neighbor_indices;
  int size;
  int capacity;

  PBVHVertRef neighbors_fixed[SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY];
  int neighbor_indices_fixed[SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY];

  /* Internal iterator. */
  int num_duplicates;
  int i;

  /* Public */
  int index;
  PBVHVertRef vertex;
  bool is_duplicate;
};

/* Sculpt Original Data */
struct SculptOrigVertData {
  BMLog *bm_log;

  blender::ed::sculpt_paint::undo::Node *unode;
  float (*coords)[3];
  float (*normals)[3];
  const float *vmasks;
  float (*colors)[4];

  /* Original coordinate, normal, and mask. */
  const float *co;
  const float *no;
  float mask;
  const float *col;
};

struct SculptOrigFaceData {
  blender::ed::sculpt_paint::undo::Node *unode;
  BMLog *bm_log;
  const int *face_sets;
  int face_set;
};

enum eBoundaryAutomaskMode {
  AUTOMASK_INIT_BOUNDARY_EDGES = 1,
  AUTOMASK_INIT_BOUNDARY_FACE_SETS = 2,
};

namespace blender::ed::sculpt_paint::undo {

enum class Type {
  Position,
  HideVert,
  HideFace,
  Mask,
  DyntopoBegin,
  DyntopoEnd,
  DyntopoSymmetrize,
  Geometry,
  FaceSet,
  Color,
};

/* Storage of geometry for the undo node.
 * Is used as a storage for either original or modified geometry. */
struct NodeGeometry {
  /* Is used for sanity check, helping with ensuring that two and only two
   * geometry pushes happened in the undo stack. */
  bool is_initialized;

  CustomData vert_data;
  CustomData edge_data;
  CustomData corner_data;
  CustomData face_data;
  int *face_offset_indices;
  const ImplicitSharingInfo *face_offsets_sharing_info;
  int totvert;
  int totedge;
  int totloop;
  int faces_num;
};

struct Node {
  Type type;

  char idname[MAX_ID_NAME]; /* Name instead of pointer. */
  const void *node;         /* only during push, not valid afterwards! */

  Array<float3> position;
  Array<float3> orig_position;
  Array<float3> normal;
  Array<float4> col;
  Array<float> mask;

  Array<float4> loop_col;
  Array<float4> orig_loop_col;

  /* Mesh. */

  /* to verify if totvert it still the same */
  int mesh_verts_num;
  int mesh_corners_num;

  Array<int> vert_indices;
  int unique_verts_num;

  Array<int> corner_indices;

  BitVector<> vert_hidden;
  BitVector<> face_hidden;

  /* Multires. */

  /** The number of grids in the entire mesh. */
  int mesh_grids_num;
  /** A copy of #SubdivCCG::grid_size. */
  int grid_size;
  /** Indices of grids in the PBVH node. */
  Array<int> grids;
  BitGroupVector<> grid_hidden;

  /* bmesh */
  BMLogEntry *bm_entry;
  bool applied;

  /* shape keys */
  char shapeName[MAX_NAME]; /* `sizeof(KeyBlock::name)`. */

  /* Geometry modification operations.
   *
   * Original geometry is stored before some modification is run and is used to restore state of
   * the object when undoing the operation
   *
   * Modified geometry is stored after the modification and is used to redo the modification. */
  bool geometry_clear_pbvh;
  undo::NodeGeometry geometry_original;
  undo::NodeGeometry geometry_modified;

  /* Geometry at the bmesh enter moment. */
  undo::NodeGeometry geometry_bmesh_enter;

  /* pivot */
  float3 pivot_pos;
  float pivot_rot[4];

  /* Sculpt Face Sets */
  Array<int> face_sets;

  Vector<int> face_indices;

  size_t undo_size;
};

}

/* Factor of brush to have rake point following behind
 * (could be configurable but this is reasonable default). */
#define SCULPT_RAKE_BRUSH_FACTOR 0.25f

struct SculptRakeData {
  float follow_dist;
  blender::float3 follow_co;
  float angle;
};

/*************** Brush testing declarations ****************/
struct SculptBrushTest {
  float radius_squared;
  float radius;
  blender::float3 location;
  float dist;
  ePaintSymmetryFlags mirror_symmetry_pass;

  int radial_symmetry_pass;
  blender::float4x4 symm_rot_mat_inv;

  /* For circle (not sphere) projection. */
  float plane_view[4];

  /* Some tool code uses a plane for its calculations. */
  float plane_tool[4];

  /* View3d clipping - only set rv3d for clipping */
  RegionView3D *clip_rv3d;
};

using SculptBrushTestFn = bool (*)(SculptBrushTest *test, const float co[3]);

/* Sculpt Filters */
enum SculptFilterOrientation {
  SCULPT_FILTER_ORIENTATION_LOCAL = 0,
  SCULPT_FILTER_ORIENTATION_WORLD = 1,
  SCULPT_FILTER_ORIENTATION_VIEW = 2,
};

/* Defines how transform tools are going to apply its displacement. */
enum SculptTransformDisplacementMode {
  /* Displaces the elements from their original coordinates. */
  SCULPT_TRANSFORM_DISPLACEMENT_ORIGINAL = 0,
  /* Displaces the elements incrementally from their previous position. */
  SCULPT_TRANSFORM_DISPLACEMENT_INCREMENTAL = 1,
};

#define SCULPT_CLAY_STABILIZER_LEN 10

namespace blender::ed::sculpt_paint {

namespace filter {

struct Cache {
  bool enabled_axis[3];
  bool enabled_force_axis[3];
  int random_seed;

  /* Used for alternating between filter operations in filters that need to apply different ones to
   * achieve certain effects. */
  int iteration_count;

  /* Stores the displacement produced by the laplacian step of HC smooth. */
  float (*surface_smooth_laplacian_disp)[3];
  float surface_smooth_shape_preservation;
  float surface_smooth_current_vertex;

  /* Sharpen mesh filter. */
  float sharpen_smooth_ratio;
  float sharpen_intensify_detail_strength;
  int sharpen_curvature_smooth_iterations;
  float *sharpen_factor;
  float (*detail_directions)[3];

  /* Filter orientation. */
  SculptFilterOrientation orientation;
  float4x4 obmat;
  float4x4 obmat_inv;
  float4x4 viewmat;
  float4x4 viewmat_inv;

  /* Displacement eraser. */
  float (*limit_surface_co)[3];

  /* unmasked nodes */
  Vector<PBVHNode *> nodes;

  /* Cloth filter. */
  cloth::SimulationData *cloth_sim;
  float3 cloth_sim_pinch_point;

  /* mask expand iteration caches */
  int mask_update_current_it;
  int mask_update_last_it;
  int *mask_update_it;
  float *normal_factor;
  float *edge_factor;
  float *prev_mask;
  float3 mask_expand_initial_co;

  int new_face_set;
  int *prev_face_set;

  int active_face_set;

  SculptTransformDisplacementMode transform_displacement_mode;

  std::unique_ptr<auto_mask::Cache> automasking;
  float3 initial_normal;
  float3 view_normal;

  /* Pre-smoothed colors used by sharpening. Colors are HSL. */
  float (*pre_smoothed_color)[4];

  ViewContext vc;
  float start_filter_strength;
  bool no_orig_co;
};

}

/**
 * This structure contains all the temporary data
 * needed for individual brush strokes.
 */
struct StrokeCache {
  /* Invariants */
  float initial_radius;
  float3 scale;
  int flag;
  float3 clip_tolerance;
  float4x4 clip_mirror_mtx;
  float2 initial_mouse;

  /* Variants */
  float radius;
  float radius_squared;
  float3 true_location;
  float3 true_last_location;
  float3 location;
  float3 last_location;
  float stroke_distance;

  /* Used for alternating between deformation in brushes that need to apply different ones to
   * achieve certain effects. */
  int iteration_count;

  /* Original pixel radius with the pressure curve applied for dyntopo detail size */
  float dyntopo_pixel_radius;

  bool is_last_valid;

  bool pen_flip;
  bool invert;
  float pressure;
  float bstrength;
  float normal_weight; /* from brush (with optional override) */
  float x_tilt;
  float y_tilt;

  /* Position of the mouse corresponding to the stroke location, modified by the paint_stroke
   * operator according to the stroke type. */
  float2 mouse;
  /* Position of the mouse event in screen space, not modified by the stroke type. */
  float2 mouse_event;

  float (*prev_colors)[4];
  GArray<> prev_colors_vpaint;

  /* Multires Displacement Smear. */
  float (*prev_displacement)[3];
  float (*limit_surface_co)[3];

  /* The rest is temporary storage that isn't saved as a property */

  bool first_time; /* Beginning of stroke may do some things special */

  /* from ED_view3d_ob_project_mat_get() */
  float4x4 projection_mat;

  /* Clean this up! */
  ViewContext *vc;
  const Brush *brush;

  float special_rotation;
  float3 grab_delta, grab_delta_symmetry;
  float3 old_grab_location, orig_grab_location;

  /* screen-space rotation defined by mouse motion */
  float rake_rotation[4], rake_rotation_symmetry[4];
  bool is_rake_rotation_valid;
  SculptRakeData rake_data;

  /* Face Sets */
  int paint_face_set;

  /* Symmetry index between 0 and 7 bit combo 0 is Brush only;
   * 1 is X mirror; 2 is Y mirror; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  int symmetry;
  /* The symmetry pass we are currently on between 0 and 7. */
  ePaintSymmetryFlags mirror_symmetry_pass;
  float3 true_view_normal;
  float3 view_normal;

  /* sculpt_normal gets calculated by calc_sculpt_normal(), then the
   * sculpt_normal_symm gets updated quickly with the usual symmetry
   * transforms */
  float3 sculpt_normal;
  float3 sculpt_normal_symm;

  /* Used for area texture mode, local_mat gets calculated by
   * calc_brush_local_mat() and used in sculpt_apply_texture().
   * Transforms from model-space coords to local area coords.
   */
  float4x4 brush_local_mat;
  /* The matrix from local area coords to model-space coords is used to calculate the vector
   * displacement in area plane mode. */
  float4x4 brush_local_mat_inv;

  float3 plane_offset; /* used to shift the plane around when doing tiled strokes */
  int tile_pass;

  float3 last_center;
  int radial_symmetry_pass;
  float4x4 symm_rot_mat;
  float4x4 symm_rot_mat_inv;

  /* Accumulate mode. Note: inverted for SCULPT_TOOL_DRAW_SHARP. */
  bool accum;

  float3 anchored_location;

  /* Paint Brush. */
  struct {
    float hardness;
    float flow;
    float wet_mix;
    float wet_persistence;
    float density;
  } paint_brush;

  /* Pose brush */
  std::unique_ptr<SculptPoseIKChain> pose_ik_chain;

  /* Enhance Details. */
  float (*detail_directions)[3];

  /* Clay Thumb brush */
  /* Angle of the front tilting plane of the brush to simulate clay accumulation. */
  float clay_thumb_front_angle;
  /* Stores pressure samples to get an stabilized strength and radius variation. */
  float clay_pressure_stabilizer[SCULPT_CLAY_STABILIZER_LEN];
  int clay_pressure_stabilizer_index;

  /* Cloth brush */
  cloth::SimulationData *cloth_sim;
  float3 initial_location;
  float3 true_initial_location;
  float3 initial_normal;
  float3 true_initial_normal;

  /* Boundary brush */
  SculptBoundary *boundaries[PAINT_SYMM_AREAS];

  /* Surface Smooth Brush */
  /* Stores the displacement produced by the laplacian step of HC smooth. */
  float (*surface_smooth_laplacian_disp)[3];

  /* Layer brush */
  float *layer_displacement_factor;

  float vertex_rotation; /* amount to rotate the vertices when using rotate brush */
  Dial *dial;

  char saved_active_brush_name[MAX_ID_NAME];
  char saved_mask_brush_tool;
  int saved_smooth_size; /* smooth tool copies the size of the current tool */
  bool alt_smooth;

  float plane_trim_squared;

  bool supports_gravity;
  float3 true_gravity_direction;
  float3 gravity_direction;

  std::unique_ptr<auto_mask::Cache> automasking;

  float4x4 stroke_local_mat;
  float multiplane_scrape_angle;

  float4 wet_mix_prev_color;
  float density_seed;

  rcti previous_r; /* previous redraw rectangle */
  rcti current_r;  /* current redraw rectangle */

  int stroke_id;
};

/* -------------------------------------------------------------------- */
/** \name Sculpt Expand
 * \{ */

namespace expand {

enum eSculptExpandFalloffType {
  SCULPT_EXPAND_FALLOFF_GEODESIC,
  SCULPT_EXPAND_FALLOFF_TOPOLOGY,
  SCULPT_EXPAND_FALLOFF_TOPOLOGY_DIAGONALS,
  SCULPT_EXPAND_FALLOFF_NORMALS,
  SCULPT_EXPAND_FALLOFF_SPHERICAL,
  SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY,
  SCULPT_EXPAND_FALLOFF_BOUNDARY_FACE_SET,
  SCULPT_EXPAND_FALLOFF_ACTIVE_FACE_SET,
};

enum eSculptExpandTargetType {
  SCULPT_EXPAND_TARGET_MASK,
  SCULPT_EXPAND_TARGET_FACE_SETS,
  SCULPT_EXPAND_TARGET_COLORS,
};

enum eSculptExpandRecursionType {
  SCULPT_EXPAND_RECURSION_TOPOLOGY,
  SCULPT_EXPAND_RECURSION_GEODESICS,
};

#define EXPAND_SYMM_AREAS 8

struct Cache {
  /* Target data elements that the expand operation will affect. */
  eSculptExpandTargetType target;

  /* Falloff data. */
  eSculptExpandFalloffType falloff_type;

  /* Indexed by vertex index, precalculated falloff value of that vertex (without any falloff
   * editing modification applied). */
  Array<float> vert_falloff;
  /* Max falloff value in *vert_falloff. */
  float max_vert_falloff;

  /* Indexed by base mesh face index, precalculated falloff value of that face. These values are
   * calculated from the per vertex falloff (*vert_falloff) when needed. */
  float *face_falloff;
  float max_face_falloff;

  /* Falloff value of the active element (vertex or base mesh face) that Expand will expand to. */
  float active_falloff;

  /* When set to true, expand skips all falloff computations and considers all elements as enabled.
   */
  bool all_enabled;

  /* Initial mouse and cursor data from where the current falloff started. This data can be changed
   * during the execution of Expand by moving the origin. */
  float2 initial_mouse_move;
  float2 initial_mouse;
  PBVHVertRef initial_active_vertex;
  int initial_active_vertex_i;
  int initial_active_face_set;

  /* Maximum number of vertices allowed in the SculptSession for previewing the falloff using
   * geodesic distances. */
  int max_geodesic_move_preview;

  /* Original falloff type before starting the move operation. */
  eSculptExpandFalloffType move_original_falloff_type;
  /* Falloff type using when moving the origin for preview. */
  eSculptExpandFalloffType move_preview_falloff_type;

  /* Face set ID that is going to be used when creating a new Face Set. */
  int next_face_set;

  /* Face Set ID of the Face set selected for editing. */
  int update_face_set;

  /* Mouse position since the last time the origin was moved. Used for reference when moving the
   * initial position of Expand. */
  float2 original_mouse_move;

  /* Active island checks. */
  /* Indexed by symmetry pass index, contains the connected island ID for that
   * symmetry pass. Other connected island IDs not found in this
   * array will be ignored by Expand. */
  int active_connected_islands[EXPAND_SYMM_AREAS];

  /* Snapping. */
  /* Set containing all Face Sets IDs that Expand will use to snap the new data. */
  std::unique_ptr<Set<int>> snap_enabled_face_sets;

  /* Texture distortion data. */
  Brush *brush;
  Scene *scene;
  // struct MTex *mtex;

  /* Controls how much texture distortion will be applied to the current falloff */
  float texture_distortion_strength;

  /* Cached PBVH nodes. This allows to skip gathering all nodes from the PBVH each time expand
   * needs to update the state of the elements. */
  Vector<PBVHNode *> nodes;

  /* Expand state options. */

  /* Number of loops (times that the falloff is going to be repeated). */
  int loop_count;

  /* Invert the falloff result. */
  bool invert;

  /* When set to true, preserves the previous state of the data and adds the new one on top. */
  bool preserve;

  /* When set to true, the mask or colors will be applied as a gradient. */
  bool falloff_gradient;

  /* When set to true, Expand will use the Brush falloff curve data to shape the gradient. */
  bool brush_gradient;

  /* When set to true, Expand will move the origin (initial active vertex and cursor position)
   * instead of updating the active vertex and active falloff. */
  bool move;

  /* When set to true, Expand will snap the new data to the Face Sets IDs found in
   * *original_face_sets. */
  bool snap;

  /* When set to true, Expand will use the current Face Set ID to modify an existing Face Set
   * instead of creating a new one. */
  bool modify_active_face_set;

  /* When set to true, Expand will reposition the sculpt pivot to the boundary of the expand result
   * after finishing the operation. */
  bool reposition_pivot;

  /* If nothing is masked set mask of every vertex to 0. */
  bool auto_mask;

  /* Color target data type related data. */
  float fill_color[4];
  short blend_mode;

  /* Face Sets at the first step of the expand operation, before starting modifying the active
   * vertex and active falloff. These are not the original Face Sets of the sculpt before starting
   * the operator as they could have been modified by Expand when initializing the operator and
   * before starting changing the active vertex. These Face Sets are used for restoring and
   * checking the Face Sets state while the Expand operation modal runs. */
  Array<int> initial_face_sets;

  /* Original data of the sculpt as it was before running the Expand operator. */
  Array<float> original_mask;
  Array<int> original_face_sets;
  float (*original_colors)[4];

  bool check_islands;
  int normal_falloff_blur_steps;
};

}

}

/** \} */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Poll Functions
 * \{ */

bool SCULPT_mode_poll(bContext *C);
bool SCULPT_mode_poll_view3d(bContext *C);
/**
 * Checks for a brush, not just sculpt mode.
 */
bool SCULPT_poll(bContext *C);

/**
 * Returns true if sculpt session can handle color attributes
 * (BKE_pbvh_type(ss->pbvh) == PBVH_FACES).  If false an error
 * message will be shown to the user.  Operators should return
 * OPERATOR_CANCELLED in this case.
 *
 * NOTE: Does not check if a color attribute actually exists.
 * Calling code must handle this itself; in most cases a call to
 * BKE_sculpt_color_layer_create_if_needed() is sufficient.
 */
bool SCULPT_handles_colors_report(SculptSession *ss, ReportList *reports);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Update Functions
 * \{ */

void SCULPT_flush_update_step(bContext *C, SculptUpdateType update_flags);
void SCULPT_flush_update_done(const bContext *C, Object *ob, SculptUpdateType update_flags);

void SCULPT_pbvh_clear(Object *ob);

/**
 * Flush displacement from deformed PBVH to original layer.
 */
void SCULPT_flush_stroke_deform(Sculpt *sd, Object *ob, bool is_proxy_used);

/**
 * Should be used after modifying the mask or Face Sets IDs.
 */
void SCULPT_tag_update_overlays(bContext *C);
/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Functions
 * \{ */

/**
 * Do a ray-cast in the tree to find the 3d brush location
 * (This allows us to ignore the GL depth buffer)
 * Returns 0 if the ray doesn't hit the mesh, non-zero otherwise.
 *
 * If check_closest is true and the ray test fails a point closest
 * to the ray will be found. If limit_closest_radius is true then
 * the closest point will be tested against the active brush radius.
 */
bool SCULPT_stroke_get_location_ex(bContext *C,
                                   float out[3],
                                   const float mval[2],
                                   bool force_original,
                                   bool check_closest,
                                   bool limit_closest_radius);

bool SCULPT_stroke_get_location(bContext *C,
                                float out[3],
                                const float mouse[2],
                                bool force_original);
/**
 * Gets the normal, location and active vertex location of the geometry under the cursor. This also
 * updates the active vertex and cursor related data of the SculptSession using the mouse position
 */
bool SCULPT_cursor_geometry_info_update(bContext *C,
                                        SculptCursorGeometryInfo *out,
                                        const float mouse[2],
                                        bool use_sampled_normal);
void SCULPT_geometry_preview_lines_update(bContext *C, SculptSession *ss, float radius);

void SCULPT_stroke_modifiers_check(const bContext *C, Object *ob, const Brush *brush);
float SCULPT_raycast_init(ViewContext *vc,
                          const float mval[2],
                          float ray_start[3],
                          float ray_end[3],
                          float ray_normal[3],
                          bool original);

/* Symmetry */
ePaintSymmetryFlags SCULPT_mesh_symmetry_xyz_get(Object *object);

/**
 * Returns true when the step belongs to the stroke that is directly performed by the brush and
 * not by one of the symmetry passes.
 */
bool SCULPT_stroke_is_main_symmetry_pass(blender::ed::sculpt_paint::StrokeCache *cache);
/**
 * Return true only once per stroke on the first symmetry pass, regardless of the symmetry passes
 * enabled.
 *
 * This should be used for functionality that needs to be computed once per stroke of a particular
 * tool (allocating memory, updating random seeds...).
 */
bool SCULPT_stroke_is_first_brush_step(blender::ed::sculpt_paint::StrokeCache *cache);
/**
 * Returns true on the first brush step of each symmetry pass.
 */
bool SCULPT_stroke_is_first_brush_step_of_symmetry_pass(
    blender::ed::sculpt_paint::StrokeCache *cache);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt mesh accessor API
 * \{ */

struct SculptMaskWriteInfo {
  float *layer = nullptr;
  int bm_offset = -1;
};
SculptMaskWriteInfo SCULPT_mask_get_for_write(SculptSession *ss);
inline void SCULPT_mask_vert_set(const PBVHType type,
                                 const SculptMaskWriteInfo mask_write,
                                 const float value,
                                 PBVHVertexIter &vd)
{
  switch (type) {
    case PBVH_FACES:
      mask_write.layer[vd.index] = value;
      break;
    case PBVH_BMESH:
      BM_ELEM_CD_SET_FLOAT(vd.bm_vert, mask_write.bm_offset, value);
      break;
    case PBVH_GRIDS:
      *CCG_elem_mask(&vd.key, vd.grid) = value;
      break;
  }
}

/** Ensure random access; required for PBVH_BMESH */
void SCULPT_vertex_random_access_ensure(SculptSession *ss);

int SCULPT_vertex_count_get(const SculptSession *ss);
const float *SCULPT_vertex_co_get(const SculptSession *ss, PBVHVertRef vertex);

/** Get the normal for a given sculpt vertex; do not modify the result */
void SCULPT_vertex_normal_get(const SculptSession *ss, PBVHVertRef vertex, float no[3]);

float SCULPT_mask_get_at_grids_vert_index(const SubdivCCG &subdiv_ccg,
                                          const CCGKey &key,
                                          int vert_index);
void SCULPT_vertex_color_get(const SculptSession *ss, PBVHVertRef vertex, float r_color[4]);
void SCULPT_vertex_color_set(SculptSession *ss, PBVHVertRef vertex, const float color[4]);

bool SCULPT_vertex_is_occluded(SculptSession *ss, PBVHVertRef vertex, bool original);

/** Returns true if a color attribute exists in the current sculpt session. */
bool SCULPT_has_colors(const SculptSession *ss);

/** Returns true if the active color attribute is on loop (AttrDomain::Corner) domain. */
bool SCULPT_has_loop_colors(const Object *ob);

const float *SCULPT_vertex_persistent_co_get(SculptSession *ss, PBVHVertRef vertex);
void SCULPT_vertex_persistent_normal_get(SculptSession *ss, PBVHVertRef vertex, float no[3]);

/**
 * Coordinates used for manipulating the base mesh when Grab Active Vertex is enabled.
 */
const float *SCULPT_vertex_co_for_grab_active_get(SculptSession *ss, PBVHVertRef vertex);

/**
 * Returns the info of the limit surface when multi-res is available,
 * otherwise it returns the current coordinate of the vertex.
 */
void SCULPT_vertex_limit_surface_get(SculptSession *ss, PBVHVertRef vertex, float r_co[3]);

/**
 * Returns the pointer to the coordinates that should be edited from a brush tool iterator
 * depending on the given deformation target.
 */
float *SCULPT_brush_deform_target_vertex_co_get(SculptSession *ss,
                                                int deform_target,
                                                PBVHVertexIter *iter);

void SCULPT_vertex_neighbors_get(SculptSession *ss,
                                 PBVHVertRef vertex,
                                 bool include_duplicates,
                                 SculptVertexNeighborIter *iter);

/** Iterator over neighboring vertices. */
#define SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN(ss, v_index, neighbor_iterator) \
  SCULPT_vertex_neighbors_get(ss, v_index, false, &neighbor_iterator); \
  for (neighbor_iterator.i = 0; neighbor_iterator.i < neighbor_iterator.size; \
       neighbor_iterator.i++) \
  { \
    neighbor_iterator.vertex = neighbor_iterator.neighbors[neighbor_iterator.i]; \
    neighbor_iterator.index = neighbor_iterator.neighbor_indices[neighbor_iterator.i];

/**
 * Iterate over neighboring and duplicate vertices (for PBVH_GRIDS).
 * Duplicates come first since they are nearest for flood-fill.
 */
#define SCULPT_VERTEX_DUPLICATES_AND_NEIGHBORS_ITER_BEGIN(ss, v_index, neighbor_iterator) \
  SCULPT_vertex_neighbors_get(ss, v_index, true, &neighbor_iterator); \
  for (neighbor_iterator.i = neighbor_iterator.size - 1; neighbor_iterator.i >= 0; \
       neighbor_iterator.i--) \
  { \
    neighbor_iterator.vertex = neighbor_iterator.neighbors[neighbor_iterator.i]; \
    neighbor_iterator.index = neighbor_iterator.neighbor_indices[neighbor_iterator.i]; \
    neighbor_iterator.is_duplicate = (neighbor_iterator.i >= \
                                      neighbor_iterator.size - neighbor_iterator.num_duplicates);

#define SCULPT_VERTEX_NEIGHBORS_ITER_END(neighbor_iterator) \
  } \
  if (neighbor_iterator.neighbors != neighbor_iterator.neighbors_fixed) { \
    MEM_freeN(neighbor_iterator.neighbors); \
  } \
  ((void)0)

PBVHVertRef SCULPT_active_vertex_get(SculptSession *ss);
const float *SCULPT_active_vertex_co_get(SculptSession *ss);

/* Returns PBVH deformed vertices array if shape keys or deform modifiers are used, otherwise
 * returns mesh original vertices array. */
blender::MutableSpan<blender::float3> SCULPT_mesh_deformed_positions_get(SculptSession *ss);

/* Fake Neighbors */

#define FAKE_NEIGHBOR_NONE -1

void SCULPT_fake_neighbors_ensure(Object *ob, float max_dist);
void SCULPT_fake_neighbors_enable(Object *ob);
void SCULPT_fake_neighbors_disable(Object *ob);
void SCULPT_fake_neighbors_free(Object *ob);

/* Vertex Info. */
void SCULPT_boundary_info_ensure(Object *object);
/* Boundary Info needs to be initialized in order to use this function. */
bool SCULPT_vertex_is_boundary(const SculptSession *ss, PBVHVertRef vertex);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Visibility API
 * \{ */

namespace blender::ed::sculpt_paint {

namespace hide {

bool vert_visible_get(const SculptSession *ss, PBVHVertRef vertex);
bool vert_all_faces_visible_get(const SculptSession *ss, PBVHVertRef vertex);
bool vert_any_face_visible_get(SculptSession *ss, PBVHVertRef vertex);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Sets API
 * \{ */

namespace face_set {

int active_face_set_get(SculptSession *ss);
int vert_face_set_get(SculptSession *ss, PBVHVertRef vertex);

bool vert_has_face_set(SculptSession *ss, PBVHVertRef vertex, int face_set);
bool vert_has_unique_face_set(SculptSession *ss, PBVHVertRef vertex);

bke::SpanAttributeWriter<int> ensure_face_sets_mesh(Object &object);
int ensure_face_sets_bmesh(Object &object);
Array<int> duplicate_face_sets(const Mesh &mesh);

}

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Original Data API
 * \{ */

/**
 * Initialize a #SculptOrigVertData for accessing original vertex data;
 * handles #BMesh, #Mesh, and multi-resolution.
 */
void SCULPT_orig_vert_data_init(SculptOrigVertData *data,
                                Object *ob,
                                PBVHNode *node,
                                blender::ed::sculpt_paint::undo::Type type);
/**
 * Update a #SculptOrigVertData for a particular vertex from the PBVH iterator.
 */
void SCULPT_orig_vert_data_update(SculptOrigVertData *orig_data, PBVHVertexIter *iter);
/**
 * Initialize a #SculptOrigVertData for accessing original vertex data;
 * handles #BMesh, #Mesh, and multi-resolution.
 */
void SCULPT_orig_vert_data_unode_init(SculptOrigVertData *data,
                                      Object *ob,
                                      blender::ed::sculpt_paint::undo::Node *unode);
/** \} */

/* -------------------------------------------------------------------- */
/** \name Brush Utilities.
 * \{ */

bool SCULPT_tool_needs_all_pbvh_nodes(const Brush *brush);

void SCULPT_calc_brush_plane(Sculpt *sd,
                             Object *ob,
                             blender::Span<PBVHNode *> nodes,
                             float r_area_no[3],
                             float r_area_co[3]);

std::optional<blender::float3> SCULPT_calc_area_normal(Sculpt *sd,
                                                       Object *ob,
                                                       blender::Span<PBVHNode *> nodes);
/**
 * This calculates flatten center and area normal together,
 * amortizing the memory bandwidth and loop overhead to calculate both at the same time.
 */
void SCULPT_calc_area_normal_and_center(Sculpt *sd,
                                        Object *ob,
                                        blender::Span<PBVHNode *> nodes,
                                        float r_area_no[3],
                                        float r_area_co[3]);
void SCULPT_calc_area_center(Sculpt *sd,
                             Object *ob,
                             blender::Span<PBVHNode *> nodes,
                             float r_area_co[3]);

PBVHVertRef SCULPT_nearest_vertex_get(Object *ob,
                                      const float co[3],
                                      float max_distance,
                                      bool use_original);

int SCULPT_plane_point_side(const float co[3], const float plane[4]);
int SCULPT_plane_trim(const blender::ed::sculpt_paint::StrokeCache *cache,
                      const Brush *brush,
                      const float val[3]);
/**
 * Handles clipping against a mirror modifier and #SCULPT_LOCK_X/Y/Z axis flags.
 */
void SCULPT_clip(Sculpt *sd, SculptSession *ss, float co[3], const float val[3]);

float SCULPT_brush_plane_offset_get(Sculpt *sd, SculptSession *ss);

ePaintSymmetryAreas SCULPT_get_vertex_symm_area(const float co[3]);
bool SCULPT_check_vertex_pivot_symmetry(const float vco[3], const float pco[3], char symm);
/**
 * Checks if a vertex is inside the brush radius from any of its mirrored axis.
 */
bool SCULPT_is_vertex_inside_brush_radius_symm(const float vertex[3],
                                               const float br_co[3],
                                               float radius,
                                               char symm);
bool SCULPT_is_symmetry_iteration_valid(char i, char symm);
void SCULPT_flip_v3_by_symm_area(float v[3],
                                 ePaintSymmetryFlags symm,
                                 ePaintSymmetryAreas symmarea,
                                 const float pivot[3]);
void SCULPT_flip_quat_by_symm_area(float quat[4],
                                   ePaintSymmetryFlags symm,
                                   ePaintSymmetryAreas symmarea,
                                   const float pivot[3]);

/**
 * Initialize a point-in-brush test
 */
void SCULPT_brush_test_init(SculptSession *ss, SculptBrushTest *test);

bool SCULPT_brush_test_sphere_sq(SculptBrushTest *test, const float co[3]);
bool SCULPT_brush_test_cube(SculptBrushTest *test,
                            const float co[3],
                            const float local[4][4],
                            const float roundness,
                            const float tip_scale_x);
bool SCULPT_brush_test_circle_sq(SculptBrushTest *test, const float co[3]);

namespace blender::ed::sculpt_paint {

bool node_fully_masked_or_hidden(const PBVHNode &node);
bool node_in_sphere(const PBVHNode &node, const float3 &location, float radius_sq, bool original);
bool node_in_cylinder(const DistRayAABB_Precalc &dist_ray_precalc,
                      const PBVHNode &node,
                      float radius_sq,
                      bool original);

}

void SCULPT_combine_transform_proxies(Sculpt *sd, Object *ob);

/**
 * Initialize a point-in-brush test with a given falloff shape.
 *
 * \param falloff_shape: #PAINT_FALLOFF_SHAPE_SPHERE or #PAINT_FALLOFF_SHAPE_TUBE.
 * \return The brush falloff function.
 */

SculptBrushTestFn SCULPT_brush_test_init_with_falloff_shape(SculptSession *ss,
                                                            SculptBrushTest *test,
                                                            char falloff_shape);
const float *SCULPT_brush_frontface_normal_from_falloff_shape(SculptSession *ss,
                                                              char falloff_shape);
void SCULPT_cube_tip_init(Sculpt *sd, Object *ob, Brush *brush, float mat[4][4]);

/**
 * Return a multiplier for brush strength on a particular vertex.
 */
float SCULPT_brush_strength_factor(
    SculptSession *ss,
    const Brush *br,
    const float point[3],
    float len,
    const float vno[3],
    const float fno[3],
    float mask,
    const PBVHVertRef vertex,
    int thread_id,
    const blender::ed::sculpt_paint::auto_mask::NodeData *automask_data);

/**
 * Return a color of a brush texture on a particular vertex multiplied by active masks.
 */
void SCULPT_brush_strength_color(
    SculptSession *ss,
    const Brush *brush,
    const float brush_point[3],
    float len,
    const float vno[3],
    const float fno[3],
    float mask,
    const PBVHVertRef vertex,
    int thread_id,
    const blender::ed::sculpt_paint::auto_mask::NodeData *automask_data,
    float r_rgba[4]);

/**
 * Calculates the vertex offset for a single vertex depending on the brush setting rgb as vector
 * displacement.
 */
void SCULPT_calc_vertex_displacement(SculptSession *ss,
                                     const Brush *brush,
                                     float rgba[3],
                                     float r_offset[3]);

/**
 * Tilts a normal by the x and y tilt values using the view axis.
 */
void SCULPT_tilt_apply_to_normal(float r_normal[3],
                                 blender::ed::sculpt_paint::StrokeCache *cache,
                                 float tilt_strength);

/**
 * Get effective surface normal with pen tilt and tilt strength applied to it.
 */
void SCULPT_tilt_effective_normal_get(const SculptSession *ss, const Brush *brush, float r_no[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flood Fill
 * \{ */

namespace blender::ed::sculpt_paint::flood_fill {

struct FillData {
  std::queue<PBVHVertRef> queue;
  blender::BitVector<> visited_verts;
};

FillData init_fill(SculptSession *ss);
void add_active(Object *ob, SculptSession *ss, FillData *flood, float radius);
void add_initial_with_symmetry(
    Object *ob, SculptSession *ss, FillData *flood, PBVHVertRef vertex, float radius);
void add_initial(FillData *flood, PBVHVertRef vertex);
void add_and_skip_initial(FillData *flood, PBVHVertRef vertex);
void execute(SculptSession *ss,
             FillData *flood,
             FunctionRef<bool(PBVHVertRef from_v, PBVHVertRef to_v, bool is_duplicate)> func);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dynamic topology
 * \{ */

namespace blender::ed::sculpt_paint::dyntopo {

enum WarnFlag {
  VDATA = (1 << 0),
  EDATA = (1 << 1),
  LDATA = (1 << 2),
  MODIFIER = (1 << 3),
};
ENUM_OPERATORS(WarnFlag, MODIFIER);

/** Enable dynamic topology; mesh will be triangulated */
void enable_ex(Main *bmain, Depsgraph *depsgraph, Object *ob);
void disable(bContext *C, undo::Node *unode);
void disable_with_undo(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob);

/**
 * Returns true if the stroke will use dynamic topology, false
 * otherwise.
 *
 * Factors: some brushes like grab cannot do dynamic topology.
 * Others, like smooth, are better without.
 * Same goes for alt-key smoothing.
 */
bool stroke_is_dyntopo(const SculptSession *ss, const Brush *brush);

void triangulate(BMesh *bm);

WarnFlag check_attribute_warning(Scene *scene, Object *ob);

namespace detail_size {

/**
 * Scaling factor to match the displayed size to the actual sculpted size
 */
constexpr float RELATIVE_SCALE_FACTOR = 0.4f;

/**
 * Converts from Sculpt#constant_detail to the PBVH max edge length.
 */
float constant_to_detail_size(const float constant_detail, const Object *ob);

/**
 * Converts from Sculpt#detail_percent to the PBVH max edge length.
 */
float brush_to_detail_size(const float brush_percent, const float brush_radius);

/**
 * Converts from Sculpt#detail_size to the PBVH max edge length.
 */
float relative_to_detail_size(const float relative_detail,
                              const float brush_radius,
                              const float pixel_radius,
                              const float pixel_size);

/**
 * Converts from Sculpt#constant_detail to equivalent Sculpt#detail_percent value.
 *
 * Corresponds to a change from Constant & Manual Detailing to Brush Detailing.
 */
float constant_to_brush_detail(const float constant_detail,
                               const float brush_radius,
                               const Object *ob);

/**
 * Converts from Sculpt#constant_detail to equivalent Sculpt#detail_size value.
 *
 * Corresponds to a change from Constant & Manual Detailing to Relative Detailing.
 */
float constant_to_relative_detail(const float constant_detail,
                                  const float brush_radius,
                                  const float pixel_radius,
                                  const float pixel_size,
                                  const Object *ob);
}
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto-masking.
 * \{ */

namespace blender::ed::sculpt_paint::auto_mask {

struct Settings {
  /* eAutomasking_flag. */
  int flags;
  int initial_face_set;
  int initial_island_nr;

  float cavity_factor;
  int cavity_blur_steps;
  CurveMapping *cavity_curve;

  float start_normal_limit, start_normal_falloff;
  float view_normal_limit, view_normal_falloff;

  bool topology_use_brush_limit;
};

struct Cache {
  Settings settings;

  bool can_reuse_mask;
  uchar current_stroke_id;
};

struct NodeData {
  SculptOrigVertData orig_data;
  bool have_orig_data;
};

/**
 * Call before PBVH vertex iteration.
 * \param automask_data: pointer to an uninitialized #auto_mask::NodeData struct.
 */
NodeData node_begin(Object &object, const Cache *automasking, PBVHNode &node);

/* Call before factor_get and SCULPT_brush_strength_factor. */
void node_update(NodeData &automask_data, PBVHVertexIter &vd);

float factor_get(const Cache *automasking,
                 SculptSession *ss,
                 PBVHVertRef vertex,
                 const NodeData *automask_data);

/* Returns the automasking cache depending on the active tool. Used for code that can run both for
 * brushes and filter. */
Cache *active_cache_get(SculptSession *ss);

/**
 * Creates and initializes an automasking cache.
 *
 * For automasking modes that cannot be calculated in real time,
 * data is also stored at the vertex level prior to the stroke starting.
 */
std::unique_ptr<Cache> cache_init(const Sculpt *sd, Object *ob);
std::unique_ptr<Cache> cache_init(const Sculpt *sd, const Brush *brush, Object *ob);
void cache_free(Cache *automasking);

bool mode_enabled(const Sculpt *sd, const Brush *br, eAutomasking_flag mode);
bool is_enabled(const Sculpt *sd, const SculptSession *ss, const Brush *br);

bool needs_normal(const SculptSession *ss, const Sculpt *sculpt, const Brush *brush);
int settings_hash(const Object &ob, const Cache &automasking);

bool tool_can_reuse_automask(int sculpt_tool);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geodesic distances.
 * \{ */

namespace blender::ed::sculpt_paint::geodesic {

/**
 * Returns an array indexed by vertex index containing the geodesic distance to the closest vertex
 * in the initial vertex set. The caller is responsible for freeing the array.
 * Geodesic distances will only work when used with PBVH_FACES, for other types of PBVH it will
 * fallback to euclidean distances to one of the initial vertices in the set.
 */
Array<float> distances_create(Object *ob, const Set<int> &initial_verts, float limit_radius);
Array<float> distances_create_from_vert_and_symm(Object *ob,
                                                 PBVHVertRef vertex,
                                                 float limit_radius);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Filter API
 * \{ */

namespace blender::ed::sculpt_paint::filter {

void cache_init(bContext *C,
                Object *ob,
                Sculpt *sd,
                undo::Type undo_type,
                const float mval_fl[2],
                float area_normal_radius,
                float start_strength);
void cache_free(SculptSession *ss);
void register_operator_props(wmOperatorType *ot);

/* Filter orientation utils. */
void to_orientation_space(float r_v[3], filter::Cache *filter_cache);
void to_object_space(float r_v[3], filter::Cache *filter_cache);
void zero_disabled_axis_components(float r_v[3], filter::Cache *filter_cache);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cloth Simulation.
 * \{ */

namespace blender::ed::sculpt_paint::cloth {

/* Cloth Simulation. */
enum NodeSimState {
  /* Constraints were not built for this node, so it can't be simulated. */
  SCULPT_CLOTH_NODE_UNINITIALIZED,

  /* There are constraints for the geometry in this node, but it should not be simulated. */
  SCULPT_CLOTH_NODE_INACTIVE,

  /* There are constraints for this node and they should be used by the solver. */
  SCULPT_CLOTH_NODE_ACTIVE,
};

enum ConstraintType {
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

struct LengthConstraint {
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

  /* Index in #SimulationData.node_state of the node from where this constraint was created.
   * This constraints will only be used by the solver if the state is active. */
  int node;

  ConstraintType type;
};

struct SimulationData {
  LengthConstraint *length_constraints;
  int tot_length_constraints;
  Set<OrderedEdge> created_length_constraints;
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
  /** #PBVHNode pointer as a key, index in #SimulationData.node_state as value. */
  GHash *node_state_index;
  NodeSimState *node_state;

  VArraySpan<float> mask_mesh;
  int mask_cd_offset_bmesh;
  CCGKey grid_key;
};

/* Main cloth brush function */
void do_cloth_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);

void simulation_free(SimulationData *cloth_sim);

/* Public functions. */

SimulationData *brush_simulation_create(Object *ob,
                                        float cloth_mass,
                                        float cloth_damping,
                                        float cloth_softbody_strength,
                                        bool use_collisions,
                                        bool needs_deform_coords);
void brush_simulation_init(SculptSession *ss, SimulationData *cloth_sim);

void sim_activate_nodes(SimulationData *cloth_sim, Span<PBVHNode *> nodes);

void brush_store_simulation_state(SculptSession *ss, SimulationData *cloth_sim);

void do_simulation_step(Sculpt *sd, Object *ob, SimulationData *cloth_sim, Span<PBVHNode *> nodes);

void ensure_nodes_constraints(Sculpt *sd,
                              Object *ob,
                              Span<PBVHNode *> nodes,
                              SimulationData *cloth_sim,
                              float initial_location[3],
                              float radius);

/**
 * Cursor drawing function.
 */
void simulation_limits_draw(uint gpuattr,
                            const Brush *brush,
                            const float location[3],
                            const float normal[3],
                            float rds,
                            float line_width,
                            const float outline_col[3],
                            float alpha);
void plane_falloff_preview_draw(uint gpuattr,
                                SculptSession *ss,
                                const float outline_col[3],
                                float outline_alpha);

Vector<PBVHNode *> brush_affected_nodes_gather(SculptSession *ss, Brush *brush);

bool is_cloth_deform_brush(const Brush *brush);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smoothing API
 * \{ */

namespace blender::ed::sculpt_paint::smooth {

/**
 * For bmesh: Average surrounding verts based on an orthogonality measure.
 * Naturally converges to a quad-like structure.
 */
void bmesh_four_neighbor_average(float avg[3], float direction[3], BMVert *v);

void neighbor_coords_average(SculptSession *ss, float result[3], PBVHVertRef vertex);
float neighbor_mask_average(SculptSession *ss, SculptMaskWriteInfo write_info, PBVHVertRef vertex);
void neighbor_color_average(SculptSession *ss, float result[4], PBVHVertRef vertex);

/**
 * Mask the mesh boundaries smoothing only the mesh surface without using auto-masking.
 */
void neighbor_coords_average_interior(SculptSession *ss, float result[3], PBVHVertRef vertex);

void do_smooth_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float bstrength);
void do_smooth_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);

void do_smooth_mask_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float bstrength);

/* Surface Smooth Brush. */

void surface_smooth_laplacian_step(SculptSession *ss,
                                   float *disp,
                                   const float co[3],
                                   float (*laplacian_disp)[3],
                                   PBVHVertRef vertex,
                                   const float origco[3],
                                   float alpha);
void surface_smooth_displace_step(SculptSession *ss,
                                  float *co,
                                  float (*laplacian_disp)[3],
                                  PBVHVertRef vertex,
                                  float beta,
                                  float fade);
void do_surface_smooth_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);

/* Slide/Relax */
void relax_vertex(SculptSession *ss,
                  PBVHVertexIter *vd,
                  float factor,
                  bool filter_boundary_face_sets,
                  float *r_final_pos);

}

/** \} */

/**
 * Expose 'calc_area_normal' externally (just for vertex paint).
 */
std::optional<blender::float3> SCULPT_pbvh_calc_area_normal(const Brush *brush,
                                                            Object *ob,
                                                            blender::Span<PBVHNode *> nodes);

/**
 * Flip all the edit-data across the axis/axes specified by \a symm.
 * Used to calculate multiple modifications to the mesh when symmetry is enabled.
 */
void SCULPT_cache_calc_brushdata_symm(blender::ed::sculpt_paint::StrokeCache *cache,
                                      ePaintSymmetryFlags symm,
                                      char axis,
                                      float angle);
void SCULPT_cache_free(blender::ed::sculpt_paint::StrokeCache *cache);

/* -------------------------------------------------------------------- */
/** \name Sculpt Undo
 * \{ */

namespace blender::ed::sculpt_paint::undo {

undo::Node *push_node(const Object &object, const PBVHNode *node, undo::Type type);
undo::Node *get_node(const PBVHNode *node, undo::Type type);

/**
 * Pushes an undo step using the operator name. This is necessary for
 * redo panels to work; operators that do not support that may use
 * #push_begin_ex instead if so desired.
 */
void push_begin(Object *ob, const wmOperator *op);

/**
 * NOTE: #push_begin is preferred since `name`
 * must match operator name for redo panels to work.
 */
void push_begin_ex(Object *ob, const char *name);
void push_end(Object *ob);
void push_end_ex(Object *ob, const bool use_nested_undo);

}

/** \} */

void SCULPT_vertcos_to_key(Object *ob, KeyBlock *kb, blender::Span<blender::float3> vertCos);

/**
 * Get a screen-space rectangle of the modified area.
 */
bool SCULPT_get_redraw_rect(ARegion *region, RegionView3D *rv3d, Object *ob, rcti *rect);

/* Operators. */

/* -------------------------------------------------------------------- */
/** \name Expand Operator
 * \{ */

namespace blender::ed::sculpt_paint::expand {

void SCULPT_OT_expand(wmOperatorType *ot);
void modal_keymap(wmKeyConfig *keyconf);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gesture Operators
 * \{ */

namespace blender::ed::sculpt_paint::gesture {
enum ShapeType {
  Box = 0,
  Lasso = 1,
  Line = 2,
};

enum class SelectionType {
  Inside = 0,
  Outside = 1,
};

struct LassoData {
  float4x4 projviewobjmat;

  rcti boundbox;
  int width;

  /* 2D bitmap to test if a vertex is affected by the lasso shape. */
  blender::BitVector<> mask_px;
};

struct LineData {
  /* Plane aligned to the gesture line. */
  float true_plane[4];
  float plane[4];

  /* Planes to limit the action to the length of the gesture segment at both sides of the affected
   * area. */
  float side_plane[2][4];
  float true_side_plane[2][4];
  bool use_side_planes;

  bool flip;
};

struct Operation;

/* Common data used for executing a gesture operation. */
struct GestureData {
  SculptSession *ss;
  ViewContext vc;

  /* Enabled and currently active symmetry. */
  ePaintSymmetryFlags symm;
  ePaintSymmetryFlags symmpass;

  /* Operation parameters. */
  ShapeType shape_type;
  bool front_faces_only;
  SelectionType selection_type;

  Operation *operation;

  /* Gesture data. */
  /* Screen space points that represent the gesture shape. */
  Array<float2> gesture_points;

  /* View parameters. */
  float3 true_view_normal;
  float3 view_normal;

  float3 true_view_origin;
  float3 view_origin;

  float true_clip_planes[4][4];
  float clip_planes[4][4];

  /* These store the view origin and normal in world space, which is used in some gestures to
   * generate geometry aligned from the view directly in world space. */
  /* World space view origin and normal are not affected by object symmetry when doing symmetry
   * passes, so there is no separate variables with the `true_` prefix to store their original
   * values without symmetry modifications. */
  float3 world_space_view_origin;
  float3 world_space_view_normal;

  /* Lasso Gesture. */
  LassoData lasso;

  /* Line Gesture. */
  LineData line;

  /* Task Callback Data. */
  Vector<PBVHNode *> nodes;

  ~GestureData();
};

/* Common abstraction structure for gesture operations. */
struct Operation {
  /* Initial setup (data updates, special undo push...). */
  void (*begin)(bContext &, GestureData &);

  /* Apply the gesture action for each symmetry pass. */
  void (*apply_for_symmetry_pass)(bContext &, GestureData &);

  /* Remaining actions after finishing the symmetry passes iterations
   * (updating data-layers, tagging PBVH updates...). */
  void (*end)(bContext &, GestureData &);
};

/* Determines whether or not a gesture action should be applied. */
bool is_affected(GestureData &gesture_data, const float3 &co, const float3 &vertex_normal);

/* Initialization functions. */
std::unique_ptr<GestureData> init_from_box(bContext *C, wmOperator *op);
std::unique_ptr<GestureData> init_from_lasso(bContext *C, wmOperator *op);
std::unique_ptr<GestureData> init_from_line(bContext *C, wmOperator *op);

/* Common gesture operator properties. */
void operator_properties(wmOperatorType *ot, ShapeType shapeType);

/* Apply the gesture action to the selected nodes. */
void apply(bContext &C, GestureData &gesture_data, wmOperator &op);

}

namespace blender::ed::sculpt_paint::project {
void SCULPT_OT_project_line_gesture(wmOperatorType *ot);
}

namespace blender::ed::sculpt_paint::trim {
void SCULPT_OT_trim_lasso_gesture(wmOperatorType *ot);
void SCULPT_OT_trim_box_gesture(wmOperatorType *ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Set Operators
 * \{ */

namespace blender::ed::sculpt_paint::face_set {

void SCULPT_OT_face_sets_randomize_colors(wmOperatorType *ot);
void SCULPT_OT_face_set_change_visibility(wmOperatorType *ot);
void SCULPT_OT_face_sets_init(wmOperatorType *ot);
void SCULPT_OT_face_sets_create(wmOperatorType *ot);
void SCULPT_OT_face_sets_edit(wmOperatorType *ot);

void SCULPT_OT_face_set_lasso_gesture(wmOperatorType *ot);
void SCULPT_OT_face_set_box_gesture(wmOperatorType *ot);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Operators
 * \{ */

namespace blender::ed::sculpt_paint {

void SCULPT_OT_set_pivot_position(wmOperatorType *ot);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Filter Operators
 * \{ */

namespace blender::ed::sculpt_paint::filter {

void SCULPT_OT_mesh_filter(wmOperatorType *ot);
wmKeyMap *modal_keymap(wmKeyConfig *keyconf);

}

namespace blender::ed::sculpt_paint::cloth {
void SCULPT_OT_cloth_filter(wmOperatorType *ot);
}

namespace blender::ed::sculpt_paint::color {
void SCULPT_OT_color_filter(wmOperatorType *ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interactive Mask Operators
 * \{ */

namespace blender::ed::sculpt_paint::mask {

void SCULPT_OT_mask_filter(wmOperatorType *ot);
void SCULPT_OT_mask_init(wmOperatorType *ot);

}

/** \} */

/* Detail size. */

/* -------------------------------------------------------------------- */
/** \name Dyntopo/Retopology Operators
 * \{ */

namespace blender::ed::sculpt_paint::dyntopo {

void SCULPT_OT_detail_flood_fill(wmOperatorType *ot);
void SCULPT_OT_sample_detail_size(wmOperatorType *ot);
void SCULPT_OT_dyntopo_detail_size_edit(wmOperatorType *ot);
void SCULPT_OT_dynamic_topology_toggle(wmOperatorType *ot);

}

/** \} */

/* sculpt_brush_types.cc */

/* -------------------------------------------------------------------- */
/** \name Brushes
 * \{ */

namespace blender::ed::sculpt_paint::pose {

/**
 * Main Brush Function.
 */
void do_pose_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
/**
 * Calculate the pose origin and (Optionally the pose factor)
 * that is used when using the pose brush.
 *
 * \param r_pose_origin: Must be a valid pointer.
 * \param r_pose_factor: Optional, when set to NULL it won't be calculated.
 */
void calc_pose_data(Object *ob,
                    SculptSession *ss,
                    const float3 &initial_location,
                    float radius,
                    float pose_offset,
                    float3 &r_pose_origin,
                    MutableSpan<float> r_pose_factor);
void pose_brush_init(Object *ob, SculptSession *ss, Brush *br);
std::unique_ptr<SculptPoseIKChain> ik_chain_init(
    Object *ob, SculptSession *ss, Brush *br, const float3 &initial_location, float radius);

}

namespace blender::ed::sculpt_paint::boundary {

/**
 * Main function to get #SculptBoundary data both for brush deformation and viewport preview.
 * Can return NULL if there is no boundary from the given vertex using the given radius.
 */
SculptBoundary *data_init(Object *object, Brush *brush, PBVHVertRef initial_vertex, float radius);
void data_free(SculptBoundary *boundary);
/* Main Brush Function. */
void do_boundary_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);

void edges_preview_draw(uint gpuattr,
                        SculptSession *ss,
                        const float outline_col[3],
                        float outline_alpha);
void pivot_line_preview_draw(uint gpuattr, SculptSession *ss);

}

/* Multi-plane Scrape Brush. */
/* Main Brush Function. */
void SCULPT_do_multiplane_scrape_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_multiplane_scrape_preview_draw(uint gpuattr,
                                           Brush *brush,
                                           SculptSession *ss,
                                           const float outline_col[3],
                                           float outline_alpha);

namespace blender::ed::sculpt_paint {

namespace face_set {

void do_draw_face_sets_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);

}

namespace color {

void do_paint_brush(PaintModeSettings *paint_mode_settings,
                    Sculpt *sd,
                    Object *ob,
                    Span<PBVHNode *> nodes,
                    Span<PBVHNode *> texnodes);
void do_smear_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
}

}
/**
 * \brief Get the image canvas for painting on the given object.
 *
 * \return #true if an image is found. The #r_image and #r_image_user fields are filled with
 * the image and image user. Returns false when the image isn't found. In the later case the
 * r_image and r_image_user are set to NULL.
 */
bool SCULPT_paint_image_canvas_get(PaintModeSettings *paint_mode_settings,
                                   Object *ob,
                                   Image **r_image,
                                   ImageUser **r_image_user) ATTR_NONNULL();
void SCULPT_do_paint_brush_image(PaintModeSettings *paint_mode_settings,
                                 Sculpt *sd,
                                 Object *ob,
                                 blender::Span<PBVHNode *> texnodes);
bool SCULPT_use_image_paint_brush(PaintModeSettings *settings, Object *ob) ATTR_NONNULL();

float SCULPT_clay_thumb_get_stabilized_pressure(blender::ed::sculpt_paint::StrokeCache *cache);

void SCULPT_do_draw_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);

void SCULPT_do_fill_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_scrape_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_clay_thumb_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_flatten_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_clay_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_clay_strips_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_snake_hook_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_thumb_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_rotate_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_layer_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_inflate_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_nudge_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_crease_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_pinch_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_grab_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_elastic_deform_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_draw_sharp_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_slide_relax_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);

void SCULPT_do_displacement_smear_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_displacement_eraser_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_mask_brush_draw(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
void SCULPT_do_mask_brush(Sculpt *sd, Object *ob, blender::Span<PBVHNode *> nodes);
/** \} */

void SCULPT_bmesh_topology_rake(Sculpt *sd,
                                Object *ob,
                                blender::Span<PBVHNode *> nodes,
                                float bstrength);

/* end sculpt_brush_types.cc */

/* sculpt_ops.cc */

namespace blender::ed::sculpt_paint {

void SCULPT_OT_brush_stroke(wmOperatorType *ot);

}

inline bool SCULPT_tool_is_paint(int tool)
{
  return ELEM(tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR);
}

inline bool SCULPT_tool_is_mask(int tool)
{
  return ELEM(tool, SCULPT_TOOL_MASK);
}

BLI_INLINE bool SCULPT_tool_is_attribute_only(int tool)
{
  return SCULPT_tool_is_paint(tool) || SCULPT_tool_is_mask(tool) ||
         ELEM(tool, SCULPT_TOOL_DRAW_FACE_SETS);
}

void SCULPT_stroke_id_ensure(Object *ob);
void SCULPT_stroke_id_next(Object *ob);

namespace blender::ed::sculpt_paint {
void ensure_valid_pivot(const Object *ob, Scene *scene);
}

/* -------------------------------------------------------------------- */
/** \name Topology island API
 * \{
 * Each mesh island shell gets its own integer
 * key; these are temporary and internally limited to 8 bits.
 * Uses the `ss->topology_island_key` attribute.
 */

/* Ensures vertex island keys exist and are valid. */
void SCULPT_topology_islands_ensure(Object *ob);

/**
 * Mark vertex island keys as invalid.
 * Call when adding or hiding geometry.
 */
void SCULPT_topology_islands_invalidate(SculptSession *ss);

/** Get vertex island key. */
int SCULPT_vertex_island_get(const SculptSession *ss, PBVHVertRef vertex);

/** \} */

namespace blender::ed::sculpt_paint {
float sculpt_calc_radius(ViewContext *vc,
                         const Brush *brush,
                         const Scene *scene,
                         const float3 location);
}

inline void *SCULPT_vertex_attr_get(const PBVHVertRef vertex, const SculptAttribute *attr)
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
