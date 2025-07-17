/* SPDX-FileCopyrightText: 2006 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include <optional>

#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"

#include "ED_view3d.hh"

namespace blender::ed::sculpt_paint {
namespace auto_mask {
struct Cache;
}
namespace boundary {
struct SculptBoundary;
}
namespace cloth {
struct SimulationData;
}
namespace pose {
struct IKChain;
}
namespace undo {
struct Node;
enum class Type : int8_t;
}  // namespace undo
}  // namespace blender::ed::sculpt_paint
struct bContext;
struct BMLog;
struct Dial;
struct DistRayAABB_Precalc;
struct Image;
struct ImageUser;
struct Key;
struct KeyBlock;
struct Object;
struct PaintModeSettings;
struct ReportList;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperatorType;

/* -------------------------------------------------------------------- */
/** \name Sculpt Types
 * \{ */

namespace blender::ed::sculpt_paint {

/** Contains shape key array data for quick access for deformation. */
struct ShapeKeyData {
  MutableSpan<float3> active_key_data;
  bool basis_key_active;
  Vector<MutableSpan<float3>> dependent_keys;

  static std::optional<ShapeKeyData> from_object(Object &object);
};

/**
 * This class represents an API to deform original positions based on translations created from
 * evaluated positions. It should be constructed once outside of a parallel context.
 */
class PositionDeformData {
 public:
  /**
   * Positions from after procedural deformation from modifiers, used to build the
   * pbvh::Tree. Translations are built for these values, then applied to the original positions.
   * When there are no deforming modifiers, this will reference the same array as #orig.
   */
  Span<float3> eval;

 private:
  /**
   * In some cases deformations must also apply to the evaluated positions (#eval) in case the
   * changed values are needed elsewhere before the object is reevaluated (which would update the
   * evaluated positions).
   */
  std::optional<MutableSpan<float3>> eval_mut_;

  /**
   * Transforms from deforming modifiers, used to convert translations of evaluated positions to
   * "original" translations.
   */
  std::optional<Span<float3x3>> deform_imats_;

  /**
   * Positions from the original mesh. Not the same as #eval if there are deform modifiers.
   */
  MutableSpan<float3> orig_;

  std::optional<ShapeKeyData> shape_key_data_;

 public:
  PositionDeformData(const Depsgraph &depsgraph, Object &object_orig);
  void deform(MutableSpan<float3> translations, Span<int> verts) const;
};

enum class UpdateType {
  Position,
  Mask,
  Visibility,
  Color,
  Image,
  FaceSet,
};

}  // namespace blender::ed::sculpt_paint

/* Factor of brush to have rake point following behind
 * (could be configurable but this is reasonable default). */
#define SCULPT_RAKE_BRUSH_FACTOR 0.25f

struct SculptRakeData {
  float follow_dist = 0.0f;
  blender::float3 follow_co = blender::float3(0);
  float angle = 0.0f;
};

namespace blender::ed::sculpt_paint {
enum class TransformDisplacementMode {
  /* Displaces the elements from their original coordinates. */
  Original = 0,
  /* Displaces the elements incrementally from their previous position. */
  Incremental = 1,
};
}
/* Defines how transform tools are going to apply its displacement. */

namespace blender::ed::sculpt_paint {

static constexpr int plane_brush_max_rolling_average_num = 20;

/**
 * This structure contains all the temporary data
 * needed for individual brush strokes.
 */
struct StrokeCache {
  /* Invariants */
  float initial_radius = 0.0f;
  float3 scale = float3(0);
  struct {
    uint8_t flag = 0;
    float3 tolerance = float3(0);
    float4x4 mat = float4x4::identity();
    float4x4 mat_inv = float4x4::identity();
  } mirror_modifier_clip;
  float2 initial_mouse = float2(0);

  /**
   * Some brushes change behavior drastically depending on the directional value (i.e. the smooth
   * and enhance details functionality being bound to the Smooth brush).
   *
   * Storing the initial direction allows discerning the behavior without checking the sign of the
   * brush direction at every step, which would have ambiguity at 0.
   */
  bool initial_direction_flipped = false;

  /* Variants */
  float radius = 0.0f;
  float radius_squared = 0.0f;
  float3 location = float3(0);
  float3 last_location = float3(0);
  float3 location_symm = float3(0);
  float3 last_location_symm = float3(0);
  float stroke_distance = 0.0f;

  /**
   * Used for alternating between deformations in brushes that need to apply different ones to
   * achieve certain effects.
   */
  int iteration_count = 0;

  /* Original pixel radius with the pressure curve applied for dyntopo detail size */
  float dyntopo_pixel_radius = 0.0f;

  bool is_last_valid = false;

  bool pen_flip = false;

  /**
   * Whether the modifier key that controls inverting brush behavior is active currently.
   * Generally signals a change in behavior for brushes.
   *
   * \see BrushStrokeMode::BRUSH_STROKE_INVERT.
   */
  bool invert = false;
  float pressure = 0.0f;
  float hardness = 0.0f;
  /**
   * Depending on the mode, can either be the raw brush strength, or a scaled (possibly negative)
   * value.
   *
   * \see #brush_strength for Sculpt Mode.
   */
  float bstrength = 0.0f;
  float normal_weight = 0.0f; /* from brush (with optional override) */
  float2 tilt = float2(0);

  /**
   * Position of the mouse corresponding to the stroke location, modified by the paint_stroke
   * operator according to the stroke type.
   */
  float2 mouse = float2(0);
  /* Position of the mouse event in screen space, not modified by the stroke type. */
  float2 mouse_event = float2(0);

  struct {
    Array<float3> prev_displacement;
    Array<float3> limit_surface_co;
  } displacement_smear;

  /* The rest is temporary storage that isn't saved as a property */

  /* Store initial starting points for perlin noise on the beginning of each stroke when using
   * color jitter. */
  std::optional<blender::float3> initial_hsv_jitter;
  /* Beginning of stroke may do some things special. */
  bool first_time = false;

  /* from ED_view3d_ob_project_mat_get(). */
  float4x4 projection_mat = float4x4::identity();

  /* TODO: Clean this up! */
  ViewContext *vc = nullptr;
  const Brush *brush = nullptr;
  const Paint *paint = nullptr;

  float special_rotation = 0.0f;
  float3 grab_delta = float3(0);
  float3 grab_delta_symm = float3(0);
  float3 old_grab_location = float3(0);
  float3 orig_grab_location = float3(0);

  /* Screen-space rotation defined by mouse motion. */
  std::optional<math::Quaternion> rake_rotation;
  std::optional<math::Quaternion> rake_rotation_symm;
  SculptRakeData rake_data;

  /* The face set being painted. */
  int paint_face_set = SCULPT_FACE_SET_NONE;

  /**
   * Symmetry index between 0 and 7 bit combo.
   *
   * 0 is Brush only; 1 is X mirror; 2 is Y mirror; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ.
   */
  int symmetry = 0;
  /* The symmetry pass we are currently on between 0 and 7. */
  ePaintSymmetryFlags mirror_symmetry_pass = ePaintSymmetryFlags(0);
  float3 view_normal = float3(0);
  float3 view_normal_symm = float3(0);

  /**
   * The primary direction of influence for a brush stroke.
   *
   * May be unused for some brushes (e.g. Smooth)
   * May be only calculated at the beginning of a stroke (e.g. Grab)
   *
   * Calculated by either #calc_sculpt_normal or #calc_brush_plane.
   */
  float3 sculpt_normal = float3(0);
  float3 sculpt_normal_symm = float3(0);

  /**
   * Used for area texture mode, local_mat gets calculated by
   * calc_brush_local_mat() and used in sculpt_apply_texture().
   * Transforms from model-space coords to local area coords.
   */
  float4x4 brush_local_mat = float4x4::identity();
  /**
   * The matrix from local area coords to model-space coords is used to calculate the vector
   * displacement in area plane mode.
   */
  float4x4 brush_local_mat_inv = float4x4::identity();

  /* used to shift the plane around when doing tiled strokes */
  float3 plane_offset = float3(0);
  int tile_pass = 0;

  float3 last_center = float3(0);
  int radial_symmetry_pass = 0;
  float4x4 symm_rot_mat = float4x4::identity();
  float4x4 symm_rot_mat_inv = float4x4::identity();

  /**
   * Accumulate mode.
   * \note inverted for #SCULPT_BRUSH_TYPE_DRAW_SHARP.
   */
  bool accum = false;

  /* Paint Brush. */
  struct {
    float flow = 0.0f;

    float4 wet_mix_prev_color = float4(0);
    float wet_mix = 0.0f;
    float wet_persistence = 0.0f;

    std::optional<float> density_seed;
    float density = 0.0f;

    /**
     * Used by the color attribute paint brush tool to store the brush color during a stroke and
     * composite it over the original color.
     */
    Array<float4> mix_colors;
    Array<float4> prev_colors;
  } paint_brush;

  /* Pose brush */
  std::unique_ptr<pose::IKChain> pose_ik_chain;

  /* Enhance Details. */
  Array<float3> detail_directions;

  /* Clay Thumb brush */
  struct {
    /* Angle of the front tilting plane of the brush to simulate clay accumulation. */
    float front_angle = 0.0f;
    /* Stores the last 10 pressure samples to get a stabilized strength and radius variation. */
    std::array<float, 10> pressure_stabilizer;
    int stabilizer_index = 0;

  } clay_thumb_brush;

  /* Plane Brush */
  struct {
    std::optional<float3> last_normal;
    std::optional<float3> last_center;
    Array<float3> normals;
    Array<float3> centers;
    int normal_index = 0;
    int center_index = 0;

    /**
     * True if the current step is the first time the Plane brush is being evaluated.
     *
     * We cannot use the generic `first_time` variable used by other brushes because
     * the Plane brush uses `grab_delta` to compute its local matrix. Since `grab_delta` requires
     * at least two stroke steps, the first step (and successive steps if the user does not move
     * the cursor) of the Plane brush is always skipped.
     */
    bool first_time = false;
  } plane_brush;

  /* Cloth brush */
  std::unique_ptr<cloth::SimulationData> cloth_sim;
  float3 initial_location_symm = float3(0);
  float3 initial_location = float3(0);
  float3 initial_normal_symm = float3(0);
  float3 initial_normal = float3(0);

  /* Boundary brush */
  std::array<std::unique_ptr<boundary::SculptBoundary>, PAINT_SYMM_AREAS> boundaries;

  /* Surface Smooth Brush */
  /* Stores the displacement produced by the laplacian step of HC smooth. */
  Array<float3> surface_smooth_laplacian_disp;

  /* Layer brush */
  Array<float> layer_displacement_factor;

  /* Amount to rotate the vertices when using rotate brush. */
  float vertex_rotation = 0.0f;
  Dial *dial = nullptr;

  Brush *saved_active_brush = nullptr;
  char saved_mask_brush_tool = 0;
  /* Smooth tool copies the size of the current tool. */
  int saved_smooth_size = 0;

  /**
   * Whether the modifier key that controls smoothing is active currently.
   * Generally signals a change in behavior for different brushes.
   *
   * \see BrushStrokeMode::BRUSH_STROKE_SMOOTH.
   */
  bool alt_smooth = false;

  float plane_trim_squared = 0.0f;

  bool supports_gravity = false;
  float3 gravity_direction = float3(0);
  float3 gravity_direction_symm = float3(0);

  std::unique_ptr<auto_mask::Cache> automasking;

  float4x4 stroke_local_mat = float4x4::identity();
  float multiplane_scrape_angle = 0.0f;

  StrokeCache();
  ~StrokeCache();
};

}  // namespace blender::ed::sculpt_paint

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

namespace blender::ed::sculpt_paint {
/**
 * Returns true if the current Mesh type can handle color attributes. If false an error message
 * will be shown to the user.  Operators should return OPERATOR_CANCELLED in this case.
 *
 * NOTE: Does not check if a color attribute actually exists. Calling code must handle this itself;
 * in most cases a call to BKE_sculpt_color_layer_create_if_needed() is sufficient.
 */
bool color_supported_check(const Scene &scene, Object &object, ReportList *reports);
}  // namespace blender::ed::sculpt_paint

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Update Functions
 * \{ */

namespace blender::ed::sculpt_paint {

/**
 * Triggers redraws, updates, and dependency graph tags as necessary after each brush calculation.
 */
void flush_update_step(const bContext *C, UpdateType update_type);
/**
 * Triggers redraws, updates, and dependency graph tags as necessary when a brush stroke finishes.
 */
void flush_update_done(const bContext *C, Object &ob, UpdateType update_type);

}  // namespace blender::ed::sculpt_paint

/**
 * Should be used after modifying the mask or Face Sets IDs.
 */
void SCULPT_tag_update_overlays(bContext *C);
/** \} */

/* -------------------------------------------------------------------- */
/** \name Stroke Functions
 * \{ */

namespace blender::ed::sculpt_paint {
/**
 * Do a ray-cast in the tree to find the 3d brush location
 * (This allows us to ignore the GL depth buffer)
 *
 * TODO: This should be updated to return std::optional<float3>
 */
bool stroke_get_location_bvh(bContext *C, float out[3], const float mval[2], bool force_original);

struct CursorGeometryInfo {
  float3 location;
  float3 normal;
};

/**
 * Gets the normal, location and active vertex location of the geometry under the cursor. This also
 * updates the active vertex and cursor related data of the SculptSession using the mouse position
 *
 * TODO: This should be updated to return `std::optional<CursorGeometryInfo>`
 */
bool cursor_geometry_info_update(bContext *C,
                                 CursorGeometryInfo *out,
                                 const float2 &mval,
                                 bool use_sampled_normal);

void geometry_preview_lines_update(Depsgraph &depsgraph,
                                   Object &object,
                                   SculptSession &ss,
                                   float radius);

}  // namespace blender::ed::sculpt_paint

void SCULPT_stroke_modifiers_check(const bContext *C, Object &ob, const Brush &brush);
namespace blender::ed::sculpt_paint {
float raycast_init(ViewContext *vc,
                   const float2 &mval,
                   float3 &ray_start,
                   float3 &ray_end,
                   float3 &ray_normal,
                   bool original);
}

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

namespace blender::ed::sculpt_paint {
/** Ensure random access; required for blender::bke::pbvh::Type::BMesh */
void vert_random_access_ensure(Object &object);
}  // namespace blender::ed::sculpt_paint

int SCULPT_vertex_count_get(const Object &object);

namespace blender::ed::sculpt_paint {
bool vertex_is_occluded(const Depsgraph &depsgraph,
                        const Object &object,
                        const float3 &position,
                        bool original);

/**
 * Coordinates used for manipulating the base mesh when Grab Active Vertex is enabled.
 */
Span<float3> vert_positions_for_grab_active_get(const Depsgraph &depsgraph, const Object &object);

using BMeshNeighborVerts = Vector<BMVert *, 64>;
Span<BMVert *> vert_neighbors_get_bmesh(BMVert &vert, BMeshNeighborVerts &r_neighbors);
Span<BMVert *> vert_neighbors_get_interior_bmesh(BMVert &vert, BMeshNeighborVerts &r_neighbors);

Span<int> vert_neighbors_get_mesh(OffsetIndices<int> faces,
                                  Span<int> corner_verts,
                                  GroupedSpan<int> vert_to_face,
                                  Span<bool> hide_poly,
                                  int vert,
                                  Vector<int> &r_neighbors);
}  // namespace blender::ed::sculpt_paint

/* Fake Neighbors */

#define FAKE_NEIGHBOR_NONE -1

/**
 * This allows the sculpt brushes to work on meshes with multiple connected components as if they
 * had only one connected component. These neighbors are calculated for each vertex using the
 * minimum distance to a vertex that is in a different connected component.
 */
blender::Span<int> SCULPT_fake_neighbors_ensure(const Depsgraph &depsgraph,
                                                Object &ob,
                                                float max_dist);
void SCULPT_fake_neighbors_free(Object &ob);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Brush Utilities.
 * \{ */

namespace blender::ed::sculpt_paint {

float brush_plane_offset_get(const Brush &brush, const SculptSession &ss);

/**
 * \warning This call is *not* idempotent and changes values inside the StrokeCache.
 *
 * Brushes may behave incorrectly if preserving original plane / normal when this
 * method is not called.
 */
void calc_brush_plane(const Depsgraph &depsgraph,
                      const Brush &brush,
                      Object &ob,
                      const IndexMask &node_mask,
                      float3 &r_area_no,
                      float3 &r_area_co);

std::optional<float3> calc_area_normal(const Depsgraph &depsgraph,
                                       const Brush &brush,
                                       const Object &ob,
                                       const IndexMask &node_mask);

/**
 * This calculates flatten center and area normal together,
 * amortizing the memory bandwidth and loop overhead to calculate both at the same time.
 */
void calc_area_normal_and_center(const Depsgraph &depsgraph,
                                 const Brush &brush,
                                 const Object &ob,
                                 const IndexMask &node_mask,
                                 float r_area_no[3],
                                 float r_area_co[3]);
void calc_area_center(const Depsgraph &depsgraph,
                      const Brush &brush,
                      const Object &ob,
                      const IndexMask &node_mask,
                      float r_area_co[3]);

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
}  // namespace blender::ed::sculpt_paint

ePaintSymmetryAreas SCULPT_get_vertex_symm_area(const float co[3]);
bool SCULPT_check_vertex_pivot_symmetry(const float vco[3], const float pco[3], char symm);
/**
 * Checks if a vertex is inside the brush radius from any of its mirrored axis.
 */
bool SCULPT_is_vertex_inside_brush_radius_symm(const float vertex[3],
                                               const float br_co[3],
                                               float radius,
                                               char symm);
blender::float3 SCULPT_flip_v3_by_symm_area(const blender::float3 &vector,
                                            ePaintSymmetryFlags symm,
                                            ePaintSymmetryAreas symmarea,
                                            const blender::float3 &pivot);
void SCULPT_flip_quat_by_symm_area(float quat[4],
                                   ePaintSymmetryFlags symm,
                                   ePaintSymmetryAreas symmarea,
                                   const float pivot[3]);

namespace blender::ed::sculpt_paint {

/**
 * Utility functions to get the closest vertices after flipping an original vertex position for
 * all symmetry passes. The returned vector is sorted.
 */
Vector<int> find_symm_verts_mesh(const Depsgraph &depsgraph,
                                 const Object &object,
                                 int original_vert,
                                 float max_distance = std::numeric_limits<float>::max());
Vector<int> find_symm_verts_grids(const Object &object,
                                  int original_vert,
                                  float max_distance = std::numeric_limits<float>::max());
Vector<int> find_symm_verts_bmesh(const Object &object,
                                  int original_vert,
                                  float max_distance = std::numeric_limits<float>::max());
Vector<int> find_symm_verts(const Depsgraph &depsgraph,
                            const Object &object,
                            int original_vert,
                            float max_distance = std::numeric_limits<float>::max());

bool node_fully_masked_or_hidden(const bke::pbvh::Node &node);
bool node_in_sphere(const bke::pbvh::Node &node,
                    const float3 &location,
                    float radius_sq,
                    bool original);
bool node_in_cylinder(const DistRayAABB_Precalc &ray_dist_precalc,
                      const bke::pbvh::Node &node,
                      float radius_sq,
                      bool original);
IndexMask gather_nodes(const bke::pbvh::Tree &pbvh,
                       eBrushFalloffShape falloff_shape,
                       bool use_original,
                       const float3 &location,
                       float radius_sq,
                       const std::optional<float3> &ray_direction,
                       IndexMaskMemory &memory);

}  // namespace blender::ed::sculpt_paint

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
                                     float translation[3]);

namespace blender::ed::sculpt_paint {
/**
 * Tilts a normal by the x and y tilt values using the view axis.
 */
float3 tilt_apply_to_normal(const Object &object,
                            const float4x4 &view_inverse,
                            const float3 &normal,
                            const float2 &tilt,
                            float tilt_strength);
float3 tilt_apply_to_normal(const float3 &normal, const StrokeCache &cache, float tilt_strength);

/**
 * Get effective surface normal with pen tilt and tilt strength applied to it.
 */
float3 tilt_effective_normal_get(const SculptSession &ss, const Brush &brush);
}  // namespace blender::ed::sculpt_paint

namespace blender::ed::sculpt_paint {
/**
 * The brush uses translations calculated at the beginning of the stroke. They can't be calculated
 * dynamically because changing positions will influence neighboring translations. However we can
 * reduce the cost in some cases by skipping initializing values for vertices in hidden or masked
 * nodes.
 */
void calc_smooth_translations(const Depsgraph &depsgraph,
                              const Object &object,
                              const IndexMask &node_mask,
                              MutableSpan<float3> translations);

}  // namespace blender::ed::sculpt_paint

/**
 * Flip all the edit-data across the axis/axes specified by \a symm.
 * Used to calculate multiple modifications to the mesh when symmetry is enabled.
 */
void SCULPT_cache_calc_brushdata_symm(blender::ed::sculpt_paint::StrokeCache &cache,
                                      ePaintSymmetryFlags symm,
                                      char axis,
                                      float angle);

namespace blender::ed::sculpt_paint {

struct OrigPositionData {
  Span<float3> positions;
  Span<float3> normals;
};
/**
 * Retrieve positions from the latest undo state. This is often used for modal actions that depend
 * on the initial state of the geometry from before the start of the action.
 */
std::optional<OrigPositionData> orig_position_data_lookup_mesh_all_verts(
    const Object &object, const bke::pbvh::MeshNode &node);
std::optional<OrigPositionData> orig_position_data_lookup_mesh(const Object &object,
                                                               const bke::pbvh::MeshNode &node);
inline OrigPositionData orig_position_data_get_mesh(const Object &object,
                                                    const bke::pbvh::MeshNode &node)
{
  const std::optional<OrigPositionData> result = orig_position_data_lookup_mesh(object, node);
  BLI_assert(result.has_value());
  return *result;
}

std::optional<OrigPositionData> orig_position_data_lookup_grids(const Object &object,
                                                                const bke::pbvh::GridsNode &node);
inline OrigPositionData orig_position_data_get_grids(const Object &object,
                                                     const bke::pbvh::GridsNode &node)
{
  const std::optional<OrigPositionData> result = orig_position_data_lookup_grids(object, node);
  BLI_assert(result.has_value());
  return *result;
}

void orig_position_data_gather_bmesh(const BMLog &bm_log,
                                     const Set<BMVert *, 0> &verts,
                                     MutableSpan<float3> positions,
                                     MutableSpan<float3> normals);

std::optional<Span<float4>> orig_color_data_lookup_mesh(const Object &object,
                                                        const bke::pbvh::MeshNode &node);
inline Span<float4> orig_color_data_get_mesh(const Object &object, const bke::pbvh::MeshNode &node)
{
  return *orig_color_data_lookup_mesh(object, node);
}

std::optional<Span<int>> orig_face_set_data_lookup_mesh(const Object &object,
                                                        const bke::pbvh::MeshNode &node);

std::optional<Span<int>> orig_face_set_data_lookup_grids(const Object &object,
                                                         const bke::pbvh::GridsNode &node);

std::optional<Span<float>> orig_mask_data_lookup_mesh(const Object &object,
                                                      const bke::pbvh::MeshNode &node);

std::optional<Span<float>> orig_mask_data_lookup_grids(const Object &object,
                                                       const bke::pbvh::GridsNode &node);

inline bool brush_type_is_paint(const int tool)
{
  return ELEM(tool, SCULPT_BRUSH_TYPE_PAINT, SCULPT_BRUSH_TYPE_SMEAR);
}

inline bool brush_type_is_mask(const int tool)
{
  return ELEM(tool, SCULPT_BRUSH_TYPE_MASK);
}

BLI_INLINE bool brush_type_is_attribute_only(const int tool)
{
  return brush_type_is_paint(tool) || brush_type_is_mask(tool) ||
         ELEM(tool, SCULPT_BRUSH_TYPE_DRAW_FACE_SETS);
}

inline bool brush_uses_vector_displacement(const Brush &brush)
{
  return brush.sculpt_brush_type == SCULPT_BRUSH_TYPE_DRAW &&
         brush.flag2 & BRUSH_USE_COLOR_AS_DISPLACEMENT &&
         brush.mtex.brush_map_mode == MTEX_MAP_MODE_AREA;
}

void ensure_valid_pivot(const Object &ob, Paint &paint);

/** Retrieve or calculate the object space radius depending on brush settings. */
float object_space_radius_get(const ViewContext &vc,
                              const Paint &paint,
                              const Brush &brush,
                              const float3 &location,
                              float scale_factor = 1.0);
}  // namespace blender::ed::sculpt_paint

/** \} */

/* -------------------------------------------------------------------- */
/** \name 3D Texture Paint (Experimental)
 * \{ */

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
void SCULPT_do_paint_brush_image(const Depsgraph &depsgraph,
                                 PaintModeSettings &paint_mode_settings,
                                 const Sculpt &sd,
                                 Object &ob,
                                 const blender::IndexMask &node_mask);
bool SCULPT_use_image_paint_brush(PaintModeSettings &settings, Object &ob);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Operators
 * \{ */

namespace blender::ed::sculpt_paint {

void SCULPT_OT_brush_stroke(wmOperatorType *ot);

}

namespace blender::ed::sculpt_paint::expand {

void SCULPT_OT_expand(wmOperatorType *ot);
void modal_keymap(wmKeyConfig *keyconf);

}  // namespace blender::ed::sculpt_paint::expand

namespace blender::ed::sculpt_paint::project {
void SCULPT_OT_project_line_gesture(wmOperatorType *ot);
}

namespace blender::ed::sculpt_paint::trim {
void SCULPT_OT_trim_lasso_gesture(wmOperatorType *ot);
void SCULPT_OT_trim_box_gesture(wmOperatorType *ot);
void SCULPT_OT_trim_line_gesture(wmOperatorType *ot);
void SCULPT_OT_trim_polyline_gesture(wmOperatorType *ot);
}  // namespace blender::ed::sculpt_paint::trim

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

}  // namespace blender::ed::sculpt_paint::face_set

namespace blender::ed::sculpt_paint {

void SCULPT_OT_set_pivot_position(wmOperatorType *ot);

}

namespace blender::ed::sculpt_paint::filter {

void SCULPT_OT_mesh_filter(wmOperatorType *ot);
wmKeyMap *modal_keymap(wmKeyConfig *keyconf);

}  // namespace blender::ed::sculpt_paint::filter

namespace blender::ed::sculpt_paint::cloth {
void SCULPT_OT_cloth_filter(wmOperatorType *ot);
}

namespace blender::ed::sculpt_paint::color {
void SCULPT_OT_color_filter(wmOperatorType *ot);
}

namespace blender::ed::sculpt_paint::mask {

void SCULPT_OT_mask_filter(wmOperatorType *ot);
void SCULPT_OT_mask_init(wmOperatorType *ot);

}  // namespace blender::ed::sculpt_paint::mask

namespace blender::ed::sculpt_paint::dyntopo {

void SCULPT_OT_detail_flood_fill(wmOperatorType *ot);
void SCULPT_OT_sample_detail_size(wmOperatorType *ot);
void SCULPT_OT_dyntopo_detail_size_edit(wmOperatorType *ot);
void SCULPT_OT_dynamic_topology_toggle(wmOperatorType *ot);

}  // namespace blender::ed::sculpt_paint::dyntopo

/** \} */
