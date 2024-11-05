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
#include "BKE_paint.hh"
#include "BKE_paint_bvh.hh"
#include "BKE_subdiv_ccg.hh"

#include "BLI_array.hh"
#include "BLI_generic_array.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "DNA_brush_enums.h"

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
struct BMLog;
struct Dial;
struct DistRayAABB_Precalc;
struct Image;
struct ImageUser;
struct Key;
struct KeyBlock;
struct Object;
struct bContext;
struct PaintModeSettings;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperatorType;

/* -------------------------------------------------------------------- */
/** \name Sculpt Types
 * \{ */

namespace blender::ed::sculpt_paint {

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

  Key *keys_;
  KeyBlock *active_key_;
  bool basis_active_;
  std::optional<Array<bool>> dependent_keys_;

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

struct SculptCursorGeometryInfo {
  blender::float3 location;
  blender::float3 normal;
  blender::float3 active_vertex_co;
};

/* Factor of brush to have rake point following behind
 * (could be configurable but this is reasonable default). */
#define SCULPT_RAKE_BRUSH_FACTOR 0.25f

struct SculptRakeData {
  float follow_dist;
  blender::float3 follow_co;
  float angle;
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

/**
 * This structure contains all the temporary data
 * needed for individual brush strokes.
 */
struct StrokeCache {
  /* Invariants */
  float initial_radius;
  float3 scale;
  struct {
    uint8_t flag = 0;
    float3 tolerance;
    float4x4 mat;
    float4x4 mat_inv;
  } mirror_modifier_clip;
  float2 initial_mouse;

  /**
   * Some brushes change behavior drastically depending on the directional value (i.e. the smooth
   * and enhance details functionality being bound to the Smooth brush).
   *
   * Storing the initial direction allows discerning the behavior without checking the sign of the
   * brush direction at every step, which would have ambiguity at 0.
   */
  bool initial_direction_flipped;

  /* Variants */
  float radius;
  float radius_squared;
  float3 location;
  float3 last_location;
  float3 location_symm;
  float3 last_location_symm;
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
  float hardness;
  /**
   * Depending on the mode, can either be the raw brush strength, or a scaled (possibly negative)
   * value.
   *
   * \see #brush_strength for Sculpt Mode.
   */
  float bstrength;
  float normal_weight; /* from brush (with optional override) */
  float2 tilt;

  /* Position of the mouse corresponding to the stroke location, modified by the paint_stroke
   * operator according to the stroke type. */
  float2 mouse;
  /* Position of the mouse event in screen space, not modified by the stroke type. */
  float2 mouse_event;

  struct {
    Array<float3> prev_displacement;
    Array<float3> limit_surface_co;
  } displacement_smear;

  /* The rest is temporary storage that isn't saved as a property */

  bool first_time; /* Beginning of stroke may do some things special */

  /* from ED_view3d_ob_project_mat_get() */
  float4x4 projection_mat;

  /* Clean this up! */
  ViewContext *vc;
  const Brush *brush;

  float special_rotation;
  float3 grab_delta, grab_delta_symm;
  float3 old_grab_location, orig_grab_location;

  /* screen-space rotation defined by mouse motion */
  std::optional<math::Quaternion> rake_rotation;
  std::optional<math::Quaternion> rake_rotation_symm;
  SculptRakeData rake_data;

  /* Face Sets */
  int paint_face_set;

  /* Symmetry index between 0 and 7 bit combo 0 is Brush only;
   * 1 is X mirror; 2 is Y mirror; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  int symmetry;
  /* The symmetry pass we are currently on between 0 and 7. */
  ePaintSymmetryFlags mirror_symmetry_pass;
  float3 view_normal;
  float3 view_normal_symm;

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
   * \note inverted for #SCULPT_BRUSH_TYPE_DRAW_SHARP.
   */
  bool accum;

  /* Paint Brush. */
  struct {
    float flow;

    float4 wet_mix_prev_color;
    float wet_mix;
    float wet_persistence;

    float density_seed;
    float density;

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
    float front_angle;
    /* Stores the last 10 pressure samples to get an stabilized strength and radius variation. */
    std::array<float, 10> pressure_stabilizer;
    int stabilizer_index;

  } clay_thumb_brush;

  /* Cloth brush */
  std::unique_ptr<cloth::SimulationData> cloth_sim;
  float3 initial_location_symm;
  float3 initial_location;
  float3 initial_normal_symm;
  float3 initial_normal;

  /* Boundary brush */
  std::array<std::unique_ptr<boundary::SculptBoundary>, PAINT_SYMM_AREAS> boundaries;

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
  float3 gravity_direction;
  float3 gravity_direction_symm;

  std::unique_ptr<auto_mask::Cache> automasking;

  float4x4 stroke_local_mat;
  float multiplane_scrape_angle;

  rcti previous_r; /* previous redraw rectangle */
  rcti current_r;  /* current redraw rectangle */

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

/**
 * Returns true if sculpt session can handle color attributes
 * (pbvh->type() == bke::pbvh::Type::Mesh).  If false an error
 * message will be shown to the user.  Operators should return
 * OPERATOR_CANCELLED in this case.
 *
 * NOTE: Does not check if a color attribute actually exists.
 * Calling code must handle this itself; in most cases a call to
 * BKE_sculpt_color_layer_create_if_needed() is sufficient.
 */
bool SCULPT_handles_colors_report(const Object &object, ReportList *reports);

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

}  // namespace blender::ed::sculpt_paint

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

void geometry_preview_lines_update(Depsgraph &depsgraph,
                                   Object &object,
                                   SculptSession &ss,
                                   float radius);

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
void SCULPT_vertex_random_access_ensure(Object &object);

int SCULPT_vertex_count_get(const Object &object);

bool SCULPT_vertex_is_occluded(const Depsgraph &depsgraph,
                               const Object &object,
                               const blender::float3 &position,
                               bool original);

namespace blender::ed::sculpt_paint {

/**
 * Coordinates used for manipulating the base mesh when Grab Active Vertex is enabled.
 */
Span<float3> vert_positions_for_grab_active_get(const Depsgraph &depsgraph, const Object &object);

Span<BMVert *> vert_neighbors_get_bmesh(BMVert &vert, Vector<BMVert *, 64> &r_neighbors);
Span<BMVert *> vert_neighbors_get_interior_bmesh(BMVert &vert, Vector<BMVert *, 64> &r_neighbors);

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

bool SCULPT_brush_type_needs_all_pbvh_nodes(const Brush &brush);

namespace blender::ed::sculpt_paint {

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
bool node_in_cylinder(const DistRayAABB_Precalc &dist_ray_precalc,
                      const bke::pbvh::Node &node,
                      float radius_sq,
                      bool original);

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

}  // namespace blender::ed::sculpt_paint

/** \} */

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

}  // namespace blender::ed::sculpt_paint::expand

/** \} */

namespace blender::ed::sculpt_paint::project {
void SCULPT_OT_project_line_gesture(wmOperatorType *ot);
}

namespace blender::ed::sculpt_paint::trim {
void SCULPT_OT_trim_lasso_gesture(wmOperatorType *ot);
void SCULPT_OT_trim_box_gesture(wmOperatorType *ot);
void SCULPT_OT_trim_line_gesture(wmOperatorType *ot);
void SCULPT_OT_trim_polyline_gesture(wmOperatorType *ot);
}  // namespace blender::ed::sculpt_paint::trim

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

}  // namespace blender::ed::sculpt_paint::face_set

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

}  // namespace blender::ed::sculpt_paint::filter

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

}  // namespace blender::ed::sculpt_paint::mask

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

}  // namespace blender::ed::sculpt_paint::dyntopo

/** \} */

/* sculpt_brush_types.cc */

/* -------------------------------------------------------------------- */
/** \name Brushes
 * \{ */

namespace blender::ed::sculpt_paint {

void multiplane_scrape_preview_draw(uint gpuattr,
                                    const Brush &brush,
                                    const SculptSession &ss,
                                    const float outline_col[3],
                                    float outline_alpha);

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
void SCULPT_do_paint_brush_image(const Scene &scene,
                                 const Depsgraph &depsgraph,
                                 PaintModeSettings &paint_mode_settings,
                                 const Sculpt &sd,
                                 Object &ob,
                                 const blender::IndexMask &node_mask);
bool SCULPT_use_image_paint_brush(PaintModeSettings &settings, Object &ob);

namespace blender::ed::sculpt_paint {

float clay_thumb_get_stabilized_pressure(const blender::ed::sculpt_paint::StrokeCache &cache);

void SCULPT_OT_brush_stroke(wmOperatorType *ot);

}  // namespace blender::ed::sculpt_paint

inline bool SCULPT_brush_type_is_paint(int tool)
{
  return ELEM(tool, SCULPT_BRUSH_TYPE_PAINT, SCULPT_BRUSH_TYPE_SMEAR);
}

inline bool SCULPT_brush_type_is_mask(int tool)
{
  return ELEM(tool, SCULPT_BRUSH_TYPE_MASK);
}

BLI_INLINE bool SCULPT_brush_type_is_attribute_only(int tool)
{
  return SCULPT_brush_type_is_paint(tool) || SCULPT_brush_type_is_mask(tool) ||
         ELEM(tool, SCULPT_BRUSH_TYPE_DRAW_FACE_SETS);
}

namespace blender::ed::sculpt_paint {
void ensure_valid_pivot(const Object &ob, Scene &scene);
}

namespace blender::ed::sculpt_paint {
float sculpt_calc_radius(const ViewContext &vc,
                         const Brush &brush,
                         const Scene &scene,
                         float3 location);
}
