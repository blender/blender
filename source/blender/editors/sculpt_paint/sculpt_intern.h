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
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "DNA_brush_types.h"
#include "DNA_key_types.h"
#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_vec_types.h"

#include "BLI_bitmap.h"
#include "BLI_gsqueue.h"
#include "BLI_threads.h"

#include "BKE_paint.h"
#include "BKE_pbvh.h"

struct AutomaskingCache;
struct KeyBlock;
struct Object;
struct SculptUndoNode;
struct bContext;

enum ePaintSymmetryFlags;

bool SCULPT_mode_poll(struct bContext *C);
bool SCULPT_mode_poll_view3d(struct bContext *C);
/* checks for a brush, not just sculpt mode */
bool SCULPT_poll(struct bContext *C);
bool SCULPT_poll_view3d(struct bContext *C);

bool SCULPT_vertex_colors_poll(struct bContext *C);

/* Updates */

typedef enum SculptUpdateType {
  SCULPT_UPDATE_COORDS = 1 << 0,
  SCULPT_UPDATE_MASK = 1 << 1,
  SCULPT_UPDATE_VISIBILITY = 1 << 2,
  SCULPT_UPDATE_COLOR = 1 << 3,
} SculptUpdateType;

void SCULPT_flush_update_step(bContext *C, SculptUpdateType update_flags);
void SCULPT_flush_update_done(const bContext *C, Object *ob, SculptUpdateType update_flags);
void SCULPT_flush_stroke_deform(struct Sculpt *sd, Object *ob, bool is_proxy_used);

/* Should be used after modifying the mask or Face Sets IDs. */
void SCULPT_tag_update_overlays(bContext *C);

/* Stroke */

typedef struct SculptCursorGeometryInfo {
  float location[3];
  float normal[3];
  float active_vertex_co[3];
} SculptCursorGeometryInfo;

bool SCULPT_stroke_get_location(struct bContext *C, float out[3], const float mouse[2]);
bool SCULPT_cursor_geometry_info_update(bContext *C,
                                        SculptCursorGeometryInfo *out,
                                        const float mouse[2],
                                        bool use_sampled_normal);
void SCULPT_geometry_preview_lines_update(bContext *C, struct SculptSession *ss, float radius);

void SCULPT_stroke_modifiers_check(const bContext *C, Object *ob, const Brush *brush);
float SCULPT_raycast_init(struct ViewContext *vc,
                          const float mouse[2],
                          float ray_start[3],
                          float ray_end[3],
                          float ray_normal[3],
                          bool original);

/* Symmetry */
char SCULPT_mesh_symmetry_xyz_get(Object *object);

/* Sculpt PBVH abstraction API */
void SCULPT_vertex_random_access_ensure(struct SculptSession *ss);

int SCULPT_vertex_count_get(struct SculptSession *ss);
const float *SCULPT_vertex_co_get(struct SculptSession *ss, int index);
void SCULPT_vertex_normal_get(SculptSession *ss, int index, float no[3]);
float SCULPT_vertex_mask_get(struct SculptSession *ss, int index);
const float *SCULPT_vertex_color_get(SculptSession *ss, int index);

const float *SCULPT_vertex_persistent_co_get(SculptSession *ss, int index);
void SCULPT_vertex_persistent_normal_get(SculptSession *ss, int index, float no[3]);

/* Coordinates used for manipulating the base mesh when Grab Active Vertex is enabled. */
const float *SCULPT_vertex_co_for_grab_active_get(SculptSession *ss, int index);

/* Returns the info of the limit surface when Multires is available, otherwise it returns the
 * current coordinate of the vertex. */
void SCULPT_vertex_limit_surface_get(SculptSession *ss, int index, float r_co[3]);

/* Returns the pointer to the coordinates that should be edited from a brush tool iterator
 * depending on the given deformation target. */
float *SCULPT_brush_deform_target_vertex_co_get(SculptSession *ss,
                                                const int deform_target,
                                                PBVHVertexIter *iter);

#define SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY 256
typedef struct SculptVertexNeighborIter {
  /* Storage */
  int *neighbors;
  int size;
  int capacity;
  int neighbors_fixed[SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY];

  /* Internal iterator. */
  int num_duplicates;
  int i;

  /* Public */
  int index;
  bool is_duplicate;
} SculptVertexNeighborIter;

void SCULPT_vertex_neighbors_get(struct SculptSession *ss,
                                 const int index,
                                 const bool include_duplicates,
                                 SculptVertexNeighborIter *iter);

/* Iterator over neighboring vertices. */
#define SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN(ss, v_index, neighbor_iterator) \
  SCULPT_vertex_neighbors_get(ss, v_index, false, &neighbor_iterator); \
  for (neighbor_iterator.i = 0; neighbor_iterator.i < neighbor_iterator.size; \
       neighbor_iterator.i++) { \
    neighbor_iterator.index = neighbor_iterator.neighbors[neighbor_iterator.i];

/* Iterate over neighboring and duplicate vertices (for PBVH_GRIDS). Duplicates come
 * first since they are nearest for floodfill. */
#define SCULPT_VERTEX_DUPLICATES_AND_NEIGHBORS_ITER_BEGIN(ss, v_index, neighbor_iterator) \
  SCULPT_vertex_neighbors_get(ss, v_index, true, &neighbor_iterator); \
  for (neighbor_iterator.i = neighbor_iterator.size - 1; neighbor_iterator.i >= 0; \
       neighbor_iterator.i--) { \
    neighbor_iterator.index = neighbor_iterator.neighbors[neighbor_iterator.i]; \
    neighbor_iterator.is_duplicate = (neighbor_iterator.i >= \
                                      neighbor_iterator.size - neighbor_iterator.num_duplicates);

#define SCULPT_VERTEX_NEIGHBORS_ITER_END(neighbor_iterator) \
  } \
  if (neighbor_iterator.neighbors != neighbor_iterator.neighbors_fixed) { \
    MEM_freeN(neighbor_iterator.neighbors); \
  } \
  ((void)0)

int SCULPT_active_vertex_get(SculptSession *ss);
const float *SCULPT_active_vertex_co_get(SculptSession *ss);
void SCULPT_active_vertex_normal_get(SculptSession *ss, float normal[3]);

/* Returns PBVH deformed vertices array if shape keys or deform modifiers are used, otherwise
 * returns mesh original vertices array. */
struct MVert *SCULPT_mesh_deformed_mverts_get(SculptSession *ss);

/* Fake Neighbors */

#define FAKE_NEIGHBOR_NONE -1

void SCULPT_fake_neighbors_ensure(struct Sculpt *sd, Object *ob, const float max_dist);
void SCULPT_fake_neighbors_enable(Object *ob);
void SCULPT_fake_neighbors_disable(Object *ob);
void SCULPT_fake_neighbors_free(struct Object *ob);

/* Vertex Info. */
void SCULPT_boundary_info_ensure(Object *object);
/* Boundary Info needs to be initialized in order to use this function. */
bool SCULPT_vertex_is_boundary(const SculptSession *ss, const int index);

void SCULPT_connected_components_ensure(Object *ob);

/* Sculpt Visibility API */

void SCULPT_vertex_visible_set(SculptSession *ss, int index, bool visible);
bool SCULPT_vertex_visible_get(SculptSession *ss, int index);

void SCULPT_visibility_sync_all_face_sets_to_vertices(struct Object *ob);
void SCULPT_visibility_sync_all_vertex_to_face_sets(struct SculptSession *ss);

/* Face Sets API */

int SCULPT_active_face_set_get(SculptSession *ss);
int SCULPT_vertex_face_set_get(SculptSession *ss, int index);
void SCULPT_vertex_face_set_set(SculptSession *ss, int index, int face_set);

bool SCULPT_vertex_has_face_set(SculptSession *ss, int index, int face_set);
bool SCULPT_vertex_has_unique_face_set(SculptSession *ss, int index);

int SCULPT_face_set_next_available_get(SculptSession *ss);

void SCULPT_face_set_visibility_set(SculptSession *ss, int face_set, bool visible);
bool SCULPT_vertex_all_face_sets_visible_get(const SculptSession *ss, int index);
bool SCULPT_vertex_any_face_set_visible_get(SculptSession *ss, int index);

void SCULPT_face_sets_visibility_invert(SculptSession *ss);
void SCULPT_face_sets_visibility_all_set(SculptSession *ss, bool visible);

bool SCULPT_stroke_is_main_symmetry_pass(struct StrokeCache *cache);
bool SCULPT_stroke_is_first_brush_step(struct StrokeCache *cache);
bool SCULPT_stroke_is_first_brush_step_of_symmetry_pass(struct StrokeCache *cache);

/* Sculpt Original Data */
typedef struct {
  struct BMLog *bm_log;

  struct SculptUndoNode *unode;
  float (*coords)[3];
  short (*normals)[3];
  const float *vmasks;
  float (*colors)[4];

  /* Original coordinate, normal, and mask. */
  const float *co;
  const short *no;
  float mask;
  const float *col;
} SculptOrigVertData;

void SCULPT_orig_vert_data_init(SculptOrigVertData *data, Object *ob, PBVHNode *node);
void SCULPT_orig_vert_data_update(SculptOrigVertData *orig_data, PBVHVertexIter *iter);
void SCULPT_orig_vert_data_unode_init(SculptOrigVertData *data,
                                      Object *ob,
                                      struct SculptUndoNode *unode);

/* Utils. */
void SCULPT_calc_brush_plane(struct Sculpt *sd,
                             struct Object *ob,
                             struct PBVHNode **nodes,
                             int totnode,
                             float r_area_no[3],
                             float r_area_co[3]);

void SCULPT_calc_area_normal(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3]);

int SCULPT_nearest_vertex_get(struct Sculpt *sd,
                              struct Object *ob,
                              const float co[3],
                              float max_distance,
                              bool use_original);

int SCULPT_plane_point_side(const float co[3], const float plane[4]);
int SCULPT_plane_trim(const struct StrokeCache *cache,
                      const struct Brush *brush,
                      const float val[3]);
void SCULPT_clip(Sculpt *sd, SculptSession *ss, float co[3], const float val[3]);

float SCULPT_brush_plane_offset_get(Sculpt *sd, SculptSession *ss);

ePaintSymmetryAreas SCULPT_get_vertex_symm_area(const float co[3]);
bool SCULPT_check_vertex_pivot_symmetry(const float vco[3], const float pco[3], const char symm);
bool SCULPT_is_vertex_inside_brush_radius_symm(const float vertex[3],
                                               const float br_co[3],
                                               float radius,
                                               char symm);
bool SCULPT_is_symmetry_iteration_valid(char i, char symm);
void SCULPT_flip_v3_by_symm_area(float v[3],
                                 const ePaintSymmetryFlags symm,
                                 const ePaintSymmetryAreas symmarea,
                                 const float pivot[3]);
void SCULPT_flip_quat_by_symm_area(float quat[3],
                                   const ePaintSymmetryFlags symm,
                                   const ePaintSymmetryAreas symmarea,
                                   const float pivot[3]);

/* Flood Fill. */
typedef struct {
  GSQueue *queue;
  BLI_bitmap *visited_vertices;
} SculptFloodFill;

void SCULPT_floodfill_init(struct SculptSession *ss, SculptFloodFill *flood);
void SCULPT_floodfill_add_active(struct Sculpt *sd,
                                 struct Object *ob,
                                 struct SculptSession *ss,
                                 SculptFloodFill *flood,
                                 float radius);
void SCULPT_floodfill_add_initial_with_symmetry(struct Sculpt *sd,
                                                struct Object *ob,
                                                struct SculptSession *ss,
                                                SculptFloodFill *flood,
                                                int index,
                                                float radius);
void SCULPT_floodfill_add_initial(SculptFloodFill *flood, int index);
void SCULPT_floodfill_add_and_skip_initial(SculptFloodFill *flood, int index);
void SCULPT_floodfill_execute(
    struct SculptSession *ss,
    SculptFloodFill *flood,
    bool (*func)(SculptSession *ss, int from_v, int to_v, bool is_duplicate, void *userdata),
    void *userdata);
void SCULPT_floodfill_free(SculptFloodFill *flood);

/* Dynamic topology */

enum eDynTopoWarnFlag {
  DYNTOPO_WARN_VDATA = (1 << 0),
  DYNTOPO_WARN_EDATA = (1 << 1),
  DYNTOPO_WARN_LDATA = (1 << 2),
  DYNTOPO_WARN_MODIFIER = (1 << 3),
};

void SCULPT_dynamic_topology_enable_ex(struct Main *bmain,
                                       struct Depsgraph *depsgraph,
                                       Scene *scene,
                                       Object *ob);
void SCULPT_dynamic_topology_disable(bContext *C, struct SculptUndoNode *unode);
void sculpt_dynamic_topology_disable_with_undo(struct Main *bmain,
                                               struct Depsgraph *depsgraph,
                                               Scene *scene,
                                               Object *ob);

bool SCULPT_stroke_is_dynamic_topology(const SculptSession *ss, const Brush *brush);

void SCULPT_dynamic_topology_triangulate(struct BMesh *bm);
void SCULPT_dyntopo_node_layers_add(struct SculptSession *ss);

enum eDynTopoWarnFlag SCULPT_dynamic_topology_check(Scene *scene, Object *ob);

void SCULPT_pbvh_clear(Object *ob);

/* Auto-masking. */
float SCULPT_automasking_factor_get(struct AutomaskingCache *automasking,
                                    SculptSession *ss,
                                    int vert);

/* Returns the automasking cache depending on the active tool. Used for code that can run both for
 * brushes and filter. */
struct AutomaskingCache *SCULPT_automasking_active_cache_get(SculptSession *ss);

struct AutomaskingCache *SCULPT_automasking_cache_init(Sculpt *sd, Brush *brush, Object *ob);
void SCULPT_automasking_cache_free(struct AutomaskingCache *automasking);

bool SCULPT_is_automasking_mode_enabled(const Sculpt *sd,
                                        const Brush *br,
                                        const eAutomasking_flag mode);
bool SCULPT_is_automasking_enabled(const Sculpt *sd, const SculptSession *ss, const Brush *br);

typedef enum eBoundaryAutomaskMode {
  AUTOMASK_INIT_BOUNDARY_EDGES = 1,
  AUTOMASK_INIT_BOUNDARY_FACE_SETS = 2,
} eBoundaryAutomaskMode;
float *SCULPT_boundary_automasking_init(Object *ob,
                                        eBoundaryAutomaskMode mode,
                                        int propagation_steps,
                                        float *automask_factor);

/* Geodesic distances. */

/* Returns an array indexed by vertex index containing the geodesic distance to the closest vertex
in the initial vertex set. The caller is responsible for freeing the array.
Geodesic distances will only work when used with PBVH_FACES, for other types of PBVH it will
fallback to euclidean distances to one of the initial vertices in the set. */
float *SCULPT_geodesic_distances_create(struct Object *ob,
                                        struct GSet *initial_vertices,
                                        const float limit_radius);
float *SCULPT_geodesic_from_vertex_and_symm(struct Sculpt *sd,
                                            struct Object *ob,
                                            const int vertex,
                                            const float limit_radius);
float *SCULPT_geodesic_from_vertex(Object *ob, const int vertex, const float limit_radius);

/* Filters. */
void SCULPT_filter_cache_init(struct bContext *C, Object *ob, Sculpt *sd, const int undo_type);
void SCULPT_filter_cache_free(SculptSession *ss);

void SCULPT_mask_filter_smooth_apply(
    Sculpt *sd, Object *ob, PBVHNode **nodes, const int totnode, const int smooth_iterations);

/* Brushes. */

/* Cloth Brush. */
void SCULPT_do_cloth_brush(struct Sculpt *sd,
                           struct Object *ob,
                           struct PBVHNode **nodes,
                           int totnode);

void SCULPT_cloth_simulation_free(struct SculptClothSimulation *cloth_sim);

struct SculptClothSimulation *SCULPT_cloth_brush_simulation_create(
    struct SculptSession *ss,
    const float cloth_mass,
    const float cloth_damping,
    const float cloth_softbody_strength,
    const bool use_collisions,
    const bool needs_deform_coords);
void SCULPT_cloth_brush_simulation_init(struct SculptSession *ss,
                                        struct SculptClothSimulation *cloth_sim);

void SCULPT_cloth_sim_activate_nodes(struct SculptClothSimulation *cloth_sim,
                                     PBVHNode **nodes,
                                     int totnode);

void SCULPT_cloth_brush_store_simulation_state(struct SculptSession *ss,
                                               struct SculptClothSimulation *cloth_sim);

void SCULPT_cloth_brush_do_simulation_step(struct Sculpt *sd,
                                           struct Object *ob,
                                           struct SculptClothSimulation *cloth_sim,
                                           struct PBVHNode **nodes,
                                           int totnode);

void SCULPT_cloth_brush_ensure_nodes_constraints(struct Sculpt *sd,
                                                 struct Object *ob,
                                                 struct PBVHNode **nodes,
                                                 int totnode,
                                                 struct SculptClothSimulation *cloth_sim,
                                                 float initial_location[3],
                                                 const float radius);

void SCULPT_cloth_simulation_limits_draw(const uint gpuattr,
                                         const struct Brush *brush,
                                         const float location[3],
                                         const float normal[3],
                                         const float rds,
                                         const float line_width,
                                         const float outline_col[3],
                                         const float alpha);
void SCULPT_cloth_plane_falloff_preview_draw(const uint gpuattr,
                                             struct SculptSession *ss,
                                             const float outline_col[3],
                                             float outline_alpha);

PBVHNode **SCULPT_cloth_brush_affected_nodes_gather(SculptSession *ss,
                                                    Brush *brush,
                                                    int *r_totnode);

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
      brush->snake_hook_deform_type == BRUSH_SNAKE_HOOK_DEFORM_ELASTIC) {
    /* Snake hook in elastic deform type has same requirements as the elastic deform tool. */
    return true;
  }
  return false;
}

/* Pose Brush. */
void SCULPT_do_pose_brush(struct Sculpt *sd,
                          struct Object *ob,
                          struct PBVHNode **nodes,
                          int totnode);
void SCULPT_pose_calc_pose_data(struct Sculpt *sd,
                                struct Object *ob,
                                struct SculptSession *ss,
                                float initial_location[3],
                                float radius,
                                float pose_offset,
                                float *r_pose_origin,
                                float *r_pose_factor);
void SCULPT_pose_brush_init(struct Sculpt *sd,
                            struct Object *ob,
                            struct SculptSession *ss,
                            struct Brush *br);
struct SculptPoseIKChain *SCULPT_pose_ik_chain_init(struct Sculpt *sd,
                                                    struct Object *ob,
                                                    struct SculptSession *ss,
                                                    struct Brush *br,
                                                    const float initial_location[3],
                                                    const float radius);
void SCULPT_pose_ik_chain_free(struct SculptPoseIKChain *ik_chain);

/* Boundary Brush. */
struct SculptBoundary *SCULPT_boundary_data_init(Object *object,
                                                 Brush *brush,
                                                 const int initial_vertex,
                                                 const float radius);
void SCULPT_boundary_data_free(struct SculptBoundary *boundary);
void SCULPT_do_boundary_brush(struct Sculpt *sd,
                              struct Object *ob,
                              struct PBVHNode **nodes,
                              int totnode);

void SCULPT_boundary_edges_preview_draw(const uint gpuattr,
                                        struct SculptSession *ss,
                                        const float outline_col[3],
                                        const float outline_alpha);
void SCULPT_boundary_pivot_line_preview_draw(const uint gpuattr, struct SculptSession *ss);

/* Multi-plane Scrape Brush. */
void SCULPT_do_multiplane_scrape_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode);
void SCULPT_multiplane_scrape_preview_draw(const uint gpuattr,
                                           Brush *brush,
                                           SculptSession *ss,
                                           const float outline_col[3],
                                           const float outline_alpha);
/* Draw Face Sets Brush. */
void SCULPT_do_draw_face_sets_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode);

/* Paint Brush. */
void SCULPT_do_paint_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode);

/* Smear Brush. */
void SCULPT_do_smear_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode);

/* Smooth Brush. */
void SCULPT_bmesh_four_neighbor_average(float avg[3], float direction[3], struct BMVert *v);

void SCULPT_neighbor_coords_average(SculptSession *ss, float result[3], int index);
float SCULPT_neighbor_mask_average(SculptSession *ss, int index);
void SCULPT_neighbor_color_average(SculptSession *ss, float result[4], int index);

/* Mask the mesh boundaries smoothing only the mesh surface without using automasking. */
void SCULPT_neighbor_coords_average_interior(SculptSession *ss, float result[3], int index);

void SCULPT_smooth(Sculpt *sd,
                   Object *ob,
                   PBVHNode **nodes,
                   const int totnode,
                   float bstrength,
                   const bool smooth_mask);
void SCULPT_do_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode);

/* Surface Smooth Brush. */

void SCULPT_surface_smooth_laplacian_step(SculptSession *ss,
                                          float *disp,
                                          const float co[3],
                                          float (*laplacian_disp)[3],
                                          const int v_index,
                                          const float origco[3],
                                          const float alpha);
void SCULPT_surface_smooth_displace_step(SculptSession *ss,
                                         float *co,
                                         float (*laplacian_disp)[3],
                                         const int v_index,
                                         const float beta,
                                         const float fade);
void SCULPT_do_surface_smooth_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode);

/* Slide/Relax */
void SCULPT_relax_vertex(struct SculptSession *ss,
                         struct PBVHVertexIter *vd,
                         float factor,
                         bool filter_boundary_face_sets,
                         float *r_final_pos);

/* Undo */

typedef enum {
  SCULPT_UNDO_COORDS,
  SCULPT_UNDO_HIDDEN,
  SCULPT_UNDO_MASK,
  SCULPT_UNDO_DYNTOPO_BEGIN,
  SCULPT_UNDO_DYNTOPO_END,
  SCULPT_UNDO_DYNTOPO_SYMMETRIZE,
  SCULPT_UNDO_GEOMETRY,
  SCULPT_UNDO_FACE_SETS,
  SCULPT_UNDO_COLOR,
} SculptUndoType;

/* Storage of geometry for the undo node.
 * Is used as a storage for either original or modified geometry. */
typedef struct SculptUndoNodeGeometry {
  /* Is used for sanity check, helping with ensuring that two and only two
   * geometry pushes happened in the undo stack. */
  bool is_initialized;

  CustomData vdata;
  CustomData edata;
  CustomData ldata;
  CustomData pdata;
  int totvert;
  int totedge;
  int totloop;
  int totpoly;
} SculptUndoNodeGeometry;

typedef struct SculptUndoNode {
  struct SculptUndoNode *next, *prev;

  SculptUndoType type;

  char idname[MAX_ID_NAME]; /* name instead of pointer*/
  void *node;               /* only during push, not valid afterwards! */

  float (*co)[3];
  float (*orig_co)[3];
  short (*no)[3];
  float (*col)[4];
  float *mask;
  int totvert;

  /* non-multires */
  int maxvert; /* to verify if totvert it still the same */
  int *index;  /* to restore into right location */
  BLI_bitmap *vert_hidden;

  /* multires */
  int maxgrid;  /* same for grid */
  int gridsize; /* same for grid */
  int totgrid;  /* to restore into right location */
  int *grids;   /* to restore into right location */
  BLI_bitmap **grid_hidden;

  /* bmesh */
  struct BMLogEntry *bm_entry;
  bool applied;

  /* shape keys */
  char shapeName[sizeof(((KeyBlock *)0))->name];

  /* Geometry modification operations.
   *
   * Original geometry is stored before some modification is run and is used to restore state of
   * the object when undoing the operation
   *
   * Modified geometry is stored after the modification and is used to redo the modification. */
  bool geometry_clear_pbvh;
  SculptUndoNodeGeometry geometry_original;
  SculptUndoNodeGeometry geometry_modified;

  /* Geometry at the bmesh enter moment. */
  SculptUndoNodeGeometry geometry_bmesh_enter;

  /* pivot */
  float pivot_pos[3];
  float pivot_rot[4];

  /* Sculpt Face Sets */
  int *face_sets;

  size_t undo_size;
} SculptUndoNode;

/* Factor of brush to have rake point following behind
 * (could be configurable but this is reasonable default). */
#define SCULPT_RAKE_BRUSH_FACTOR 0.25f

struct SculptRakeData {
  float follow_dist;
  float follow_co[3];
};

/* Single struct used by all BLI_task threaded callbacks, let's avoid adding 10's of those... */
typedef struct SculptThreadedTaskData {
  struct bContext *C;
  struct Sculpt *sd;
  struct Object *ob;
  const struct Brush *brush;
  struct PBVHNode **nodes;
  int totnode;

  struct VPaint *vp;
  struct VPaintData *vpd;
  struct WPaintData *wpd;
  struct WeightPaintInfo *wpi;
  unsigned int *lcol;
  struct Mesh *me;
  /* For passing generic params. */
  void *custom_data;

  /* Data specific to some callbacks. */

  /* Note: even if only one or two of those are used at a time,
   *       keeping them separated, names help figuring out
   *       what it is, and memory overhead is ridiculous anyway. */
  float flippedbstrength;
  float angle;
  float strength;
  bool smooth_mask;
  bool has_bm_orco;

  struct SculptProjectVector *spvc;
  float *offset;
  float *grab_delta;
  float *cono;
  float *area_no;
  float *area_no_sp;
  float *area_co;
  float (*mat)[4];
  float (*vertCos)[3];

  /* X and Z vectors aligned to the stroke direction for operations where perpendicular vectors to
   * the stroke direction are needed. */
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

  float *prev_mask;

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

  /* Boundary brush */
  float boundary_deform_strength;

  float cloth_time_step;
  SculptClothSimulation *cloth_sim;
  float *cloth_sim_initial_location;
  float cloth_sim_radius;

  float dirty_mask_min;
  float dirty_mask_max;
  bool dirty_mask_dirty_only;

  /* Mask By Color Tool */

  float mask_by_color_threshold;
  bool mask_by_color_invert;
  bool mask_by_color_preserve_mask;

  /* Index of the vertex that is going to be used as a reference for the colors. */
  int mask_by_color_vertex;
  float *mask_by_color_floodfill;

  int face_set;
  int filter_undo_type;

  ThreadMutex mutex;

} SculptThreadedTaskData;

/*************** Brush testing declarations ****************/
typedef struct SculptBrushTest {
  float radius_squared;
  float radius;
  float location[3];
  float dist;
  int mirror_symmetry_pass;

  int radial_symmetry_pass;
  float symm_rot_mat_inv[4][4];

  /* For circle (not sphere) projection. */
  float plane_view[4];

  /* Some tool code uses a plane for its calculations. */
  float plane_tool[4];

  /* View3d clipping - only set rv3d for clipping */
  struct RegionView3D *clip_rv3d;
} SculptBrushTest;

typedef bool (*SculptBrushTestFn)(SculptBrushTest *test, const float co[3]);

typedef struct {
  struct Sculpt *sd;
  struct SculptSession *ss;
  float radius_squared;
  const float *center;
  bool original;
  /* This ignores fully masked and fully hidden nodes. */
  bool ignore_fully_ineffective;
} SculptSearchSphereData;

typedef struct {
  struct Sculpt *sd;
  struct SculptSession *ss;
  float radius_squared;
  bool original;
  bool ignore_fully_ineffective;
  struct DistRayAABB_Precalc *dist_ray_to_aabb_precalc;
} SculptSearchCircleData;

void SCULPT_brush_test_init(struct SculptSession *ss, SculptBrushTest *test);
bool SCULPT_brush_test_sphere(SculptBrushTest *test, const float co[3]);
bool SCULPT_brush_test_sphere_sq(SculptBrushTest *test, const float co[3]);
bool SCULPT_brush_test_sphere_fast(const SculptBrushTest *test, const float co[3]);
bool SCULPT_brush_test_cube(SculptBrushTest *test,
                            const float co[3],
                            const float local[4][4],
                            const float roundness);
bool SCULPT_brush_test_circle_sq(SculptBrushTest *test, const float co[3]);
bool SCULPT_search_sphere_cb(PBVHNode *node, void *data_v);
bool SCULPT_search_circle_cb(PBVHNode *node, void *data_v);

SculptBrushTestFn SCULPT_brush_test_init_with_falloff_shape(SculptSession *ss,
                                                            SculptBrushTest *test,
                                                            char falloff_shape);
const float *SCULPT_brush_frontface_normal_from_falloff_shape(SculptSession *ss,
                                                              char falloff_shape);

float SCULPT_brush_strength_factor(struct SculptSession *ss,
                                   const struct Brush *br,
                                   const float point[3],
                                   const float len,
                                   const short vno[3],
                                   const float fno[3],
                                   const float mask,
                                   const int vertex_index,
                                   const int thread_id);

/* Tilts a normal by the x and y tilt values using the view axis. */
void SCULPT_tilt_apply_to_normal(float r_normal[3],
                                 struct StrokeCache *cache,
                                 const float tilt_strength);

/* Get effective surface normal with pen tilt and tilt strength applied to it. */
void SCULPT_tilt_effective_normal_get(const SculptSession *ss, const Brush *brush, float r_no[3]);

/* just for vertex paint. */
bool SCULPT_pbvh_calc_area_normal(const struct Brush *brush,
                                  Object *ob,
                                  PBVHNode **nodes,
                                  int totnode,
                                  bool use_threading,
                                  float r_area_no[3]);

/* Cache stroke properties. Used because
 * RNA property lookup isn't particularly fast.
 *
 * For descriptions of these settings, check the operator properties.
 */

#define SCULPT_CLAY_STABILIZER_LEN 10

typedef struct AutomaskingSettings {
  /* Flags from eAutomasking_flag. */
  int flags;
  int initial_face_set;
} AutomaskingSettings;

typedef struct AutomaskingCache {
  AutomaskingSettings settings;
  /* Precomputed auto-mask factor indexed by vertex, owned by the auto-masking system and
   * initialized in #SCULPT_automasking_cache_init when needed. */
  float *factor;
} AutomaskingCache;

typedef struct StrokeCache {
  /* Invariants */
  float initial_radius;
  float scale[3];
  int flag;
  float clip_tolerance[3];
  float initial_mouse[2];

  /* Variants */
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

  /* Multires Displacement Smear. */
  float (*prev_displacement)[3];
  float (*limit_surface_co)[3];

  /* The rest is temporary storage that isn't saved as a property */

  bool first_time; /* Beginning of stroke may do some things special */

  /* from ED_view3d_ob_project_mat_get() */
  float projection_mat[4][4];

  /* Clean this up! */
  struct ViewContext *vc;
  const struct Brush *brush;

  float special_rotation;
  float grab_delta[3], grab_delta_symmetry[3];
  float old_grab_location[3], orig_grab_location[3];

  /* screen-space rotation defined by mouse motion */
  float rake_rotation[4], rake_rotation_symmetry[4];
  bool is_rake_rotation_valid;
  struct SculptRakeData rake_data;

  /* Face Sets */
  int paint_face_set;

  /* Symmetry index between 0 and 7 bit combo 0 is Brush only;
   * 1 is X mirror; 2 is Y mirror; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  int symmetry;
  int mirror_symmetry_pass; /* the symmetry pass we are currently on between 0 and 7*/
  float true_view_normal[3];
  float view_normal[3];

  /* sculpt_normal gets calculated by calc_sculpt_normal(), then the
   * sculpt_normal_symm gets updated quickly with the usual symmetry
   * transforms */
  float sculpt_normal[3];
  float sculpt_normal_symm[3];

  /* Used for area texture mode, local_mat gets calculated by
   * calc_brush_local_mat() and used in tex_strength(). */
  float brush_local_mat[4][4];

  float plane_offset[3]; /* used to shift the plane around when doing tiled strokes */
  int tile_pass;

  float last_center[3];
  int radial_symmetry_pass;
  float symm_rot_mat[4][4];
  float symm_rot_mat_inv[4][4];
  bool original;
  float anchored_location[3];

  /* Paint Brush. */
  struct {
    float hardness;
    float flow;
    float wet_mix;
    float wet_persistence;
    float density;
  } paint_brush;

  /* Pose brush */
  struct SculptPoseIKChain *pose_ik_chain;

  /* Enhance Details. */
  float (*detail_directions)[3];

  /* Clay Thumb brush */
  /* Angle of the front tilting plane of the brush to simulate clay accumulation. */
  float clay_thumb_front_angle;
  /* Stores pressure samples to get an stabilized strength and radius variation. */
  float clay_pressure_stabilizer[SCULPT_CLAY_STABILIZER_LEN];
  int clay_pressure_stabilizer_index;

  /* Cloth brush */
  struct SculptClothSimulation *cloth_sim;
  float initial_location[3];
  float true_initial_location[3];
  float initial_normal[3];
  float true_initial_normal[3];

  /* Boundary brush */
  struct SculptBoundary *boundaries[PAINT_SYMM_AREAS];

  /* Surface Smooth Brush */
  /* Stores the displacement produced by the laplacian step of HC smooth. */
  float (*surface_smooth_laplacian_disp)[3];

  /* Layer brush */
  float *layer_displacement_factor;

  float vertex_rotation; /* amount to rotate the vertices when using rotate brush */
  struct Dial *dial;

  char saved_active_brush_name[MAX_ID_NAME];
  char saved_mask_brush_tool;
  int saved_smooth_size; /* smooth tool copies the size of the current tool */
  bool alt_smooth;

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

} StrokeCache;

/* Sculpt Filters */
typedef enum SculptFilterOrientation {
  SCULPT_FILTER_ORIENTATION_LOCAL = 0,
  SCULPT_FILTER_ORIENTATION_WORLD = 1,
  SCULPT_FILTER_ORIENTATION_VIEW = 2,
} SculptFilterOrientation;

/* Defines how transform tools are going to apply its displacement. */
typedef enum SculptTransformDisplacementMode {
  /* Displaces the elements from their original coordinates. */
  SCULPT_TRANSFORM_DISPLACEMENT_ORIGINAL = 0,
  /* Displaces the elements incrementally from their previous position. */
  SCULPT_TRANSFORM_DISPLACEMENT_INCREMENTAL = 1,
} SculptTransformDisplacementMode;

void SCULPT_filter_to_orientation_space(float r_v[3], struct FilterCache *filter_cache);
void SCULPT_filter_to_object_space(float r_v[3], struct FilterCache *filter_cache);
void SCULPT_filter_zero_disabled_axis_components(float r_v[3], struct FilterCache *filter_cache);

/* Sculpt Expand. */
typedef enum eSculptExpandFalloffType {
  SCULPT_EXPAND_FALLOFF_GEODESIC,
  SCULPT_EXPAND_FALLOFF_TOPOLOGY,
  SCULPT_EXPAND_FALLOFF_TOPOLOGY_DIAGONALS,
  SCULPT_EXPAND_FALLOFF_NORMALS,
  SCULPT_EXPAND_FALLOFF_SPHERICAL,
  SCULPT_EXPAND_FALLOFF_BOUNDARY_TOPOLOGY,
  SCULPT_EXPAND_FALLOFF_BOUNDARY_FACE_SET,
  SCULPT_EXPAND_FALLOFF_ACTIVE_FACE_SET,
} eSculptExpandFalloffType;

typedef enum eSculptExpandTargetType {
  SCULPT_EXPAND_TARGET_MASK,
  SCULPT_EXPAND_TARGET_FACE_SETS,
  SCULPT_EXPAND_TARGET_COLORS,
} eSculptExpandTargetType;

typedef enum eSculptExpandRecursionType {
  SCULPT_EXPAND_RECURSION_TOPOLOGY,
  SCULPT_EXPAND_RECURSION_GEODESICS,
} eSculptExpandRecursionType;

#define EXPAND_SYMM_AREAS 8

typedef struct ExpandCache {
  /* Target data elements that the expand operation will affect. */
  eSculptExpandTargetType target;

  /* Falloff data. */
  eSculptExpandFalloffType falloff_type;

  /* Indexed by vertex index, precalculated falloff value of that vertex (without any falloff
   * editing modification applied). */
  float *vert_falloff;
  /* Max falloff value in *vert_falloff. */
  float max_vert_falloff;

  /* Indexed by base mesh poly index, precalculated falloff value of that face. These values are
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
  float initial_mouse_move[2];
  float initial_mouse[2];
  int initial_active_vertex;
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

  /* Active components checks. */
  /* Indexed by symmetry pass index, contains the connected component ID found in
   * SculptSession->vertex_info.connected_component. Other connected components not found in this
   * array will be ignored by Expand. */
  int active_connected_components[EXPAND_SYMM_AREAS];

  /* Snapping. */
  /* GSet containing all Face Sets IDs that Expand will use to snap the new data. */
  GSet *snap_enabled_face_sets;

  /* Texture distortion data. */
  Brush *brush;
  struct Scene *scene;
  struct MTex *mtex;

  /* Controls how much texture distortion will be applied to the current falloff */
  float texture_distortion_strength;

  /* Cached PBVH nodes. This allows to skip gathering all nodes from the PBVH each time expand
   * needs to update the state of the elements. */
  PBVHNode **nodes;
  int totnode;

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

  /* Color target data type related data. */
  float fill_color[4];
  short blend_mode;

  /* Face Sets at the first step of the expand operation, before starting modifying the active
   * vertex and active falloff. These are not the original Face Sets of the sculpt before starting
   * the operator as they could have been modified by Expand when initializing the operator and
   * before starting changing the active vertex. These Face Sets are used for restoring and
   * checking the Face Sets state while the Expand operation modal runs. */
  int *initial_face_sets;

  /* Original data of the sculpt as it was before running the Expand operator. */
  float *original_mask;
  int *original_face_sets;
  float (*original_colors)[4];
} ExpandCache;

typedef struct FilterCache {
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
  float obmat[4][4];
  float obmat_inv[4][4];
  float viewmat[4][4];
  float viewmat_inv[4][4];

  /* Displacement eraser. */
  float (*limit_surface_co)[3];

  /* unmasked nodes */
  PBVHNode **nodes;
  int totnode;

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

  /* Auto-masking. */
  AutomaskingCache *automasking;
} FilterCache;

void SCULPT_cache_calc_brushdata_symm(StrokeCache *cache,
                                      const char symm,
                                      const char axis,
                                      const float angle);
void SCULPT_cache_free(StrokeCache *cache);

SculptUndoNode *SCULPT_undo_push_node(Object *ob, PBVHNode *node, SculptUndoType type);
SculptUndoNode *SCULPT_undo_get_node(PBVHNode *node);
SculptUndoNode *SCULPT_undo_get_first_node(void);
void SCULPT_undo_push_begin(struct Object *ob, const char *name);
void SCULPT_undo_push_end(void);
void SCULPT_undo_push_end_ex(const bool use_nested_undo);

void SCULPT_vertcos_to_key(Object *ob, KeyBlock *kb, const float (*vertCos)[3]);

void SCULPT_update_object_bounding_box(struct Object *ob);

bool SCULPT_get_redraw_rect(struct ARegion *region,
                            struct RegionView3D *rv3d,
                            Object *ob,
                            rcti *rect);

/* Operators. */

/* Expand. */
void SCULPT_OT_expand(struct wmOperatorType *ot);
void sculpt_expand_modal_keymap(struct wmKeyConfig *keyconf);

/* Gestures. */
void SCULPT_OT_face_set_lasso_gesture(struct wmOperatorType *ot);
void SCULPT_OT_face_set_box_gesture(struct wmOperatorType *ot);

void SCULPT_OT_trim_lasso_gesture(struct wmOperatorType *ot);
void SCULPT_OT_trim_box_gesture(struct wmOperatorType *ot);

void SCULPT_OT_project_line_gesture(struct wmOperatorType *ot);

/* Face Sets. */
void SCULPT_OT_face_sets_randomize_colors(struct wmOperatorType *ot);
void SCULPT_OT_face_sets_change_visibility(struct wmOperatorType *ot);
void SCULPT_OT_face_sets_init(struct wmOperatorType *ot);
void SCULPT_OT_face_sets_create(struct wmOperatorType *ot);
void SCULPT_OT_face_sets_edit(struct wmOperatorType *ot);

/* Transform. */
void SCULPT_OT_set_pivot_position(struct wmOperatorType *ot);

/* Mesh Filter. */
void SCULPT_OT_mesh_filter(struct wmOperatorType *ot);

/* Cloth Filter. */
void SCULPT_OT_cloth_filter(struct wmOperatorType *ot);

/* Color Filter. */
void SCULPT_OT_color_filter(struct wmOperatorType *ot);

/* Mask filter and Dirty Mask. */
void SCULPT_OT_mask_filter(struct wmOperatorType *ot);
void SCULPT_OT_dirty_mask(struct wmOperatorType *ot);

/* Mask and Face Sets Expand. */
void SCULPT_OT_mask_expand(struct wmOperatorType *ot);

/* Detail size. */
void SCULPT_OT_detail_flood_fill(struct wmOperatorType *ot);
void SCULPT_OT_sample_detail_size(struct wmOperatorType *ot);
void SCULPT_OT_set_detail_size(struct wmOperatorType *ot);
void SCULPT_OT_dyntopo_detail_size_edit(struct wmOperatorType *ot);

/* Dyntopo. */
void SCULPT_OT_dynamic_topology_toggle(struct wmOperatorType *ot);
