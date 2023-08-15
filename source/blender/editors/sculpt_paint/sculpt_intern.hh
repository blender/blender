/* SPDX-FileCopyrightText: 2006 by Nicholas Bishop. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "DNA_brush_enums.h" /* For eAttrCorrectMode. */
#include "DNA_brush_types.h"
#include "DNA_key_types.h"
#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_scene_types.h"
#include "DNA_vec_types.h"

#include "BKE_attribute.h"
#include "BKE_dyntopo.hh"
#include "BKE_paint.hh"
#include "BKE_pbvh_api.hh"
#include "BKE_sculpt.h"
#include "BKE_sculpt.hh"

#include "BLI_bitmap.h"
#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_generic_array.hh"
#include "BLI_gsqueue.h"
#include "BLI_implicit_sharing.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "ED_view3d.hh"

#include "bmesh.h"

#include <functional>

struct AutomaskingCache;
struct AutomaskingNodeData;
struct Dial;
struct DistRayAABB_Precalc;
struct Image;
struct ImageUser;
struct KeyBlock;
struct Object;
struct SculptProjectVector;
struct SculptUndoNode;
struct bContext;
struct BrushChannelSet;
struct TaskParallelTLS;

enum ePaintSymmetryFlags;
struct PaintModeSettings;
struct WeightPaintInfo;
struct WPaintData;
struct wmKeyConfig;
struct wmKeyMap;
struct wmOperator;
struct wmOperatorType;

/*
maximum symmetry passes returned by SCULPT_get_symmetry_pass.
enough for about ~30 radial symmetry passes, which seems like plenty

used by various code that needs to statically store per-pass state.
*/
#define SCULPT_MAX_SYMMETRY_PASSES 255

using blender::float3;
using blender::Span;
using blender::Vector;

/* Updates */

/* -------------------------------------------------------------------- */
/** \name Sculpt Types
 * \{ */

enum { SCULPT_SHARP_SIMPLE, SCULPT_SHARP_PLANE };

enum SculptUpdateType {
  SCULPT_UPDATE_COORDS = 1 << 0,
  SCULPT_UPDATE_MASK = 1 << 1,
  SCULPT_UPDATE_VISIBILITY = 1 << 2,
  SCULPT_UPDATE_COLOR = 1 << 3,
  SCULPT_UPDATE_IMAGE = 1 << 4,
};

struct SculptCursorGeometryInfo {
  float location[3];
  float back_location[3];
  float normal[3];
  float active_vertex_co[3];
};

struct _SculptNeighborRef {
  PBVHVertRef vertex;
  PBVHEdgeRef edge;
};

#define SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY 16

struct SculptVertexNeighborIter {
  /* Storage */
  struct _SculptNeighborRef *neighbors;
  int *neighbor_indices;

  int size;
  int capacity;
  struct _SculptNeighborRef neighbors_fixed[SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY];
  int neighbor_indices_fixed[SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY];

  /* Internal iterator. */
  int num_duplicates;
  int i;

  /* Public */
  PBVHVertRef vertex;
  PBVHEdgeRef edge;
  int index;
  bool has_edge;  // does this iteration step have an edge, fake neighbors do not
  bool is_duplicate;
  bool no_free;
};

struct SculptFaceSetIsland {
  PBVHFaceRef *faces;
  int totface;
};

struct SculptFaceSetIslands {
  SculptFaceSetIsland *islands;
  int totisland;
};

/* Sculpt Original Data */
struct SculptOrigVertData {
  BMLog *bm_log;

  struct SculptUndoNode *unode;
  int datatype;
  float (*coords)[3];
  float (*normals)[3];
  const float *vmasks;
  float (*colors)[4];
  float _no[3];

  /* Original coordinate, normal, and mask. */
  const float *co;
  const float *no;
  float mask;
  const float *col;
  struct PBVH *pbvh;
  struct SculptSession *ss;
};

struct SculptOrigFaceData {
  SculptUndoNode *unode;
  BMLog *bm_log;
  const int *face_sets;
  int face_set;
};

/* Flood Fill. */
struct SculptFloodFill {
  GSQueue *queue;
  BLI_bitmap *visited_verts;
};

enum eBoundaryAutomaskMode {
  AUTOMASK_INIT_BOUNDARY_EDGES = 1,
  AUTOMASK_INIT_BOUNDARY_FACE_SETS = 2,
};

enum SculptUndoType {
  SCULPT_UNDO_NO_TYPE = 0,
  SCULPT_UNDO_COORDS = 1 << 0,
  SCULPT_UNDO_HIDDEN = 1 << 1,
  SCULPT_UNDO_MASK = 1 << 2,
  SCULPT_UNDO_DYNTOPO_BEGIN = 1 << 3,
  SCULPT_UNDO_DYNTOPO_END = 1 << 4,
  SCULPT_UNDO_DYNTOPO_SYMMETRIZE = 1 << 5,
  SCULPT_UNDO_GEOMETRY = 1 << 6,
  SCULPT_UNDO_FACE_SETS = 1 << 7,
  SCULPT_UNDO_COLOR = 1 << 8,
};
ENUM_OPERATORS(SculptUndoType, SCULPT_UNDO_COLOR);

/* Storage of geometry for the undo node.
 * Is used as a storage for either original or modified geometry. */
struct SculptUndoNodeGeometry {
  /* Is used for sanity check, helping with ensuring that two and only two
   * geometry pushes happened in the undo stack. */
  bool is_initialized;

  CustomData vert_data;
  CustomData edge_data;
  CustomData loop_data;
  CustomData face_data;
  int *face_offset_indices;
  const blender::ImplicitSharingInfo *face_offsets_sharing_info;
  int totvert;
  int totedge;
  int totloop;
  int faces_num;
};

struct SculptUndoNode {
  SculptUndoNode *next, *prev;

  SculptUndoType type;

  char idname[MAX_ID_NAME]; /* Name instead of pointer. */
  void *node;               /* only during push, not valid afterwards! */

  float (*co)[3];
  float (*orig_co)[3];
  float (*no)[3];
  float (*col)[4];
  float *mask;
  int totvert;

  float (*loop_col)[4];
  float (*orig_loop_col)[4];
  int totloop;

  /* non-multires */
  int maxvert; /* to verify if totvert it still the same */
  int *index;  /* to restore into right location */
  int maxloop;
  int *loop_index;

  BLI_bitmap *vert_hidden;

  /* multires */
  int maxgrid;  /* same for grid */
  int gridsize; /* same for grid */
  int totgrid;  /* to restore into right location */
  int *grids;   /* to restore into right location */
  BLI_bitmap **grid_hidden;

  /* bmesh */
  BMLogEntry *bm_entry;
  BMLog *bm_log;
  bool applied;

  /* shape keys */
  char shapeName[sizeof(KeyBlock::name)];

  /* Geometry modification operations.
   *
   * Original geometry is stored before some modification is run and is used to restore state of
   * the object when undoing the operation
   *
   * Modified geometry is stored after the modification and is used to redo the modification. */
  bool geometry_clear_pbvh;
  SculptUndoNodeGeometry geometry_original;
  SculptUndoNodeGeometry geometry_modified;

  /* pivot */
  float pivot_pos[3];
  float pivot_rot[4];

  /* Sculpt Face Sets */
  int *face_sets;

  // dyntopo stuff

  int *nodemap;
  int nodemap_size;
  int typemask;

  PBVHFaceRef *faces;
  int faces_num;

  size_t undo_size;
  // int gen, lasthash;
};

/* Factor of brush to have rake point following behind
 * (could be configurable but this is reasonable default). */
#define SCULPT_RAKE_BRUSH_FACTOR 0.25f

struct SculptRakeData {
  float follow_dist;
  float follow_co[3];
  float angle;
};

/**
 * Generic thread data. The size of this struct has gotten a little out of hand;
 * normally we would split it up, but it might be better to see if we can't eliminate it
 * altogether after moving to C++ (where we'll be able to use lambdas).
 */
struct SculptThreadedTaskData {
  bContext *C;
  Sculpt *sd;
  Object *ob;
  const Brush *brush;
  SculptSession *ss;
  Span<PBVHNode *> nodes;

  VPaint *vp;
  WPaintData *wpd;
  WeightPaintInfo *wpi;
  unsigned int *lcol;
  Mesh *me;
  /* For passing generic params. */
  void *custom_data;

  /* Data specific to some callbacks. */

  /* NOTE: even if only one or two of those are used at a time,
   *       keeping them separated, names help figuring out
   *       what it is, and memory overhead is ridiculous anyway. */
  float flippedbstrength;
  float angle;
  float strength;
  bool smooth_mask;
  bool smooth_origco;
  bool has_bm_orco;

  SculptProjectVector *spvc;
  float *offset;
  float *grab_delta;
  float *cono;
  float *area_no;
  float *area_no_sp;
  float *area_co;
  float (*mat)[4];
  float (*vertCos)[3];

  /* When true, the displacement stored in the proxies will be applied to the original
   * coordinates instead of to the current coordinates. */
  bool use_proxies_orco;

  /* X and Z vectors aligned to the stroke direction for operations where perpendicular vectors
   * to the stroke direction are needed. */
  float (*stroke_xz)[3];

  int filter_type;
  float filter_strength;
  float *filter_fill_color;

  bool use_area_cos;
  bool use_area_nos;

  /* 0=towards view, 1=flipped */
  float (*area_cos)[3];
  float (*area_nos)[3];
  int *count_no;
  int *count_co;

  bool any_vertex_sampled;

  float *wet_mix_sampled_color;
  float hue_offset;

  float *prev_mask;
  float *new_mask;
  float *next_mask;
  float mask_interpolation;

  float *pose_factor;
  float *pose_initial_co;
  int pose_chain_segment;

  float multiplane_scrape_angle;
  float multiplane_scrape_planes[2][4];

  float max_distance_squared;
  float nearest_vertex_search_co[3];

  /* Stabilized strength for the Clay Thumb brush. */
  float clay_strength;

  int mask_expand_update_it;
  bool mask_expand_invert_mask;
  bool mask_expand_use_normals;
  bool mask_expand_keep_prev_mask;
  bool mask_expand_create_face_set;

  float transform_mats[8][4][4];
  float elastic_transform_mat[4][4];
  float elastic_transform_pivot[3];
  float elastic_transform_pivot_init[3];
  float elastic_transform_radius;

  /* Boundary brush */
  float boundary_deform_strength;

  float cloth_time_step;
  SculptClothSimulation *cloth_sim;
  float *cloth_sim_initial_location;
  float cloth_sim_radius;

  /* Mask By Color Tool */

  float mask_by_color_threshold;
  bool mask_by_color_invert;
  bool mask_by_color_preserve_mask;

  /* Index of the vertex that is going to be used as a reference for the colors. */
  PBVHVertRef mask_by_color_vertex;
  float *mask_by_color_floodfill;

  int face_set, face_set2;
  int filter_undo_type;

  int mask_init_mode;
  int mask_init_seed;

  ThreadMutex mutex;

  // Layer brush
  int cd_temp, cd_temp2, cd_temp3;

  float smooth_projection;
  float rake_projection;
  SculptAttribute *scl, *scl2;
  bool do_origco;
  float *brush_color;

  float fset_slide, bound_smooth;
  float crease_pinch_factor;
  bool use_curvature;
  float vel_smooth_fac;
  int iterations;
  int iteration;
};

/*************** Brush testing declarations ****************/
struct SculptBrushTest {
  float radius_squared;
  float radius;
  float location[3];
  float dist;
  ePaintSymmetryFlags mirror_symmetry_pass;

  int radial_symmetry_pass;
  float symm_rot_mat_inv[4][4];

  float tip_roundness;
  float tip_scale_x;
  bool test_cube_z;

  float cube_matrix[4][4];

  /* For circle (not sphere) projection. */
  float plane_view[4];

  /* Some tool code uses a plane for its calculations. */
  float plane_tool[4];

  /* View3d clipping - only set rv3d for clipping */
  RegionView3D *clip_rv3d;
  char falloff_shape;
};

using SculptBrushTestFn = bool (*)(SculptBrushTest *test, const float co[3]);

struct SculptSearchSphereData {
  Sculpt *sd;
  SculptSession *ss;
  float radius_squared;
  const float *center;
  bool original;
  /* This ignores fully masked and fully hidden nodes. */
  bool ignore_fully_ineffective;
  struct Object *ob;
  struct Brush *brush;
};

struct SculptSearchCircleData {
  Sculpt *sd;
  SculptSession *ss;
  float radius_squared;
  bool original;
  bool ignore_fully_ineffective;
  DistRayAABB_Precalc *dist_ray_to_aabb_precalc;
};

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
#define SCULPT_SPEED_MA_SIZE 4
#define GRAB_DELTA_MA_SIZE 3

struct AutomaskingSettings {
  /* Flags from eAutomasking_flag. */
  int flags;
  int initial_face_set;
  int current_face_set;  // used by faceset draw tool
  bool original_normal;
  int initial_island_nr;

  float cavity_factor;
  int cavity_blur_steps;
  struct CurveMapping *cavity_curve;

  float start_normal_limit, start_normal_falloff;
  float view_normal_limit, view_normal_falloff;

  bool topology_use_brush_limit;
};

struct AutomaskingCache {
  AutomaskingSettings settings;

  bool can_reuse_mask;
  uchar current_stroke_id;
};

enum eSculptGradientType {
  SCULPT_GRADIENT_LINEAR,
  SCULPT_GRADIENT_SPHERICAL,
  SCULPT_GRADIENT_RADIAL,
  SCULPT_GRADIENT_ANGLE,
  SCULPT_GRADIENT_REFLECTED,
};
struct SculptGradientContext {
  eSculptGradientType gradient_type;
  ViewContext vc;

  int symm;

  int update_type;
  float line_points[2][2];

  float line_length;

  float depth_point[3];

  float gradient_plane[4];
  float initial_location[3];

  float gradient_line[3];
  float initial_projected_location[2];

  float strength;
  void (*sculpt_gradient_begin)(struct bContext *);

  void (*sculpt_gradient_apply_for_element)(struct Sculpt *,
                                            struct SculptSession *,
                                            SculptOrigVertData *orig_data,
                                            PBVHVertexIter *vd,
                                            float gradient_value,
                                            float fade_value);
  void (*sculpt_gradient_node_update)(struct PBVHNode *);
  void (*sculpt_gradient_end)(struct bContext *);
};

/* IPMask filter vertex callback function. */
typedef float(SculptIPMaskFilterStepVertexCB)(struct SculptSession *, PBVHVertRef, float *);

struct FilterCache {
  bool enabled_axis[3];
  bool enabled_force_axis[3];
  int random_seed;

  /* Used for alternating between filter operations in filters that need to apply different ones
   * to achieve certain effects. */
  int iteration_count;

  /* Stores the displacement produced by the laplacian step of HC smooth. */
  float surface_smooth_shape_preservation;
  float surface_smooth_current_vertex;

  /* Sharpen mesh filter. */
  float sharpen_smooth_ratio;
  float sharpen_intensify_detail_strength;
  int sharpen_curvature_smooth_iterations;
  float *sharpen_factor;
  float (*detail_directions)[3];

  /* Sphere mesh filter. */
  float sphere_center[3];
  float sphere_radius;

  /* Filter orientation. */
  SculptFilterOrientation orientation;
  float obmat[4][4];
  float obmat_inv[4][4];
  float viewmat[4][4];
  float viewmat_inv[4][4];

  /* Displacement eraser. */
  float (*limit_surface_co)[3];

  /* unmasked nodes */
  Vector<PBVHNode *> nodes;

  /* Cloth filter. */
  SculptClothSimulation *cloth_sim;
  float cloth_sim_pinch_point[3];

  /* mask expand iteration caches */
  int mask_update_current_it;
  int mask_update_last_it;
  int *mask_update_it;
  float *normal_factor;
  float *edge_factor;
  float *prev_mask;
  float mask_expand_initial_co[3];

  int new_face_set;
  int *prev_face_set;

  int active_face_set;

  /* Transform. */
  SculptTransformDisplacementMode transform_displacement_mode;

  /* Gradient. */
  SculptGradientContext *gradient_context;

  /* Auto-masking. */
  AutomaskingCache *automasking;
  float initial_normal[3];
  float view_normal[3];

  /* Mask Filter. */
  int mask_filter_current_step;
  float *mask_filter_ref;
  SculptIPMaskFilterStepVertexCB *mask_filter_step_forward;
  SculptIPMaskFilterStepVertexCB *mask_filter_step_backward;

  GHash *mask_delta_step;

  bool preserve_fset_boundaries;
  bool weighted_smooth;
  float hard_edge_fac;
  bool hard_edge_mode;
  float hard_corner_pin;
  float bound_smooth_radius;
  float bevel_smooth_fac;

  float (*pre_smoothed_color)[4];

  ViewContext vc;
  float start_filter_strength;
  bool no_orig_co;
};

/**
 * This structure contains all the temporary data
 * needed for individual brush strokes.
 */
struct StrokeCache {
  /* Invariants */
  float initial_radius;
  float scale[3];
  int flag;
  float clip_tolerance[3];
  float clip_mirror_mtx[4][4];
  float initial_mouse[2];

  /* Variants */
  float last_anchored_radius; /* Used by paint_mesh_restore_co. */
  float radius;
  float radius_squared;
  float true_location[3];
  float true_last_location[3];
  float location[3];
  float last_location[3];

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
  float mouse[2];
  /* Position of the mouse event in screen space, not modified by the stroke type. */
  float mouse_event[2];

  float (*prev_colors)[4];
  blender::GArray<> prev_colors_vpaint;

  /* Multires Displacement Smear. */
  float (*prev_displacement)[3];
  float (*limit_surface_co)[3];

  /* The rest is temporary storage that isn't saved as a property */

  bool first_time; /* Beginning of stroke may do some things special */

  /* from ED_view3d_ob_project_mat_get() */
  float projection_mat[4][4];

  /* Clean this up! */
  ViewContext *vc;
  const Brush *brush;

  float special_rotation;
  float grab_delta[3], grab_delta_symmetry[3];
  float old_grab_location[3], orig_grab_location[3];

  // next_grab_delta is same as grab_delta except in smooth rake mode
  float prev_grab_delta[3], next_grab_delta[3];
  float prev_grab_delta_symmetry[3], next_grab_delta_symmetry[3];
  float grab_delta_avg[GRAB_DELTA_MA_SIZE][3];
  int grab_delta_avg_cur;

  /* screen-space rotation defined by mouse motion */
  float rake_rotation[4], rake_rotation_symmetry[4];
  bool is_rake_rotation_valid;
  SculptRakeData rake_data;

  /* Geodesic distances. */
  float *geodesic_dists[PAINT_SYMM_AREAS];

  /* Face Sets */
  int paint_face_set;

  /* Symmetry index between 0 and 7 bit combo 0 is Brush only;
   * 1 is X mirror; 2 is Y mirror; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  int symmetry;
  ePaintSymmetryFlags
      mirror_symmetry_pass; /* The symmetry pass we are currently on between 0 and 7. */
  float true_view_normal[3];
  float view_normal[3];

  float view_origin[3];
  float true_view_origin[3];

  /* sculpt_normal gets calculated by calc_sculpt_normal(), then the
   * sculpt_normal_symm gets updated quickly with the usual symmetry
   * transforms */
  float sculpt_normal[3];
  float sculpt_normal_symm[3];

  float cached_area_normal[3];

  /* Used for area texture mode, local_mat gets calculated by
   * calc_brush_local_mat() and used in sculpt_apply_texture().
   * Transforms from model-space coords to local area coords.
   */
  float brush_local_mat[4][4];
  /* The matrix from local area coords to model-space coords is used to calculate the vector
   * displacement in area plane mode. */
  float brush_local_mat_inv[4][4];

  float plane_offset[3]; /* used to shift the plane around when doing tiled strokes */
  int tile_pass;

  float last_center[3];
  int radial_symmetry_pass;
  float symm_rot_mat[4][4];
  float symm_rot_mat_inv[4][4];

  /* Accumulate mode. Note: inverted for SCULPT_TOOL_DRAW_SHARP. */
  bool accum;

  float anchored_location[3];

  /* Fairing. */

  /* Paint Brush. */
  struct {
    float hardness;
    float flow;
    float wet_mix;
    float wet_persistence;
    float density;
  } paint_brush;

  /* Pose brush */
  SculptPoseIKChain *pose_ik_chain;

  /* Clay Thumb brush */
  /* Angle of the front tilting plane of the brush to simulate clay accumulation. */
  float clay_thumb_front_angle;
  /* Stores pressure samples to get an stabilized strength and radius variation. */
  float clay_pressure_stabilizer[SCULPT_CLAY_STABILIZER_LEN];
  int clay_pressure_stabilizer_index;

  /* Cloth brush */
  SculptClothSimulation *cloth_sim;
  float initial_location[3];
  float true_initial_location[3];
  float initial_normal[3];
  float true_initial_normal[3];

  /* Boundary brush */
  SculptBoundary *boundaries[PAINT_SYMM_AREAS];

  float vertex_rotation; /* amount to rotate the vertices when using rotate brush */
  Dial *dial;

  char saved_active_brush_name[MAX_ID_NAME];
  char saved_mask_brush_tool;
  int saved_smooth_size; /* smooth tool copies the size of the current tool */
  bool alt_smooth;

  /* Scene Project Brush */
  struct SnapObjectContext *snap_context;
  struct Depsgraph *depsgraph;

  float plane_trim_squared;

  bool supports_gravity;
  float true_gravity_direction[3];
  float gravity_direction[3];

  /* Auto-masking. */
  AutomaskingCache *automasking;

  float stroke_local_mat[4][4];
  float multiplane_scrape_angle;

  float wet_mix_prev_color[4];
  float density_seed;

  rcti previous_r; /* previous redraw rectangle */
  rcti current_r;  /* current redraw rectangle */

  float stroke_distance;    // copy of PaintStroke->stroke_distance
  float stroke_distance_t;  // copy of PaintStroke->stroke_distance_t
  float stroke_spacing_t;
  float last_stroke_distance_t;

  float last_dyntopo_t;
  float last_smooth_t[SCULPT_MAX_SYMMETRY_PASSES];
  float last_rake_t[SCULPT_MAX_SYMMETRY_PASSES];

  int layer_disp_map_size;
  BLI_bitmap *layer_disp_map;

  struct PaintStroke *stroke;
  struct bContext *C;

  struct BrushCommandList *commandlist;
  bool use_plane_trim;

  struct NeighborCache *ncache;
  float speed_avg[SCULPT_SPEED_MA_SIZE];  // moving average for speed
  int speed_avg_cur;
  double last_speed_time;

  // if nonzero, override brush sculpt tool
  int tool_override;

  float mouse_cubic[4][3];
  float world_cubic[4][3];
  float world_cubic_arclength;
  float mouse_cubic_arclength;
  bool has_cubic;
  int stroke_id;
};

/* -------------------------------------------------------------------- */
/** \name Sculpt Expand
 * \{ */
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

struct ExpandCache {
  /* Target data elements that the expand operation will affect. */
  eSculptExpandTargetType target;

  /* Falloff data. */
  eSculptExpandFalloffType falloff_type;

  /* Indexed by vertex index, precalculated falloff value of that vertex (without any falloff
   * editing modification applied). */
  float *vert_falloff;
  /* Max falloff value in *vert_falloff. */
  float max_vert_falloff;

  /* Indexed by base mesh face index, precalculated falloff value of that face. These values are
   * calculated from the per vertex falloff (*vert_falloff) when needed. */
  float *face_falloff;
  float max_face_falloff;

  /* Falloff value of the active element (vertex or base mesh face) that Expand will expand to.
   */
  float active_falloff;

  /* When set to true, expand skips all falloff computations and considers all elements as
   * enabled.
   */
  bool all_enabled;

  /* Initial mouse and cursor data from where the current falloff started. This data can be
   * changed during the execution of Expand by moving the origin. */
  float initial_mouse_move[2];
  float initial_mouse[2];
  PBVHVertRef initial_active_vertex;
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
  float original_mouse_move[2];

  /* Active island checks. */
  /* Indexed by symmetry pass index, contains the connected island ID for that
   * symmetry pass. Other connected island IDs not found in this
   * array will be ignored by Expand. */
  int active_connected_islands[EXPAND_SYMM_AREAS];

  /* Snapping. */
  /* GSet containing all Face Sets IDs that Expand will use to snap the new data. */
  GSet *snap_enabled_face_sets;

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

  /* When true, preserve mode will flip in inverse mode */
  bool preserve_flip_inverse;

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

  /* When set to true, Expand will reposition the sculpt pivot to the boundary of the expand
   * result after finishing the operation. */
  bool reposition_pivot;

  /* If nothing is masked set mask of every vertex to 0. */
  bool auto_mask;

  /* Color target data type related data. */
  float fill_color[4];
  short blend_mode;

  /* Face Sets at the first step of the expand operation, before starting modifying the active
   * vertex and active falloff. These are not the original Face Sets of the sculpt before
   * starting the operator as they could have been modified by Expand when initializing the
   * operator and before starting changing the active vertex. These Face Sets are used for
   * restoring and checking the Face Sets state while the Expand operation modal runs. */
  int *initial_face_sets;

  /* Original data of the sculpt as it was before running the Expand operator. */
  float *original_mask;
  int *original_face_sets;
  float (*original_colors)[4];

  bool check_islands;
  int normal_falloff_blur_steps;
};

struct MaskFilterDeltaStep {
  int totelem;
  int *index;
  float *delta;
};

struct SculptCurvatureData {
  float ks[3];
  float principle[3][3];  // normalized
};

struct SculptFaceSetDrawData {
  struct Sculpt *sd;
  struct Object *ob;
  Span<PBVHNode *> nodes;
  struct Brush *brush;
  float bstrength;

  int faceset;
  int count;
  bool use_fset_curve;
  bool use_fset_strength;

  float *prev_stroke_direction;
  float *stroke_direction;
  float *next_stroke_direction;
  struct CurveMapping *curve;
  int iteration;
};

enum eDynTopoWarnFlag {
  DYNTOPO_WARN_EDATA = (1 << 1),
  DYNTOPO_WARN_MODIFIER = (1 << 3),
  DYNTOPO_ERROR_MULTIRES = (1 << 4)
};
ENUM_OPERATORS(eDynTopoWarnFlag, DYNTOPO_ERROR_MULTIRES);

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
bool SCULPT_poll_view3d(bContext *C);

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

enum PBVHClearFlags {
  PBVH_CLEAR_CACHE_PBVH = 1 << 1,
  PBVH_CLEAR_FREE_BMESH = 1 << 2,
};

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

/* Stroke */

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
 * Gets the normal, location and active vertex location of the geometry under the cursor. This
 * also updates the active vertex and cursor related data of the SculptSession using the mouse
 * position
 */
bool SCULPT_cursor_geometry_info_update(bContext *C,
                                        SculptCursorGeometryInfo *out,
                                        const float mouse[2],
                                        bool use_sampled_normal,
                                        bool use_back_depth);
void SCULPT_geometry_preview_lines_update(bContext *C, struct SculptSession *ss, float radius);

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
bool SCULPT_stroke_is_main_symmetry_pass(StrokeCache *cache);
/**
 * Return true only once per stroke on the first symmetry pass, regardless of the symmetry passes
 * enabled.
 *
 * This should be used for functionality that needs to be computed once per stroke of a
 * particular tool (allocating memory, updating random seeds...).
 */
bool SCULPT_stroke_is_first_brush_step(StrokeCache *cache);
/**
 * Returns true on the first brush step of each symmetry pass.
 */
bool SCULPT_stroke_is_first_brush_step_of_symmetry_pass(StrokeCache *cache);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt mesh accessor API
 * \{ */

/** Ensure random access; required for PBVH_BMESH */
void SCULPT_vertex_random_access_ensure(SculptSession *ss);

/** Ensure random access; required for PBVH_BMESH */
void SCULPT_face_random_access_ensure(SculptSession *ss);

int SCULPT_vertex_valence_get(const SculptSession *ss, PBVHVertRef vertex);
int SCULPT_vertex_count_get(const SculptSession *ss);

const float *SCULPT_vertex_co_get(SculptSession *ss, PBVHVertRef vertex);
void SCULPT_vertex_co_set(SculptSession *ss, PBVHVertRef vertex, const float *co);
void SCULPT_vertex_normal_get(SculptSession *ss, PBVHVertRef vertex, float no[3]);
const float *SCULPT_vertex_origco_get(SculptSession *ss, PBVHVertRef vertex);
void SCULPT_vertex_origno_get(SculptSession *ss, PBVHVertRef vertex, float no[3]);

const float *SCULPT_vertex_persistent_co_get(SculptSession *ss, PBVHVertRef vertex);
void SCULPT_vertex_persistent_normal_get(SculptSession *ss, PBVHVertRef vertex, float no[3]);

float SCULPT_vertex_mask_get(SculptSession *ss, PBVHVertRef vertex);
void SCULPT_vertex_color_get(const SculptSession *ss, PBVHVertRef vertex, float r_color[4]);
void SCULPT_vertex_color_set(SculptSession *ss, PBVHVertRef vertex, const float color[4]);

bool SCULPT_vertex_is_occluded(SculptSession *ss, PBVHVertRef vertex, bool original);

/** Returns true if a color attribute exists in the current sculpt session. */
bool SCULPT_has_colors(const SculptSession *ss);

/** Returns true if the active color attribute is on loop (ATTR_DOMAIN_CORNER) domain. */
bool SCULPT_has_loop_colors(const Object *ob);

bool SCULPT_has_persistent_base(SculptSession *ss);

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

void SCULPT_vertex_neighbors_get(const struct SculptSession *ss,
                                 const PBVHVertRef vref,
                                 const bool include_duplicates,
                                 SculptVertexNeighborIter *iter);

/* Iterator over neighboring vertices. */
#define SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN(ss, v_index, neighbor_iterator) \
  SCULPT_vertex_neighbors_get(ss, v_index, false, &neighbor_iterator); \
  for (neighbor_iterator.i = 0; neighbor_iterator.i < neighbor_iterator.size; \
       neighbor_iterator.i++) { \
    neighbor_iterator.has_edge = neighbor_iterator.neighbors[neighbor_iterator.i].edge.i != \
                                 PBVH_REF_NONE; \
    neighbor_iterator.vertex = neighbor_iterator.neighbors[neighbor_iterator.i].vertex; \
    neighbor_iterator.edge = neighbor_iterator.neighbors[neighbor_iterator.i].edge; \
    neighbor_iterator.index = neighbor_iterator.neighbor_indices[neighbor_iterator.i];

/* Iterate over neighboring and duplicate vertices (for PBVH_GRIDS). Duplicates come
 * first since they are nearest for floodfill. */
#define SCULPT_VERTEX_DUPLICATES_AND_NEIGHBORS_ITER_BEGIN(ss, v_index, neighbor_iterator) \
  SCULPT_vertex_neighbors_get(ss, v_index, true, &neighbor_iterator); \
  for (neighbor_iterator.i = neighbor_iterator.size - 1; neighbor_iterator.i >= 0; \
       neighbor_iterator.i--) \
  { \
    neighbor_iterator.has_edge = neighbor_iterator.neighbors[neighbor_iterator.i].edge.i != \
                                 PBVH_REF_NONE; \
    neighbor_iterator.vertex = neighbor_iterator.neighbors[neighbor_iterator.i].vertex; \
    neighbor_iterator.edge = neighbor_iterator.neighbors[neighbor_iterator.i].edge; \
    neighbor_iterator.index = neighbor_iterator.neighbor_indices[neighbor_iterator.i]; \
    neighbor_iterator.is_duplicate = (neighbor_iterator.i >= \
                                      neighbor_iterator.size - neighbor_iterator.num_duplicates);

#define SCULPT_VERTEX_NEIGHBORS_ITER_END(neighbor_iterator) \
  } \
  if (!neighbor_iterator.no_free && \
      neighbor_iterator.neighbors != neighbor_iterator.neighbors_fixed) { \
    MEM_freeN(neighbor_iterator.neighbors); \
    MEM_freeN(neighbor_iterator.neighbor_indices); \
  } \
  ((void)0)

#define SCULPT_VERTEX_NEIGHBORS_ITER_FREE(neighbor_iterator) \
  if (neighbor_iterator.neighbors && !neighbor_iterator.no_free && \
      neighbor_iterator.neighbors != neighbor_iterator.neighbors_fixed) \
  { \
    MEM_freeN(neighbor_iterator.neighbors); \
    MEM_freeN(neighbor_iterator.neighbor_indices); \
  } \
  ((void)0)

PBVHVertRef SCULPT_active_vertex_get(SculptSession *ss);
const float *SCULPT_active_vertex_co_get(SculptSession *ss);
void SCULPT_active_vertex_normal_get(SculptSession *ss, float normal[3]);

/* Returns PBVH deformed vertices array if shape keys or deform modifiers are used, otherwise
 * returns mesh original vertices array. */
float (*SCULPT_mesh_deformed_positions_get(SculptSession *ss))[3];

/* Fake Neighbors */

#define FAKE_NEIGHBOR_NONE -1

void SCULPT_fake_neighbors_ensure(Sculpt *sd, Object *ob, float max_dist);
void SCULPT_fake_neighbors_enable(Object *ob);
void SCULPT_fake_neighbors_disable(Object *ob);
void SCULPT_fake_neighbors_free(Object *ob);

/* Vertex Info. */
void SCULPT_boundary_info_ensure(Object *object);

/* Update all boundary and valence info in the mesh. */
void SCULPT_update_all_valence_boundary(Object *ob);

/* Boundary Info needs to be initialized in order to use this function. */
eSculptCorner SCULPT_vertex_is_corner(const SculptSession *ss,
                                      const PBVHVertRef index,
                                      eSculptCorner cornertype);

/* Boundary Info needs to be initialized in order to use this function. */
eSculptBoundary SCULPT_vertex_is_boundary(const SculptSession *ss,
                                          const PBVHVertRef index,
                                          eSculptBoundary boundary_types);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Visibility API
 * \{ */

bool SCULPT_vertex_visible_get(SculptSession *ss, PBVHVertRef vertex);
bool SCULPT_vertex_all_faces_visible_get(const SculptSession *ss, PBVHVertRef vertex);
bool SCULPT_vertex_any_face_visible_get(SculptSession *ss, PBVHVertRef vertex);

void SCULPT_face_visibility_all_invert(SculptSession *ss);
void SCULPT_face_visibility_all_set(Object *ob, bool visible);

/* Flags all the vertices of face for boundary update. For PBVH_GRIDS
 * this includes all the verts in all the grids belonging to that face.
 */
void SCULPT_face_mark_boundary_update(SculptSession *ss, PBVHFaceRef face);

void SCULPT_visibility_sync_all_from_faces(Object *ob);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face API
 * \{ */

PBVHEdgeRef sculpt_poly_loop_initial_edge_from_cursor(Object *ob);
BLI_bitmap *sculpt_poly_loop_from_cursor(struct Object *ob);

SculptFaceSetIslands *SCULPT_face_set_islands_get(SculptSession *ss, int fset);
void SCULPT_face_set_islands_free(SculptSession *ss, SculptFaceSetIslands *islands);

SculptFaceSetIsland *SCULPT_face_set_island_get(SculptSession *ss, PBVHFaceRef face, int fset);
void SCULPT_face_set_island_free(SculptFaceSetIsland *island);

void SCULPT_face_normal_get(SculptSession *ss, PBVHFaceRef face, float no[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Sets API
 * \{ */

int SCULPT_active_face_set_get(SculptSession *ss);
int SCULPT_vertex_face_set_get(SculptSession *ss, PBVHVertRef vertex);
void SCULPT_vertex_face_set_set(SculptSession *ss, PBVHVertRef vertex, int face_set);

int SCULPT_face_set_get(const SculptSession *ss, PBVHFaceRef face);
void SCULPT_face_set_set(SculptSession *ss, PBVHFaceRef face, int fset);

bool SCULPT_vertex_has_face_set(SculptSession *ss, PBVHVertRef vertex, int face_set);
bool SCULPT_vertex_has_unique_face_set(const SculptSession *ss, PBVHVertRef vertex);

int SCULPT_face_set_next_available_get(SculptSession *ss);

void SCULPT_face_set_visibility_set(SculptSession *ss, int face_set, bool visible);

int SCULPT_face_set_original_get(SculptSession *ss, PBVHFaceRef face);

bool SCULPT_face_select_get(SculptSession *ss, PBVHFaceRef face);
bool SCULPT_face_is_hidden(const SculptSession *ss, PBVHFaceRef face);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Original Data API
 * \{ */

/**
 * DEPRECATED: use SCULPT_vertex_check_origdata and SCULPT_vertex_get_sculptvert
 * Initialize a #SculptOrigVertData for accessing original vertex data;
 * handles #BMesh, #Mesh, and multi-resolution.
 */
void SCULPT_orig_vert_data_init(SculptOrigVertData *data,
                                Object *ob,
                                PBVHNode *node,
                                SculptUndoType type);
/**
 * DEPRECATED: use SCULPT_vertex_check_origdata and SCULPT_vertex_get_sculptvert
 * Update a #SculptOrigVertData for a particular vertex from the PBVH iterator.
 */
void SCULPT_orig_vert_data_update(SculptSession *ss,
                                  SculptOrigVertData *orig_data,
                                  PBVHVertRef vertex);

/**
 * DEPRECATED: use SCULPT_vertex_check_origdata and SCULPT_vertex_get_sculptvert
 * Initialize a #SculptOrigVertData for accessing original vertex data;
 * handles #BMesh, #Mesh, and multi-resolution.
 */
void SCULPT_orig_vert_data_unode_init(SculptOrigVertData *data,
                                      Object *ob,
                                      struct SculptUndoNode *unode);

void SCULPT_face_check_origdata(SculptSession *ss, PBVHFaceRef face);
bool SCULPT_vertex_check_origdata(SculptSession *ss, PBVHVertRef vertex);

/** check that original face set values are up to date
 * TODO: rename to SCULPT_face_set_original_ensure
 */
void SCULPT_face_ensure_original(SculptSession *ss, struct Object *ob);

/**
 * Initialize a #SculptOrigFaceData for accessing original face data;
 * handles #BMesh, #Mesh, and multi-resolution.
 */
void SCULPT_orig_face_data_init(SculptOrigFaceData *data,
                                Object *ob,
                                PBVHNode *node,
                                SculptUndoType type);
/**
 * Update a #SculptOrigFaceData for a particular vertex from the PBVH iterator.
 */
void SCULPT_orig_face_data_update(SculptOrigFaceData *orig_data, PBVHFaceIter *iter);
/**
 * Initialize a #SculptOrigVertData for accessing original vertex data;
 * handles #BMesh, #Mesh, and multi-resolution.
 */
void SCULPT_orig_face_data_unode_init(SculptOrigFaceData *data, Object *ob, SculptUndoNode *unode);
/** \} */

/* -------------------------------------------------------------------- */
/** \name Brush Utilities.
 * \{ */

BLI_INLINE bool SCULPT_tool_needs_all_pbvh_nodes(const Brush *brush)
{
  if (brush->sculpt_tool == SCULPT_TOOL_ELASTIC_DEFORM) {
    /* Elastic deformations in any brush need all nodes to avoid artifacts as the effect
     * of the Kelvinlet is not constrained by the radius. */
    return true;
  }

  if (brush->sculpt_tool == SCULPT_TOOL_POSE) {
    /* Pose needs all nodes because it applies all symmetry iterations at the same time
     * and the IK chain can grow to any area of the model. */
    /* TODO: This can be optimized by filtering the nodes after calculating the chain. */
    return true;
  }

  if (brush->sculpt_tool == SCULPT_TOOL_BOUNDARY) {
    /* Boundary needs all nodes because it is not possible to know where the boundary
     * deformation is going to be propagated before calculating it. */
    /* TODO: after calculating the boundary info in the first iteration, it should be
     * possible to get the nodes that have vertices included in any boundary deformation
     * and cache them. */
    return true;
  }

  if (brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK &&
      brush->snake_hook_deform_type == BRUSH_SNAKE_HOOK_DEFORM_ELASTIC)
  {
    /* Snake hook in elastic deform type has same requirements as the elastic deform tool. */
    return true;
  }
  return false;
}

void SCULPT_calc_brush_plane(
    Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float r_area_no[3], float r_area_co[3]);

void SCULPT_calc_area_normal(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float r_area_no[3]);
/**
 * This calculates flatten center and area normal together,
 * amortizing the memory bandwidth and loop overhead to calculate both at the same time.
 */
void SCULPT_calc_area_normal_and_center(
    Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float r_area_no[3], float r_area_co[3]);
void SCULPT_calc_area_center(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float r_area_co[3]);

PBVHVertRef SCULPT_nearest_vertex_get(
    Sculpt *sd, Object *ob, const float co[3], float max_distance, bool use_original);

int SCULPT_plane_point_side(const float co[3], const float plane[4]);
int SCULPT_plane_trim(const StrokeCache *cache, const Brush *brush, const float val[3]);
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
 * Initialize a point-in-brush test with a given falloff shape.
 *
 * \param falloff_shape: #PAINT_FALLOFF_SHAPE_SPHERE, #PAINT_FALLOFF_SHAPE_TUBE or
 * #PAINT_FALLOFF_SHAPE_NOOP \return The brush falloff function, or nullptr if falloff_shape was
 * #PAINT_FALLOFF_SHAPE_NOOP
 */

float SCULPT_calc_radius(ViewContext *vc,
                         const Brush *brush,
                         const Scene *scene,
                         const float3 location);

SculptBrushTestFn SCULPT_brush_test_init(const SculptSession *ss, SculptBrushTest *test);
SculptBrushTestFn SCULPT_brush_test_init_ex(const SculptSession *ss,
                                            SculptBrushTest *test,
                                            char falloff_shape,
                                            float tip_roundness,
                                            float tip_scale_x);

bool SCULPT_brush_test_sphere(SculptBrushTest *test, const float co[3]);
bool SCULPT_brush_test_sphere_sq(SculptBrushTest *test, const float co[3]);
bool SCULPT_brush_test_sphere_fast(const SculptBrushTest *test, const float co[3]);
bool SCULPT_brush_test_cube(SculptBrushTest *test,
                            const float co[3],
                            const float local[4][4],
                            const float roundness,
                            const float tip_scale_x,
                            bool test_z);

bool SCULPT_brush_test_circle_sq(SculptBrushTest *test, const float co[3]);
bool SCULPT_brush_test(SculptBrushTest *test, const float co[3]);
/**
 * Test AABB against sphere.
 */
bool SCULPT_search_sphere_cb(PBVHNode *node, void *data_v);
/**
 * 2D projection (distance to line).
 */
bool SCULPT_search_circle_cb(PBVHNode *node, void *data_v);

void SCULPT_combine_transform_proxies(Sculpt *sd, Object *ob);

/**
 * Initialize a point-in-brush test with a given falloff shape.
 *
 * \param falloff_shape: #PAINT_FALLOFF_SHAPE_SPHERE or #PAINT_FALLOFF_SHAPE_TUBE.
 * \return The brush falloff function.
 */

SculptBrushTestFn SCULPT_brush_test_init_with_falloff_shape(const SculptSession *ss,
                                                            SculptBrushTest *test,
                                                            char falloff_shape);
const float *SCULPT_brush_frontface_normal_from_falloff_shape(const SculptSession *ss,
                                                              char falloff_shape);
void SCULPT_cube_tip_init(Sculpt *sd,
                          Object *ob,
                          Brush *brush,
                          float mat[4][4],
                          const float *co = nullptr,  /* Custom brush center. */
                          const float *no = nullptr); /* Custom brush normal. */

/**
 * Return a multiplier for brush strength on a particular vertex.
 */
float SCULPT_brush_strength_factor(SculptSession *ss,
                                   const Brush *br,
                                   const float point[3],
                                   float len,
                                   const float vno[3],
                                   const float fno[3],
                                   float mask,
                                   const PBVHVertRef vertex,
                                   int thread_id,
                                   AutomaskingNodeData *automask_data);

/**
 * Return a color of a brush texture on a particular vertex multiplied by active masks.
 */
void SCULPT_brush_strength_color(SculptSession *ss,
                                 const Brush *brush,
                                 const float brush_point[3],
                                 float len,
                                 const float vno[3],
                                 const float fno[3],
                                 float mask,
                                 const PBVHVertRef vertex,
                                 int thread_id,
                                 AutomaskingNodeData *automask_data,
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
void SCULPT_tilt_apply_to_normal(float r_normal[3], StrokeCache *cache, float tilt_strength);

/**
 * Get effective surface normal with pen tilt and tilt strength applied to it.
 */
void SCULPT_tilt_effective_normal_get(const SculptSession *ss, const Brush *brush, float r_no[3]);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flood Fill
 * \{ */

void SCULPT_floodfill_init(SculptSession *ss, SculptFloodFill *flood);
void SCULPT_floodfill_add_active(
    Sculpt *sd, Object *ob, SculptSession *ss, SculptFloodFill *flood, float radius);
void SCULPT_floodfill_add_initial_with_symmetry(Sculpt *sd,
                                                Object *ob,
                                                SculptSession *ss,
                                                SculptFloodFill *flood,
                                                PBVHVertRef index,
                                                float radius);

void SCULPT_floodfill_add_initial(SculptFloodFill *flood, PBVHVertRef index);
void SCULPT_floodfill_add_and_skip_initial(struct SculptSession *ss,
                                           SculptFloodFill *flood,
                                           PBVHVertRef vertex);
void SCULPT_floodfill_execute(struct SculptSession *ss,
                              SculptFloodFill *flood,
                              bool (*func)(SculptSession *ss,
                                           PBVHVertRef from_v,
                                           PBVHVertRef to_v,
                                           bool is_duplicate,
                                           void *userdata),
                              void *userdata);
void SCULPT_floodfill_free(SculptFloodFill *flood);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Dynamic topology
 * \{ */

/** Enable dynamic topology; mesh will be triangulated */
void SCULPT_dynamic_topology_enable_ex(Main *bmain, Depsgraph *depsgraph, Object *ob);
void SCULPT_dynamic_topology_disable(bContext *C, SculptUndoNode *unode);
void sculpt_dynamic_topology_disable_with_undo(Main *bmain,
                                               Depsgraph *depsgraph,
                                               Scene *scene,
                                               Object *ob);

/**
 * Returns true if the stroke will use dynamic topology, false
 * otherwise.
 *
 * Factors: some brushes like grab cannot do dynamic topology.
 * Others, like smooth, are better without.
 * Same goes for alt-key smoothing.
 */
bool SCULPT_stroke_is_dynamic_topology(const SculptSession *ss,
                                       const Sculpt *sd,
                                       const Brush *brush);

void SCULPT_dynamic_topology_triangulate(struct SculptSession *ss, struct BMesh *bm);

enum eDynTopoWarnFlag SCULPT_dynamic_topology_check(Scene *scene, Object *ob);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Auto-masking.
 * \{ */

struct AutomaskingNodeData {
  PBVHNode *node;
  SculptOrigVertData orig_data;
  bool have_orig_data;
};

/**
 * Call before PBVH vertex iteration.
 * \param automask_data: pointer to an uninitialized #AutomaskingNodeData struct.
 */
void SCULPT_automasking_node_begin(Object *ob,
                                   const SculptSession *ss,
                                   AutomaskingCache *automasking,
                                   AutomaskingNodeData *automask_data,
                                   PBVHNode *node);

/* Call before SCULPT_automasking_factor_get and SCULPT_brush_strength_factor. */
void SCULPT_automasking_node_update(SculptSession *ss,
                                    AutomaskingNodeData *automask_data,
                                    PBVHVertexIter *vd);

float SCULPT_automasking_factor_get(AutomaskingCache *automasking,
                                    SculptSession *ss,
                                    PBVHVertRef vertex,
                                    AutomaskingNodeData *automask_data);

/* Returns the automasking cache depending on the active tool. Used for code that can run both
 * for brushes and filter. */
struct AutomaskingCache *SCULPT_automasking_active_cache_get(SculptSession *ss);

/* Brush can be null. */
struct AutomaskingCache *SCULPT_automasking_cache_init(Sculpt *sd, const Brush *brush, Object *ob);
void SCULPT_automasking_cache_free(SculptSession *ss,
                                   Object *ob,
                                   struct AutomaskingCache *automasking);

bool SCULPT_is_automasking_mode_enabled(const SculptSession *ss,
                                        const Sculpt *sd,
                                        const Brush *br,
                                        const eAutomasking_flag mode);
bool SCULPT_is_automasking_enabled(const Sculpt *sd, const SculptSession *ss, const Brush *br);
void SCULPT_automasking_cache_settings_update(AutomaskingCache *automasking,
                                              SculptSession *ss,
                                              const Sculpt *sd,
                                              const Brush *brush);

bool SCULPT_automasking_needs_normal(const SculptSession *ss,
                                     const Sculpt *sculpt,
                                     const Brush *brush);
bool SCULPT_automasking_needs_original(const Sculpt *sd, const Brush *brush);
int SCULPT_automasking_settings_hash(Object *ob, AutomaskingCache *automasking);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Geodesic distances.
 * \{ */

/**
 * Returns an array indexed by vertex index containing the geodesic distance to the closest
 * vertex in the initial vertex set. The caller is responsible for freeing the array. Geodesic
 * distances will only work when used with PBVH_FACES, for other types of PBVH it will fallback
 * to euclidean distances to one of the initial vertices in the set.
 */
float *SCULPT_geodesic_distances_create(struct Object *ob,
                                        struct GSet *initial_vertices,
                                        const float limit_radius,
                                        PBVHVertRef *r_closest_verts,
                                        float (*vertco_override)[3]);
float *SCULPT_geodesic_from_vertex_and_symm(struct Sculpt *sd,
                                            struct Object *ob,
                                            const PBVHVertRef vertex,
                                            const float limit_radius);
float *SCULPT_geodesic_from_vertex(Object *ob, const PBVHVertRef vertex, const float limit_radius);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Filter API
 * \{ */

void SCULPT_filter_cache_init(bContext *C,
                              Object *ob,
                              Sculpt *sd,
                              int undo_type,
                              const float mval_fl[2],
                              float area_normal_radius,
                              float start_strength);
void SCULPT_filter_cache_free(SculptSession *ss, Object *ob);
void SCULPT_mesh_filter_properties(wmOperatorType *ot);

void SCULPT_mask_filter_smooth_apply(Sculpt *sd,
                                     Object *ob,
                                     Span<PBVHNode *> nodes,
                                     int smooth_iterations);

/* Filter orientation utils. */
void SCULPT_filter_to_orientation_space(float r_v[3], FilterCache *filter_cache);
void SCULPT_filter_to_object_space(float r_v[3], FilterCache *filter_cache);
void SCULPT_filter_zero_disabled_axis_components(float r_v[3], FilterCache *filter_cache);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cloth Simulation.
 * \{ */

/* Main cloth brush function */
void SCULPT_do_cloth_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);

void SCULPT_cloth_simulation_free(SculptClothSimulation *cloth_sim);

/* Public functions. */

struct SculptClothSimulation *SCULPT_cloth_brush_simulation_create(
    Object *ob,
    const float cloth_mass,
    const float cloth_damping,
    const float cloth_softbody_strength,
    const bool use_collisions,
    const bool needs_deform_coords);

void SCULPT_cloth_brush_simulation_init(struct SculptSession *ss,
                                        struct SculptClothSimulation *cloth_sim);

void SCULPT_cloth_sim_activate_nodes(SculptClothSimulation *cloth_sim, Span<PBVHNode *> nodes);

void SCULPT_cloth_brush_store_simulation_state(SculptSession *ss,
                                               SculptClothSimulation *cloth_sim);

void SCULPT_cloth_brush_do_simulation_step(Sculpt *sd,
                                           Object *ob,
                                           SculptClothSimulation *cloth_sim,
                                           Span<PBVHNode *> nodes);

void SCULPT_cloth_brush_ensure_nodes_constraints(Sculpt *sd,
                                                 Object *ob,
                                                 Span<PBVHNode *> nodes,
                                                 SculptClothSimulation *cloth_sim,
                                                 float initial_location[3],
                                                 float radius);

/**
 * Cursor drawing function.
 */
void SCULPT_cloth_simulation_limits_draw(const SculptSession *ss,
                                         const Sculpt *sd,
                                         const uint gpuattr,
                                         const struct Brush *brush,
                                         const float location[3],
                                         const float normal[3],
                                         float rds,
                                         float line_width,
                                         const float outline_col[3],
                                         float alpha);
void SCULPT_cloth_plane_falloff_preview_draw(uint gpuattr,
                                             SculptSession *ss,
                                             const float outline_col[3],
                                             float outline_alpha);

Vector<PBVHNode *> SCULPT_cloth_brush_affected_nodes_gather(SculptSession *ss, Brush *brush);

BLI_INLINE bool SCULPT_is_cloth_deform_brush(const Brush *brush)
{
  return (brush->sculpt_tool == SCULPT_TOOL_CLOTH && ELEM(brush->cloth_deform_type,
                                                          BRUSH_CLOTH_DEFORM_GRAB,
                                                          BRUSH_CLOTH_DEFORM_SNAKE_HOOK)) ||
         /* All brushes that are not the cloth brush deform the simulation using softbody
          * constraints instead of applying forces. */
         (brush->sculpt_tool != SCULPT_TOOL_CLOTH &&
          brush->deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Smoothing API
 * \{ */

/**
 * For bmesh: Average surrounding verts based on an orthogonality measure.
 * Naturally converges to a quad-like structure.
 */
void SCULPT_bmesh_four_neighbor_average(SculptSession *ss,
                                        float avg[3],
                                        float direction[3],
                                        struct BMVert *v,
                                        float projection,
                                        float hard_corner_pin,
                                        int cd_temp,
                                        bool weighted,
                                        bool do_origco,
                                        float factor,
                                        bool reproject_uvs);

void SCULPT_neighbor_coords_average(SculptSession *ss,
                                    float result[3],
                                    PBVHVertRef index,
                                    float projection,
                                    float hard_corner_pin,
                                    bool weighted,
                                    float factor = 1.0f);
float SCULPT_neighbor_mask_average(SculptSession *ss, PBVHVertRef index);
void SCULPT_neighbor_color_average(SculptSession *ss, float result[4], PBVHVertRef index);

/* Mask the mesh boundaries smoothing only the mesh surface without using automasking. */

void SCULPT_neighbor_coords_average_interior(SculptSession *ss,
                                             float result[3],
                                             PBVHVertRef vertex,
                                             float projection,
                                             float hard_corner_pin,
                                             bool use_area_weights,
                                             bool smooth_origco = false,
                                             float factor = 1.0f);

BLI_INLINE eAttrCorrectMode SCULPT_need_reproject(const SculptSession *ss)
{
  return ss->bm ? ss->distort_correction_mode : UNDISTORT_NONE;
}

int SCULPT_vertex_island_get(SculptSession *ss, PBVHVertRef vertex);

/** \} */
void SCULPT_smooth_undo_push(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, Brush *brush);

void SCULPT_smooth(
    Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float bstrength, const bool smooth_mask);
void SCULPT_do_smooth_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);

/* Surface Smooth Brush. */
void SCULPT_surface_smooth_laplacian_init(Object *ob);

void SCULPT_surface_smooth_laplacian_step(SculptSession *ss,
                                          float *disp,
                                          const float co[3],
                                          const PBVHVertRef vertex,
                                          const float origco[3],
                                          const float alpha,
                                          bool use_area_weights);

void SCULPT_surface_smooth_displace_step(
    SculptSession *ss, float *co, PBVHVertRef vertex, float beta, float fade);
void SCULPT_do_surface_smooth_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);

/* Slide/Relax */
void SCULPT_relax_vertex(SculptSession *ss,
                         PBVHVertexIter *vd,
                         float factor,
                         eSculptBoundary boundary_mask,
                         float *r_final_pos);

/** \} */

/**
 * Expose 'calc_area_normal' externally (just for vertex paint).
 */
bool SCULPT_pbvh_calc_area_normal(const Brush *brush,
                                  Object *ob,
                                  Span<PBVHNode *> nodes,
                                  bool use_threading,
                                  float r_area_no[3]);

/**
 * Flip all the edit-data across the axis/axes specified by \a symm.
 * Used to calculate multiple modifications to the mesh when symmetry is enabled.
 */
void SCULPT_cache_calc_brushdata_symm(StrokeCache *cache,
                                      ePaintSymmetryFlags symm,
                                      const char axis,
                                      const float angle);
void SCULPT_cache_free(StrokeCache *cache);

/* -------------------------------------------------------------------- */
/** \name Sculpt Undo
 * \{ */

SculptUndoNode *SCULPT_undo_push_node(Object *ob, PBVHNode *node, SculptUndoType type);
SculptUndoNode *SCULPT_undo_get_node(PBVHNode *node, SculptUndoType type);
SculptUndoNode *SCULPT_undo_get_first_node();

/**
 * Pushes an undo step using the operator name. This is necessary for
 * redo panels to work; operators that do not support that may use
 * #SCULPT_undo_push_begin_ex instead if so desired.
 */
void SCULPT_undo_push_begin(Object *ob, const wmOperator *op);

/**
 * NOTE: #SCULPT_undo_push_begin is preferred since `name`
 * must match operator name for redo panels to work.
 */
void SCULPT_undo_push_begin_ex(Object *ob, const char *name);
void SCULPT_undo_push_end(Object *ob);
void SCULPT_undo_push_end_ex(Object *ob, const bool use_nested_undo);

/** \} */

void SCULPT_vertcos_to_key(Object *ob, KeyBlock *kb, const float (*vertCos)[3]);

/**
 * Copy the PBVH bounding box into the object's bounding box.
 */
void SCULPT_update_object_bounding_box(Object *ob);

/**
 * Get a screen-space rectangle of the modified area.
 */
bool SCULPT_get_redraw_rect(ARegion *region, RegionView3D *rv3d, Object *ob, rcti *rect);

/* Operators. */

/* -------------------------------------------------------------------- */
/** \name Expand Operator
 * \{ */

void SCULPT_OT_expand(wmOperatorType *ot);
void sculpt_expand_modal_keymap(wmKeyConfig *keyconf);
/** \} */

/* -------------------------------------------------------------------- */
/** \name Gesture Operators
 * \{ */

void SCULPT_OT_face_set_lasso_gesture(wmOperatorType *ot);
void SCULPT_OT_face_set_box_gesture(wmOperatorType *ot);

void SCULPT_OT_trim_lasso_gesture(wmOperatorType *ot);
void SCULPT_OT_trim_box_gesture(wmOperatorType *ot);

void SCULPT_OT_project_line_gesture(struct wmOperatorType *ot);

void SCULPT_OT_face_set_by_topology(struct wmOperatorType *ot);

void SCULPT_OT_project_line_gesture(wmOperatorType *ot);
/** \} */

/* -------------------------------------------------------------------- */
/** \name Face Set Operators
 * \{ */

void SCULPT_OT_face_sets_randomize_colors(wmOperatorType *ot);
void SCULPT_OT_face_set_change_visibility(wmOperatorType *ot);
void SCULPT_OT_face_sets_invert_visibility(wmOperatorType *ot);
void SCULPT_OT_face_sets_init(wmOperatorType *ot);
void SCULPT_OT_face_sets_create(wmOperatorType *ot);
void SCULPT_OT_face_sets_edit(wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Operators
 * \{ */

void SCULPT_OT_set_pivot_position(wmOperatorType *ot);
/** \} */

/* -------------------------------------------------------------------- */
/** \name Filter Operators
 * \{ */

/* Mesh Filter. */

void SCULPT_OT_mesh_filter(wmOperatorType *ot);
wmKeyMap *filter_mesh_modal_keymap(wmKeyConfig *keyconf);

/* Cloth Filter. */

void SCULPT_OT_cloth_filter(wmOperatorType *ot);

/* Color Filter. */

void SCULPT_OT_color_filter(wmOperatorType *ot);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mask Operators
 * \{ */

/* Mask filter and Dirty Mask. */

void SCULPT_OT_mask_filter(wmOperatorType *ot);

/* Mask and Face Sets Expand. */

void SCULPT_OT_mask_expand(wmOperatorType *ot);

/* Mask Init. */

void SCULPT_OT_mask_init(wmOperatorType *ot);
void SCULPT_OT_ipmask_filter(wmOperatorType *ot);

/** \} */

/* Detail size. */

/* -------------------------------------------------------------------- */
/** \name Dyntopo/Retopology Operators
 * \{ */

void SCULPT_OT_detail_flood_fill(wmOperatorType *ot);
void SCULPT_OT_sample_detail_size(wmOperatorType *ot);
void SCULPT_OT_set_detail_size(wmOperatorType *ot);
void SCULPT_OT_dyntopo_detail_size_edit(wmOperatorType *ot);
void SCULPT_apply_dyntopo_settings(Scene *scene, SculptSession *ss, Sculpt *sculpt, Brush *brush);
/** \} */

/* Dyntopo. */

void SCULPT_OT_dynamic_topology_toggle(wmOperatorType *ot);

/* sculpt_brush_types.cc */

/* -------------------------------------------------------------------- */
/** \name Brushes
 * \{ */

/* Pose Brush. */

/**
 * Main Brush Function.
 */
void SCULPT_do_pose_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
/**
 * Calculate the pose origin and (Optionally the pose factor)
 * that is used when using the pose brush.
 *
 * \param r_pose_origin: Must be a valid pointer.
 * \param r_pose_factor: Optional, when set to NULL it won't be calculated.
 */
void SCULPT_pose_calc_pose_data(Sculpt *sd,
                                Object *ob,
                                SculptSession *ss,
                                float initial_location[3],
                                float radius,
                                float pose_offset,
                                float *r_pose_origin,
                                float *r_pose_factor);
void SCULPT_pose_brush_init(Sculpt *sd, Object *ob, SculptSession *ss, Brush *br);
SculptPoseIKChain *SCULPT_pose_ik_chain_init(Sculpt *sd,
                                             Object *ob,
                                             SculptSession *ss,
                                             Brush *br,
                                             const float initial_location[3],
                                             float radius);
void SCULPT_pose_ik_chain_free(SculptPoseIKChain *ik_chain);

/* Boundary Brush. */

/**
 * Main function to get #SculptBoundary data both for brush deformation and viewport preview.
 * Can return NULL if there is no boundary from the given vertex using the given radius.
 */

struct SculptBoundary *SCULPT_boundary_data_init(struct Sculpt *sd,
                                                 Object *object,
                                                 Brush *brush,
                                                 const PBVHVertRef initial_vertex,
                                                 const float radius);
void SCULPT_boundary_data_free(struct SculptBoundary *boundary);

/* Main Brush Function. */
void SCULPT_do_boundary_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);

void SCULPT_boundary_edges_preview_draw(uint gpuattr,
                                        SculptSession *ss,
                                        const float outline_col[3],
                                        float outline_alpha);
void SCULPT_boundary_pivot_line_preview_draw(uint gpuattr, SculptSession *ss);

/* Multi-plane Scrape Brush. */
/* Main Brush Function. */
void SCULPT_do_multiplane_scrape_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_multiplane_scrape_preview_draw(uint gpuattr,
                                           Brush *brush,
                                           SculptSession *ss,
                                           const float outline_col[3],
                                           float outline_alpha);
/* Draw Face Sets Brush. */
void SCULPT_do_draw_face_sets_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);

/* Paint Brush. */
void SCULPT_do_paint_brush(PaintModeSettings *paint_mode_settings,
                           Scene *scene,
                           Sculpt *sd,
                           Object *ob,
                           Span<PBVHNode *> nodes,
                           Span<PBVHNode *> texnodes);

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
                                 Span<PBVHNode *> texnodes);
bool SCULPT_use_image_paint_brush(PaintModeSettings *settings, Object *ob) ATTR_NONNULL();

/* Smear Brush. */
void SCULPT_do_smear_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);

float SCULPT_clay_thumb_get_stabilized_pressure(StrokeCache *cache);

void SCULPT_do_draw_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_fill_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_scrape_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_clay_thumb_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_flatten_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_clay_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_clay_strips_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_snake_hook_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_thumb_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_rotate_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_layer_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_inflate_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_nudge_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_crease_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_pinch_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_grab_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_elastic_deform_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_draw_sharp_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_slide_relax_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);

void SCULPT_do_displacement_smear_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_displacement_eraser_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_mask_brush_draw(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
void SCULPT_do_mask_brush(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes);
/** \} */

void SCULPT_bmesh_topology_rake(Sculpt *sd, Object *ob, Span<PBVHNode *> nodes, float bstrength);

/* end sculpt_brush_types.cc */

/* sculpt_ops.cc */

void SCULPT_OT_brush_stroke(wmOperatorType *ot);

/* end sculpt_ops.cc */

eSculptBoundary SCULPT_edge_is_boundary(const SculptSession *ss,
                                        const PBVHEdgeRef edge,
                                        eSculptBoundary typemask);
void SCULPT_edge_get_verts(const SculptSession *ss,
                           const PBVHEdgeRef edge,
                           PBVHVertRef *r_v1,
                           PBVHVertRef *r_v2);
PBVHVertRef SCULPT_edge_other_vertex(const SculptSession *ss,
                                     const PBVHEdgeRef edge,
                                     const PBVHVertRef vertex);

// #define SCULPT_REPLAY
#ifdef SCULPT_REPLAY
struct SculptReplayLog;
struct SculptBrushSample;

#  ifdef WIN32
#    define REPLAY_EXPORT __declspec(dllexport)
#  else
#    define REPLAY_EXPORT
#  endif

void SCULPT_replay_log_free(struct SculptReplayLog *log);
struct SculptReplayLog *SCULPT_replay_log_create();
void SCULPT_replay_log_end();
void SCULPT_replay_log_start();
char *SCULPT_replay_serialize();
void SCULPT_replay_log_append(struct Sculpt *sd, struct SculptSession *ss, struct Object *ob);
void SCULPT_replay_test(void);

#endif

void SCULPT_undo_ensure_bmlog(struct Object *ob);
bool SCULPT_ensure_dyntopo_node_undo(struct Object *ob,
                                     struct PBVHNode *node,
                                     SculptUndoType type,
                                     int extraType,
                                     SculptUndoType force_push_mask = SCULPT_UNDO_COORDS |
                                                                      SCULPT_UNDO_COLOR |
                                                                      SCULPT_UNDO_MASK);

struct BMesh *SCULPT_dyntopo_empty_bmesh();

/* initializes customdata layer used by SCULPT_neighbor_coords_average_interior when bound_smooth
 * > 0.0f*/
void SCULPT_bound_smooth_ensure(SculptSession *ss, struct Object *ob);

/** \} */

int SCULPT_get_tool(const SculptSession *ss, const struct Brush *br);

/* Sculpt API to get brush channel data
If ss->cache exists then ss->cache->channels_final
will be used, otherwise brush and tool settings channels
will be used (taking inheritence into account).
*/

/* -------------------------------------------------------------------- */
/** \name Brush channel accessor API
 * \{ */

/** Get brush channel value.  The channel will be
    fetched from ss->cache->channels_final.  If
    ss->cache is NULL, channel will be fetched
    from sd->channels and br->channels taking
    inheritance flags into account.

    Note that sd or br may be NULL, but not
    both.*/
float SCULPT_get_float_intern(const SculptSession *ss,
                              const char *idname,
                              const Sculpt *sd,
                              const Brush *br);
#define SCULPT_get_float(ss, idname, sd, br) \
  SCULPT_get_float_intern(ss, BRUSH_BUILTIN_##idname, sd, br)

int SCULPT_get_int_intern(const SculptSession *ss,
                          const char *idname,
                          const Sculpt *sd,
                          const Brush *br);
#define SCULPT_get_int(ss, idname, sd, br) \
  SCULPT_get_int_intern(ss, BRUSH_BUILTIN_##idname, sd, br)
#define SCULPT_get_bool(ss, idname, sd, br) SCULPT_get_int(ss, idname, sd, br)

int SCULPT_get_vector_intern(
    const SculptSession *ss, const char *idname, float out[4], const Sculpt *sd, const Brush *br);
#define SCULPT_get_vector(ss, idname, out, sd, br) \
  SCULPT_get_vector_intern(ss, BRUSH_BUILTIN_##idname, out, sd, br)

/** \} */

float SCULPT_calc_concavity(SculptSession *ss, PBVHVertRef vref);

/*
If useAccurateSolver is false, a faster but less accurate
power solver will be used.  If true then BLI_eigen_solve_selfadjoint_m3
will be called.

Must call BKE_sculpt_ensure_curvature_dir to ensure ss->attrs.curvature_dir exists.
*/
bool SCULPT_calc_principle_curvatures(SculptSession *ss,
                                      PBVHVertRef vertex,
                                      SculptCurvatureData *out,
                                      bool useAccurateSolver);

void SCULPT_curvature_begin(SculptSession *ss, struct PBVHNode *node, bool useAccurateSolver);
void SCULPT_curvature_dir_get(SculptSession *ss,
                              PBVHVertRef v,
                              float dir[3],
                              bool useAccurateSolver);

/* -------------------------------------------------------------------- */
/** \name Cotangent API
 * \{ */

void SCULPT_ensure_persistent_layers(SculptSession *ss, struct Object *ob);
void SCULPT_ensure_epmap(SculptSession *ss);
void SCULPT_ensure_vemap(SculptSession *ss);
bool SCULPT_dyntopo_automasking_init(const SculptSession *ss,
                                     Sculpt *sd,
                                     const Brush *br,
                                     Object *ob,
                                     blender::bke::dyntopo::DyntopoMaskCB *r_mask_cb,
                                     void **r_mask_cb_data);
void SCULPT_dyntopo_automasking_end(void *mask_data);

#define SCULPT_LAYER_PERS_CO "Persistent Base Co"
#define SCULPT_LAYER_PERS_NO "Persistent Base No"
#define SCULPT_LAYER_PERS_DISP "Persistent Base Height"
#define SCULPT_LAYER_DISP "__temp_layer_disp"

// these tools don't support dynamic pbvh splitting during the stroke
#define DYNTOPO_HAS_DYNAMIC_SPLIT(tool) true

#define SCULPT_stroke_needs_original(brush) \
  ELEM(brush->sculpt_tool, \
       SCULPT_TOOL_DRAW_SHARP, \
       SCULPT_TOOL_GRAB, \
       SCULPT_TOOL_ROTATE, \
       SCULPT_TOOL_THUMB, \
       SCULPT_TOOL_ELASTIC_DEFORM, \
       SCULPT_TOOL_BOUNDARY, \
       SCULPT_TOOL_POSE)

// exponent to make boundary_smooth_factor more user-friendly
#define BOUNDARY_SMOOTH_EXP 2.0

bool SCULPT_needs_area_normal(SculptSession *ss, Sculpt *sd, Brush *brush);
BLI_INLINE bool SCULPT_tool_is_paint(int tool)
{
  return ELEM(tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR);
}

BLI_INLINE bool SCULPT_tool_is_mask(int tool)
{
  return ELEM(tool, SCULPT_TOOL_MASK);
}

BLI_INLINE bool SCULPT_tool_is_face_sets(int tool)
{
  return ELEM(tool, SCULPT_TOOL_DRAW_FACE_SETS);
}

void SCULPT_stroke_id_ensure(Object *ob);
void SCULPT_stroke_id_next(Object *ob);
bool SCULPT_tool_can_reuse_automask(int sculpt_tool);

void SCULPT_ensure_valid_pivot(const Object *ob, Scene *scene);

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
int SCULPT_vertex_island_get(SculptSession *ss, PBVHVertRef vertex);

/** \} */

int SCULPT_get_symmetry_pass(const struct SculptSession *ss);

#define SCULPT_boundary_flag_update BKE_sculpt_boundary_flag_update

/* Some tools need original coordinates to be smoothed during
 * autosmooth.
 */
#define SCULPT_tool_needs_smooth_origco(tool) ELEM(tool, SCULPT_TOOL_DRAW_SHARP)
