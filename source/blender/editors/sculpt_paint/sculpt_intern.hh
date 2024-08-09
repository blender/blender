/* SPDX-FileCopyrightText: 2006 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include <optional>
#include <queue>

#include "BKE_attribute.hh"
#include "BKE_collision.h"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_subdiv_ccg.hh"

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
struct StepData;
enum class Type : int8_t;
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

namespace blender::ed::sculpt_paint {

enum class UpdateType {
  Position,
  Mask,
  Visibility,
  Color,
  Image,
  FaceSet,
};

}

struct SculptCursorGeometryInfo {
  blender::float3 location;
  blender::float3 normal;
  blender::float3 active_vertex_co;
};

#define SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY 256

struct SculptVertexNeighborIter {
  /* Storage */
  blender::Vector<PBVHVertRef, SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY> neighbors;
  blender::Vector<int, SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY> neighbor_indices;

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

  blender::ed::sculpt_paint::undo::Type undo_type;
  const blender::float3 *coords;
  const blender::float3 *normals;
  const float *vmasks;
  const blender::float4 *colors;

  /* Original coordinate, normal, and mask. */
  const float *co;
  const float *no;
  float mask;
  const float *col;
};

namespace blender::ed::sculpt_paint::undo {

enum class Type : int8_t {
  None,
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

struct Node {
  Array<float3> position;
  Array<float3> orig_position;
  Array<float3> normal;
  Array<float4> col;
  Array<float> mask;

  Array<float4> loop_col;
  Array<float4> orig_loop_col;

  /* Mesh. */

  Array<int> vert_indices;
  int unique_verts_num;

  Array<int> corner_indices;

  BitVector<> vert_hidden;
  BitVector<> face_hidden;

  /* Multires. */

  /** Indices of grids in the pbvh::Tree node. */
  Array<int> grids;
  BitGroupVector<> grid_hidden;

  /* Sculpt Face Sets */
  Array<int> face_sets;

  Vector<int> face_indices;
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

enum class FilterOrientation {
  Local = 0,
  World = 1,
  View = 2,
};

struct Cache {
  std::array<bool, 3> enabled_axis;
  int random_seed;

  /* Used for alternating between filter operations in filters that need to apply different ones to
   * achieve certain effects. */
  int iteration_count;

  /* Stores the displacement produced by the laplacian step of HC smooth. */
  Array<float3> surface_smooth_laplacian_disp;
  float surface_smooth_shape_preservation;
  float surface_smooth_current_vertex;

  /* Sharpen mesh filter. */
  float sharpen_smooth_ratio;
  float sharpen_intensify_detail_strength;
  int sharpen_curvature_smooth_iterations;
  Array<float> sharpen_factor;
  Array<float3> detail_directions;

  /* Filter orientation. */
  FilterOrientation orientation;
  float4x4 obmat;
  float4x4 obmat_inv;
  float4x4 viewmat;
  float4x4 viewmat_inv;

  /* Displacement eraser. */
  Array<float3> limit_surface_co;

  /* unmasked nodes */
  Vector<bke::pbvh::Node *> nodes;

  /* Cloth filter. */
  std::unique_ptr<cloth::SimulationData> cloth_sim;
  float3 cloth_sim_pinch_point;

  /* mask expand iteration caches */
  int mask_update_current_it;
  int mask_update_last_it;
  Array<int> mask_update_it;
  Array<float> normal_factor;
  Array<float> edge_factor;
  Array<float> prev_mask;
  float3 mask_expand_initial_co;

  int new_face_set;
  Array<int> prev_face_set;

  int active_face_set;

  SculptTransformDisplacementMode transform_displacement_mode;

  std::unique_ptr<auto_mask::Cache> automasking;
  float3 initial_normal;
  float3 view_normal;

  /* Pre-smoothed colors used by sharpening. Colors are HSL. */
  Array<float4> pre_smoothed_color;

  ViewContext vc;
  float start_filter_strength;
};

}

/** Pose Brush IK Chain. */
struct SculptPoseIKChainSegment {
  float3 orig;
  float3 head;

  float3 initial_orig;
  float3 initial_head;
  float len;
  float3 scale;
  float rot[4];
  Array<float> weights;

  /* Store a 4x4 transform matrix for each of the possible combinations of enabled XYZ symmetry
   * axis. */
  std::array<float4x4, PAINT_SYMM_AREAS> trans_mat;
  std::array<float4x4, PAINT_SYMM_AREAS> pivot_mat;
  std::array<float4x4, PAINT_SYMM_AREAS> pivot_mat_inv;
};

struct SculptPoseIKChain {
  Array<SculptPoseIKChainSegment> segments;
  float3 grab_delta_offset;
};

struct SculptBoundary {
  /* Vertex indices of the active boundary. */
  Vector<int> verts;

  /* Distance from a vertex in the boundary to initial vertex indexed by vertex index, taking into
   * account the length of all edges between them. Any vertex that is not in the boundary will have
   * a distance of 0. */
  Map<int, float> distance;

  /* Data for drawing the preview. */
  Vector<std::pair<float3, float3>> edges;

  /* Initial vertex index in the boundary which is closest to the current sculpt active vertex. */
  int initial_vert_i;

  /* Vertex that at max_propagation_steps from the boundary and closest to the original active
   * vertex that was used to initialize the boundary. This is used as a reference to check how much
   * the deformation will go into the mesh and to calculate the strength of the brushes. */
  float3 pivot_position;

  /* Stores the initial positions of the pivot and boundary initial vertex as they may be deformed
   * during the brush action. This allows to use them as a reference positions and vectors for some
   * brush effects. */
  float3 initial_vert_position;

  /* Maximum number of topology steps that were calculated from the boundary. */
  int max_propagation_steps;

  /* Indexed by vertex index, contains the topology information needed for boundary deformations.
   */
  struct {
    /* Vertex index from where the topology propagation reached this vertex. */
    Array<int> original_vertex_i;

    /* How many steps were needed to reach this vertex from the boundary. */
    Array<int> propagation_steps_num;

    /* Strength that is used to deform this vertex. */
    Array<float> strength_factor;
  } edit_info;

  /* Bend Deform type. */
  struct {
    Array<float3> pivot_rotation_axis;
    Array<float3> pivot_positions;
  } bend;

  /* Slide Deform type. */
  struct {
    Array<float3> directions;
  } slide;

  /* Twist Deform type. */
  struct {
    float3 rotation_axis;
    float3 pivot_position;
  } twist;
};

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

  /**
   * Whether or not the modifier key that controls inverting brush behavior is active currently.
   * Generally signals a change in behavior for brushes.
   *
   * \see BrushStrokeMode::BRUSH_STROKE_INVERT.
   */
  bool invert;
  float pressure;
  /**
   * Depending on the mode, can either be the raw brush strength, or a scaled (possibly negative)
   * value.
   *
   * \see #brush_strength for Sculpt Mode.
   */
  float bstrength;
  float normal_weight; /* from brush (with optional override) */
  float x_tilt;
  float y_tilt;

  /* Position of the mouse corresponding to the stroke location, modified by the paint_stroke
   * operator according to the stroke type. */
  float2 mouse;
  /* Position of the mouse event in screen space, not modified by the stroke type. */
  float2 mouse_event;

  /**
   * Used by the color attribute paint brush tool to store the brush color during a stroke and
   * composite it over the original color.
   */
  Array<float4> mix_colors;

  Array<float4> prev_colors;
  GArray<> prev_colors_vpaint;

  /* Multires Displacement Smear. */
  Array<float3> prev_displacement;
  Array<float3> limit_surface_co;

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

  /**
   * Accumulate mode.
   * \note inverted for #SCULPT_TOOL_DRAW_SHARP.
   */
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
  Array<float3> detail_directions;

  /* Clay Thumb brush */
  /* Angle of the front tilting plane of the brush to simulate clay accumulation. */
  float clay_thumb_front_angle;
  /* Stores pressure samples to get an stabilized strength and radius variation. */
  float clay_pressure_stabilizer[SCULPT_CLAY_STABILIZER_LEN];
  int clay_pressure_stabilizer_index;

  /* Cloth brush */
  std::unique_ptr<cloth::SimulationData> cloth_sim;
  float3 initial_location;
  float3 true_initial_location;
  float3 initial_normal;
  float3 true_initial_normal;

  /* Boundary brush */
  std::array<std::unique_ptr<SculptBoundary>, PAINT_SYMM_AREAS> boundaries;

  /* Surface Smooth Brush */
  /* Stores the displacement produced by the laplacian step of HC smooth. */
  Array<float3> surface_smooth_laplacian_disp;

  /* Layer brush */
  Array<float> layer_displacement_factor;

  float vertex_rotation; /* amount to rotate the vertices when using rotate brush */
  Dial *dial;

  Brush *saved_active_brush;
  char saved_mask_brush_tool;
  int saved_smooth_size; /* smooth tool copies the size of the current tool */

  /**
   * Whether or not the modifier key that controls smoothing is active currently.
   * Generally signals a change in behavior for different brushes.
   *
   * \see BrushStrokeMode::BRUSH_STROKE_SMOOTH.
   */
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

  ~StrokeCache();
};

/* -------------------------------------------------------------------- */
/** \name Sculpt Expand
 * \{ */

namespace expand {

enum class FalloffType {
  Geodesic,
  Topology,
  TopologyNormals,
  Normals,
  Sphere,
  BoundaryTopology,
  BoundaryFaceSet,
  ActiveFaceSet,
};

enum class TargetType {
  Mask,
  FaceSets,
  Colors,
};

enum class RecursionType {
  Topology,
  Geodesic,
};

#define EXPAND_SYMM_AREAS 8

struct Cache {
  /* Target data elements that the expand operation will affect. */
  TargetType target;

  /* Falloff data. */
  FalloffType falloff_type;

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
  FalloffType move_original_falloff_type;
  /* Falloff type using when moving the origin for preview. */
  FalloffType move_preview_falloff_type;

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
  const Brush *brush;
  Scene *scene;
  // struct MTex *mtex;

  /* Controls how much texture distortion will be applied to the current falloff */
  float texture_distortion_strength;

  /* Cached pbvh::Tree nodes. This allows to skip gathering all nodes from the pbvh::Tree each time
   * expand needs to update the state of the elements. */
  Vector<bke::pbvh::Node *> nodes;

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
  Array<float4> original_colors;

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
 * Determines whether or not the brush cursor should be shown in the viewport
 */
bool SCULPT_brush_cursor_poll(bContext *C);

/**
 * Returns true if sculpt session can handle color attributes
 * (ss->pbvh->type() == bke::pbvh::Type::Mesh).  If false an error
 * message will be shown to the user.  Operators should return
 * OPERATOR_CANCELLED in this case.
 *
 * NOTE: Does not check if a color attribute actually exists.
 * Calling code must handle this itself; in most cases a call to
 * BKE_sculpt_color_layer_create_if_needed() is sufficient.
 */
bool SCULPT_handles_colors_report(SculptSession &ss, ReportList *reports);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Update Functions
 * \{ */

namespace blender::ed::sculpt_paint {

/**
 * Triggers redraws, updates, and dependency graph tags as necessary after each brush calculation.
 */
void flush_update_step(bContext *C, UpdateType update_type);
/**
 * Triggers redraws, updates, and dependency graph tags as necessary when a brush stroke finishes.
 */
void flush_update_done(const bContext *C, Object &ob, UpdateType update_type);

}

void SCULPT_pbvh_clear(Object &ob);

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

namespace blender::ed::sculpt_paint {

void geometry_preview_lines_update(bContext *C, SculptSession &ss, float radius);

}

void SCULPT_stroke_modifiers_check(const bContext *C, Object &ob, const Brush &brush);
float SCULPT_raycast_init(ViewContext *vc,
                          const float mval[2],
                          float ray_start[3],
                          float ray_end[3],
                          float ray_normal[3],
                          bool original);

/* Symmetry */
ePaintSymmetryFlags SCULPT_mesh_symmetry_xyz_get(const Object &object);

/**
 * Returns true when the step belongs to the stroke that is directly performed by the brush and
 * not by one of the symmetry passes.
 */
bool SCULPT_stroke_is_main_symmetry_pass(const blender::ed::sculpt_paint::StrokeCache &cache);
/**
 * Return true only once per stroke on the first symmetry pass, regardless of the symmetry passes
 * enabled.
 *
 * This should be used for functionality that needs to be computed once per stroke of a particular
 * tool (allocating memory, updating random seeds...).
 */
bool SCULPT_stroke_is_first_brush_step(const blender::ed::sculpt_paint::StrokeCache &cache);
/**
 * Returns true on the first brush step of each symmetry pass.
 */
bool SCULPT_stroke_is_first_brush_step_of_symmetry_pass(
    const blender::ed::sculpt_paint::StrokeCache &cache);

/**
 * Align the grab delta to the brush normal.
 *
 * \param grab_delta: Typically from `ss.cache->grab_delta_symmetry`.
 */
void sculpt_project_v3_normal_align(const SculptSession &ss,
                                    float normal_weight,
                                    float grab_delta[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt mesh accessor API
 * \{ */

/** Ensure random access; required for blender::bke::pbvh::Type::BMesh */
void SCULPT_vertex_random_access_ensure(SculptSession &ss);

int SCULPT_vertex_count_get(const SculptSession &ss);
const float *SCULPT_vertex_co_get(const SculptSession &ss, PBVHVertRef vertex);

/** Get the normal for a given sculpt vertex; do not modify the result */
const blender::float3 SCULPT_vertex_normal_get(const SculptSession &ss, PBVHVertRef vertex);

bool SCULPT_vertex_is_occluded(SculptSession &ss, PBVHVertRef vertex, bool original);

namespace blender::ed::sculpt_paint {

/**
 * Coordinates used for manipulating the base mesh when Grab Active Vertex is enabled.
 */
Span<float3> vert_positions_for_grab_active_get(const Object &object);

}

void SCULPT_vertex_neighbors_get(const SculptSession &ss,
                                 PBVHVertRef vertex,
                                 bool include_duplicates,
                                 SculptVertexNeighborIter *iter);

/** Iterator over neighboring vertices. */
#define SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN(ss, v_index, neighbor_iterator) \
  SCULPT_vertex_neighbors_get(ss, v_index, false, &neighbor_iterator); \
  for (neighbor_iterator.i = 0; neighbor_iterator.i < neighbor_iterator.neighbors.size(); \
       neighbor_iterator.i++) \
  { \
    neighbor_iterator.vertex = neighbor_iterator.neighbors[neighbor_iterator.i]; \
    neighbor_iterator.index = neighbor_iterator.neighbor_indices[neighbor_iterator.i];

/**
 * Iterate over neighboring and duplicate vertices (for blender::bke::pbvh::Type::Grids).
 * Duplicates come first since they are nearest for flood-fill.
 */
#define SCULPT_VERTEX_DUPLICATES_AND_NEIGHBORS_ITER_BEGIN(ss, v_index, neighbor_iterator) \
  SCULPT_vertex_neighbors_get(ss, v_index, true, &neighbor_iterator); \
  for (neighbor_iterator.i = neighbor_iterator.neighbors.size() - 1; neighbor_iterator.i >= 0; \
       neighbor_iterator.i--) \
  { \
    neighbor_iterator.vertex = neighbor_iterator.neighbors[neighbor_iterator.i]; \
    neighbor_iterator.index = neighbor_iterator.neighbor_indices[neighbor_iterator.i]; \
    neighbor_iterator.is_duplicate = (neighbor_iterator.i >= \
                                      neighbor_iterator.neighbors.size() - \
                                          neighbor_iterator.num_duplicates);

#define SCULPT_VERTEX_NEIGHBORS_ITER_END(neighbor_iterator) \
  } \
  ((void)0)

namespace blender::ed::sculpt_paint {

Span<BMVert *> vert_neighbors_get_bmesh(BMVert &vert, Vector<BMVert *, 64> &neighbors);
Span<BMVert *> vert_neighbors_get_interior_bmesh(BMVert &vert, Vector<BMVert *, 64> &neighbors);

Span<int> vert_neighbors_get_mesh(int vert,
                                  OffsetIndices<int> faces,
                                  Span<int> corner_verts,
                                  GroupedSpan<int> vert_to_face,
                                  Span<bool> hide_poly,
                                  Vector<int> &r_neighbors);
}

/* Fake Neighbors */

#define FAKE_NEIGHBOR_NONE -1

void SCULPT_fake_neighbors_ensure(Object &ob, float max_dist);
void SCULPT_fake_neighbors_enable(Object &ob);
void SCULPT_fake_neighbors_disable(Object &ob);
void SCULPT_fake_neighbors_free(Object &ob);

namespace blender::ed::sculpt_paint {

namespace boundary {

/**
 * Populates boundary information for a mesh.
 *
 * \see SculptVertexInfo
 */
void ensure_boundary_info(Object &object);

/**
 * Determine if a vertex is a boundary vertex.
 *
 * Requires #ensure_boundary_info to have been called.
 */
bool vert_is_boundary(const SculptSession &ss, PBVHVertRef vertex);
bool vert_is_boundary(Span<bool> hide_poly,
                      GroupedSpan<int> vert_to_face_map,
                      BitSpan boundary,
                      int vert);
bool vert_is_boundary(const SubdivCCG &subdiv_ccg,
                      Span<bool> hide_poly,
                      Span<int> corner_verts,
                      OffsetIndices<int> faces,
                      BitSpan boundary,
                      SubdivCCGCoord vert);
bool vert_is_boundary(BMVert *vert);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Visibility API
 * \{ */

namespace hide {

Span<int> node_visible_verts(const bke::pbvh::Node &node,
                             Span<bool> hide_vert,
                             Vector<int> &indices);

bool vert_visible_get(const SculptSession &ss, PBVHVertRef vertex);

/* Determines if all faces attached to a given vertex are visible. */
bool vert_all_faces_visible_get(const SculptSession &ss, PBVHVertRef vertex);
bool vert_all_faces_visible_get(Span<bool> hide_poly, GroupedSpan<int> vert_to_face_map, int vert);
bool vert_all_faces_visible_get(Span<bool> hide_poly,
                                const SubdivCCG &subdiv_ccg,
                                SubdivCCGCoord vert);
bool vert_all_faces_visible_get(BMVert *vert);

bool vert_any_face_visible_get(const SculptSession &ss, PBVHVertRef vertex);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Sets API
 * \{ */

namespace face_set {

int active_face_set_get(const SculptSession &ss);
int vert_face_set_get(const SculptSession &ss, PBVHVertRef vertex);

bool vert_has_face_set(const SculptSession &ss, PBVHVertRef vertex, int face_set);
bool vert_has_face_set(GroupedSpan<int> vert_to_face_map,
                       const int *face_sets,
                       int vert,
                       int face_set);
bool vert_has_face_set(const SubdivCCG &subdiv_ccg, const int *face_sets, int grid, int face_set);
bool vert_has_face_set(int face_set_offset, const BMVert &vert, int face_set);
bool vert_has_unique_face_set(const SculptSession &ss, PBVHVertRef vertex);
bool vert_has_unique_face_set(GroupedSpan<int> vert_to_face_map, const int *face_sets, int vert);
bool vert_has_unique_face_set(GroupedSpan<int> vert_to_face_map,
                              Span<int> corner_verts,
                              OffsetIndices<int> faces,
                              const int *face_sets,
                              const SubdivCCG &subdiv_ccg,
                              SubdivCCGCoord coord);
bool vert_has_unique_face_set(const BMVert *vert);

/**
 * Creates the sculpt face set attribute on the mesh if it doesn't exist.
 *
 * \see face_set::ensure_face_sets_mesh if further writing to the attribute is desired.
 */
bool create_face_sets_mesh(Object &object);

/**
 * Ensures that the sculpt face set attribute exists on the mesh.
 *
 * \see face_set::create_face_sets_mesh to avoid having to remember to call .finish()
 */
bke::SpanAttributeWriter<int> ensure_face_sets_mesh(Object &object);
int ensure_face_sets_bmesh(Object &object);
Array<int> duplicate_face_sets(const Mesh &mesh);
Set<int> gather_hidden_face_sets(Span<bool> hide_poly, Span<int> face_sets);

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
SculptOrigVertData SCULPT_orig_vert_data_init(const Object &ob,
                                              const blender::bke::pbvh::Node &node,
                                              blender::ed::sculpt_paint::undo::Type type);
/**
 * Update a #SculptOrigVertData for a particular vertex from the blender::bke::pbvh::Tree iterator.
 */
void SCULPT_orig_vert_data_update(SculptOrigVertData &orig_data, const BMVert &vert);
void SCULPT_orig_vert_data_update(SculptOrigVertData &orig_data, int i);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Brush Utilities.
 * \{ */

bool SCULPT_tool_needs_all_pbvh_nodes(const Brush &brush);

namespace blender::ed::sculpt_paint {

void calc_brush_plane(const Brush &brush,
                      Object &ob,
                      Span<bke::pbvh::Node *> nodes,
                      float3 &r_area_no,
                      float3 &r_area_co);

std::optional<float3> calc_area_normal(const Brush &brush,
                                       Object &ob,
                                       Span<bke::pbvh::Node *> nodes);

/**
 * This calculates flatten center and area normal together,
 * amortizing the memory bandwidth and loop overhead to calculate both at the same time.
 */
void calc_area_normal_and_center(const Brush &brush,
                                 const Object &ob,
                                 Span<bke::pbvh::Node *> nodes,
                                 float r_area_no[3],
                                 float r_area_co[3]);
void calc_area_center(const Brush &brush,
                      const Object &ob,
                      Span<bke::pbvh::Node *> nodes,
                      float r_area_co[3]);

PBVHVertRef nearest_vert_calc(const Object &object,
                              const float3 &location,
                              float max_distance,
                              bool use_original);
std::optional<int> nearest_vert_calc_mesh(const bke::pbvh::Tree &pbvh,
                                          Span<float3> vert_positions,
                                          Span<bool> hide_vert,
                                          const float3 &location,
                                          float max_distance,
                                          bool use_original);
std::optional<SubdivCCGCoord> nearest_vert_calc_grids(const bke::pbvh::Tree &pbvh,
                                                      const SubdivCCG &subdiv_ccg,
                                                      const float3 &location,
                                                      float max_distance,
                                                      bool use_original);
std::optional<BMVert *> nearest_vert_calc_bmesh(const bke::pbvh::Tree &pbvh,
                                                const float3 &location,
                                                float max_distance,
                                                bool use_original);
}

float SCULPT_brush_plane_offset_get(const Sculpt &sd, const SculptSession &ss);

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
blender::float3 SCULPT_flip_v3_by_symm_area(const blender::float3 &vector,
                                            ePaintSymmetryFlags symm,
                                            ePaintSymmetryAreas symmarea,
                                            const blender::float3 &pivot);
void SCULPT_flip_quat_by_symm_area(float quat[4],
                                   ePaintSymmetryFlags symm,
                                   ePaintSymmetryAreas symmarea,
                                   const float pivot[3]);

namespace blender::ed::sculpt_paint {

bool node_fully_masked_or_hidden(const bke::pbvh::Node &node);
bool node_in_sphere(const bke::pbvh::Node &node,
                    const float3 &location,
                    float radius_sq,
                    bool original);
bool node_in_cylinder(const DistRayAABB_Precalc &dist_ray_precalc,
                      const bke::pbvh::Node &node,
                      float radius_sq,
                      bool original);

}

const float *SCULPT_brush_frontface_normal_from_falloff_shape(const SculptSession &ss,
                                                              char falloff_shape);
void SCULPT_cube_tip_init(const Sculpt &sd, const Object &ob, const Brush &brush, float mat[4][4]);

/** Sample the brush's texture value. */
void sculpt_apply_texture(const SculptSession &ss,
                          const Brush &brush,
                          const float brush_point[3],
                          int thread_id,
                          float *r_value,
                          float r_rgba[4]);

/**
 * Calculates the vertex offset for a single vertex depending on the brush setting rgb as vector
 * displacement.
 */
void SCULPT_calc_vertex_displacement(const SculptSession &ss,
                                     const Brush &brush,
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
void SCULPT_tilt_effective_normal_get(const SculptSession &ss, const Brush &brush, float r_no[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flood Fill
 * \{ */

namespace blender::ed::sculpt_paint::flood_fill {

struct FillData {
  std::queue<PBVHVertRef> queue;
  BitVector<> visited_verts;
};

struct FillDataMesh {
  FillDataMesh(int size) : visited_verts(size) {}

  std::queue<int> queue;
  BitVector<> visited_verts;

  void add_initial(int vertex);
  void add_and_skip_initial(int vertex, int index);
  void add_initial_with_symmetry(const Object &object,
                                 const bke::pbvh::Tree &pbvh,
                                 int vertex,
                                 float radius);
  void execute(Object &object,
               GroupedSpan<int> vert_to_face_map,
               FunctionRef<bool(int from_v, int to_v)> func);
};

struct FillDataGrids {
  FillDataGrids(int size) : visited_verts(size) {}

  std::queue<SubdivCCGCoord> queue;
  BitVector<> visited_verts;

  void add_initial(SubdivCCGCoord vertex);
  void add_and_skip_initial(SubdivCCGCoord vertex, int index);
  void add_initial_with_symmetry(const Object &object,
                                 const bke::pbvh::Tree &pbvh,
                                 const SubdivCCG &subdiv_ccg,
                                 SubdivCCGCoord vertex,
                                 float radius);
  void execute(
      Object &object,
      const SubdivCCG &subdiv_ccg,
      FunctionRef<bool(SubdivCCGCoord from_v, SubdivCCGCoord to_v, bool is_duplicate)> func);
};

struct FillDataBMesh {
  FillDataBMesh(int size) : visited_verts(size) {}

  std::queue<BMVert *> queue;
  BitVector<> visited_verts;

  void add_initial(BMVert *vertex);
  void add_and_skip_initial(BMVert *vertex, int index);
  void add_initial_with_symmetry(const Object &object,
                                 const bke::pbvh::Tree &pbvh,
                                 BMVert *vertex,
                                 float radius);
  void execute(Object &object, FunctionRef<bool(BMVert *from_v, BMVert *to_v)> func);
};

/**
 * \deprecated See the individual FillData constructors instead of this method.
 */
FillData init_fill(SculptSession &ss);

void add_initial(FillData &flood, PBVHVertRef vertex);
void add_and_skip_initial(FillData &flood, PBVHVertRef vertex);
void add_initial_with_symmetry(
    const Object &ob, const SculptSession &ss, FillData &flood, PBVHVertRef vertex, float radius);
void execute(SculptSession &ss,
             FillData &flood,
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
void enable_ex(Main &bmain, Depsgraph &depsgraph, Object &ob);
void disable(bContext *C, undo::StepData *undo_step);
void disable_with_undo(Main &bmain, Depsgraph &depsgraph, Scene &scene, Object &ob);

/**
 * Returns true if the stroke will use dynamic topology, false
 * otherwise.
 *
 * Factors: some brushes like grab cannot do dynamic topology.
 * Others, like smooth, are better without.
 * Same goes for alt-key smoothing.
 */
bool stroke_is_dyntopo(const SculptSession &ss, const Brush &brush);

void triangulate(BMesh *bm);

WarnFlag check_attribute_warning(Scene &scene, Object &ob);

namespace detail_size {

/**
 * Scaling factor to match the displayed size to the actual sculpted size
 */
constexpr float RELATIVE_SCALE_FACTOR = 0.4f;

/**
 * Converts from Sculpt#constant_detail to the pbvh::Tree max edge length.
 */
float constant_to_detail_size(float constant_detail, const Object &ob);

/**
 * Converts from Sculpt#detail_percent to the pbvh::Tree max edge length.
 */
float brush_to_detail_size(float brush_percent, float brush_radius);

/**
 * Converts from Sculpt#detail_size to the pbvh::Tree max edge length.
 */
float relative_to_detail_size(float relative_detail,
                              float brush_radius,
                              float pixel_radius,
                              float pixel_size);

/**
 * Converts from Sculpt#constant_detail to equivalent Sculpt#detail_percent value.
 *
 * Corresponds to a change from Constant & Manual Detailing to Brush Detailing.
 */
float constant_to_brush_detail(float constant_detail, float brush_radius, const Object &ob);

/**
 * Converts from Sculpt#constant_detail to equivalent Sculpt#detail_size value.
 *
 * Corresponds to a change from Constant & Manual Detailing to Relative Detailing.
 */
float constant_to_relative_detail(float constant_detail,
                                  float brush_radius,
                                  float pixel_radius,
                                  float pixel_size,
                                  const Object &ob);
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
  std::optional<SculptOrigVertData> orig_data;
};

/**
 * Call before pbvh::Tree vertex iteration.
 */
NodeData node_begin(const Object &object, const Cache *automasking, const bke::pbvh::Node &node);

/* Call before factor_get. */
void node_update(NodeData &automask_data, const BMVert &vert);
/**
 * Call before factor_get. The index is in the range of the pbvh::Tree node's vertex indices.
 */
void node_update(NodeData &automask_data, int i);

float factor_get(const Cache *automasking,
                 SculptSession &ss,
                 PBVHVertRef vertex,
                 const NodeData *automask_data);

/* Returns the automasking cache depending on the active tool. Used for code that can run both for
 * brushes and filter. */
const Cache *active_cache_get(const SculptSession &ss);

/**
 * Creates and initializes an automasking cache.
 *
 * For automasking modes that cannot be calculated in real time,
 * data is also stored at the vertex level prior to the stroke starting.
 */
std::unique_ptr<Cache> cache_init(const Sculpt &sd, Object &ob);
std::unique_ptr<Cache> cache_init(const Sculpt &sd, const Brush *brush, Object &ob);

bool mode_enabled(const Sculpt &sd, const Brush *br, eAutomasking_flag mode);
bool is_enabled(const Sculpt &sd, const SculptSession *ss, const Brush *br);

bool needs_normal(const SculptSession &ss, const Sculpt &sd, const Brush *brush);
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
 * Geodesic distances will only work when used with blender::bke::pbvh::Type::Mesh, for other
 * types of blender::bke::pbvh::Tree it will fallback to euclidean distances to one of the initial
 * vertices in the set.
 */
Array<float> distances_create(Object &ob, const Set<int> &initial_verts, float limit_radius);
Array<float> distances_create_from_vert_and_symm(Object &ob,
                                                 PBVHVertRef vertex,
                                                 float limit_radius);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Filter API
 * \{ */

namespace blender::ed::sculpt_paint::filter {

void cache_init(bContext *C,
                Object &ob,
                const Sculpt &sd,
                undo::Type undo_type,
                const float mval_fl[2],
                float area_normal_radius,
                float start_strength);
void register_operator_props(wmOperatorType *ot);

/* Filter orientation utils. */
float3x3 to_orientation_space(const filter::Cache &filter_cache);
float3x3 to_object_space(const filter::Cache &filter_cache);
void zero_disabled_axis_components(const filter::Cache &filter_cache, MutableSpan<float3> vectors);

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
  Vector<LengthConstraint> length_constraints;
  Array<float> length_constraint_tweak;

  /* Position anchors for deformation brushes. These positions are modified by the brush and the
   * final positions of the simulated vertices are updated with constraints that use these points
   * as targets. */
  Array<float3> deformation_pos;
  Array<float> deformation_strength;

  float mass;
  float damping;
  float softbody_strength;

  Array<float3> acceleration;
  Array<float3> pos;
  Array<float3> init_pos;
  Array<float3> init_no;
  Array<float3> softbody_pos;
  Array<float3> prev_pos;
  Array<float3> last_iteration_pos;

  Vector<ColliderCache> collider_list;

  int totnode;
  Map<const bke::pbvh::Node *, int> node_state_index;
  Array<NodeSimState> node_state;

  ~SimulationData();
};

/* Main cloth brush function */
void do_cloth_brush(const Sculpt &sd, Object &ob, Span<blender::bke::pbvh::Node *> nodes);

/* Public functions. */

std::unique_ptr<SimulationData> brush_simulation_create(Object &ob,
                                                        float cloth_mass,
                                                        float cloth_damping,
                                                        float cloth_softbody_strength,
                                                        bool use_collisions,
                                                        bool needs_deform_coords);

void sim_activate_nodes(SimulationData &cloth_sim, Span<blender::bke::pbvh::Node *> nodes);

void brush_store_simulation_state(const SculptSession &ss, SimulationData &cloth_sim);

void do_simulation_step(const Sculpt &sd,
                        Object &ob,
                        SimulationData &cloth_sim,
                        Span<blender::bke::pbvh::Node *> nodes);

void ensure_nodes_constraints(const Sculpt &sd,
                              const Object &ob,
                              Span<bke::pbvh::Node *> nodes,
                              SimulationData &cloth_sim,
                              const float3 &initial_location,
                              float radius);

/**
 * Cursor drawing function.
 */
void simulation_limits_draw(uint gpuattr,
                            const Brush &brush,
                            const float location[3],
                            const float normal[3],
                            float rds,
                            float line_width,
                            const float outline_col[3],
                            float alpha);
void plane_falloff_preview_draw(uint gpuattr,
                                SculptSession &ss,
                                const float outline_col[3],
                                float outline_alpha);

Vector<blender::bke::pbvh::Node *> brush_affected_nodes_gather(SculptSession &ss,
                                                               const Brush &brush);

bool is_cloth_deform_brush(const Brush &brush);

}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Smoothing API
 * \{ */

namespace blender::ed::sculpt_paint {

void calc_smooth_translations(const Object &object,
                              Span<bke::pbvh::Node *> nodes,
                              MutableSpan<float3> translations);

}

namespace blender::ed::sculpt_paint::smooth {

/**
 * For bmesh: Average surrounding verts based on an orthogonality measure.
 * Naturally converges to a quad-like structure.
 */
void bmesh_four_neighbor_average(float avg[3], const float3 &direction, const BMVert *v);

void neighbor_color_average(OffsetIndices<int> faces,
                            Span<int> corner_verts,
                            GroupedSpan<int> vert_to_face_map,
                            GSpan color_attribute,
                            bke::AttrDomain color_domain,
                            Span<Vector<int>> vert_neighbors,
                            MutableSpan<float4> smooth_colors);

void neighbor_position_average_grids(const SubdivCCG &subdiv_ccg,
                                     Span<int> grids,
                                     MutableSpan<float3> new_positions);
void neighbor_position_average_interior_grids(OffsetIndices<int> faces,
                                              Span<int> corner_verts,
                                              BitSpan boundary_verts,
                                              const SubdivCCG &subdiv_ccg,
                                              Span<int> grids,
                                              MutableSpan<float3> new_positions);

void neighbor_position_average_bmesh(const Set<BMVert *, 0> &verts,
                                     MutableSpan<float3> new_positions);
void neighbor_position_average_interior_bmesh(const Set<BMVert *, 0> &verts,
                                              MutableSpan<float3> new_positions);

template<typename T>
void neighbor_data_average_mesh(Span<T> src, Span<Vector<int>> vert_neighbors, MutableSpan<T> dst);
template<typename T>
void neighbor_data_average_mesh_check_loose(Span<T> src,
                                            Span<int> verts,
                                            Span<Vector<int>> vert_neighbors,
                                            MutableSpan<T> dst);

template<typename T>
void average_data_grids(const SubdivCCG &subdiv_ccg,
                        Span<T> src,
                        Span<int> grids,
                        MutableSpan<T> dst);

template<typename T>
void average_data_bmesh(Span<T> src, const Set<BMVert *, 0> &verts, MutableSpan<T> dst);

/* Surface Smooth Brush. */

void surface_smooth_laplacian_step(Span<float3> positions,
                                   Span<float3> orig_positions,
                                   Span<float3> average_positions,
                                   float alpha,
                                   MutableSpan<float3> laplacian_disp,
                                   MutableSpan<float3> translations);
void surface_smooth_displace_step(Span<float3> laplacian_disp,
                                  Span<float3> average_laplacian_disp,
                                  float beta,
                                  MutableSpan<float3> translations);

/* Slide/Relax */
void relax_vertex(SculptSession &ss,
                  PBVHVertRef vert,
                  float factor,
                  bool filter_boundary_face_sets,
                  float *r_final_pos);

}

/** \} */

/**
 * Flip all the edit-data across the axis/axes specified by \a symm.
 * Used to calculate multiple modifications to the mesh when symmetry is enabled.
 */
void SCULPT_cache_calc_brushdata_symm(blender::ed::sculpt_paint::StrokeCache &cache,
                                      ePaintSymmetryFlags symm,
                                      char axis,
                                      float angle);

/* -------------------------------------------------------------------- */
/** \name Sculpt Undo
 * \{ */

namespace blender::ed::sculpt_paint::undo {

/**
 * Store undo data of the given type for a pbvh::Tree node. This function can be called by multiple
 * threads concurrently, as long as they don't pass the same pbvh::Tree node.
 *
 * This is only possible when building an undo step, in between #push_begin and #push_end.
 */
void push_node(const Object &object, const bke::pbvh::Node *node, undo::Type type);
void push_nodes(Object &object, Span<const bke::pbvh::Node *> nodes, undo::Type type);

/**
 * Retrieve the undo data of a given type for the active undo step. For example, this is used to
 * access "original" data from before the current stroke.
 *
 * This is only possible when building an undo step, in between #push_begin and #push_end.
 */
const undo::Node *get_node(const bke::pbvh::Node *node, undo::Type type);

/**
 * Pushes an undo step using the operator name. This is necessary for
 * redo panels to work; operators that do not support that may use
 * #push_begin_ex instead if so desired.
 */
void push_begin(Object &ob, const wmOperator *op);

/**
 * NOTE: #push_begin is preferred since `name`
 * must match operator name for redo panels to work.
 */
void push_begin_ex(Object &ob, const char *name);
void push_end(Object &ob);
void push_end_ex(Object &ob, bool use_nested_undo);

void restore_from_bmesh_enter_geometry(const StepData &step_data, Mesh &mesh);
BMLogEntry *get_bmesh_log_entry();

void restore_position_from_undo_step(Object &object);

}

namespace blender::ed::sculpt_paint {

struct OrigPositionData {
  Span<float3> positions;
  Span<float3> normals;
};
/**
 * Retrieve positions from the latest undo state. This is often used for modal actions that depend
 * on the initial state of the geometry from before the start of the action.
 */
OrigPositionData orig_position_data_get_mesh(const Object &object, const bke::pbvh::Node &node);
OrigPositionData orig_position_data_get_grids(const Object &object, const bke::pbvh::Node &node);
void orig_position_data_gather_bmesh(const BMLog &bm_log,
                                     const Set<BMVert *, 0> &verts,
                                     MutableSpan<float3> positions,
                                     MutableSpan<float3> normals);

Span<float4> orig_color_data_get_mesh(const Object &object, const bke::pbvh::Node &node);

}

/** \} */

void SCULPT_vertcos_to_key(Object &ob, KeyBlock *kb, blender::Span<blender::float3> vertCos);

/**
 * Get a screen-space rectangle of the modified area.
 */
bool SCULPT_get_redraw_rect(const ARegion &region,
                            const RegionView3D &rv3d,
                            const Object &ob,
                            rcti &rect);

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

  /* In the context of a sculpt gesture, both lasso and polyline modal
   * operators are handled as the same general shape. */
  Lasso = 1,
  Line = 2,
};

enum class SelectionType {
  Inside = 0,
  Outside = 1,
};

/* Common data structure for both lasso and polyline. */
struct LassoData {
  float4x4 projviewobjmat;

  rcti boundbox;
  int width;

  /* 2D bitmap to test if a vertex is affected by the surrounding shape. */
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

  /* Lasso & Polyline Gesture. */
  LassoData lasso;

  /* Line Gesture. */
  LineData line;

  /* Task Callback Data. */
  Vector<bke::pbvh::Node *> nodes;

  ~GestureData();
};

/* Common abstraction structure for gesture operations. */
struct Operation {
  /* Initial setup (data updates, special undo push...). */
  void (*begin)(bContext &, wmOperator &, GestureData &);

  /* Apply the gesture action for each symmetry pass. */
  void (*apply_for_symmetry_pass)(bContext &, GestureData &);

  /* Remaining actions after finishing the symmetry passes iterations
   * (updating data-layers, tagging bke::pbvh::Tree updates...). */
  void (*end)(bContext &, GestureData &);
};

/* Determines whether or not a gesture action should be applied. */
bool is_affected(const GestureData &gesture_data, const float3 &position, const float3 &normal);
void filter_factors(const GestureData &gesture_data,
                    Span<float3> positions,
                    Span<float3> normals,
                    MutableSpan<float> factors);

/* Initialization functions. */
std::unique_ptr<GestureData> init_from_box(bContext *C, wmOperator *op);
std::unique_ptr<GestureData> init_from_lasso(bContext *C, wmOperator *op);
std::unique_ptr<GestureData> init_from_polyline(bContext *C, wmOperator *op);
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
void SCULPT_OT_trim_line_gesture(wmOperatorType *ot);
void SCULPT_OT_trim_polyline_gesture(wmOperatorType *ot);
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
void SCULPT_OT_face_set_line_gesture(wmOperatorType *ot);
void SCULPT_OT_face_set_polyline_gesture(wmOperatorType *ot);

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
void do_pose_brush(const Sculpt &sd, Object &ob, Span<bke::pbvh::Node *> nodes);
/**
 * Calculate the pose origin and (Optionally the pose factor)
 * that is used when using the pose brush.
 *
 * \param r_pose_origin: Must be a valid pointer.
 * \param r_pose_factor: Optional, when set to NULL it won't be calculated.
 */
void calc_pose_data(Object &ob,
                    SculptSession &ss,
                    const float3 &initial_location,
                    float radius,
                    float pose_offset,
                    float3 &r_pose_origin,
                    MutableSpan<float> r_pose_factor);
void pose_brush_init(Object &ob, SculptSession &ss, const Brush &brush);
std::unique_ptr<SculptPoseIKChainPreview> preview_ik_chain_init(Object &ob,
                                                                SculptSession &ss,
                                                                const Brush &brush,
                                                                const float3 &initial_location,
                                                                float radius);

}

namespace blender::ed::sculpt_paint::boundary {

/**
 * Main function to get #SculptBoundary data both for brush deformation and viewport preview.
 * Can return NULL if there is no boundary from the given vertex using the given radius.
 */
std::unique_ptr<SculptBoundary> data_init(Object &object,
                                          const Brush *brush,
                                          PBVHVertRef initial_vert,
                                          float radius);
std::unique_ptr<SculptBoundary> data_init_mesh(Object &object,
                                               const Brush *brush,
                                               int initial_vert,
                                               float radius);
std::unique_ptr<SculptBoundary> data_init_grids(Object &object,
                                                const Brush *brush,
                                                SubdivCCGCoord initial_vert,
                                                float radius);
std::unique_ptr<SculptBoundary> data_init_bmesh(Object &object,
                                                const Brush *brush,
                                                BMVert *initial_vert,
                                                float radius);
std::unique_ptr<SculptBoundaryPreview> preview_data_init(Object &object,
                                                         const Brush *brush,
                                                         PBVHVertRef initial_vertex,
                                                         float radius);

/* Main Brush Function. */
void do_boundary_brush(const Sculpt &sd, Object &ob, Span<bke::pbvh::Node *> nodes);

void edges_preview_draw(uint gpuattr,
                        SculptSession &ss,
                        const float outline_col[3],
                        float outline_alpha);
void pivot_line_preview_draw(uint gpuattr, SculptSession &ss);

}

namespace blender::ed::sculpt_paint {

void multiplane_scrape_preview_draw(uint gpuattr,
                                    const Brush &brush,
                                    const SculptSession &ss,
                                    const float outline_col[3],
                                    float outline_alpha);

namespace color {

/* Swaps colors at each element in indices with values in colors. */
void swap_gathered_colors(Span<int> indices,
                          GMutableSpan color_attribute,
                          MutableSpan<float4> r_colors);

/* Stores colors from the elements in indices into colors. */
void gather_colors(GSpan color_attribute, Span<int> indices, MutableSpan<float4> r_colors);

/* Like gather_colors but handles loop->vert conversion */
void gather_colors_vert(OffsetIndices<int> faces,
                        Span<int> corner_verts,
                        GroupedSpan<int> vert_to_face_map,
                        GSpan color_attribute,
                        bke::AttrDomain color_domain,
                        Span<int> verts,
                        MutableSpan<float4> r_colors);

void color_vert_set(OffsetIndices<int> faces,
                    Span<int> corner_verts,
                    GroupedSpan<int> vert_to_face_map,
                    bke::AttrDomain color_domain,
                    int vert,
                    const float4 &color,
                    GMutableSpan color_attribute);
float4 color_vert_get(OffsetIndices<int> faces,
                      Span<int> corner_verts,
                      GroupedSpan<int> vert_to_face_map,
                      GSpan color_attribute,
                      bke::AttrDomain color_domain,
                      int vert);

bke::GAttributeReader active_color_attribute(const Mesh &mesh);
bke::GSpanAttributeWriter active_color_attribute_for_write(Mesh &mesh);

void do_paint_brush(PaintModeSettings &paint_mode_settings,
                    const Sculpt &sd,
                    Object &ob,
                    Span<bke::pbvh::Node *> nodes,
                    Span<bke::pbvh::Node *> texnodes);
void do_smear_brush(const Sculpt &sd, Object &ob, Span<bke::pbvh::Node *> nodes);
}

}
/**
 * \brief Get the image canvas for painting on the given object.
 *
 * \return #true if an image is found. The #r_image and #r_image_user fields are filled with
 * the image and image user. Returns false when the image isn't found. In the later case the
 * r_image and r_image_user are set to NULL.
 */
bool SCULPT_paint_image_canvas_get(PaintModeSettings &paint_mode_settings,
                                   Object &ob,
                                   Image **r_image,
                                   ImageUser **r_image_user) ATTR_NONNULL();
void SCULPT_do_paint_brush_image(PaintModeSettings &paint_mode_settings,
                                 const Sculpt &sd,
                                 Object &ob,
                                 blender::Span<blender::bke::pbvh::Node *> texnodes);
bool SCULPT_use_image_paint_brush(PaintModeSettings &settings, Object &ob);

namespace blender::ed::sculpt_paint {

float clay_thumb_get_stabilized_pressure(const blender::ed::sculpt_paint::StrokeCache &cache);

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

void SCULPT_stroke_id_ensure(Object &ob);
void SCULPT_stroke_id_next(Object &ob);

namespace blender::ed::sculpt_paint {
void ensure_valid_pivot(const Object &ob, Scene &scene);
}

/* -------------------------------------------------------------------- */
/** \name Topology island API
 * \{
 * Each mesh island shell gets its own integer
 * key; these are temporary and internally limited to 8 bits.
 */

namespace blender::ed::sculpt_paint::islands {

/* Ensure vertex island keys exist and are valid. */
void ensure_cache(Object &object);

/** Mark vertex island keys as invalid. Call when adding or hiding geometry. */
void invalidate(SculptSession &ss);

/** Get vertex island key. */
int vert_id_get(const SculptSession &ss, int vert);

}

/** \} */

namespace blender::ed::sculpt_paint {
float sculpt_calc_radius(const ViewContext &vc,
                         const Brush &brush,
                         const Scene &scene,
                         float3 location);
}

inline void *SCULPT_vertex_attr_get(const PBVHVertRef vert, const SculptAttribute *attr)
{
  if (attr->data) {
    char *p = (char *)attr->data;
    int idx = (int)vert.i;

    if (attr->data_for_bmesh) {
      BMElem *v = (BMElem *)vert.i;
      idx = v->head.index;
    }

    return p + attr->elem_size * idx;
  }

  BMElem *v = (BMElem *)vert.i;
  return BM_ELEM_CD_GET_VOID_P(v, attr->bmesh_cd_offset);
}
inline void *SCULPT_vertex_attr_get(const int vert, const SculptAttribute *attr)
{
  if (attr->data) {
    char *p = (char *)attr->data;

    return p + attr->elem_size * vert;
  }

  BLI_assert_unreachable();
  return nullptr;
}

inline void *SCULPT_vertex_attr_get(const CCGKey &key,
                                    const SubdivCCGCoord vert,
                                    const SculptAttribute *attr)
{
  if (attr->data) {
    char *p = (char *)attr->data;
    int idx = vert.to_index(key);

    return p + attr->elem_size * idx;
  }

  BLI_assert_unreachable();
  return nullptr;
}

inline void *SCULPT_vertex_attr_get(const BMVert *vert, const SculptAttribute *attr)
{
  if (attr->data) {
    char *p = (char *)attr->data;
    int idx = BM_elem_index_get(vert);

    if (attr->data_for_bmesh) {
      idx = vert->head.index;
    }

    return p + attr->elem_size * idx;
  }

  return BM_ELEM_CD_GET_VOID_P(vert, attr->bmesh_cd_offset);
}
