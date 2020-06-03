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

#ifndef __SCULPT_INTERN_H__
#define __SCULPT_INTERN_H__

#include "DNA_brush_types.h"
#include "DNA_key_types.h"
#include "DNA_listBase.h"
#include "DNA_vec_types.h"

#include "BLI_bitmap.h"
#include "BLI_gsqueue.h"
#include "BLI_threads.h"

#include "BKE_paint.h"
#include "BKE_pbvh.h"

struct KeyBlock;
struct Object;
struct SculptPoseIKChainSegment;
struct SculptUndoNode;
struct bContext;

enum ePaintSymmetryFlags;

bool SCULPT_mode_poll(struct bContext *C);
bool SCULPT_mode_poll_view3d(struct bContext *C);
/* checks for a brush, not just sculpt mode */
bool SCULPT_poll(struct bContext *C);
bool SCULPT_poll_view3d(struct bContext *C);

/* Updates */

typedef enum SculptUpdateType {
  SCULPT_UPDATE_COORDS = 1 << 0,
  SCULPT_UPDATE_MASK = 1 << 1,
  SCULPT_UPDATE_VISIBILITY = 1 << 2,
} SculptUpdateType;

void SCULPT_flush_update_step(bContext *C, SculptUpdateType update_flags);
void SCULPT_flush_update_done(const bContext *C, Object *ob, SculptUpdateType update_flags);
void SCULPT_flush_stroke_deform(struct Sculpt *sd, Object *ob, bool is_proxy_used);

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

/* Sculpt PBVH abstraction API */
void SCULPT_vertex_random_access_init(struct SculptSession *ss);

int SCULPT_vertex_count_get(struct SculptSession *ss);
const float *SCULPT_vertex_co_get(struct SculptSession *ss, int index);
void SCULPT_vertex_normal_get(SculptSession *ss, int index, float no[3]);
float SCULPT_vertex_mask_get(struct SculptSession *ss, int index);

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

bool SCULPT_vertex_is_boundary(SculptSession *ss, const int index);

/* Sculpt Visibility API */

void SCULPT_vertex_visible_set(SculptSession *ss, int index, bool visible);
bool SCULPT_vertex_visible_get(SculptSession *ss, int index);

void SCULPT_visibility_sync_all_face_sets_to_vertices(struct SculptSession *ss);
void SCULPT_visibility_sync_all_vertex_to_face_sets(struct SculptSession *ss);

/* Face Sets API */

int SCULPT_active_face_set_get(SculptSession *ss);
int SCULPT_vertex_face_set_get(SculptSession *ss, int index);
void SCULPT_vertex_face_set_set(SculptSession *ss, int index, int face_set);

bool SCULPT_vertex_has_face_set(SculptSession *ss, int index, int face_set);
bool SCULPT_vertex_has_unique_face_set(SculptSession *ss, int index);

int SCULPT_face_set_next_available_get(SculptSession *ss);

void SCULPT_face_set_visibility_set(SculptSession *ss, int face_set, bool visible);
bool SCULPT_vertex_all_face_sets_visible_get(SculptSession *ss, int index);
bool SCULPT_vertex_any_face_set_visible_get(SculptSession *ss, int index);

void SCULPT_face_sets_visibility_invert(SculptSession *ss);
void SCULPT_face_sets_visibility_all_set(SculptSession *ss, bool visible);

/* Sculpt Original Data */
typedef struct {
  struct BMLog *bm_log;

  struct SculptUndoNode *unode;
  float (*coords)[3];
  short (*normals)[3];
  const float *vmasks;

  /* Original coordinate, normal, and mask. */
  const float *co;
  const short *no;
  float mask;
} SculptOrigVertData;

void SCULPT_orig_vert_data_init(SculptOrigVertData *data, Object *ob, PBVHNode *node);
void SCULPT_orig_vert_data_update(SculptOrigVertData *orig_data, PBVHVertexIter *iter);

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

/* Automasking. */
float SCULPT_automasking_factor_get(SculptSession *ss, int vert);

void SCULPT_automasking_init(Sculpt *sd, Object *ob);
void SCULPT_automasking_end(Object *ob);

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

/* Filters. */
void SCULPT_filter_cache_init(Object *ob, Sculpt *sd);
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

void SCULPT_cloth_simulation_limits_draw(const uint gpuattr,
                                         const struct Brush *brush,
                                         const float obmat[4][4],
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

BLI_INLINE bool SCULPT_is_cloth_deform_brush(const Brush *brush)
{
  return brush->sculpt_tool == SCULPT_TOOL_CLOTH &&
         brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB;
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

/* Multiplane Scrape Brush. */
void SCULPT_do_multiplane_scrape_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode);
void SCULPT_multiplane_scrape_preview_draw(const uint gpuattr,
                                           SculptSession *ss,
                                           const float outline_col[3],
                                           const float outline_alpha);
/* Draw Face Sets Brush. */
void SCULPT_do_draw_face_sets_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode);

/* Smooth Brush. */

void SCULPT_neighbor_average(SculptSession *ss, float avg[3], uint vert);
void SCULPT_bmesh_neighbor_average(float avg[3], struct BMVert *v);

void SCULPT_bmesh_four_neighbor_average(float avg[3], float direction[3], struct BMVert *v);

void SCULPT_neighbor_coords_average(SculptSession *ss, float result[3], int index);
float SCULPT_neighbor_mask_average(SculptSession *ss, int index);

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

  bool use_area_cos;
  bool use_area_nos;

  /* 0=towards view, 1=flipped */
  float (*area_cos)[3];
  float (*area_nos)[3];
  int *count_no;
  int *count_co;

  bool any_vertex_sampled;

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

  float cloth_time_step;
  SculptClothSimulation *cloth_sim;
  float *cloth_sim_initial_location;
  float cloth_sim_radius;

  float dirty_mask_min;
  float dirty_mask_max;
  bool dirty_mask_dirty_only;

  int face_set;

  ThreadMutex mutex;

} SculptThreadedTaskData;

/*************** Brush testing declarations ****************/
typedef struct SculptBrushTest {
  float radius_squared;
  float radius;
  float location[3];
  float dist;
  int mirror_symmetry_pass;

  /* For circle (not sphere) projection. */
  float plane_view[4];

  /* Some tool code uses a plane for it's calculateions. */
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
  bool ignore_fully_masked;
} SculptSearchSphereData;

typedef struct {
  struct Sculpt *sd;
  struct SculptSession *ss;
  float radius_squared;
  bool original;
  bool ignore_fully_masked;
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
  float mouse[2];
  float bstrength;
  float normal_weight; /* from brush (with optional override) */

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

  /* Pose brush */
  struct SculptPoseIKChain *pose_ik_chain;

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

  float *automask;

  float stroke_local_mat[4][4];
  float multiplane_scrape_angle;

  rcti previous_r; /* previous redraw rectangle */
  rcti current_r;  /* current redraw rectangle */

} StrokeCache;

typedef struct FilterCache {
  bool enabled_axis[3];
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
  float *sharpen_factor;

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

  /* Used to prevent undesired results on certain mesh filters. */
  float *automask;

  int new_face_set;
  int *prev_face_set;

  int active_face_set;
} FilterCache;

void SCULPT_cache_calc_brushdata_symm(StrokeCache *cache,
                                      const char symm,
                                      const char axis,
                                      const float angle);
void SCULPT_cache_free(StrokeCache *cache);

SculptUndoNode *SCULPT_undo_push_node(Object *ob, PBVHNode *node, SculptUndoType type);
SculptUndoNode *SCULPT_undo_get_node(PBVHNode *node);
SculptUndoNode *SCULPT_undo_get_first_node(void);
void SCULPT_undo_push_begin(const char *name);
void SCULPT_undo_push_end(void);
void SCULPT_undo_push_end_ex(const bool use_nested_undo);

void SCULPT_vertcos_to_key(Object *ob, KeyBlock *kb, const float (*vertCos)[3]);

void SCULPT_update_object_bounding_box(struct Object *ob);

bool SCULPT_get_redraw_rect(struct ARegion *region,
                            struct RegionView3D *rv3d,
                            Object *ob,
                            rcti *rect);

/* Operators. */

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

/* Mask filter and Dirty Mask. */
void SCULPT_OT_mask_filter(struct wmOperatorType *ot);
void SCULPT_OT_dirty_mask(struct wmOperatorType *ot);

/* Mask and Face Sets Expand. */
void SCULPT_OT_mask_expand(struct wmOperatorType *ot);

/* Detail size. */
void SCULPT_OT_detail_flood_fill(struct wmOperatorType *ot);
void SCULPT_OT_sample_detail_size(struct wmOperatorType *ot);
void SCULPT_OT_set_detail_size(struct wmOperatorType *ot);

/* Dyntopo. */
void SCULPT_OT_dynamic_topology_toggle(struct wmOperatorType *ot);

#endif
