/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 * \brief A BVH for high poly meshes.
 */

#include <optional>
#include <string>

#include "BLI_array.hh"
#include "BLI_bit_group_vector.hh"
#include "BLI_bounds_types.hh"
#include "BLI_compiler_compat.h"
#include "BLI_function_ref.hh"
#include "BLI_generic_span.hh"
#include "BLI_index_mask_fwd.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "DNA_customdata_types.h"

/* For embedding CCGKey in iterator. */
#include "BKE_ccg.hh"
#include "BKE_pbvh.hh"

#include "bmesh.hh"

struct BMLog;
struct BMesh;
struct CCGElem;
struct CCGKey;
struct CustomData;
struct IsectRayPrecalc;
struct Mesh;
struct SubdivCCG;
struct Image;
struct ImageUser;
namespace blender {
namespace bke::pbvh {
class Node;
class Tree;
namespace pixels {
struct PBVHData;
struct NodeData;
}  // namespace pixels
}  // namespace bke::pbvh
namespace draw::pbvh {
struct PBVHBatches;
struct PBVH_GPU_Args;
}  // namespace draw::pbvh
}  // namespace blender

namespace blender::bke::pbvh {

class Tree;

/**
 * \todo Most data is public but should either be removed or become private in the future.
 * The "_" suffix means that fields shouldn't be used by consumers of the `bke::pbvh` API.
 */
class Node {
  friend Tree;

 public:
  /* Opaque handle for drawing code */
  draw::pbvh::PBVHBatches *draw_batches_ = nullptr;

  /** Axis aligned min and max of all vertex positions in the node. */
  Bounds<float3> bounds_ = {};
  /** Bounds from the start of current brush stroke. */
  Bounds<float3> bounds_orig_ = {};

  /* For internal nodes, the offset of the children in the blender::bke::pbvh::Tree
   * 'nodes' array. */
  int children_offset_ = 0;

  /* List of primitives for this node. Semantics depends on
   * blender::bke::pbvh::Tree type:
   *
   * - Type::Mesh: Indices into the #blender::bke::pbvh::Tree::corner_tris array.
   * - Type::Grids: Multires grid indices.
   * - Type::BMesh: Unused.  See Node.bm_faces.
   *
   * NOTE: This is a pointer inside of blender::bke::pbvh::Tree.prim_indices; it
   * is not allocated separately per node.
   */
  Span<int> prim_indices_;

  /* Array of indices into the mesh's vertex array. Contains the
   * indices of all vertices used by faces that are within this
   * node's bounding box.
   *
   * Note that a vertex might be used by a multiple faces, and
   * these faces might be in different leaf nodes. Such a vertex
   * will appear in the vert_indices array of each of those leaf
   * nodes.
   *
   * In order to support cases where you want access to multiple
   * nodes' vertices without duplication, the vert_indices array
   * is ordered such that the first part of the array, up to
   * index 'uniq_verts', contains "unique" vertex indices. These
   * vertices might not be truly unique to this node, but if
   * they appear in another node's vert_indices array, they will
   * be above that node's 'uniq_verts' value.
   *
   * Used for leaf nodes in a mesh-based blender::bke::pbvh::Tree (not multires.)
   */
  Array<int, 0> vert_indices_;
  /** The number of vertices in #vert_indices not shared with (owned by) another node. */
  int unique_verts_num_ = 0;

  /* Array of indices into the Mesh's corner array.
   * Type::Mesh only.
   */
  Array<int, 0> corner_indices_;

  /* An array mapping face corners into the vert_indices
   * array. The array is sized to match 'totprim', and each of
   * the face's corners gets an index into the vert_indices
   * array, in the same order as the corners in the original
   * triangle.
   *
   * Used for leaf nodes in a mesh-based blender::bke::pbvh::Tree (not multires.)
   */
  Array<int3, 0> face_vert_indices_;

  /* Indicates whether this node is a leaf or not; also used for
   * marking various updates that need to be applied. */
  PBVHNodeFlags flag_ = PBVHNodeFlags(0);

  /* Used for ray-casting: how close the bounding-box is to the ray point. */
  float tmin_ = 0.0f;

  /* Dyntopo */

  /* Set of pointers to the BMFaces used by this node.
   * NOTE: Type::BMesh only. Faces are always triangles
   * (dynamic topology forcibly triangulates the mesh).
   */
  Set<BMFace *, 0> bm_faces_;
  Set<BMVert *, 0> bm_unique_verts_;
  Set<BMVert *, 0> bm_other_verts_;

  /* Deprecated. Stores original coordinates of triangles. */
  float (*bm_orco_)[3] = nullptr;
  int (*bm_ortri_)[3] = nullptr;
  BMVert **bm_orvert_ = nullptr;
  int bm_tot_ortri_ = 0;

  pixels::NodeData *pixels_ = nullptr;

  /* Used to flash colors of updated node bounding boxes in
   * debug draw mode (when G.debug_value / bpy.app.debug_value is 889).
   */
  int debug_draw_gen_ = 0;
};

/**
 * \todo Most data is public but should either be removed or become private in the future.
 * The "_" suffix means that fields shouldn't be used by consumers of the `bke::pbvh` API.
 */
class Tree {
  friend Node;
  Type type_;

 public:
  BMesh *bm_ = nullptr;

  Vector<Node> nodes_;

  /* Memory backing for Node.prim_indices. */
  Array<int> prim_indices_;

  /* Mesh data. The evaluated deform mesh for mesh sculpting, and the base mesh for grids. */
  Mesh *mesh_ = nullptr;

  /** Local array used when not sculpting base mesh positions directly. */
  Array<float3> vert_positions_deformed_;
  /** Local array used when not sculpting base mesh positions directly. */
  Array<float3> vert_normals_deformed_;
  /** Local array used when not sculpting base mesh positions directly. */
  Array<float3> face_normals_deformed_;

  MutableSpan<float3> vert_positions_;
  Span<float3> vert_normals_;
  Span<float3> face_normals_;

  /* Grid Data */
  SubdivCCG *subdiv_ccg_ = nullptr;

  /* flag are verts/faces deformed */
  bool deformed_ = false;

  /* Dynamic topology */
  float bm_max_edge_len_;
  float bm_min_edge_len_;

  float planes_[6][4];
  int num_planes_;

  pixels::PBVHData *pixels_ = nullptr;

 public:
  Tree(const Type type) : type_(type) {}
  ~Tree();

  Type type() const
  {
    return this->type_;
  }
};

}  // namespace blender::bke::pbvh

struct PBVHFrustumPlanes {
  float (*planes)[4];
  int num_planes;
};

BLI_INLINE BMesh *BKE_pbvh_get_bmesh(blender::bke::pbvh::Tree &pbvh)
{
  return pbvh.bm_;
}

Mesh *BKE_pbvh_get_mesh(blender::bke::pbvh::Tree &pbvh);

BLI_INLINE PBVHVertRef BKE_pbvh_make_vref(intptr_t i)
{
  PBVHVertRef ret = {i};
  return ret;
}

BLI_INLINE int BKE_pbvh_vertex_to_index(blender::bke::pbvh::Tree &pbvh, PBVHVertRef v)
{
  return (pbvh.type() == blender::bke::pbvh::Type::BMesh && v.i != PBVH_REF_NONE ?
              BM_elem_index_get((BMVert *)(v.i)) :
              (v.i));
}

BLI_INLINE PBVHVertRef BKE_pbvh_index_to_vertex(blender::bke::pbvh::Tree &pbvh, int index)
{
  switch (pbvh.type()) {
    case blender::bke::pbvh::Type::Mesh:
    case blender::bke::pbvh::Type::Grids:
      return BKE_pbvh_make_vref(index);
    case blender::bke::pbvh::Type::BMesh:
      return BKE_pbvh_make_vref((intptr_t)BKE_pbvh_get_bmesh(pbvh)->vtable[index]);
  }

  return BKE_pbvh_make_vref(PBVH_REF_NONE);
}

/* Callbacks */

namespace blender::bke::pbvh {

/**
 * Do a full rebuild with on Mesh data structure.
 */
std::unique_ptr<Tree> build_mesh(Mesh *mesh);
void update_mesh_pointers(Tree &pbvh, Mesh *mesh);
/**
 * Do a full rebuild with on Grids data structure.
 */
std::unique_ptr<Tree> build_grids(Mesh *mesh, SubdivCCG *subdiv_ccg);
/**
 * Build a Tree from a BMesh.
 */
std::unique_ptr<Tree> build_bmesh(BMesh *bm);

void build_pixels(Tree &pbvh, Mesh *mesh, Image *image, ImageUser *image_user);
void free(std::unique_ptr<Tree> &pbvh);

/* Hierarchical Search in the BVH, two methods:
 * - For each hit calling a callback.
 * - Gather nodes in an array (easy to multi-thread) see search_gather.
 */

void search_callback(Tree &pbvh,
                     FunctionRef<bool(Node &)> filter_fn,
                     FunctionRef<void(Node &)> hit_fn);

/* Ray-cast
 * the hit callback is called for all leaf nodes intersecting the ray;
 * it's up to the callback to find the primitive within the leaves that is
 * hit first */

void raycast(Tree &pbvh,
             FunctionRef<void(Node &node, float *tmin)> cb,
             const float ray_start[3],
             const float ray_normal[3],
             bool original);

bool raycast_node(Tree &pbvh,
                  Node &node,
                  const float (*origco)[3],
                  bool use_origco,
                  Span<int> corner_verts,
                  Span<int3> corner_tris,
                  Span<int> corner_tri_faces,
                  Span<bool> hide_poly,
                  const float ray_start[3],
                  const float ray_normal[3],
                  IsectRayPrecalc *isect_precalc,
                  float *depth,
                  PBVHVertRef *active_vertex,
                  int *active_face_grid_index,
                  float *face_normal);

bool bmesh_node_raycast_detail(Node &node,
                               const float ray_start[3],
                               IsectRayPrecalc *isect_precalc,
                               float *depth,
                               float *r_edge_length);

/**
 * For orthographic cameras, project the far away ray segment points to the root node so
 * we can have better precision.
 *
 * \note the interval is not guaranteed to lie between ray_start and ray_end; this is
 * not necessary for orthographic views and is impossible anyhow due to the necessity of
 * projecting the far clipping plane into the local object space.  This works out to
 * dividing view3d->clip_end by the object scale, which for small object and large
 * clip_end's can easily lead to floating-point overflows.
 */
void clip_ray_ortho(
    Tree &pbvh, bool original, float ray_start[3], float ray_end[3], float ray_normal[3]);

void find_nearest_to_ray(Tree &pbvh,
                         const FunctionRef<void(Node &node, float *tmin)> fn,
                         const float ray_start[3],
                         const float ray_normal[3],
                         bool original);

bool find_nearest_to_ray_node(Tree &pbvh,
                              Node &node,
                              const float (*origco)[3],
                              bool use_origco,
                              Span<int> corner_verts,
                              Span<int3> corner_tris,
                              Span<int> corner_tri_faces,
                              Span<bool> hide_poly,
                              const float ray_start[3],
                              const float ray_normal[3],
                              float *depth,
                              float *dist_sq);

/* Drawing */
void set_frustum_planes(Tree &pbvh, PBVHFrustumPlanes *planes);
void get_frustum_planes(const Tree &pbvh, PBVHFrustumPlanes *planes);

void draw_cb(const Mesh &mesh,
             Tree &pbvh,
             bool update_only_visible,
             const PBVHFrustumPlanes &update_frustum,
             const PBVHFrustumPlanes &draw_frustum,
             FunctionRef<void(draw::pbvh::PBVHBatches *batches,
                              const draw::pbvh::PBVH_GPU_Args &args)> draw_fn);
/**
 * Get the Tree root's bounding box.
 */
Bounds<float3> bounds_get(const Tree &pbvh);

}  // namespace blender::bke::pbvh

void BKE_pbvh_sync_visibility_from_verts(blender::bke::pbvh::Tree &pbvh, Mesh *mesh);

namespace blender::bke::pbvh {

/**
 * Returns the number of visible quads in the nodes' grids.
 */
int count_grid_quads(const BitGroupVector<> &grid_visibility,
                     Span<int> grid_indices,
                     int gridsize,
                     int display_gridsize);

}  // namespace blender::bke::pbvh

int BKE_pbvh_get_grid_num_verts(const blender::bke::pbvh::Tree &pbvh);
int BKE_pbvh_get_grid_num_faces(const blender::bke::pbvh::Tree &pbvh);

/**
 * Only valid for type == #blender::bke::pbvh::Type::BMesh.
 */
void BKE_pbvh_bmesh_detail_size_set(blender::bke::pbvh::Tree &pbvh, float detail_size);

enum PBVHTopologyUpdateMode {
  PBVH_Subdivide = 1,
  PBVH_Collapse = 2,
};
ENUM_OPERATORS(PBVHTopologyUpdateMode, PBVH_Collapse);

namespace blender::bke::pbvh {

/**
 * Collapse short edges, subdivide long edges.
 */
bool bmesh_update_topology(Tree &pbvh,
                           BMLog &bm_log,
                           PBVHTopologyUpdateMode mode,
                           const float center[3],
                           const float view_normal[3],
                           float radius,
                           bool use_frontface,
                           bool use_projected);

}  // namespace blender::bke::pbvh

/* Node Access */

void BKE_pbvh_node_mark_update(blender::bke::pbvh::Node *node);
void BKE_pbvh_node_mark_update_mask(blender::bke::pbvh::Node *node);
void BKE_pbvh_node_mark_update_color(blender::bke::pbvh::Node *node);
void BKE_pbvh_node_mark_update_face_sets(blender::bke::pbvh::Node *node);
void BKE_pbvh_node_mark_update_visibility(blender::bke::pbvh::Node *node);
void BKE_pbvh_node_mark_rebuild_draw(blender::bke::pbvh::Node *node);
void BKE_pbvh_node_mark_redraw(blender::bke::pbvh::Node *node);
void BKE_pbvh_node_mark_positions_update(blender::bke::pbvh::Node *node);
void BKE_pbvh_node_mark_topology_update(blender::bke::pbvh::Node *node);
void BKE_pbvh_node_fully_hidden_set(blender::bke::pbvh::Node *node, int fully_hidden);
bool BKE_pbvh_node_fully_hidden_get(const blender::bke::pbvh::Node *node);
void BKE_pbvh_node_fully_masked_set(blender::bke::pbvh::Node *node, int fully_masked);
bool BKE_pbvh_node_fully_masked_get(const blender::bke::pbvh::Node *node);
void BKE_pbvh_node_fully_unmasked_set(blender::bke::pbvh::Node *node, int fully_masked);
bool BKE_pbvh_node_fully_unmasked_get(const blender::bke::pbvh::Node *node);

void BKE_pbvh_mark_rebuild_pixels(blender::bke::pbvh::Tree &pbvh);

namespace blender::bke::pbvh {

Span<int> node_grid_indices(const Node &node);

Span<int> node_verts(const Node &node);
Span<int> node_unique_verts(const Node &node);
Span<int> node_corners(const Node &node);

/**
 * Gather the indices of all faces (not triangles) used by the node.
 * For convenience, pass a reference to the data in the result.
 */
Span<int> node_face_indices_calc_mesh(Span<int> corner_tri_faces,
                                      const Node &node,
                                      Vector<int> &faces);

/**
 * Gather the indices of all base mesh faces in the node.
 * For convenience, pass a reference to the data in the result.
 */
Span<int> node_face_indices_calc_grids(const Tree &pbvh, const Node &node, Vector<int> &faces);

Bounds<float3> node_bounds(const Node &node);

}  // namespace blender::bke::pbvh

blender::Bounds<blender::float3> BKE_pbvh_node_get_original_BB(
    const blender::bke::pbvh::Node *node);

float BKE_pbvh_node_get_tmin(const blender::bke::pbvh::Node *node);

/**
 * Test if AABB is at least partially inside the #PBVHFrustumPlanes volume.
 */
bool BKE_pbvh_node_frustum_contain_AABB(const blender::bke::pbvh::Node *node,
                                        const PBVHFrustumPlanes *frustum);
/**
 * Test if AABB is at least partially outside the #PBVHFrustumPlanes volume.
 */
bool BKE_pbvh_node_frustum_exclude_AABB(const blender::bke::pbvh::Node *node,
                                        const PBVHFrustumPlanes *frustum);

const blender::Set<BMVert *, 0> &BKE_pbvh_bmesh_node_unique_verts(blender::bke::pbvh::Node *node);
const blender::Set<BMVert *, 0> &BKE_pbvh_bmesh_node_other_verts(blender::bke::pbvh::Node *node);
const blender::Set<BMFace *, 0> &BKE_pbvh_bmesh_node_faces(blender::bke::pbvh::Node *node);

/**
 * In order to perform operations on the original node coordinates
 * (currently just ray-cast), store the node's triangles and vertices.
 *
 * Skips triangles that are hidden.
 */
void BKE_pbvh_bmesh_node_save_orig(BMesh *bm,
                                   BMLog *log,
                                   blender::bke::pbvh::Node *node,
                                   bool use_original);
void BKE_pbvh_bmesh_after_stroke(blender::bke::pbvh::Tree &pbvh);

namespace blender::bke::pbvh {

/**
 * Recalculate node bounding boxes based on the current coordinates. Calculation is only done for
 * affected nodes with the #PBVH_UpdateBB flag set.
 */
void update_bounds(Tree &pbvh);

/**
 * Copy all current node bounds to the original bounds. "Original" bounds are typically from before
 * a brush stroke started (while the "regular" bounds update on every change of positions). These
 * are stored to optimize the BVH traversal for original coordinates enabled by various "use
 * original" arguments in the Tree API.
 */
void store_bounds_orig(Tree &pbvh);

void update_mask(Tree &pbvh);
void update_visibility(Tree &pbvh);
void update_normals(Tree &pbvh, SubdivCCG *subdiv_ccg);
}  // namespace blender::bke::pbvh

blender::Bounds<blender::float3> BKE_pbvh_redraw_BB(blender::bke::pbvh::Tree &pbvh);
namespace blender::bke::pbvh {
IndexMask nodes_to_face_selection_grids(const SubdivCCG &subdiv_ccg,
                                        Span<const Node *> nodes,
                                        IndexMaskMemory &memory);
}
void BKE_pbvh_subdiv_cgg_set(blender::bke::pbvh::Tree &pbvh, SubdivCCG *subdiv_ccg);

void BKE_pbvh_vert_coords_apply(blender::bke::pbvh::Tree &pbvh,
                                blender::Span<blender::float3> vert_positions);
bool BKE_pbvh_is_deformed(const blender::bke::pbvh::Tree &pbvh);

/* Vertex Iterator. */

/* This iterator has quite a lot of code, but it's designed to:
 * - allow the compiler to eliminate dead code and variables
 * - spend most of the time in the relatively simple inner loop */

/* NOTE: PBVH_ITER_ALL does not skip hidden vertices,
 * PBVH_ITER_UNIQUE does */
#define PBVH_ITER_ALL 0
#define PBVH_ITER_UNIQUE 1

struct PBVHVertexIter {
  /* iteration */
  int g;
  int width;
  int height;
  int gx;
  int gy;
  int i;
  int index;
  PBVHVertRef vertex;

  /* grid */
  CCGKey key;
  CCGElem *const *grids;
  CCGElem *grid;
  const blender::BitGroupVector<> *grid_hidden;
  std::optional<blender::BoundedBitSpan> gh;
  const int *grid_indices;
  int totgrid;
  int gridsize;

  /* mesh */
  blender::MutableSpan<blender::float3> vert_positions;
  blender::Span<blender::float3> vert_normals;
  const bool *hide_vert;
  int totvert;
  const int *vert_indices;
  const float *vmask;
  bool is_mesh;

  /* bmesh */
  std::optional<blender::Set<BMVert *, 0>::Iterator> bm_unique_verts;
  std::optional<blender::Set<BMVert *, 0>::Iterator> bm_unique_verts_end;
  std::optional<blender::Set<BMVert *, 0>::Iterator> bm_other_verts;
  std::optional<blender::Set<BMVert *, 0>::Iterator> bm_other_verts_end;
  CustomData *bm_vdata;
  int cd_vert_mask_offset;

  /* result: these are all computed in the macro, but we assume
   * that compiler optimization's will skip the ones we don't use */
  BMVert *bm_vert;
  float *co;
  const float *no;
  const float *fno;
  float mask;
  bool visible;
};

void pbvh_vertex_iter_init(blender::bke::pbvh::Tree &pbvh,
                           blender::bke::pbvh::Node *node,
                           PBVHVertexIter *vi,
                           int mode);

#define BKE_pbvh_vertex_iter_begin(pbvh, node, vi, mode) \
  pbvh_vertex_iter_init(pbvh, node, &vi, mode); \
\
  for (vi.i = 0, vi.g = 0; vi.g < vi.totgrid; vi.g++) { \
    if (vi.grids) { \
      vi.width = vi.gridsize; \
      vi.height = vi.gridsize; \
      vi.index = vi.vertex.i = vi.grid_indices[vi.g] * vi.key.grid_area - 1; \
      vi.grid = CCG_elem_offset(vi.key, vi.grids[vi.grid_indices[vi.g]], -1); \
      if (mode == PBVH_ITER_UNIQUE) { \
        if (vi.grid_hidden) { \
          vi.gh.emplace((*vi.grid_hidden)[vi.grid_indices[vi.g]]); \
        } \
        else { \
          vi.gh.reset(); \
        } \
      } \
    } \
    else { \
      vi.width = vi.totvert; \
      vi.height = 1; \
    } \
\
    for (vi.gy = 0; vi.gy < vi.height; vi.gy++) { \
      for (vi.gx = 0; vi.gx < vi.width; vi.gx++, vi.i++) { \
        if (vi.grid) { \
          vi.grid = CCG_elem_next(vi.key, vi.grid); \
          vi.co = CCG_elem_co(vi.key, vi.grid); \
          vi.fno = CCG_elem_no(vi.key, vi.grid); \
          vi.mask = vi.key.has_mask ? CCG_elem_mask(vi.key, vi.grid) : 0.0f; \
          vi.index++; \
          vi.vertex.i++; \
          vi.visible = true; \
          if (vi.gh) { \
            if ((*vi.gh)[vi.gy * vi.gridsize + vi.gx]) { \
              continue; \
            } \
          } \
        } \
        else if (!vi.vert_positions.is_empty()) { \
          vi.visible = !(vi.hide_vert && vi.hide_vert[vi.vert_indices[vi.gx]]); \
          if (mode == PBVH_ITER_UNIQUE && !vi.visible) { \
            continue; \
          } \
          vi.co = vi.vert_positions[vi.vert_indices[vi.gx]]; \
          vi.no = vi.vert_normals[vi.vert_indices[vi.gx]]; \
          vi.index = vi.vertex.i = vi.vert_indices[vi.i]; \
          vi.mask = vi.vmask ? vi.vmask[vi.index] : 0.0f; \
        } \
        else { \
          if (*vi.bm_unique_verts != *vi.bm_unique_verts_end) { \
            vi.bm_vert = **vi.bm_unique_verts; \
            (*vi.bm_unique_verts)++; \
          } \
          else { \
            vi.bm_vert = **vi.bm_other_verts; \
            (*vi.bm_other_verts)++; \
          } \
          vi.visible = !BM_elem_flag_test_bool(vi.bm_vert, BM_ELEM_HIDDEN); \
          if (mode == PBVH_ITER_UNIQUE && !vi.visible) { \
            continue; \
          } \
          vi.co = vi.bm_vert->co; \
          vi.fno = vi.bm_vert->no; \
          vi.vertex = BKE_pbvh_make_vref((intptr_t)vi.bm_vert); \
          vi.index = BM_elem_index_get(vi.bm_vert); \
          vi.mask = BM_ELEM_CD_GET_FLOAT(vi.bm_vert, vi.cd_vert_mask_offset); \
        }

#define BKE_pbvh_vertex_iter_end \
  } \
  } \
  } \
  ((void)0)

#define PBVH_FACE_ITER_VERTS_RESERVED 8

void BKE_pbvh_node_get_bm_orco_data(blender::bke::pbvh::Node *node,
                                    int (**r_orco_tris)[3],
                                    int *r_orco_tris_num,
                                    float (**r_orco_coords)[3],
                                    BMVert ***r_orco_verts);

bool pbvh_has_mask(const blender::bke::pbvh::Tree &pbvh);

bool pbvh_has_face_sets(blender::bke::pbvh::Tree &pbvh);

blender::Span<blender::float3> BKE_pbvh_get_vert_positions(const blender::bke::pbvh::Tree &pbvh);
blender::MutableSpan<blender::float3> BKE_pbvh_get_vert_positions(blender::bke::pbvh::Tree &pbvh);
blender::Span<blender::float3> BKE_pbvh_get_vert_normals(const blender::bke::pbvh::Tree &pbvh);

void BKE_pbvh_ensure_node_face_corners(blender::bke::pbvh::Tree &pbvh,
                                       blender::Span<blender::int3> corner_tris);
int BKE_pbvh_debug_draw_gen_get(blender::bke::pbvh::Node &node);

namespace blender::bke::pbvh {
Vector<Node *> search_gather(Tree &pbvh,
                             FunctionRef<bool(Node &)> scb,
                             PBVHNodeFlags leaf_flag = PBVH_Leaf);

void node_update_mask_mesh(Span<float> mask, Node &node);
void node_update_mask_grids(const CCGKey &key, Span<CCGElem *> grids, Node &node);
void node_update_mask_bmesh(int mask_offset, Node &node);

void node_update_visibility_mesh(Span<bool> hide_vert, Node &node);
void node_update_visibility_grids(const BitGroupVector<> &grid_hidden, Node &node);
void node_update_visibility_bmesh(Node &node);

void update_node_bounds_mesh(Span<float3> positions, Node &node);
void update_node_bounds_grids(const CCGKey &key, Span<CCGElem *> grids, Node &node);
void update_node_bounds_bmesh(Node &node);

}  // namespace blender::bke::pbvh
