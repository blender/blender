/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include <fmt/format.h>

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_index_mask.hh"
#include "BLI_map.hh"
#include "BLI_math_base.hh"
#include "BLI_math_base_c.hh"
#include "BLI_math_geom.hh"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.hh"
#include "BLI_math_vector.hh"
#include "BLI_math_vector_c.hh"
#include "BLI_ordered_edge.hh"
#include "BLI_span.hh"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_filters.hh"
#include "BKE_attribute_math.hh"

#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "GEO_mesh_bevel.hh"
#include "GEO_mesh_selection.hh"

// #define DEBUG_TIME
#ifdef DEBUG_TIME
#  include "BLI_timeit.hh"
#endif

namespace blender::geometry {

enum class FKind {
  Orig = 0,
  Edge = 1,
  Vert = 2,
  Profile = 3,
};

enum class AngleKind {
  Smaller = -1,
  Straight = 0,
  Larger = 1,
};

struct ProfileSpacing {
  Array<double> xvals;
  Array<double> yvals;
  Array<double> xvals_2;
  Array<double> yvals_2;
  int seg_2;
  float fullness;
};

struct UVFace {
  int f;
  int attached_frep;
};

using UVVertBucket = Set<int>;
using UVVertMap = Map<int, Vector<UVVertBucket>>;

enum class MeshKind {
  None = 0,
  Poly,
  Adj,
  TriFan,
  Cutoff,
};

enum class VMeshMethod {
  Adj,
  Cutoff,
};

struct NewVert {
  int v = -1;
  float3 co = float3(0.0f);
};

struct Profile {
  float super_r = 0.0f;
  float height = 0.0f;
  float3 start = float3(0.0f);
  float3 middle = float3(0.0f);
  float3 end = float3(0.0f);
  float3 plane_no = float3(0.0f);
  float3 plane_co = float3(0.0f);
  float3 proj_dir = float3(0.0f);
  Array<float3> prof_co;
  Array<float3> prof_co_2;
  bool special_params = false;
};

struct BoundVert;

struct EdgeHalf {
  EdgeHalf *next = nullptr;
  EdgeHalf *prev = nullptr;

  int e = -1;
  int fprev = -1;
  int fnext = -1;

  BoundVert *leftv = nullptr;
  BoundVert *rightv = nullptr;

  int profile_index = 0;
  int seg = 0;

  float offset_l = 0.0f;
  float offset_r = 0.0f;
  float offset_l_spec = 0.0f;
  float offset_r_spec = 0.0f;

  bool is_bev = false;
  bool is_rev = false;
  bool is_seam = false;
  bool visited_rpo = false;
};

struct BoundVert {
  BoundVert *next = nullptr;
  BoundVert *prev = nullptr;

  NewVert nv;

  EdgeHalf *efirst = nullptr;
  EdgeHalf *elast = nullptr;
  EdgeHalf *ebev = nullptr;

  int index = 0;
  Profile profile;

  bool any_seam = false;
  bool visited = false;
  bool is_arc_start = false;
  bool is_patch_start = false;
  bool is_profile_start = false;

  int seam_len = 0;
  int sharp_len = 0;
};

struct VMesh {
  Array<NewVert> mesh;
  BoundVert *boundstart = nullptr;
  int count = 0;
  int seg = 0;
  MeshKind mesh_kind = MeshKind::None;
};

struct BevVert {
  int v = -1;
  int edgecount = 0;
  int selcount = 0;
  int wirecount = 0;
  float offset = 0.0f;

  bool any_seam = false;
  bool visited = false;

  Array<EdgeHalf> edges;

  Array<int> wire_edges;

  std::unique_ptr<VMesh> vmesh;

  Vector<std::unique_ptr<BoundVert>> owned_bound_verts;
};

/** Identifies the role of a newly created face for the #BevelAttributeOutputs fields. */
enum class NewFaceKind : uint8_t {
  Other = 0,
  /** Face that fills the VMesh cap around a beveled vertex. */
  VertexFace = 1,
  /** Face (quad) that fills a bevel strip along a beveled edge. */
  EdgeFace = 2,
};

/** Identifies the role of a newly created edge for the #BevelAttributeOutputs fields. */
enum class NewEdgeKind : uint8_t {
  Other = 0,
  /** Outermost edge of a bevel strip — adjacent to the surviving original geometry. */
  OuterEdge = 1,
  /** Mid-ring edge of a bevel strip (at `k = nseg / 2`, only when `nseg >= 2`). */
  MidEdge = 2,
};

class ExtendableMesh {
 public:
  const Mesh &mesh;

  Span<float3> src_positions;
  Span<int2> src_edges;
  OffsetIndices<int> src_faces;
  Span<int> src_corner_verts;
  Span<int> src_corner_edges;

  Span<float3> src_vert_normals;
  Span<float3> src_face_normals;

  GroupedSpan<int> src_vert_to_edge;
  GroupedSpan<int> src_edge_to_face;
  GroupedSpan<int> src_vert_to_corner;
  Array<int> src_corner_to_face;

 private:
  Vector<float3> new_vert_positions_;
  Vector<int> new_face_offsets_;
  Vector<int> new_corner_verts_;
  Vector<int> new_corner_edges_;

  /* Representative original element indices for attribute propagation (-1 = unknown). */
  Vector<int> new_vert_examples_;
  Vector<int> new_edge_examples_;
  /* Per-new-edge seam/sharp overrides: -1=inherit, 0=force false, 1=force true. */
  Vector<int8_t> new_edge_seam_overrides_;
  Vector<int8_t> new_edge_sharp_overrides_;
  Vector<int> new_face_examples_;
  Vector<NewFaceKind> new_face_kinds_;
  Vector<NewEdgeKind> new_edge_kinds_;
  Vector<int> new_corner_examples_;

  /* Per-corner UV face representative (-1 = use face-level new_face_examples_). */
  Vector<int> new_corner_face_reps_;
  /* Per-corner snap-edge index (-1 = no snap).  Set by #face_set_corner_reps. */
  Vector<int> new_corner_snap_edges_;

  /* Per-UV-layer float2 values for new corners, indexed [layer][new_corner]. */
  Vector<Vector<float2>> new_corner_uvs_;

  /**
   * Maps each vertex (original or new) to the new-corner indices incident on it.
   * Populated in O(1) per corner by #face_create; never rebuilt.
   */
  Map<int, Vector<int>> new_vert_to_new_corners_;

  /**
   * Maps each new vertex (index >= mesh.verts_num) to the original bevel vertex (`bv->v`)
   * it descended from.  Populated in #vert_create when `example_vert` >= 0.
   */
  Map<int, int> new_vert_bev_origin_;

  Array<bool> kill_verts_;
  Array<bool> kill_edges_;
  Array<bool> kill_faces_;
  Array<bool> kill_corners_;

  using EdgeMap = VectorSet<OrderedEdge,
                            32,
                            DefaultProbingStrategy,
                            DefaultHash<OrderedEdge>,
                            DefaultEquality<OrderedEdge>,
                            SimpleVectorSetSlot<OrderedEdge, int>,
                            GuardedAllocator>;
  EdgeMap edge_lookup_;

  IndexMaskMemory memory_;
  Array<int> vert_to_edge_offsets_;
  Array<int> vert_to_edge_indices_;
  Array<int> edge_to_face_map_offsets_;
  Array<int> edge_to_face_map_indices_;

 public:
  explicit ExtendableMesh(const Mesh &mesh);

  float3 vert_position(const int v) const;
  int2 edge_verts(const int e) const;
  IndexRange face_corners(const int f) const;
  int corner_vert(const int c) const;
  int corner_edge(const int c) const;

  /* Creates a new vertex at `co`.  `example_vert` is the index of an original vertex
   * whose non-positional attributes will be copied to this new vertex at output time;
   * pass -1 when no representative is known. */
  int vert_create(const float3 &co, int example_vert = -1);
  /* Creates (or looks up) the edge between v1 and v2.  `example_edge` is the original
   * edge index to use for attribute copying; pass -1 when unknown. */
  int edge_create(const int v1, const int v2, int example_edge = -1);
  /* Creates a new face from the given vertex ring.  `example_face` is the original face
   * whose attributes will be copied; pass -1 when unknown (deferred). */
  int face_create(Span<int> verts, int example_face = -1);

  /**
   * Sets per-corner UV face representatives and snap-edge indices for a previously created face.
   * `face_idx` must be the value returned by #face_create.
   * `face_reps` and `snap_edges` are parallel to the face's vertices; either may be empty
   * (treated as all -1 = "use the face-level example" / "no snap").
   * This must be called immediately after #face_create and before any further #face_create call,
   * so that the corner index range is known.
   */
  void face_set_corner_reps(int face_idx, Span<int> face_reps, Span<int> snap_edges);

  /* Returns the index of any existing edge (original or newly created) between v1 and v2,
   * or -1 if no such edge exists yet. */
  int find_edge(const int v1, const int v2) const
  {
    return edge_lookup_.index_of_try(OrderedEdge(v1, v2));
  }

  /* Sets the example edge of a previously created new edge.
   * `edge_index` must be a new edge (>= mesh.edges_num) returned by #edge_create or #find_edge.
   * Has no effect on original mesh edges (edge_index < mesh.edges_num). */
  void edge_set_example(const int edge_index, const int example_edge)
  {
    const int new_idx = edge_index - mesh.edges_num;
    if (new_idx >= 0 && new_idx < int(new_edge_examples_.size())) {
      new_edge_examples_[new_idx] = example_edge;
    }
  }

  /* Override the `uv_seam` attribute for a new edge, independent of its example edge.
   * `value` = 0 → force false, 1 → force true.  -1 (default) = inherit from example.
   * Applied after attribute gathering in #build_output_mesh. */
  void edge_set_seam_override(const int edge_index, int8_t value)
  {
    const int new_idx = edge_index - mesh.edges_num;
    if (new_idx >= 0 && new_idx < int(new_edge_seam_overrides_.size())) {
      new_edge_seam_overrides_[new_idx] = value;
    }
  }

  /* Override the `sharp_edge` attribute for a new edge, independent of its example edge. */
  void edge_set_sharp_override(const int edge_index, int8_t value)
  {
    const int new_idx = edge_index - mesh.edges_num;
    if (new_idx >= 0 && new_idx < int(new_edge_sharp_overrides_.size())) {
      new_edge_sharp_overrides_[new_idx] = value;
    }
  }

  /* Allocates per-UV-layer float2 storage for new corners.  Must be called after
   * UVMapInfo is initialized and before any face_create call. */
  void init_uv_storage(int uv_layers_num);

  /* Per-UV-layer float2 values for new corners (one per layer, parallel to new_corner_verts_). */
  MutableSpan<float2> new_corner_uvs(int layer_index)
  {
    return new_corner_uvs_[layer_index].as_mutable_span();
  }
  Span<float2> new_corner_uvs(int layer_index) const
  {
    return new_corner_uvs_[layer_index].as_span();
  }

  /**
   * For each vertex (original or new), the list of new-corner indices (into
   * `new_corner_verts_` / `new_corner_uvs_`) that are incident on it.
   * Populated incrementally by #face_create in O(1) per corner.
   */
  const Map<int, Vector<int>> &vert_to_new_corners() const
  {
    return new_vert_to_new_corners_;
  }

  /**
   * For each new vertex (index >= mesh.verts_num), the index of the original bevel vertex it
   * descended from (i.e., `bv->v`).  Only populated when `example_vert` is non-negative in
   * #vert_create, which is always the case at the bevel arc call sites.
   * Used by #merge_uvs to look up the parent vertex's UV bucket structure.
   */
  const Map<int, int> &vert_bev_origin() const
  {
    return new_vert_bev_origin_;
  }

  void vert_kill(const int v);
  void edge_kill(const int e);
  void face_kill(const int f);
  void corner_kill(const int c);

  bool is_vert_killed(const int v) const;
  bool is_edge_killed(const int e) const;
  bool is_face_killed(const int f) const;
  bool is_corner_killed(const int c) const;

  /** Returns the number of newly created faces (not counting the original mesh faces). */
  int new_faces_num() const
  {
    /* new_face_offsets_ starts with a sentinel 0 at index 0 and gains one entry per face_create
     * call, so the number of new faces is new_face_offsets_.size() - 1. */
    return int(new_face_offsets_.size()) - 1;
  }

  Span<float3> new_vert_positions() const
  {
    return new_vert_positions_;
  }
  Span<int2> new_edges() const
  {
    return edge_lookup_.as_span().cast<int2>().drop_front(src_edges.size());
  }
  /** Face offset array for new faces: size is new_faces_num()+1, sentinel 0 at index 0. */
  Span<int> new_face_offsets() const
  {
    return new_face_offsets_;
  }
  Span<int> new_corner_verts() const
  {
    return new_corner_verts_;
  }
  Span<int> new_corner_edges() const
  {
    return new_corner_edges_;
  }

  /* Per-new-element representative (example) original indices for attribute propagation.
   * -1 means no representative is known. */
  Span<int> new_vert_examples() const
  {
    return new_vert_examples_;
  }
  Span<int> new_edge_examples() const
  {
    return new_edge_examples_;
  }
  Span<int8_t> new_edge_seam_overrides() const
  {
    return new_edge_seam_overrides_;
  }
  Span<int8_t> new_edge_sharp_overrides() const
  {
    return new_edge_sharp_overrides_;
  }
  Span<int> new_face_examples() const
  {
    return new_face_examples_;
  }
  Span<NewFaceKind> new_face_kinds() const
  {
    return new_face_kinds_;
  }
  MutableSpan<NewFaceKind> new_face_kinds()
  {
    return new_face_kinds_.as_mutable_span();
  }
  Span<NewEdgeKind> new_edge_kinds() const
  {
    return new_edge_kinds_;
  }
  /** Tags the most recently created face with the given kind. */
  void tag_last_face(const NewFaceKind kind)
  {
    if (!new_face_kinds_.is_empty()) {
      new_face_kinds_.last() = kind;
    }
  }
  /** Tags a new edge by its combined (original + new) index with the given kind.
   * Only new edges (index >= mesh.edges_num) are tagged; original edges are silently ignored. */
  void tag_edge_kind(const int edge_index, const NewEdgeKind kind)
  {
    const int ni = edge_index - mesh.edges_num;
    if (ni >= 0 && ni < int(new_edge_kinds_.size())) {
      new_edge_kinds_[ni] = kind;
    }
  }
  Span<int> new_corner_examples() const
  {
    return new_corner_examples_;
  }
  /** Per-corner UV face representative.  -1 means "fall back to the face-level example". */
  Span<int> new_corner_face_reps() const
  {
    return new_corner_face_reps_;
  }
  /** Per-corner snap-edge index.  -1 means no snapping is required. */
  Span<int> new_corner_snap_edges() const
  {
    return new_corner_snap_edges_;
  }
  Span<bool> kill_verts_array() const
  {
    return kill_verts_;
  }
  Span<bool> kill_edges_array() const
  {
    return kill_edges_;
  }
  Span<bool> kill_faces_array() const
  {
    return kill_faces_;
  }
  Span<bool> kill_corners_array() const
  {
    return kill_corners_;
  }
};

ExtendableMesh::ExtendableMesh(const Mesh &mesh)
    : mesh(mesh),
      src_positions(mesh.vert_positions()),
      src_edges(mesh.edges()),
      src_faces(mesh.faces()),
      src_corner_verts(mesh.corner_verts()),
      src_corner_edges(mesh.corner_edges()),
      src_vert_normals(mesh.vert_normals()),
      src_face_normals(mesh.face_normals())
{
  this->src_vert_to_edge = bke::mesh::build_vert_to_edge_map(
      this->src_edges, mesh.verts_num, vert_to_edge_offsets_, vert_to_edge_indices_);
  this->src_edge_to_face = bke::mesh::build_edge_to_face_map(this->src_faces,
                                                             this->src_corner_edges,
                                                             mesh.edges_num,
                                                             edge_to_face_map_indices_,
                                                             edge_to_face_map_offsets_);
  this->src_vert_to_corner = mesh.vert_to_corner_map();
  this->src_corner_to_face = bke::mesh::build_corner_to_face_map(src_faces);

  kill_verts_ = Array<bool>(mesh.verts_num, false);
  kill_edges_ = Array<bool>(mesh.edges_num, false);
  kill_faces_ = Array<bool>(mesh.faces_num, false);
  kill_corners_ = Array<bool>(mesh.corners_num, false);

  edge_lookup_.reserve(mesh.edges_num);
  for (const int e : this->src_edges.index_range()) {
    edge_lookup_.add_new(OrderedEdge(this->src_edges[e]));
  }

  new_face_offsets_.append(0);
}

float3 ExtendableMesh::vert_position(const int v) const
{
  if (v < mesh.verts_num) {
    return this->src_positions[v];
  }
  return new_vert_positions_[v - mesh.verts_num];
}

int2 ExtendableMesh::edge_verts(const int e) const
{
  if (e < mesh.edges_num) {
    return this->src_edges[e];
  }
  return int2(edge_lookup_[e].v_low, edge_lookup_[e].v_high);
}

IndexRange ExtendableMesh::face_corners(const int f) const
{
  if (f < mesh.faces_num) {
    return this->src_faces[f];
  }
  const int f_new = f - mesh.faces_num;
  const int start = new_face_offsets_[f_new];
  const int size = new_face_offsets_[f_new + 1] - start;
  return IndexRange(mesh.corners_num + start, size);
}

int ExtendableMesh::corner_vert(const int c) const
{
  if (c < mesh.corners_num) {
    return this->src_corner_verts[c];
  }
  return new_corner_verts_[c - mesh.corners_num];
}

int ExtendableMesh::corner_edge(const int c) const
{
  if (c < mesh.corners_num) {
    return this->src_corner_edges[c];
  }
  return new_corner_edges_[c - mesh.corners_num];
}

int ExtendableMesh::vert_create(const float3 &co, const int example_vert)
{
  const int index = mesh.verts_num + new_vert_positions_.size();
  new_vert_positions_.append(co);
  new_vert_examples_.append(example_vert);
  if (example_vert >= 0) {
    new_vert_bev_origin_.add_overwrite(index, example_vert);
  }
  return index;
}

int ExtendableMesh::edge_create(const int v1, const int v2, const int example_edge)
{
  const int start_size = edge_lookup_.size();
  const int index = edge_lookup_.index_of_or_add(OrderedEdge(v1, v2));
  if (edge_lookup_.size() != start_size) {
    new_edge_examples_.append(example_edge);
    new_edge_seam_overrides_.append(-1);
    new_edge_sharp_overrides_.append(-1);
    new_edge_kinds_.append(NewEdgeKind::Other);
  }
  return index;
}

int ExtendableMesh::face_create(const Span<int> verts, const int example_face)
{
  const int face_index = mesh.faces_num + new_face_offsets_.size() - 1;

  for (const int i : verts.index_range()) {
    const int v1 = verts[i];
    const int v2 = verts[(i + 1) % verts.size()];
    /* Edges created inside face_create have no single representative edge; use -1. */
    const int e = this->edge_create(v1, v2, -1);

    const int nc = int(new_corner_verts_.size());
    new_corner_verts_.append(v1);
    new_corner_edges_.append(e);
    /* Corner examples are deferred; -1 for now. */
    new_corner_examples_.append(-1);
    /* Per-corner face rep and snap edge: -1 until overridden by face_set_corner_reps. */
    new_corner_face_reps_.append(-1);
    new_corner_snap_edges_.append(-1);
    /* Grow UV storage to match (values initialized to zero). */
    for (Vector<float2> &layer_uvs : new_corner_uvs_) {
      layer_uvs.append(float2(0.0f));
    }
    /* Record v1 → new corner for the UV merge pass. */
    new_vert_to_new_corners_.lookup_or_add_default(v1).append(nc);
  }

  new_face_offsets_.append(int(new_corner_verts_.size()));
  new_face_examples_.append(example_face);
  new_face_kinds_.append(NewFaceKind::Other);

  return face_index;
}

void ExtendableMesh::init_uv_storage(const int uv_layers_num)
{
  new_corner_uvs_.resize(uv_layers_num);
}

void ExtendableMesh::face_set_corner_reps(const int face_idx,
                                          const Span<int> face_reps,
                                          const Span<int> snap_edges)
{
  const int f_new = face_idx - mesh.faces_num;
  BLI_assert(f_new >= 0 && f_new < new_faces_num());
  /* The corner range for this face in the new_corner_* arrays is:
   * [new_face_offsets_[f_new], new_face_offsets_[f_new+1]). */
  const int c_start = new_face_offsets_[f_new];
  const int c_end = new_face_offsets_[f_new + 1];
  const int n = c_end - c_start;
  for (int i = 0; i < n; i++) {
    if (!face_reps.is_empty()) {
      new_corner_face_reps_[c_start + i] = face_reps[i];
    }
    if (!snap_edges.is_empty()) {
      new_corner_snap_edges_[c_start + i] = snap_edges[i];
    }
  }
}

void ExtendableMesh::vert_kill(const int v)
{
  if (v < mesh.verts_num) {
    kill_verts_[v] = true;
    /* Also kill all edges incident to this vertex, matching BMesh's BM_vert_kill semantics. */
    for (const int e : this->src_vert_to_edge[v]) {
      kill_edges_[e] = true;
    }
  }
}

void ExtendableMesh::edge_kill(const int e)
{
  if (e < mesh.edges_num) {
    kill_edges_[e] = true;
  }
}

void ExtendableMesh::face_kill(const int f)
{
  if (f < mesh.faces_num) {
    kill_faces_[f] = true;
    /* Also kill all corners of this face, matching BMesh's BM_face_kill semantics. */
    for (const int c : this->src_faces[f]) {
      kill_corners_[c] = true;
    }
  }
}

void ExtendableMesh::corner_kill(const int c)
{
  if (c < mesh.corners_num) {
    kill_corners_[c] = true;
  }
}

bool ExtendableMesh::is_vert_killed(const int v) const
{
  return v < mesh.verts_num ? kill_verts_[v] : false;
}

bool ExtendableMesh::is_edge_killed(const int e) const
{
  return e < mesh.edges_num ? kill_edges_[e] : false;
}

bool ExtendableMesh::is_face_killed(const int f) const
{
  return f < mesh.faces_num ? kill_faces_[f] : false;
}

bool ExtendableMesh::is_corner_killed(const int c) const
{
  return c < mesh.corners_num ? kill_corners_[c] : false;
}

namespace uv {

class UVMapInfo {
 public:
  bool has_uv_maps = false;
  Array<int> face_component;

  struct UVLayer {
    std::string name;
    Array<float2> values;
  };
  Vector<UVLayer> uv_maps;

  void init(const Mesh &mesh);

  /**
   * Determine connected components of faces, where faces in the same
   * component have contiguous UV coordinates across shared edges for ALL UV maps.
   */
  void find_components(const ExtendableMesh &emesh);

  /** Returns true when UV data is contiguous across edge `e` between faces `f1` and `f2`. */
  bool contig_uv_maps_across_edge(const ExtendableMesh &mesh, int e, int f1, int f2) const;

  /**
   * Returns true when UV data is contiguous at vertex `v`:
   * all corners at `v` have the same UV value in every layer.
   * Mirrors #contig_ldata_around_vert from `bmesh_bevel.cc`.
   */
  bool contig_uv_maps_around_vert(const ExtendableMesh &emesh, int v) const;
};

void UVMapInfo::init(const Mesh &mesh)
{
  has_uv_maps = false;
  const bke::AttributeAccessor attrs = mesh.attributes();
  attrs.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (!bke::mesh::is_uv_map(bke::AttributeMetaData(iter.domain, iter.data_type))) {
      return;
    }
    const bke::AttributeReader<float2> uv_reader = iter.get<float2>();
    if (uv_reader) {
      this->uv_maps.append(UVLayer{
          .name = iter.name,
          .values = VArraySpan<float2>(*uv_reader),
      });
      this->has_uv_maps = true;
    }
  });
}

bool UVMapInfo::contig_uv_maps_across_edge(const ExtendableMesh &emesh,
                                           const int e,
                                           const int f1,
                                           const int f2) const
{
  if (!has_uv_maps) {
    return true;
  }

  const int2 edge_verts = emesh.src_edges[e];
  const int v1 = edge_verts[0];
  const int v2 = edge_verts[1];

  const Span<int> corner_verts = emesh.src_corner_verts;
  const IndexRange f1_corners = emesh.src_faces[f1];
  const IndexRange f2_corners = emesh.src_faces[f2];

  const int c1_v1 = bke::mesh::face_find_corner_from_vert(f1_corners, corner_verts, v1);
  const int c1_v2 = bke::mesh::face_find_corner_from_vert(f1_corners, corner_verts, v2);
  const int c2_v1 = bke::mesh::face_find_corner_from_vert(f2_corners, corner_verts, v1);
  const int c2_v2 = bke::mesh::face_find_corner_from_vert(f2_corners, corner_verts, v2);

  if (c1_v1 == -1 || c1_v2 == -1 || c2_v1 == -1 || c2_v2 == -1) {
    return false;
  }

  for (const UVLayer &attr : uv_maps) {
    if (attr.values[c1_v1] != attr.values[c2_v1]) {
      return false;
    }
    if (attr.values[c1_v2] != attr.values[c2_v2]) {
      return false;
    }
  }

  return true;
}

void UVMapInfo::find_components(const ExtendableMesh &emesh)
{
  if (!has_uv_maps) {
    return;
  }

  const Span<float3> positions = emesh.src_positions;
  const OffsetIndices<int> faces = emesh.src_faces;
  const Span<int> corner_verts = emesh.src_corner_verts;
  const Span<int> corner_edges = emesh.src_corner_edges;
  this->face_component = Array<int>(faces.size(), -1);
  if (faces.is_empty()) {
    return;
  }

  const GroupedSpan<int> edge_faces = emesh.src_edge_to_face;

  Array<bool> in_stack(faces.size(), false);
  Vector<int> stack;
  stack.reserve(faces.size());

  int current_component = -1;
  for (const int f : faces.index_range()) {
    if (this->face_component[f] == -1 && !in_stack[f]) {
      current_component++;
      stack.append(f);
      in_stack[f] = true;

      while (!stack.is_empty()) {
        int f_curr = stack.pop_last();
        in_stack[f_curr] = false;

        if (this->face_component[f_curr] != -1) {
          continue;
        }
        this->face_component[f_curr] = current_component;

        /* Find neighbors via edges. */
        const Span<int> face_edges = corner_edges.slice(faces[f_curr]);
        for (const int e_index : face_edges) {
          const Span<int> adj_faces = edge_faces[e_index];
          for (const int f_other : adj_faces) {
            if (f_other != f_curr) {
              if (face_component[f_other] != -1 || in_stack[f_other]) {
                continue;
              }
              if (contig_uv_maps_across_edge(emesh, e_index, f_curr, f_other)) {
                stack.append(f_other);
                in_stack[f_other] = true;
              }
            }
          }
        }
      }
    }
  }

  /* We can usually get more pleasing result if components 0 and 1
   * are the topmost and bottom-most (in z-coordinate) components,
   * so adjust component indices to make that so. */
  if (current_component <= 0) {
    /* Only one component, so no need to do this. */
    return;
  }

  float top_face_z = -1e30f;
  int top_face_component = -1;
  float bot_face_z = 1e30f;
  int bot_face_component = -1;

  for (const int f : faces.index_range()) {
    float min_z = 1e30f;
    float max_z = -1e30f;
    for (const int corner : faces[f]) {
      const float fz = positions[corner_verts[corner]].z;
      min_z = std::min(min_z, fz);
      max_z = std::max(max_z, fz);
    }
    const float fz = (min_z + max_z) * 0.5f;

    if (fz > top_face_z) {
      top_face_z = fz;
      top_face_component = this->face_component[f];
    }
    if (fz < bot_face_z) {
      bot_face_z = fz;
      bot_face_component = this->face_component[f];
    }
  }

  auto swap_face_components = [&](int c1, int c2) {
    if (c1 == c2) {
      return;
    }
    for (int &c : this->face_component) {
      if (c == c1) {
        c = c2;
      }
      else if (c == c2) {
        c = c1;
      }
    }
  };

  swap_face_components(this->face_component[0], top_face_component);
  if (bot_face_component != top_face_component) {
    if (bot_face_component == 0) {
      /* It was swapped with old top_face_component. */
      bot_face_component = top_face_component;
    }
    swap_face_components(this->face_component[1], bot_face_component);
  }
}

bool UVMapInfo::contig_uv_maps_around_vert(const ExtendableMesh &emesh, const int v) const
{
  if (!has_uv_maps) {
    return true;
  }
  /* Gather all corners at v. */
  const Span<int> corners = emesh.src_vert_to_corner[v];
  if (corners.size() < 2) {
    return true;
  }
  const int c_first = corners[0];
  for (const UVLayer &attr : uv_maps) {
    const float2 &uv_ref = attr.values[c_first];
    for (const int c : corners.drop_front(1)) {
      if (attr.values[c] != uv_ref) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace uv

struct BevelState {
  /* Input parameters. */
  BevelParameters params;

  /* Input selection. */
  IndexMask selection;

  /* Bevel affected vertices mask and its memory. */
  index_mask::IndexMaskMemory memory;
  IndexMask bevel_affected_vertices;
  IndexMask bevel_affected_faces;
  Array<float3> face_centers;

  /* The encapsulated extendable mesh. */
  ExtendableMesh emesh;

  /* Memory Ownership. */
  Vector<BevVert> bev_verts;
  Map<int, BevVert *> vert_hash;

  Vector<UVVertMap> uv_vert_maps;

  ProfileSpacing pro_spacing;
  ProfileSpacing pro_spacing_miter;
  uv::UVMapInfo uv_layer_info;

  /* Additional State mimicking bmesh_bevel that isn't fully contained in BevelParameters. */
  bool affect_vertices_odd;
  float pro_super_r;

  /* Fast-path miter flags: true when all corners have the same miter setting.
   * These avoid per-corner lookups in the common cases. */
  bool all_miters_on;
  bool all_miters_off;

  /* Feature flags and parameters that the node version might use or we keep to mimic bmesh_bevel.
   */
  bool limit_offset;
  bool mark_seam;
  bool mark_sharp;

  /** Source-edge indices of the two outer edges of each bevel strip, accumulated during
   * #bevel_build_edge_polygons for use by the #BevelAttributeOutputs `outer_edge_id` field. */
  Vector<int> outer_edge_src_indices;

  VMeshMethod vmesh_method;

  BevelState(const Mesh &mesh, const BevelParameters &params, const IndexMask &selection);
  float3 face_center(const int f) const;
  void initialize_profile_data();
  void uv_init();
};

BevelState::BevelState(const Mesh &mesh, const BevelParameters &params, const IndexMask &selection)
    : params(params), selection(selection), emesh(mesh)
{
  if (params.affect_type == BevelAffect::Vertices) {
    this->bevel_affected_vertices = selection;
  }
  else {
    /* Mirror the BMesh operator's manifold filter (see #bmo_bevel_exec): only edges with
     * exactly two incident faces can be beveled. Boundary edges (one face) and wire edges
     * (zero faces) are silently excluded, matching the behavior of the bevel operator. */
    const GroupedSpan<int> edge_faces = emesh.src_edge_to_face;
    this->selection = IndexMask::from_predicate(
        selection, memory, [&](const int i) { return edge_faces[i].size() == 2; });

    const Span<int2> src_edges = mesh.edges();
    this->bevel_affected_vertices = vert_selection_from_edge(
        src_edges, this->selection, mesh.verts_num, memory);
  }

  /* Calculate affected faces and their centers. */
  Array<bool> is_vert_affected(mesh.verts_num, false);
  this->bevel_affected_vertices.to_bools(is_vert_affected.as_mutable_span());

  const Span<int> corner_verts = mesh.corner_verts();
  const OffsetIndices faces = mesh.faces();
  this->bevel_affected_faces = IndexMask::from_predicate(
      faces.index_range(), memory, [&](const int i) {
        return std::ranges::any_of(corner_verts.slice(faces[i]),
                                   [&](const int v) { return is_vert_affected[v]; });
      });

  this->face_centers = Array<float3>(mesh.faces_num, float3(0.0f));
  const Span<float3> positions = mesh.vert_positions();
  this->bevel_affected_faces.foreach_index(
      [&](const int f) {
        this->face_centers[f] = bke::mesh::face_center_calc(positions,
                                                            corner_verts.slice(faces[f]));
      },
      exec_mode::grain_size(1024));

  this->affect_vertices_odd = false;
  this->limit_offset = false;
  this->mark_seam = false;
  this->mark_sharp = false;
  this->vmesh_method = VMeshMethod::Adj;
  if (this->vmesh_method == VMeshMethod::Cutoff) {
    /* ignoring miters */
    this->params.miter.fill(false);
  }

  /* Precompute fast-path miter flags. */
  const array_utils::BooleanMix miter_mix = array_utils::booleans_mix_calc(
      VArray<bool>::from_span(this->params.miter));
  this->all_miters_off = this->params.miter.is_empty() ||
                         miter_mix == array_utils::BooleanMix::AllFalse;
  this->all_miters_on = !this->params.miter.is_empty() &&
                        miter_mix == array_utils::BooleanMix::AllTrue;
}

namespace geom {

constexpr float BEVEL_EPSILON_D = 1e-6f;
constexpr float BEVEL_EPSILON_SQ = 1e-12f;
constexpr float BEVEL_EPSILON_BIG = 1e-4f;
constexpr float BEVEL_EPSILON_ANG = DEG2RADF(2.0f);
constexpr float BEVEL_SMALL_ANG = DEG2RADF(10.0f);
const float BEVEL_SMALL_ANG_DOT = (1.0f - std::cos(BEVEL_SMALL_ANG));
const float BEVEL_EPSILON_ANG_DOT = (1.0f - std::cos(BEVEL_EPSILON_ANG));

static int edge_other_vert(const ExtendableMesh &emesh, int e, int v)
{
  int2 verts = emesh.edge_verts(e);
  return verts[0] == v ? verts[1] : verts[0];
}

/* Returns coordinates of a point a distance d from v along edge e. */
static float3 slide_dist(const ExtendableMesh &emesh, int e, int v, float d)
{
  float3 v_co = emesh.vert_position(v);
  float3 other_co = emesh.vert_position(geom::edge_other_vert(emesh, e, v));
  float len;
  const float3 dir = math::normalize_and_get_length(other_co - v_co, len);

  if (d > len) {
    d = len - 50.0f * BEVEL_EPSILON_D;
  }
  return v_co + dir * d;
}

/* Is co not on the edge e? If not, return the closer end of e in ret_closer_v. */
static bool is_outside_edge(const ExtendableMesh &emesh,
                            EdgeHalf *eh,
                            const float co[3],
                            int *ret_closer_v)
{
  // Actually, BMesh's is_outside_edge uses e->v1 and e->v2.
  int v1 = emesh.src_edges[eh->e][0];
  int v2 = emesh.src_edges[eh->e][1];
  float3 l1 = emesh.vert_position(v1);
  float3 u_dir = emesh.vert_position(v2) - l1;
  float3 h = float3(co[0], co[1], co[2]) - l1;
  float lenu = math::length(u_dir);
  u_dir /= lenu;
  float lambda = math::dot(u_dir, h);
  if (lambda <= -BEVEL_EPSILON_BIG * lenu) {
    *ret_closer_v = v1;
    return true;
  }
  if (lambda >= (1.0f + BEVEL_EPSILON_BIG) * lenu) {
    *ret_closer_v = v2;
    return true;
  }
  return false;
}

/* co should be approximately on the plane between e1 and e2, which share common vert v and common
 * face f (which cannot be -1). Is it between those edges, sweeping CCW? */
static bool point_between_edges(
    const ExtendableMesh &emesh, const float co[3], int v, int f, EdgeHalf *e1, EdgeHalf *e2)
{
  int v1 = geom::edge_other_vert(emesh, e1->e, v);
  int v2 = geom::edge_other_vert(emesh, e2->e, v);
  float3 dir1 = emesh.vert_position(v) - emesh.vert_position(v1);
  float3 dir2 = emesh.vert_position(v) - emesh.vert_position(v2);
  float3 dirco = emesh.vert_position(v) - float3(co[0], co[1], co[2]);
  dir1 = math::normalize(dir1);
  dir2 = math::normalize(dir2);
  dirco = math::normalize(dirco);
  float ang11 = float(math::angle_between(dir1, dir2));
  float ang1co = float(math::angle_between(dir1, dirco));
  float3 no = math::cross(dir1, dir2);
  if (math::dot(no, emesh.src_face_normals[f]) < 0.0f) {
    ang11 = float(M_PI * 2.0) - ang11;
  }
  no = math::cross(dir1, dirco);
  if (math::dot(no, emesh.src_face_normals[f]) < 0.0f) {
    ang1co = float(M_PI * 2.0) - ang1co;
  }
  return (ang11 - ang1co > -BEVEL_EPSILON_ANG);
}

static float3 offset_meet(const ExtendableMesh &emesh,
                          EdgeHalf *e1,
                          EdgeHalf *e2,
                          int v,
                          int f,
                          bool edges_between,
                          const EdgeHalf *e_in_plane)
{
  float3 v_co = emesh.vert_position(v);
  /* `dir1` points from e1's far end toward `v`; `dir2` points from `v` away along e2.
   * This asymmetry matches the BMesh convention and ensures that `cross(dir1, norm_v1)`
   * gives a perpendicular that points into the face (i.e. toward the bevel offset). */
  float3 dir1 = v_co - emesh.vert_position(geom::edge_other_vert(emesh, e1->e, v));
  float3 dir2 = emesh.vert_position(geom::edge_other_vert(emesh, e2->e, v)) - v_co;

  float3 dir1n = float3(0.0f);
  float3 dir2p = float3(0.0f);
  if (edges_between) {
    EdgeHalf *e1next = e1->next;
    EdgeHalf *e2prev = e2->prev;
    dir1n = emesh.vert_position(geom::edge_other_vert(emesh, e1next->e, v)) - v_co;
    dir2p = v_co - emesh.vert_position(geom::edge_other_vert(emesh, e2prev->e, v));
  }

  float ang = float(math::angle_between(math::normalize(dir1), math::normalize(dir2)));
  float3 norm_perp1;
  float3 meetco;
  if (ang < BEVEL_EPSILON_ANG) {
    /* Special case: e1 and e2 are parallel; put offset point perp to both, from v.
     * Need to find a suitable plane.
     * Use the average of the two directions and the offset formula for angle bisector.
     * If offsets are different, use the max (so get consistent looking results if the same
     * situation arises elsewhere but with opposite roles for e1 and e2). */
    float3 norm_v = float3(0.0f);
    if (f != -1) {
      norm_v = emesh.src_face_normals[f];
    }
    else {
      int fcount = 0;
      for (EdgeHalf *eloop = e1; eloop != e2; eloop = eloop->next) {
        if (eloop->fnext != -1) {
          norm_v += emesh.src_face_normals[eloop->fnext];
          fcount++;
        }
      }
      if (fcount == 0) {
        norm_v = emesh.src_vert_normals[v];
      }
      else {
        norm_v /= float(fcount);
      }
    }
    float3 dir_sum = dir1 + dir2;
    norm_perp1 = math::normalize(math::cross(dir_sum, norm_v));
    float d = math::max(e1->offset_r, e2->offset_l);
    d = d / math::cos(ang / 2.0f);
    meetco = v_co + norm_perp1 * d;
  }
  else if (math::abs(ang - float(M_PI)) < BEVEL_EPSILON_ANG) {
    /* Special case: e1 and e2 are anti-parallel, so bevel is into a zero-area face.
     * Just make the offset point on the common line, at offset distance from v. */
    float d = math::max(e1->offset_r, e2->offset_l);
    meetco = slide_dist(emesh, e2->e, v, d);
  }
  else {
    /* Get normal to plane where meet point should be, using cross product instead of the face
     * normal in case f is non-planar.
     * Except: sometimes locally there can be a small angle between dir1 and dir2 that leads
     * to a normal almost perpendicular to the face normal; in this case it looks wrong to use
     * the local (cross-product) normal, so use the face normal if the angle is small.
     * If e1-v-e2 is a reflex angle (viewed from vertex normal side), need to flip.
     * Use the face normal to figure out which side to look at angle from. */
    float3 norm_v1, norm_v2;
    if (f != -1 && ang < BEVEL_SMALL_ANG) {
      norm_v1 = norm_v2 = emesh.src_face_normals[f];
    }
    else if (!edges_between) {
      /* Get normal as cross product of the two edge directions. */
      norm_v1 = math::normalize(math::cross(dir2, dir1));
      if (math::dot(norm_v1, f != -1 ? emesh.src_face_normals[f] : emesh.src_vert_normals[v]) <
          0.0f)
      {
        norm_v1 = -norm_v1;
      }
      norm_v2 = norm_v1;
    }
    else {
      /* Separate faces; get face normals at corners for each edge separately. */
      norm_v1 = math::normalize(math::cross(dir1n, dir1));
      int f_curr = e1->fnext;
      if (math::dot(norm_v1,
                    f_curr != -1 ? emesh.src_face_normals[f_curr] : emesh.src_vert_normals[v]) <
          0.0f)
      {
        norm_v1 = -norm_v1;
      }
      norm_v2 = math::normalize(math::cross(dir2, dir2p));
      f_curr = e2->fprev;
      if (math::dot(norm_v2,
                    f_curr != -1 ? emesh.src_face_normals[f_curr] : emesh.src_vert_normals[v]) <
          0.0f)
      {
        norm_v2 = -norm_v2;
      }
    }

    /* Get vectors perp to each edge, perp to norm_v, pointing into face. */
    float3 norm_perp2;
    norm_perp1 = math::normalize(math::cross(dir1, norm_v1));
    norm_perp2 = math::normalize(math::cross(dir2, norm_v2));

    float3 off1a = v_co + norm_perp1 * e1->offset_r;
    float3 off1b = off1a + dir1;
    float3 off2a = v_co + norm_perp2 * e2->offset_l;
    float3 off2b = off2a + dir2;

    float3 isect2;
    /* Intersect the offset lines. */
    int isect_kind = math::isect_line_line(off1a, off1b, off2a, off2b, meetco, isect2);
    if (isect_kind == 0) {
      /* Lines are collinear: we already tested for this, but with a different epsilon. */
      meetco = off1a;
    }
    else {
      /* The lines intersect, but check that the intersection is at a reasonable place.
       * One problem: if one of the offsets is 0, don't want an intersection outside that
       * edge itself. This can happen if the angle between them is > 180 degrees, or if
       * the offset amount is > the edge length. */
      int closer_v;
      if (e1->offset_r == 0.0f && is_outside_edge(emesh, e1, meetco, &closer_v)) {
        meetco = emesh.vert_position(closer_v);
      }
      if (e2->offset_l == 0.0f && is_outside_edge(emesh, e2, meetco, &closer_v)) {
        meetco = emesh.vert_position(closer_v);
      }
      if (edges_between && e1->offset_r > 0.0f && e2->offset_l > 0.0f) {
        /* Try to drop meetco to a face between e1 and e2. */
        if (isect_kind == 2) {
          /* Lines didn't meet in 3D: get average of meetco and isect2. */
          meetco = math::midpoint(meetco, isect2);
        }
        for (EdgeHalf *e_loop = e1; e_loop != e2; e_loop = e_loop->next) {
          int fnext = e_loop->fnext;
          if (fnext == -1) {
            continue;
          }
          float3 no = emesh.src_face_normals[fnext];
          float4 plane = math::plane_from_point_normal(v_co, no);
          float3 dropco = math::closest_to_plane_normalized(plane, meetco);
          /* Don't drop to faces next to the in-plane edge. */
          if (e_in_plane) {
            float ang = float(math::angle_between(no, emesh.src_face_normals[e_in_plane->fnext]));
            if ((math::abs(ang) < BEVEL_SMALL_ANG) ||
                (math::abs(ang - float(M_PI)) < BEVEL_SMALL_ANG))
            {
              continue;
            }
          }
          if (point_between_edges(emesh, dropco, v, fnext, e_loop, e_loop->next)) {
            meetco = dropco;
            break;
          }
        }
      }
    }
  }
  return meetco;
}

/* -------------------------------------------------------------------- */
/** \name VMesh grid helpers
 * \{ */

/**
 * Return a pointer to the #NewVert at grid position (i, j, k) in `vm`.
 * The grid layout is `mesh[i * nj * nk + j * nk + k]`
 * where `nj = seg/2 + 1` and `nk = seg + 1`.
 */
static NewVert *mesh_vert(VMesh *vm, int i, int j, int k)
{
  const int nk = vm->seg + 1;
  const int nj = vm->seg / 2 + 1;
  return &vm->mesh[i * nj * nk + j * nk + k];
}

/**
 * Return the canonical representative for vmesh position (i, j, k).
 * Due to the rotational symmetry of the vmesh grid, many positions are
 * equivalent; this function maps any (i, j, k) to the one canonical
 * representative in the stored range.
 */
static NewVert *mesh_vert_canon(VMesh *vm, int i, int j, int k)
{
  const int n = vm->count;
  const int ns = vm->seg;
  const int ns2 = ns / 2;
  const int odd = ns % 2;
  BLI_assert(0 <= i && i <= n && 0 <= j && j <= ns && 0 <= k && k <= ns);

  if (!odd && j == ns2 && k == ns2) {
    return mesh_vert(vm, 0, j, k);
  }
  if (j <= ns2 - 1 + odd && k <= ns2) {
    return mesh_vert(vm, i, j, k);
  }
  if (k <= ns2) {
    return mesh_vert(vm, (i + n - 1) % n, k, ns - j);
  }
  return mesh_vert(vm, (i + 1) % n, ns - k, j);
}

/* Returns true when (i, j, k) is the canonical representative of its equivalence class. */
static bool is_canon(const VMesh *vm, int i, int j, int k)
{
  const int ns2 = vm->seg / 2;
  if (vm->seg % 2 == 1) {
    return (j <= ns2 && k <= ns2);
  }
  return ((j < ns2 && k <= ns2) || (j == ns2 && k == ns2 && i == 0));
}

/* Copies coordinates and vertex indices from canonical grid positions to all equivalent ones. */
static void vmesh_copy_equiv_verts(VMesh *vm)
{
  const int n = vm->count;
  const int ns = vm->seg;
  const int ns2 = ns / 2;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j <= ns2; j++) {
      for (int k = 0; k <= ns; k++) {
        if (is_canon(vm, i, j, k)) {
          continue;
        }
        NewVert *v1 = mesh_vert(vm, i, j, k);
        NewVert *v0 = mesh_vert_canon(vm, i, j, k);
        v1->co = v0->co;
        v1->v = v0->v;
      }
    }
  }
}

/* Returns the centroid of the center polygon of vm. */
static float3 vmesh_center(VMesh *vm)
{
  const int n = vm->count;
  const int ns2 = vm->seg / 2;
  if (vm->seg % 2) {
    float3 cent(0.0f);
    for (int i = 0; i < n; i++) {
      cent += mesh_vert(vm, i, ns2, ns2)->co;
    }
    return cent * (1.0f / float(n));
  }
  return mesh_vert(vm, 0, ns2, ns2)->co;
}

/* Returns the average of four NewVert positions. */
static float3 avg4(const NewVert *v0, const NewVert *v1, const NewVert *v2, const NewVert *v3)
{
  return (v0->co + v1->co + v2->co + v3->co) * 0.25f;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Profile geometry helpers
 * \{ */

/** Returns true when `d1` and `d2` are parallel or anti-parallel. */
static bool nearly_parallel(const float3 &d1, const float3 &d2)
{
  const float ang = float(math::angle_between(math::normalize(d1), math::normalize(d2)));
  return (math::abs(ang) < BEVEL_EPSILON_ANG) ||
         (math::abs(ang - float(M_PI)) < BEVEL_EPSILON_ANG);
}

/**
 * Builds a 4x4 matrix that maps the unit square to the triangle `(va, vmid, vb)`.
 * Returns false when the three points are collinear or degenerate.
 */
static bool make_unit_square_map(const float3 &va,
                                 const float3 &vmid,
                                 const float3 &vb,
                                 float4x4 &r_mat)
{
  const float3 va_vmid = vmid - va;
  const float3 vb_vmid = vmid - vb;

  if (math::is_zero(va_vmid) || math::is_zero(vb_vmid)) {
    return false;
  }
  if (math::abs(float(math::angle_between(math::normalize(va_vmid), math::normalize(vb_vmid))) -
                float(M_PI)) <= BEVEL_EPSILON_ANG)
  {
    return false;
  }

  const float3 vo = va - vb_vmid;
  const float3 vddir = math::normalize(math::cross(vb_vmid, va_vmid));
  const float3 vd = vo + vddir;

  /* Columns of the 4x4 map matrix (column-major, same layout as legacy float[4][4]). */
  r_mat[0] = float4(vmid - va, 0.0f);
  r_mat[1] = float4(vmid - vb, 0.0f);
  r_mat[2] = float4(vmid + vd - va - vb, 0.0f);
  r_mat[3] = float4(va + vb - vmid, 1.0f);

  return true;
}

/**
 * Returns the Sabin-modified Catmull-Clark gamma value for an n-sided corner.
 * This controls the smoothness of the center vertex during subdivision.
 */
static float sabin_gamma(int n)
{
  if (n < 3) {
    return 0.0f;
  }
  if (n == 3) {
    return 0.065247584f;
  }
  if (n == 4) {
    return 0.25f;
  }
  if (n == 5) {
    return 0.401983447f;
  }
  if (n == 6) {
    return 0.523423277f;
  }
  const double k = cos(M_PI / double(n));
  const double k2 = k * k;
  const double k4 = k2 * k2;
  const double k6 = k4 * k2;
  const double y = pow(M_SQRT3 * sqrt(64.0 * k6 - 144.0 * k4 + 135.0 * k2 - 27.0) + 9.0 * k,
                       1.0 / 3.0);
  const double x = 0.480749856769136 * y - (0.231120424783545 * (12.0 * k2 - 9.0)) / y;
  return float((k * x + 2.0 * k2 - 1.0) / (x * x * (k * x + 1.0)));
}

/** \} */

}  // namespace geom

/* -------------------------------------------------------------------- */
/** \name Debug printing utilities
 * \{ */

namespace debug {

/* Prints a Span of a printable type, 10 items per line.
 * Each line is prefixed with the starting index in brackets.
 * A label line is printed before the span. */
template<typename T> [[maybe_unused]] static void print_span(Span<T> span, const char *label)
{
  if (span.size() == 0) {
    return;
  }
  fmt::print("{}:", label);
  for (const int i : span.index_range()) {
    if (i % 10 == 0) {
      fmt::print("\n[{}] ", i);
    }
    fmt::print("{} ", span[i]);
  }
  fmt::println("");
}

/* Prints a single float3 as "(x,y,z)" with no trailing newline. */
[[maybe_unused]] static void print_float3(const float3 &v)
{
  fmt::print("({},{},{})", v[0], v[1], v[2]);
}

/* Prints a Span<float3>, 10 items per line, preceded by a label. */
[[maybe_unused]] static void print_float3_span(Span<float3> span, const char *label)
{
  if (span.size() == 0) {
    return;
  }
  fmt::print("{}:", label);
  for (const int i : span.index_range()) {
    if (i % 10 == 0) {
      fmt::print("\n[{}] ", i);
    }
    print_float3(span[i]);
    fmt::print(" ");
  }
  fmt::println("");
}

/* Prints a single int2 pair as "(a,b)" with no trailing newline. */
[[maybe_unused]] static void print_int2(const int2 pair)
{
  fmt::print("({},{})", pair[0], pair[1]);
}

/* Prints a Span<int2>, 10 items per line, preceded by a label. */
[[maybe_unused]] static void print_int2_span(Span<int2> span, const char *label)
{
  if (span.size() == 0) {
    return;
  }
  fmt::print("{}:", label);
  for (const int i : span.index_range()) {
    if (i % 10 == 0) {
      fmt::print("\n[{}] ", i);
    }
    print_int2(span[i]);
    fmt::print(" ");
  }
  fmt::println("");
}

/* Prints a single IndexRange as "[first..last]" or "[]" if empty, with no trailing newline. */
[[maybe_unused]] static void print_indexrange(const IndexRange &range)
{
  if (range.size() == 0) {
    fmt::print("[]");
  }
  else {
    fmt::print("[{}..{}]", range.first(), range.last());
  }
}

/* Prints a GroupedSpan<int>, one group per line, preceded by a label. */
[[maybe_unused]] static void print_groupedspan(const GroupedSpan<int> &groupedspan,
                                               const char *label)
{
  if (groupedspan.size() == 0) {
    return;
  }
  fmt::println("{}:", label);
  for (const int i : groupedspan.index_range()) {
    fmt::print("[{}] ", i);
    for (int v : groupedspan[i]) {
      fmt::print("{} ", v);
    }
    fmt::println("");
  }
}

/* Returns a human-readable name for a #MeshKind value. */
[[maybe_unused]] static const char *mesh_kind_name(MeshKind kind)
{
  switch (kind) {
    case MeshKind::None:
      return "NONE";
    case MeshKind::Poly:
      return "POLY";
    case MeshKind::Adj:
      return "ADJ";
    case MeshKind::TriFan:
      return "TRI_FAN";
    case MeshKind::Cutoff:
      return "CUTOFF";
    default:
      return "?";
  }
}

/* Prints a single #Profile's key parameters. */
[[maybe_unused]] static void dump_profile(const Profile &prof)
{
  fmt::print("  Profile: super_r={} height={} special_params={}\n",
             prof.super_r,
             prof.height,
             prof.special_params);
  fmt::print("    start=");
  print_float3(prof.start);
  fmt::print(" middle=");
  print_float3(prof.middle);
  fmt::print(" end=");
  print_float3(prof.end);
  fmt::println("");
  fmt::print("    plane_no=");
  print_float3(prof.plane_no);
  fmt::print(" plane_co=");
  print_float3(prof.plane_co);
  fmt::print(" proj_dir=");
  print_float3(prof.proj_dir);
  fmt::println("");
  if (!prof.prof_co.is_empty()) {
    print_float3_span(prof.prof_co, "    prof_co");
  }
}

/* Prints a single #EdgeHalf's fields. */
[[maybe_unused]] static void dump_edge_half(const EdgeHalf &eh, const int index)
{
  fmt::println("  EdgeHalf[{}]: e={} fprev={} fnext={}", index, eh.e, eh.fprev, eh.fnext);
  fmt::println("    offset_l={} offset_r={} offset_l_spec={} offset_r_spec={}",
               eh.offset_l,
               eh.offset_r,
               eh.offset_l_spec,
               eh.offset_r_spec);
  fmt::println("    is_bev={} is_rev={} is_seam={} visited_rpo={}",
               eh.is_bev,
               eh.is_rev,
               eh.is_seam,
               eh.visited_rpo);
  fmt::println("    leftv={} rightv={}",
               eh.leftv ? eh.leftv->index : -1,
               eh.rightv ? eh.rightv->index : -1);
}

/* Prints a single #BoundVert's fields. */
[[maybe_unused]] static void dump_bound_vert(const BoundVert &bndv)
{
  fmt::print("  BoundVert[{}]: co=", bndv.index);
  print_float3(bndv.nv.co);
  fmt::println("");
  fmt::println("    efirst={} elast={} ebev={}",
               bndv.efirst ? bndv.efirst->e : -1,
               bndv.elast ? bndv.elast->e : -1,
               bndv.ebev ? bndv.ebev->e : -1);
  fmt::println("    any_seam={} visited={}", bndv.any_seam, bndv.visited);
  fmt::println("    is_arc_start={} is_patch_start={} is_profile_start={}",
               bndv.is_arc_start,
               bndv.is_patch_start,
               bndv.is_profile_start);
  fmt::println("    seam_len={} sharp_len={}", bndv.seam_len, bndv.sharp_len);
  dump_profile(bndv.profile);
}

/* Prints a #VMesh and all its #BoundVert chain, plus the full #NewVert grid. */
[[maybe_unused]] static void dump_vmesh(const VMesh &vm)
{
  fmt::println(
      "  VMesh: count={} seg={} mesh_kind={}", vm.count, vm.seg, mesh_kind_name(vm.mesh_kind));
  if (vm.boundstart == nullptr) {
    fmt::println("  (no boundverts)");
    return;
  }
  /* Walk the circular linked list of BoundVerts. */
  const BoundVert *bndv = vm.boundstart;
  do {
    dump_bound_vert(*bndv);
    bndv = bndv->next;
  } while (bndv != vm.boundstart);

  /* Print the NewVert grid if it has been allocated. */
  if (!vm.mesh.is_empty()) {
    const int n = vm.count;
    const int ns = vm.seg;
    const int ns2 = ns / 2;
    /* Non-const pointer needed by mesh_vert (accessor is not const-qualified). */
    VMesh *vmp = const_cast<VMesh *>(&vm);
    fmt::println("  NewVerts (i, j, k) for 0<=i<{} 0<=j<={} 0<=k<{}:", n, ns2, ns);
    for (int i = 0; i < n; i++) {
      for (int j = 0; j <= ns2; j++) {
        fmt::print("    ({},{}): ", i, j);
        for (int k = 0; k < ns; k++) {
          const NewVert *nv = geom::mesh_vert(vmp, i, j, k);
          fmt::print("({},({:.3f},{:.3f},{:.3f})) ", nv->v, nv->co[0], nv->co[1], nv->co[2]);
        }
        fmt::println("");
      }
    }
  }
}

/* Dumps a full #BevVert, including its #EdgeHalf array, wire edges, and #VMesh. */
[[maybe_unused]] static void dump_bev_vert(const BevVert &bv)
{
  fmt::println("BevVert: v={} edgecount={} selcount={} wirecount={}",
               bv.v,
               bv.edgecount,
               bv.selcount,
               bv.wirecount);
  fmt::println("  offset={} any_seam={} visited={}", bv.offset, bv.any_seam, bv.visited);

  /* Print the EdgeHalf array. */
  fmt::println("  edges ({}):", bv.edges.size());
  for (const int i : bv.edges.index_range()) {
    dump_edge_half(bv.edges[i], i);
  }

  /* Print wire edges. */
  if (!bv.wire_edges.is_empty()) {
    print_span<int>(bv.wire_edges, "  wire_edges");
  }

  /* Print the VMesh if present. */
  if (bv.vmesh) {
    dump_vmesh(*bv.vmesh);
  }
  else {
    fmt::println("  (no vmesh)");
  }
}

/**
 * Dumps the edge-strip quads associated with `bv` that were created by
 * #construct::bevel_build_edge_polygons.
 *
 * For each beveled #EdgeHalf of `bv`, uses `leftv->index` as the boundary row `i`
 * in the #VMesh, then for each segment k walks the ring to find successive boundary
 * vertex pairs (i,0,k) and (i,0,k+1).  It searches the new faces of `emesh` for
 * a quad whose vertex list contains both those vertices in the opposite cyclic
 * order (since bevel_build_edge_polygons builds the quad with bv2's verts first).
 * Each matching quad is dumped as a list of corners with their vertex and edge indices.
 */
[[maybe_unused]] static void dump_edge_polygons(const BevelState &state, const BevVert &bv)
{
  const ExtendableMesh &emesh = state.emesh;
  VMesh *vm = bv.vmesh.get();
  if (!vm) {
    return;
  }
  const int ns = vm->seg;

  fmt::println("  Edge polygons for bv={}:", bv.v);

  for (int ei = 0; ei < bv.edgecount; ei++) {
    const EdgeHalf &eh = bv.edges[ei];
    if (!eh.is_bev || !eh.leftv) {
      continue;
    }
    const int i = eh.leftv->index;
    fmt::println("    EdgeHalf[{}] e={} i={}:", ei, eh.e, i);

    for (int k = 0; k < ns; k++) {
      /* The two boundary verts on this endpoint's ring. */
      const int va = geom::mesh_vert(vm, i, 0, k)->v;
      const int vb = geom::mesh_vert(vm, i, 0, k + 1)->v;
      if (va < 0 || vb < 0) {
        continue;
      }
      /* Search new faces for a quad containing va and vb in reverse order (vb before va).
       * new_faces_num() gives the count of faces added by face_create. */
      bool found = false;
      const int first_new_face = emesh.mesh.faces_num;
      const int past_new_face = first_new_face + emesh.new_faces_num();

      for (int fi = first_new_face; fi < past_new_face && !found; fi++) {
        const IndexRange corners = emesh.face_corners(fi);

        /* Check if va and vb appear in this face in the order (vb, va). */
        const int sz = int(corners.size());
        int pos_va = -1, pos_vb = -1;
        for (int ci = 0; ci < sz; ci++) {
          const int cv = emesh.corner_vert(corners[ci]);
          if (cv == va) {
            pos_va = ci;
          }
          if (cv == vb) {
            pos_vb = ci;
          }
        }
        if (pos_va >= 0 && pos_vb >= 0 && (pos_vb + 1) % sz == pos_va) {
          found = true;
          fmt::print("      k={} face={} corners:", k, fi);
          for (int ci = 0; ci < sz; ci++) {
            const int c = corners[ci];
            const int cv = emesh.corner_vert(c);
            const int ce = emesh.corner_edge(c);
            fmt::print(" [c={} v={} e={}]", c, cv, ce);
          }
          fmt::println("");
        }
      }
      if (!found) {
        fmt::println("      k={} va={} vb={}: no matching face found", k, va, vb);
      }
    }
  }
}

/**
 * Dumps the rebuilt face `face_idx` of `emesh`.
 * Prints the face index and each corner's vertex index, edge index, and vertex coordinates.
 */
[[maybe_unused]] static void dump_rebuilt_face(const BevelState &state, const int face_idx)
{
  const ExtendableMesh &emesh = state.emesh;
  const IndexRange corners = emesh.face_corners(face_idx);
  fmt::print("  Rebuilt face={} ({} verts):", face_idx, corners.size());
  for (const int c : corners) {
    const int v = emesh.corner_vert(c);
    const int e = emesh.corner_edge(c);
    const float3 co = emesh.vert_position(v);
    fmt::print(" [v={} e={} co=({:.3f},{:.3f},{:.3f})]", v, e, co[0], co[1], co[2]);
  }
  fmt::println("");
}

/**
 * Dumps the representative (example) face index recorded for every newly created face
 * in `emesh`.  New faces have indices `[mesh.faces_num, mesh.faces_num + new_faces_num)`.
 * Prints one line per new face: "  new_face=<idx> example=<orig_face_idx>".
 */
[[maybe_unused]] static void dump_new_face_examples(const BevelState &state)
{
  const ExtendableMesh &emesh = state.emesh;
  const int n_new = emesh.new_faces_num();
  fmt::println("MESH new face examples ({} new faces):", n_new);
  const Span<int> exs = emesh.new_face_examples();
  const uv::UVMapInfo &uvi = state.uv_layer_info;
  for (int i = 0; i < n_new; i++) {
    const int face_idx = emesh.mesh.faces_num + i;
    const int ex = (i < int(exs.size())) ? exs[i] : -1;
    if (ex >= 0 && ex < emesh.mesh.faces_num) {
      const int comp = (!uvi.face_component.is_empty()) ? uvi.face_component[ex] : -1;
      const float3 cent = state.face_center(ex);
      fmt::println("  new_face={} example={} comp={} center=({:.3f},{:.3f},{:.3f})",
                   face_idx,
                   ex,
                   comp,
                   cent[0],
                   cent[1],
                   cent[2]);
    }
    else {
      fmt::println("  new_face={} example={}", face_idx, ex);
    }
  }
}

/**
 * Dumps the representative (example) edge index recorded for every newly created edge
 * in `emesh`.  New edges have indices `[mesh.edges_num, mesh.edges_num + new_edges_num)`.
 * Prints one line per new edge: "  new_edge=<idx> example=<orig_edge_idx>".
 */
[[maybe_unused]] static void dump_new_edge_examples(const BevelState &state)
{
  const ExtendableMesh &emesh = state.emesh;
  const Span<int2> new_edges = emesh.new_edges();
  const int n_new = int(new_edges.size());
  fmt::println("MESH new edge examples ({} new edges):", n_new);
  const Span<int> exs = emesh.new_edge_examples();
  for (int i = 0; i < n_new; i++) {
    const int edge_idx = emesh.mesh.edges_num + i;
    const int ex = (i < int(exs.size())) ? exs[i] : -1;
    fmt::println(
        "  new_edge={} (v{}--v{}) example={}", edge_idx, new_edges[i][0], new_edges[i][1], ex);
  }
}

}  // namespace debug

/** \} */

/* Forward declarations for profile:: helpers needed in the first construct:: block. */
namespace profile {
static void set_profile_params(const BevelState &state, const BevVert *bv, BoundVert *bndv);
static void move_profile_plane(BoundVert *bndv, float3 bmvert_co);
}  // namespace profile

namespace construct {

/* Forward declarations for miter adjustment functions defined after build_boundary. */
static void adjust_miter_coords(const BevelState &state, BevVert *bv, EdgeHalf *emiter);
static void adjust_miter_inner_coords(const BevelState &state, BevVert *bv, EdgeHalf *emiter);

/* Assume e1 and e2 both share some vert. Do they share a face?
 * If they share a face then there is some corner around e1 that is in a face
 * where the next or previous edge in the face must be e2. */
static bool edges_face_connected_at_vert(const ExtendableMesh &emesh, const int e1, const int e2)
{
  const GroupedSpan<int> edge_faces = emesh.src_edge_to_face;
  const Span<int> e1_faces = edge_faces[e1];
  const Span<int> e2_faces = edge_faces[e2];
  for (const int f1 : e1_faces) {
    if (e2_faces.contains(f1)) {
      return true;
    }
  }
  return false;
}

/* Return 1 if a and b are in CCW order on the normal side of f,
 * and -1 if they are reversed, and 0 if there is no shared face f. */
static int bev_ccw_test(const ExtendableMesh &emesh, const int a, const int b, const int f)
{
  if (f == -1) {
    return 0;
  }
  const IndexRange corners = emesh.face_corners(f);
  int ca = -1;
  int cb = -1;
  for (const int c : corners) {
    if (emesh.corner_edge(c) == a) {
      ca = c - corners.start();
    }
    if (emesh.corner_edge(c) == b) {
      cb = c - corners.start();
    }
  }
  if (ca == -1 || cb == -1) {
    return 0;
  }
  return ((cb + 1) % corners.size() == ca) ? 1 : -1;
}

/* See if we have usual case for bevel edge order:
 * there is an ordering such that all the faces are between
 * successive edges and form a manifold "cap" at bv.
 * If this is the case, set bv->edges to such an order
 * and return true; else unmark any partial path and return false.
 * Assume the first edge is already in bv->edges[0].e.
 *
 * Add edges to bv->edges in order that keeps adjacent edges sharing
 * a unique face, if possible. */
static bool fast_bevel_edge_order(const ExtendableMesh &emesh, BevVert *bv, bool is_edge_bevel)
{
  int ntot = bv->edgecount;

  EdgeHalf *eh = &bv->edges[0];
  int e = eh->e;
  if (emesh.src_edge_to_face[e].is_empty()) {
    return false;
  }

  for (int i = 1; i < ntot; i++) {
    int num_shared_face = 0;
    int first_suc = -1;
    for (const int e2 : emesh.src_vert_to_edge[bv->v]) {
      bool used = false;
      for (int k = 0; k < i; k++) {
        if (bv->edges[k].e == e2) {
          used = true;
          break;
        }
      }
      /* In edge-bevel mode, wire edges are stored separately and excluded from the ring.
       * In vertex-bevel mode, wire edges participate in the ring ordering like BMesh does. */
      if (used || (is_edge_bevel && bv->wire_edges.as_span().contains(e2))) {
        continue;
      }

      for (const int f : emesh.src_edge_to_face[e2]) {
        if (emesh.src_edge_to_face[e].contains(f)) {
          num_shared_face++;
          if (first_suc == -1) {
            first_suc = e2;
          }
        }
      }
      if (num_shared_face >= 3) {
        break;
      }
    }
    if (num_shared_face == 1 || (i == 1 && num_shared_face == 2)) {
      eh = &bv->edges[i];
      eh->e = e = first_suc;
    }
    else {
      for (int k = 1; k < i; k++) {
        bv->edges[k].e = -1;
      }
      return false;
    }
  }
  return true;
}

/* Do a depth first search to try to find a path that orders the rest of the edges
 * (after i) around a vertex bv, such that successive edges share a face.
 * Also prefer paths where the last edge shares a face with the first edge (bv->edges[0].e),
 * but will accept a path that doesn't close if it is the longest one found.
 * This is needed to handle cases where there are multiple faces between edges, or "shells"
 * of "internal faces" at a vertex -- i.e., faces that bridge between the edges that naturally
 * form a manifold cap around bv. It is rare to have more than one of these, so unlikely
 * that the exponential time case will be hit in practice.
 * Returns the new index i' where bv->edges[i'] ends the best path found.
 * The path will be recorded in bv->edges and used edges will be marked.
 */
static int bevel_edge_order_extend(const ExtendableMesh &emesh,
                                   BevVert *bv,
                                   int i,
                                   bool is_edge_bevel)
{
  Vector<int, 4> sucs;
  Vector<int, 16> save_path;

  int e = bv->edges[i].e;

  for (const int e2 : emesh.src_vert_to_edge[bv->v]) {
    bool used = false;
    for (int k = 0; k <= i; k++) {
      if (bv->edges[k].e == e2) {
        used = true;
        break;
      }
    }
    /* Exclude wire edges from the face-connected successor search only in edge-bevel mode.
     * In vertex-bevel mode they remain in the ring and the face search simply finds nothing. */
    if (!used && !(is_edge_bevel && bv->wire_edges.as_span().contains(e2))) {
      if (edges_face_connected_at_vert(emesh, e, e2)) {
        sucs.append(e2);
      }
    }
  }

  const int nsucs = sucs.size();

  int bestj = i;
  int j = i;
  for (int sucindex = 0; sucindex < nsucs; sucindex++) {
    int nexte = sucs[sucindex];
    bv->edges[j + 1].e = nexte;
    int tryj = bevel_edge_order_extend(emesh, bv, j + 1, is_edge_bevel);
    if (tryj > bestj ||
        (tryj == bestj && edges_face_connected_at_vert(emesh, bv->edges[tryj].e, bv->edges[0].e)))
    {
      bestj = tryj;
      save_path.clear();
      for (int k = j + 1; k <= bestj; k++) {
        save_path.append(bv->edges[k].e);
      }
    }
    for (int k = j + 1; k <= tryj; k++) {
      bv->edges[k].e = -1;
    }
  }

  if (bestj > j) {
    for (int k = j + 1; k <= bestj; k++) {
      bv->edges[k].e = save_path[k - (j + 1)];
    }
  }
  return bestj;
}

/* Fill in bv->edges with a good ordering of non-wire edges around bv->v.
 * Use only edges where wire_edges is not set (if edge beveling, others are wire).
 * first_e is a good edge to start with. */
static BoundVert *add_new_bound_vert(BevVert *bv, const float3 &co)
{
  auto new_bv = std::make_unique<BoundVert>();
  BoundVert *v = new_bv.get();
  bv->owned_bound_verts.append(std::move(new_bv));
  v->nv.co = co;
  if (!bv->vmesh) {
    bv->vmesh = std::make_unique<VMesh>();
  }
  VMesh *vm = bv->vmesh.get();
  if (vm->boundstart == nullptr) {
    vm->boundstart = v;
    v->next = v->prev = v;
  }
  else {
    v->prev = vm->boundstart->prev;
    v->next = vm->boundstart;
    v->prev->next = v;
    vm->boundstart->prev = v;
  }
  v->index = vm->count++;
  /* `profile.super_r` of 1.0 is the PRO_LINE_R (straight-line profile) value. */
  v->profile.super_r = 1.0f;
  return v;
}

static void adjust_bound_vert(BoundVert *bndv, const float3 &co)
{
  bndv->nv.co = co;
}

/* If a beveled edge has a seam (check_seam == true) or a sharp (check_sharp == true),
 * then we may need to correct for discontinuities in those edge flags after beveling.
 * The code will automatically make the outer edges of a multi-segment beveled edge have
 * the same flags. So beveled edges next to each other will not lead to discontinuities.
 * But if there are beveled edges that do NOT have a seam (or sharp), then we need to mark
 * all the edge segments of such beveled edges with seam (or sharp) until we hit the next
 * beveled edge that has such a mark. This routine sets, for each rightv of a beveled edge
 * that has seam (or sharp), how many edges follow without the corresponding property.
 * The count is put in the seam_len field for seams and the sharp_len field for sharps.
 *
 * TODO: This approach doesn't work for terminal edges or miters. */
static void check_edge_data_seam_sharp_edges(const BevelState &state,
                                             BevVert *bv,
                                             bool check_seam,
                                             bool check_sharp)
{
  /* Read the uv_seam and sharp_edge edge attributes from the original mesh. */
  const bke::AttributeAccessor attrs = state.emesh.mesh.attributes();
  VArraySpan<bool> uv_seam_attr;
  VArraySpan<bool> sharp_edge_attr;
  {
    bke::AttributeReader<bool> seam_reader = attrs.lookup<bool>("uv_seam", bke::AttrDomain::Edge);
    if (seam_reader) {
      uv_seam_attr = VArraySpan<bool>(seam_reader.varray);
    }
    bke::AttributeReader<bool> sharp_reader = attrs.lookup<bool>("sharp_edge",
                                                                 bke::AttrDomain::Edge);
    if (sharp_reader) {
      sharp_edge_attr = VArraySpan<bool>(sharp_reader.varray);
    }
  }

  /* Returns true when the edge half lacks the flag being checked.
   * For seams: the original mesh edge has no `uv_seam` attribute or the attribute is false.
   * For sharps: the original mesh edge has no `sharp_edge` attribute or the attribute is false. */
  auto hasnot = [&](const EdgeHalf *e) -> bool {
    if (check_seam) {
      if (uv_seam_attr.is_empty() || e->e < 0 || e->e >= int(uv_seam_attr.size())) {
        return true; /* No uv_seam attribute → no seams. */
      }
      return !uv_seam_attr[e->e];
    }
    if (check_sharp) {
      if (sharp_edge_attr.is_empty() || e->e < 0 || e->e >= int(sharp_edge_attr.size())) {
        return true; /* No sharp_edge attribute → no sharps. */
      }
      return !sharp_edge_attr[e->e];
    }
    return true;
  };

  EdgeHalf *e = &bv->edges[0];
  EdgeHalf *efirst = &bv->edges[0];

  /* Get to first edge with the seam or sharp property. */
  while (hasnot(e)) {
    e = e->next;
    if (e == efirst) {
      break;
    }
  }

  /* If no such edge found, return. */
  if (hasnot(e)) {
    return;
  }

  /* Set efirst to this first encountered edge. */
  efirst = e;

  do {
    int flag_count = 0;
    EdgeHalf *ne = e->next;

    while (hasnot(ne) && ne != efirst) {
      if (ne->is_bev) {
        flag_count++;
      }
      ne = ne->next;
    }
    if (ne == e || (ne == efirst && hasnot(efirst))) {
      break;
    }
    /* Set seam_len / sharp_len of starting edge's rightv. */
    if (check_seam) {
      e->rightv->seam_len = flag_count;
    }
    else {
      e->rightv->sharp_len = flag_count;
    }
    e = ne;
  } while (e != efirst);
}

/* Sets the #any_seam property for a #BevVert and all its #BoundVert's. */
static void set_bound_vert_seams(const BevelState &state,
                                 BevVert *bv,
                                 bool mark_seam,
                                 bool mark_sharp)
{
  bv->any_seam = false;
  BoundVert *v = bv->vmesh->boundstart;
  do {
    v->any_seam = false;
    for (EdgeHalf *e = v->efirst; e; e = e->next) {
      v->any_seam |= e->is_seam;
      if (e == v->elast) {
        break;
      }
    }
    bv->any_seam |= v->any_seam;
  } while ((v = v->next) != bv->vmesh->boundstart);

  if (mark_seam) {
    check_edge_data_seam_sharp_edges(state, bv, true, false);
  }
  if (mark_sharp) {
    check_edge_data_seam_sharp_edges(state, bv, false, true);
  }
}

static float3 offset_in_plane(const ExtendableMesh &emesh,
                              EdgeHalf *e,
                              const float3 *plane_no,
                              bool left)
{
  int v = e->is_rev ? emesh.src_edges[e->e][1] : emesh.src_edges[e->e][0];
  float3 v_co = emesh.vert_position(v);
  float3 other_co = emesh.vert_position(geom::edge_other_vert(emesh, e->e, v));
  float3 dir = math::normalize(other_co - v_co);
  float3 no;
  if (plane_no) {
    no = *plane_no;
  }
  else {
    no = float3(0.0f);
    if (math::abs(dir[0]) < math::abs(dir[1])) {
      no[0] = 1.0f;
    }
    else {
      no[1] = 1.0f;
    }
  }

  float3 fdir = left ? math::normalize(math::cross(dir, no)) :
                       math::normalize(math::cross(no, dir));
  return v_co + fdir * (left ? e->offset_l : e->offset_r);
}

static void build_boundary_vertex_only(const BevelState &state, BevVert *bv, bool construct)
{
  BLI_assert(state.params.affect_type == BevelAffect::Vertices);

  EdgeHalf *efirst = &bv->edges[0];
  EdgeHalf *e = efirst;
  do {
    const float3 co = geom::slide_dist(state.emesh, e->e, bv->v, e->offset_l);
    if (construct) {
      BoundVert *v = add_new_bound_vert(bv, co);
      v->efirst = v->elast = e;
      e->leftv = e->rightv = v;
    }
    else {
      adjust_bound_vert(e->leftv, co);
    }
  } while ((e = e->next) != efirst);

  if (construct) {
    set_bound_vert_seams(state, bv, state.mark_seam, state.mark_sharp);
    /* Also check for UV seams at the vertex itself, matching BMesh's #build_boundary_vertex_only.
     * Only needed when the segment count is odd (the "affect_vertices_odd" condition), since
     * `uv_layer_info.face_component` is only populated for odd segment counts. */
    if (state.params.segments % 2 == 1) {
      if (!bv->any_seam && !state.uv_layer_info.contig_uv_maps_around_vert(state.emesh, bv->v)) {
        bv->any_seam = true;
      }
    }
    VMesh *vm = bv->vmesh.get();
    if (vm->count == 2) {
      vm->mesh_kind = MeshKind::None;
    }
    else if (state.params.segments == 1) {
      vm->mesh_kind = MeshKind::Poly;
    }
    else {
      vm->mesh_kind = MeshKind::Adj;
    }
  }
}

static void build_boundary_terminal_edge(const BevelState &state,
                                         BevVert *bv,
                                         EdgeHalf *efirst,
                                         const bool construct)
{
  const ExtendableMesh &emesh = state.emesh;
  EdgeHalf *e = efirst;
  if (bv->edgecount == 2) {
    /* Only 2 edges in, so terminate the edge with an artificial vertex on the unbeveled edge. */
    const float3 *no = e->fprev != -1 ?
                           &emesh.src_face_normals[e->fprev] :
                           (e->fnext != -1 ? &emesh.src_face_normals[e->fnext] : nullptr);
    const float3 co1 = offset_in_plane(emesh, e, no, true);
    if (construct) {
      BoundVert *bndv = add_new_bound_vert(bv, co1);
      bndv->efirst = bndv->elast = bndv->ebev = e;
      e->leftv = bndv;
    }
    else {
      adjust_bound_vert(e->leftv, co1);
    }
    no = e->fnext != -1 ? &emesh.src_face_normals[e->fnext] :
                          (e->fprev != -1 ? &emesh.src_face_normals[e->fprev] : nullptr);
    const float3 co2 = offset_in_plane(emesh, e, no, false);
    if (construct) {
      BoundVert *bndv = add_new_bound_vert(bv, co2);
      bndv->efirst = bndv->elast = e;
      e->rightv = bndv;
    }
    else {
      adjust_bound_vert(e->rightv, co2);
    }
    /* Make artificial extra point along unbeveled edge, and form triangle. */
    const float3 co_slide = geom::slide_dist(emesh, e->next->e, bv->v, e->offset_l);
    if (construct) {
      BoundVert *bndv = add_new_bound_vert(bv, co_slide);
      bndv->efirst = bndv->elast = e->next;
      e->next->leftv = e->next->rightv = bndv;
      set_bound_vert_seams(state, bv, state.mark_seam, state.mark_sharp);
    }
    else {
      adjust_bound_vert(e->next->leftv, co_slide);
    }
  }
  else {
    /* More than 2 edges in. Put on-edge verts on all the other edges and join with the beveled
     * edge to make a poly or adj mesh, because e->prev has offset 0 and offset_meet will put
     * co on that edge. */
    const float3 co1 = geom::offset_meet(emesh, e->prev, e, bv->v, e->fprev, false, nullptr);
    if (construct) {
      BoundVert *bndv = add_new_bound_vert(bv, co1);
      bndv->efirst = e->prev;
      bndv->elast = bndv->ebev = e;
      e->leftv = bndv;
      e->prev->leftv = e->prev->rightv = bndv;
    }
    else {
      adjust_bound_vert(e->leftv, co1);
    }
    e = e->next;
    const float3 co2 = geom::offset_meet(emesh, e->prev, e, bv->v, e->fprev, false, nullptr);
    if (construct) {
      BoundVert *bndv = add_new_bound_vert(bv, co2);
      bndv->efirst = e->prev;
      bndv->elast = e;
      e->leftv = e->rightv = bndv;
      e->prev->rightv = bndv;
    }
    else {
      adjust_bound_vert(e->leftv, co2);
    }
    float d = efirst->offset_l_spec;
    if (!state.params.custom_profile_samples.is_empty() || state.params.shape < 0.25f) {
      d *= math::sqrt(2.0f);
    }
    for (e = e->next; e->next != efirst; e = e->next) {
      const float3 co = geom::slide_dist(emesh, e->e, bv->v, d);
      if (construct) {
        BoundVert *bndv = add_new_bound_vert(bv, co);
        bndv->efirst = bndv->elast = e;
        e->leftv = e->rightv = bndv;
      }
      else {
        adjust_bound_vert(e->leftv, co);
      }
    }
    if (bv->edgecount >= 3) {
      /* Special case: snap profile to the plane of the adjacent two edges.
       * Mirrors the BMesh #build_boundary_terminal_edge logic (BMesh lines 3441-3447).
       * This is intentionally outside the `if (construct)` block so that it also
       * runs on adjustment passes, matching BMesh behavior. */
      BoundVert *bndv = bv->vmesh->boundstart;
      BLI_assert(bndv->ebev != nullptr);
      profile::set_profile_params(state, bv, bndv);
      profile::move_profile_plane(bndv, state.emesh.src_positions[bv->v]);
    }
    if (construct) {
      set_bound_vert_seams(state, bv, state.mark_seam, state.mark_sharp);

      /* Set the mesh kind for the terminal face, mirroring BMesh's
       * #build_boundary_terminal_edge (BMesh lines 3452-3471). */
      VMesh *vm = bv->vmesh.get();
      if (vm->count == 2 && bv->edgecount == 3) {
        vm->mesh_kind = MeshKind::None;
      }
      else if (vm->count == 3) {
        /* Use TRI_FAN unless the extra point is coplanar with the profile
         * (custom-profile case), in which case POLY avoids overhanging edges. */
        bool use_tri_fan = true;
        if (!state.params.custom_profile_samples.is_empty()) {
          BoundVert *bndv = efirst->leftv;
          float4 profile_plane = math::plane_from_point_normal(bndv->profile.plane_co,
                                                               bndv->profile.plane_no);
          /* The extra BoundVert placed along the non-adjacent edge. */
          bndv = efirst->rightv->next;
          if (math::dist_squared_to_plane(bndv->nv.co, profile_plane) < geom::BEVEL_EPSILON_BIG) {
            use_tri_fan = false;
          }
        }
        vm->mesh_kind = use_tri_fan ? MeshKind::TriFan : MeshKind::Poly;
      }
      else {
        vm->mesh_kind = MeshKind::Poly;
      }
    }
  }
}

/* Return the next EdgeHalf after from_e that is beveled.
 * If from_e is nullptr, find the first beveled edge. */
static EdgeHalf *next_bev(BevVert *bv, EdgeHalf *from_e)
{
  if (from_e == nullptr) {
    from_e = &bv->edges.last();
  }
  EdgeHalf *e = from_e;
  do {
    if (e->is_bev) {
      return e;
    }
  } while ((e = e->next) != from_e);
  return nullptr;
}

/* Returns true when two normalized direction vectors are nearly parallel or anti-parallel.
 * Ported from #nearly_parallel_normalized in `bmesh_bevel.cc`. */
static bool nearly_parallel_normalized(const float3 &d1, const float3 &d2)
{
  const float direction_dot = math::dot(d1, d2);
  return math::abs(math::abs(direction_dot) - 1.0f) <= geom::BEVEL_EPSILON_ANG_DOT;
}

enum AngleKind { ANGLE_SMALLER, ANGLE_STRAIGHT, ANGLE_LARGER };

/* Return whether the angle swept from e1 to e2 around v is less than, equal to,
 * or larger than 180 degrees. */
static AngleKind edges_angle_kind(const ExtendableMesh &emesh, EdgeHalf *e1, EdgeHalf *e2, int v)
{
  int v1 = geom::edge_other_vert(emesh, e1->e, v);
  int v2 = geom::edge_other_vert(emesh, e2->e, v);
  float3 dir1 = emesh.vert_position(v) - emesh.vert_position(v1);
  float3 dir2 = emesh.vert_position(v) - emesh.vert_position(v2);
  dir1 = math::normalize(dir1);
  dir2 = math::normalize(dir2);

  /* First check for in-line edges using a simpler test. */
  if (nearly_parallel_normalized(dir1, dir2)) {
    return ANGLE_STRAIGHT;
  }

  /* Angles are in [0, pi]. Need to compare cross product with normal to see if reflex. */
  float3 cross = math::normalize(math::cross(dir1, dir2));
  float3 no;
  if (e1->fnext != -1) {
    no = emesh.src_face_normals[e1->fnext];
  }
  else if (e2->fprev != -1) {
    no = emesh.src_face_normals[e2->fprev];
  }
  else {
    no = emesh.src_vert_normals[v];
  }

  if (math::dot(cross, no) < 0.0f) {
    return ANGLE_LARGER;
  }
  return ANGLE_SMALLER;
}

/* Returns true when a miter is requested at edge `e`'s outgoing corner at vertex `v`.
 * The relevant corner is the one in `e->fnext` (the face to the right of `e` when
 * traversing outward from the bevel vertex) that contains `v`.
 * Mirrors the role of #BevelParams::miter_outer / #BevelParams::miter_inner in
 * `bmesh_bevel.cc` but using a per-corner bool instead of global mode enums. */
static bool edge_has_miter(const BevelState &state, const EdgeHalf *e, int v)
{
  if (state.all_miters_off) {
    return false;
  }
  if (state.all_miters_on) {
    return true;
  }
  /* Per-corner lookup: find the corner in e->fnext at vertex v. */
  if (e->fnext == -1) {
    return false;
  }
  const IndexRange face = state.emesh.src_faces[e->fnext];
  const Span<int> face_verts = state.emesh.src_corner_verts.slice(face);
  for (const int i : face_verts.index_range()) {
    if (face_verts[i] == v) {
      const int corner = face.start() + i;
      return (corner < int(state.params.miter.size())) && state.params.miter[corner];
    }
  }
  return false;
}

static void build_boundary(const BevelState &state, BevVert *bv, bool construct)
{
  const ExtendableMesh &emesh = state.emesh;
  if (bv->edgecount <= 1) {
    return;
  }

  if (state.params.affect_type == BevelAffect::Vertices) {
    build_boundary_vertex_only(state, bv, construct);
    return;
  }

  VMesh *vm = bv->vmesh.get();

  EdgeHalf *efirst = next_bev(bv, nullptr);
  BLI_assert(efirst->is_bev);

  if (bv->selcount == 1) {
    /* Special case: only one beveled edge in. */
    build_boundary_terminal_edge(state, bv, efirst, construct);
    return;
  }

  EdgeHalf *emiter = nullptr;

  /* There is more than one beveled edge.
   * We make BoundVerts to connect the sides of the beveled edges.
   * Non-beveled edges in between will just join to the appropriate juncture point. */
  EdgeHalf *e = efirst;
  EdgeHalf *e2;
  do {
    BLI_assert(e->is_bev);
    /* Make the BoundVert for the right side of e; the other side will be made when the beveled
     * edge to the left of e is handled. */
    int between = 0; /* Count of edges between e and the next beveled edge. */
    for (e2 = e->next; !e2->is_bev; e2 = e2->next) {
      between++;
    }

    float3 co;
    if (between == 0) {
      co = geom::offset_meet(emesh, e, e2, bv->v, e->fnext, false, nullptr);
    }
    else {
      co = geom::offset_meet(emesh, e, e2, bv->v, -1, true, nullptr);
    }

    if (construct) {
      BoundVert *v = add_new_bound_vert(bv, co);
      v->efirst = e;
      v->elast = e2;
      v->ebev = e2;
      e->rightv = v;
      e2->leftv = v;
      for (EdgeHalf *e3 = e->next; e3 != e2; e3 = e3->next) {
        e3->leftv = e3->rightv = v;
      }
      AngleKind ang_kind = edges_angle_kind(emesh, e, e2, bv->v);

      /* Are we doing special mitering?
       * There can only be one outer (reflex) miter; `emiter` is set to its edge.
       * Outer corners (ANGLE_LARGER) → patch miter; inner (ANGLE_SMALLER) → arc miter.
       * No miter is added when #edge_has_miter returns false for this edge. */
      const bool want_miter = edge_has_miter(state, e, bv->v);
      const bool do_outer_miter = (want_miter && !emiter && ang_kind == ANGLE_LARGER);
      const bool do_inner_miter = (want_miter && ang_kind == ANGLE_SMALLER);

      if (do_outer_miter || do_inner_miter) {
        if (ang_kind == ANGLE_LARGER) {
          emiter = e;
        }
        BoundVert *v1 = v;
        v1->ebev = nullptr;
        BoundVert *v2 = nullptr;
        if (do_outer_miter) {
          /* Patch miter: insert extra BoundVert v2 between the two bevel offsets. */
          v2 = add_new_bound_vert(bv, co);
        }
        BoundVert *v3 = add_new_bound_vert(bv, co);
        v3->ebev = e2;
        v3->efirst = e2;
        v3->elast = e2;
        e2->leftv = v3;
        if (do_outer_miter) {
          /* Wire v2 as the middle patch vertex. Mirrors BMesh lines 3699-3717. */
          v1->is_patch_start = true;
          v2->ebev = nullptr;
          v1->elast = e;
          if (e->next == e2) {
            v2->efirst = nullptr;
            v2->elast = nullptr;
          }
          else {
            v2->efirst = e->next;
            for (EdgeHalf *e3 = e->next; e3 != e2; e3 = e3->next) {
              e3->leftv = e3->rightv = v2;
              v2->elast = e3;
            }
          }
        }
        else {
          /* Arc miter: v1 is the arc start, v3 carries the left bevel. Mirrors BMesh lines
           * 3720-3745. */
          v1->is_arc_start = true;
          v1->profile.middle = co;
          if (e->next == e2) {
            v1->elast = v1->efirst;
          }
          else {
            int bet2 = between / 2;
            bool betodd = (between % 2) == 1;
            int i = 0;
            /* Put first half of in-between edges at profile index 0, second half at index seg.
             * If between is odd, the middle one goes at seg/2. */
            for (EdgeHalf *e3 = e->next; e3 != e2; e3 = e3->next) {
              v1->elast = e3;
              if (i < bet2) {
                e3->profile_index = 0;
              }
              else if (betodd && i == bet2) {
                e3->profile_index = state.params.segments / 2;
              }
              else {
                e3->profile_index = state.params.segments;
              }
              i++;
            }
          }
        }
      }
    }
    else {
      AngleKind ang_kind = edges_angle_kind(emesh, e, e2, bv->v);
      const bool want_miter = edge_has_miter(state, e, bv->v);
      const bool do_outer_miter = (want_miter && !emiter && ang_kind == ANGLE_LARGER);
      const bool do_inner_miter = (want_miter && ang_kind == ANGLE_SMALLER);
      if (do_outer_miter || do_inner_miter) {
        if (ang_kind == ANGLE_LARGER) {
          emiter = e;
        }
        BoundVert *v1 = e->rightv;
        BoundVert *v2 = nullptr;
        BoundVert *v3;
        if (do_outer_miter) {
          v2 = v1->next;
          v3 = v2->next;
        }
        else {
          v3 = v1->next;
        }
        adjust_bound_vert(v1, co);
        if (v2) {
          adjust_bound_vert(v2, co);
        }
        adjust_bound_vert(v3, co);
      }
      else {
        adjust_bound_vert(e->rightv, co);
      }
    }
  } while ((e = e2) != efirst);

  /* Adjust miter BoundVert positions now that the full ring of BoundVerts is known.
   * Mirrors #adjust_miter_inner_coords and #adjust_miter_coords in `bmesh_bevel.cc`. */
  if (!state.all_miters_off) {
    adjust_miter_inner_coords(state, bv, emiter);
  }
  if (emiter) {
    adjust_miter_coords(state, bv, emiter);
  }

  if (construct) {
    set_bound_vert_seams(state, bv, state.mark_seam, state.mark_sharp);

    if (vm->count == 2) {
      vm->mesh_kind = MeshKind::None;
    }
    else if (efirst->seg == 1) {
      vm->mesh_kind = MeshKind::Poly;
    }
    else {
      switch (state.vmesh_method) {
        case VMeshMethod::Adj:
          vm->mesh_kind = MeshKind::Adj;
          break;
        case VMeshMethod::Cutoff:
          vm->mesh_kind = MeshKind::Cutoff;
          break;
      }
    }
  }
}

/* Adjusts the positions of the outer (patch) miter BoundVerts v1 and v3 so they lie on
 * the planes through the adjacent BoundVerts.  Ported from #adjust_miter_coords in
 * `bmesh_bevel.cc`. */
static void adjust_miter_coords(const BevelState &state, BevVert *bv, EdgeHalf *emiter)
{
  BoundVert *v1 = emiter->rightv;
  BoundVert *v3;
  if (v1->is_patch_start) {
    /* Patch miter: v2 is between v1 and v3. */
    v3 = v1->next->next;
  }
  else {
    /* Arc miter: no v2. */
    v3 = v1->next;
  }
  BoundVert *v1prev = v1->prev;
  BoundVert *v3next = v3->next;

  float3 co2 = v1->nv.co;
  if (v1->is_arc_start) {
    v1->profile.middle = co2;
  }

  /* Fallback slide distance: offset divided by half-segment-count.
   * Matches BMesh's `d = bp->offset / (bp->seg / 2.0f)`. */
  const float d = emiter->offset_l / (float(state.params.segments) / 2.0f);

  /* co1: intersection of the line through co2 in the direction of emiter->e
   * with the plane whose normal is that direction and which passes through v1prev. */
  int vother = geom::edge_other_vert(state.emesh, emiter->e, bv->v);
  float3 edge_dir = math::normalize(state.emesh.src_positions[bv->v] -
                                    state.emesh.src_positions[vother]);
  float3 line_p = co2 + d * edge_dir;
  float3 co1 = math::isect_line_plane(co2, line_p, v1prev->nv.co, edge_dir).value_or(line_p);
  adjust_bound_vert(v1, co1);

  /* co3: same idea but using the other side of the miter (edge of v3). */
  EdgeHalf *emiter_other = v3->elast;
  vother = geom::edge_other_vert(state.emesh, emiter_other->e, bv->v);
  edge_dir = math::normalize(state.emesh.src_positions[bv->v] - state.emesh.src_positions[vother]);
  line_p = co2 + d * edge_dir;
  float3 co3 = math::isect_line_plane(co2, line_p, v3next->nv.co, edge_dir).value_or(line_p);
  adjust_bound_vert(v3, co3);
}

/* Adjusts the positions of inner (arc) miter BoundVerts by spreading them along the edge
 * directions according to the per-corner spread value.  Ported from
 * #adjust_miter_inner_coords in `bmesh_bevel.cc`. */
static void adjust_miter_inner_coords(const BevelState &state, BevVert *bv, EdgeHalf *emiter)
{
  BoundVert *vstart = bv->vmesh->boundstart;
  BoundVert *v = vstart;
  do {
    if (v->is_arc_start) {
      BoundVert *v3 = v->next;
      EdgeHalf *e = v->efirst;
      if (e != emiter) {
        float3 co = v->nv.co;

        /* Spread: use per-corner value from params if available; fall back to 0. */
        int corner = -1;
        if (e->fnext != -1) {
          const IndexRange face = state.emesh.src_faces[e->fnext];
          const Span<int> face_verts = state.emesh.src_corner_verts.slice(face);
          for (const int i : face_verts.index_range()) {
            if (face_verts[i] == bv->v) {
              corner = face.start() + i;
              break;
            }
          }
        }
        const float spread = (corner >= 0 && corner < int(state.params.spread.size())) ?
                                 state.params.spread[corner] :
                                 0.0f;

        int vother = geom::edge_other_vert(state.emesh, e->e, bv->v);
        float3 edge_dir = math::normalize(state.emesh.src_positions[vother] -
                                          state.emesh.src_positions[bv->v]);
        v->nv.co = co + edge_dir * spread;

        e = v3->elast;
        vother = geom::edge_other_vert(state.emesh, e->e, bv->v);
        edge_dir = math::normalize(state.emesh.src_positions[vother] -
                                   state.emesh.src_positions[bv->v]);
        v3->nv.co = co + edge_dir * spread;
      }
      v = v3->next;
    }
    else {
      v = v->next;
    }
  } while (v != vstart);
}

static void find_bevel_edge_order(const ExtendableMesh &emesh,
                                  BevVert *bv,
                                  int first_e,
                                  bool is_edge_bevel)
{
  int ntot = bv->edgecount;
  for (int i = 0;;) {
    bv->edges[i].e = first_e;
    if (i == 0 && fast_bevel_edge_order(emesh, bv, is_edge_bevel)) {
      break;
    }
    i = bevel_edge_order_extend(emesh, bv, i, is_edge_bevel);
    i++;
    if (i >= bv->edgecount) {
      break;
    }
    first_e = -1;
    for (const int e : emesh.src_vert_to_edge[bv->v]) {
      bool used = false;
      for (int k = 0; k < i; k++) {
        if (bv->edges[k].e == e) {
          used = true;
          break;
        }
      }
      /* In edge-bevel mode, skip wire edges (they are in wire_edges, not the ring).
       * In vertex-bevel mode, all edges including wires participate in the ring. */
      if (used || (is_edge_bevel && bv->wire_edges.as_span().contains(e))) {
        continue;
      }
      if (first_e == -1) {
        first_e = e;
      }
      if (emesh.src_edge_to_face[e].size() == 1) {
        first_e = e;
        break;
      }
    }
  }
  for (int i = 0; i < ntot; i++) {
    EdgeHalf *eh = &bv->edges[i];
    EdgeHalf *eh2 = (i == bv->edgecount - 1) ? &bv->edges[0] : &bv->edges[i + 1];
    int e = eh->e;
    int e2 = eh2->e;
    if (eh->fnext != -1 || eh2->fprev != -1) {
      continue;
    }
    int bestf = -1;
    for (const int f : emesh.src_edge_to_face[e]) {
      if (emesh.src_edge_to_face[e2].contains(f)) {
        const IndexRange corners = emesh.face_corners(f);
        for (const int c : corners) {
          if (emesh.corner_vert(c) == bv->v) {
            bestf = f;
            break;
          }
        }
      }
    }
    if (bestf != -1) {
      eh->fnext = eh2->fprev = bestf;
    }
  }
}

/* -------------------------------------------------------------------- */
/** \name Profile orientation helpers
 *
 * These functions find the chain or cycle of beveled edges connected to a given edge,
 * and set consistent profile orientations along the chain.
 * \{ */

/* Forward declaration (defined in the second construct block). */
static EdgeHalf *find_edge_half_for_edge(BevVert *bv, int edge_index);

/* Given an #EdgeHalf `e`, find the #EdgeHalf at the other end of its edge.
 * If the other end has a #BevVert, return that #EdgeHalf (and set `r_bvother`). */
static EdgeHalf *find_other_end_edge_half(const BevelState &state,
                                          EdgeHalf *e,
                                          BevVert **r_bvother)
{
  const int2 verts = state.emesh.src_edges[e->e];
  int vother = e->is_rev ? verts[0] : verts[1];
  BevVert *bvo = state.vert_hash.lookup_default(vother, nullptr);
  if (bvo) {
    if (r_bvother) {
      *r_bvother = bvo;
    }
    EdgeHalf *eother = find_edge_half_for_edge(bvo, e->e);
    BLI_assert(eother != nullptr);
    return eother;
  }
  if (r_bvother) {
    *r_bvother = nullptr;
  }
  return nullptr;
}

/**
 * Helper function to return the next Beveled EdgeHalf along a path.
 *
 * \param toward_bv: Whether the direction to travel points toward or away from the BevVert
 * connected to the current EdgeHalf.
 * \param r_bv: The BevVert connected to the EdgeHalf -- updated if we're traveling to the other
 * EdgeHalf of an original edge.
 *
 * \note This only returns the most parallel edge if it's the most parallel by
 * at least 10 degrees. This is a somewhat arbitrary choice, but it makes sure that consistent
 * orientation paths only continue in obvious ways.
 */
static EdgeHalf *next_edgehalf_bev(const BevelState &state,
                                   EdgeHalf *start_edge,
                                   bool toward_bv,
                                   BevVert **r_bv)
{
  /* Case 1: The next EdgeHalf is the other side of the original edge. */
  if (!toward_bv) {
    return find_other_end_edge_half(state, start_edge, r_bv);
  }

  /* Case 2: The next EdgeHalf is across a BevVert from the current EdgeHalf. */
  if ((*r_bv)->selcount == 1) {
    return nullptr; /* No other edges to go to. */
  }

  if ((*r_bv)->selcount == 2) {
    EdgeHalf *new_edge = start_edge;
    do {
      new_edge = new_edge->next;
    } while (!new_edge->is_bev);
    return new_edge;
  }

  const int2 start_e_verts = state.emesh.src_edges[start_edge->e];
  const float3 v1_co = state.emesh.src_positions[start_e_verts[0]];
  const float3 v2_co = state.emesh.src_positions[start_e_verts[1]];

  float3 dir_start_edge;
  if (start_e_verts[0] == (*r_bv)->v) {
    dir_start_edge = math::normalize(v1_co - v2_co);
  }
  else {
    dir_start_edge = math::normalize(v2_co - v1_co);
  }

  EdgeHalf *new_edge = start_edge->next;
  float second_best_dot = 0.0f, best_dot = 0.0f;
  EdgeHalf *next_edge = nullptr;
  while (new_edge != start_edge) {
    if (!new_edge->is_bev) {
      new_edge = new_edge->next;
      continue;
    }

    const int2 new_e_verts = state.emesh.edge_verts(new_edge->e);
    const float3 nv1_co = state.emesh.vert_position(new_e_verts[0]);
    const float3 nv2_co = state.emesh.vert_position(new_e_verts[1]);

    float3 dir_new_edge;
    if (new_e_verts[1] == (*r_bv)->v) {
      dir_new_edge = math::normalize(nv1_co - nv2_co);
    }
    else {
      dir_new_edge = math::normalize(nv2_co - nv1_co);
    }

    float new_dot = math::dot(dir_new_edge, dir_start_edge);
    if (new_dot > best_dot) {
      second_best_dot = best_dot;
      best_dot = new_dot;
      next_edge = new_edge;
    }
    else if (new_dot > second_best_dot) {
      second_best_dot = new_dot;
    }

    new_edge = new_edge->next;
  }

  if ((next_edge != nullptr) && std::abs(best_dot - second_best_dot) <= geom::BEVEL_SMALL_ANG_DOT)
  {
    return nullptr;
  }
  return next_edge;
}

/**
 * Starting along any beveled edge, travel along the chain / cycle of beveled edges including that
 * edge, marking consistent profile orientations along the way. Orientations are marked by setting
 * whether the BoundVert that contains each profile's information is the side of the profile's
 * start or not.
 * Ported from BMesh's #regularize_profile_orientation.
 */
static void regularize_profile_orientation(const BevelState &state, int edge_index)
{
  const int2 ev = state.emesh.edge_verts(edge_index);
  BevVert *start_bv = state.vert_hash.lookup_default(ev[0], nullptr);
  if (!start_bv) {
    start_bv = state.vert_hash.lookup_default(ev[1], nullptr);
  }
  if (!start_bv) {
    return;
  }
  EdgeHalf *start_edgehalf = find_edge_half_for_edge(start_bv, edge_index);
  if (!start_edgehalf || !start_edgehalf->is_bev || start_edgehalf->visited_rpo) {
    return;
  }

  /* Pick a BoundVert on one side of the profile to use for the starting side. Use the one highest
   * on the Z axis because even any rule is better than an arbitrary decision. */
  bool right_highest = start_edgehalf->leftv->nv.co[2] < start_edgehalf->rightv->nv.co[2];
  start_edgehalf->leftv->is_profile_start = right_highest;
  start_edgehalf->visited_rpo = true;

  /* First loop starts in the away from BevVert direction and the second starts toward it. */
  for (int i = 0; i < 2; i++) {
    EdgeHalf *edgehalf = start_edgehalf;
    BevVert *bv = start_bv;
    bool toward_bv = (i == 0);
    edgehalf = next_edgehalf_bev(state, edgehalf, toward_bv, &bv);

    /* Keep traveling until there is no unvisited beveled edgehalf to visit next. */
    while (edgehalf && !edgehalf->visited_rpo) {
      /* Mark the correct BoundVert as the start of the newly visited profile.
       * The direction relative to the BevVert switches every step, so also switch
       * the orientation every step. */
      if (i == 0) {
        edgehalf->leftv->is_profile_start = toward_bv ^ right_highest;
      }
      else {
        /* The opposite side as the first direction because we're moving the other way. */
        edgehalf->leftv->is_profile_start = (!toward_bv) ^ right_highest;
      }

      /* The next jump will in the opposite direction relative to the BevVert. */
      toward_bv = !toward_bv;

      edgehalf->visited_rpo = true;
      edgehalf = next_edgehalf_bev(state, edgehalf, toward_bv, &bv);
    }
  }
}

/** \} */

}  // namespace construct

namespace profile {

constexpr float PRO_SQUARE_R = 1e4f;
constexpr float PRO_CIRCLE_R = 2.0f;
constexpr float PRO_LINE_R = 1.0f;
constexpr float PRO_SQUARE_IN_R = 0.0f;

/**
 * Get the coordinate on the superellipse (x^r + y^r = 1), at parameter value x
 * (or, if !rbig, mirrored (y=x)-line).
 * rbig should be true if r > 1.0 and false if <= 1.0.
 * Assume r > 0.0.
 */
static double superellipse_co(double x, float r, bool rbig)
{
  BLI_assert(r > 0.0f);
  double dr = r;
  if (rbig) {
    return math::pow((1.0 - math::pow(x, dr)), (1.0 / dr));
  }
  return 1.0 - math::pow((1.0 - math::pow(1.0 - x, dr)), (1.0 / dr));
}

/* Find xnew > x0 so that distance((x0,y0), (xnew, ynew)) = dtarget.
 * False position Illinois method used because the function is somewhat linear
 * -> linear interpolation converges fast.
 * Assumes that the gradient is always between 1 and -1 for x in [x0, x0+dtarget]. */
static double find_superellipse_chord_endpoint(double x0, double dtarget, float r, bool rbig)
{
  double y0 = superellipse_co(x0, r, rbig);
  const double tol = 1e-13;
  const int maxiter = 10;

  double xmin = x0 + std::numbers::sqrt2 / 2.0 * dtarget;
  xmin = std::min(xmin, 1.0);
  double xmax = x0 + dtarget;
  xmax = std::min(xmax, 1.0);
  double ymin = superellipse_co(xmin, r, rbig);
  double ymax = superellipse_co(xmax, r, rbig);

  double dmaxerr = math::sqrt(math::pow((xmax - x0), 2.0) + math::pow((ymax - y0), 2.0)) - dtarget;
  double dminerr = math::sqrt(math::pow((xmin - x0), 2.0) + math::pow((ymin - y0), 2.0)) - dtarget;

  double xnew = xmax - dmaxerr * (xmax - xmin) / (dmaxerr - dminerr);
  bool lastupdated_upper = true;

  for (int iter = 0; iter < maxiter; iter++) {
    double ynew = superellipse_co(xnew, r, rbig);
    double dnewerr = math::sqrt(math::pow((xnew - x0), 2.0) + math::pow((ynew - y0), 2.0)) -
                     dtarget;
    if (abs(dnewerr) < tol) {
      break;
    }
    if (dnewerr < 0) {
      xmin = xnew;
      ymin = ynew;
      dminerr = dnewerr;
      if (!lastupdated_upper) {
        xnew = (dmaxerr / 2 * xmin - dminerr * xmax) / (dmaxerr / 2 - dminerr);
      }
      else {
        xnew = xmax - dmaxerr * (xmax - xmin) / (dmaxerr - dminerr);
      }
      lastupdated_upper = false;
    }
    else {
      xmax = xnew;
      ymax = ynew;
      dmaxerr = dnewerr;
      if (lastupdated_upper) {
        xnew = (dmaxerr * xmin - dminerr / 2 * xmax) / (dmaxerr - dminerr / 2);
      }
      else {
        xnew = xmax - dmaxerr * (xmax - xmin) / (dmaxerr - dminerr);
      }
      lastupdated_upper = true;
    }
  }
  return xnew;
}

/**
 * This search procedure to find equidistant points (x,y) in the first
 * superellipse quadrant works for every superellipse exponent but is more
 * expensive than known solutions for special cases.
 * Call the point on superellipse that intersects x=y line mx.
 * For r>=1 use only the range x in [0,mx] and mirror the rest along x=y line,
 * for r<1 use only x in [mx,1]. Points are initially spaced and iteratively
 * repositioned to have the same distance.
 */
static void find_even_superellipse_chords_general(int seg,
                                                  float r,
                                                  MutableSpan<double> xvals,
                                                  MutableSpan<double> yvals)
{
  const int smoothitermax = 10;
  const double error_tol = 1e-7;
  int imax = (seg + 1) / 2 - 1;

  bool seg_odd = seg % 2;

  bool rbig;
  double mx;
  if (r > 1.0f) {
    rbig = true;
    mx = math::pow(0.5, 1.0 / r);
  }
  else {
    rbig = false;
    mx = 1 - math::pow(0.5, 1.0 / r);
  }

  for (int i = 0; i <= imax; i++) {
    xvals[i] = i * mx / seg * 2;
    yvals[i] = superellipse_co(xvals[i], r, rbig);
  }
  yvals[0] = 1;

  for (int iter = 0; iter < smoothitermax; iter++) {
    double sum = 0.0;
    double dmin = 2.0;
    double dmax = 0.0;
    for (int i = 0; i < imax; i++) {
      double d = math::sqrt(math::pow((xvals[i + 1] - xvals[i]), 2.0) +
                            math::pow((yvals[i + 1] - yvals[i]), 2.0));
      sum += d;
      dmax = std::max(d, dmax);
      dmin = std::min(d, dmin);
    }
    double davg;
    if (seg_odd) {
      sum += std::numbers::sqrt2 / 2 * (yvals[imax] - xvals[imax]);
      davg = sum / (imax + 0.5);
    }
    else {
      sum += math::sqrt(math::pow((xvals[imax] - mx), 2.0) + math::pow((yvals[imax] - mx), 2.0));
      davg = sum / (imax + 1.0);
    }
    bool precision_reached = true;
    if (dmax - davg > error_tol) {
      precision_reached = false;
    }
    if (dmin - davg < error_tol) {
      precision_reached = false;
    }
    if (precision_reached) {
      break;
    }

    for (int i = 1; i <= imax; i++) {
      xvals[i] = find_superellipse_chord_endpoint(xvals[i - 1], davg, r, rbig);
      yvals[i] = superellipse_co(xvals[i], r, rbig);
    }
  }

  if (!seg_odd) {
    xvals[imax + 1] = mx;
    yvals[imax + 1] = mx;
  }
  for (int i = imax + 1; i <= seg; i++) {
    yvals[i] = xvals[seg - i];
    xvals[i] = yvals[seg - i];
  }

  if (!rbig) {
    for (int i = 0; i <= seg; i++) {
      double temp = xvals[i];
      xvals[i] = 1.0 - yvals[i];
      yvals[i] = 1.0 - temp;
    }
  }
}

/**
 * Find equidistant points `(x0,y0), (x1,y1)... (xn,yn)` on the superellipse
 * function in the first quadrant. For special profiles (linear, arc,
 * rectangle) the point can be calculated easily, for any other profile a more
 * expensive search procedure must be used because there is no known closed
 * form for equidistant parametrization.
 * `xvals` and `yvals` should be size `n+1`.
 */
static void find_even_superellipse_chords(int n,
                                          float r,
                                          MutableSpan<double> xvals,
                                          MutableSpan<double> yvals)
{
  bool seg_odd = n % 2;
  int n2 = n / 2;

  if (r == PRO_LINE_R) {
    for (int i = 0; i <= n; i++) {
      xvals[i] = double(i) / n;
      yvals[i] = 1.0 - double(i) / n;
    }
    return;
  }
  if (r == PRO_CIRCLE_R) {
    double temp = M_PI_2 / n;
    for (int i = 0; i <= n; i++) {
      xvals[i] = math::sin(i * temp);
      yvals[i] = math::cos(i * temp);
    }
    return;
  }
  if (r == PRO_SQUARE_IN_R) {
    if (!seg_odd) {
      for (int i = 0; i <= n2; i++) {
        xvals[i] = 0.0;
        yvals[i] = 1.0 - double(i) / n2;
        xvals[n - i] = yvals[i];
        yvals[n - i] = xvals[i];
      }
    }
    else {
      double temp = 1.0 / (n2 + std::numbers::sqrt2 / 2.0);
      for (int i = 0; i <= n2; i++) {
        xvals[i] = 0.0;
        yvals[i] = 1.0 - double(i) * temp;
        xvals[n - i] = yvals[i];
        yvals[n - i] = xvals[i];
      }
    }
    return;
  }
  if (r == PRO_SQUARE_R) {
    if (!seg_odd) {
      for (int i = 0; i <= n2; i++) {
        xvals[i] = double(i) / n2;
        yvals[i] = 1.0;
        xvals[n - i] = yvals[i];
        yvals[n - i] = xvals[i];
      }
    }
    else {
      double temp = 1.0 / (n2 + std::numbers::sqrt2 / 2.0);
      for (int i = 0; i <= n2; i++) {
        xvals[i] = double(i) * temp;
        yvals[i] = 1.0;
        xvals[n - i] = yvals[i];
        yvals[n - i] = xvals[i];
      }
    }
    return;
  }
  find_even_superellipse_chords_general(n, r, xvals, yvals);
}

/**
 * Find the profile's "fullness," which is the fraction of the space it takes up way from the
 * boundvert's centroid to the original vertex for a non-custom profile, or in the case of a
 * custom profile, the average "height" of the profile points along its centerline.
 */
static float find_profile_fullness(BevelState *bs)
{
  int nseg = bs->params.segments;
  constexpr int circle_fullness_segs = 11;
  static const float circle_fullness[circle_fullness_segs] = {
      0.0f,
      0.559f,
      0.642f,
      0.551f,
      0.646f,
      0.624f,
      0.646f,
      0.619f,
      0.647f,
      0.639f,
      0.647f,
  };

  float fullness;
  if (!bs->params.custom_profile_samples.is_empty()) {
    fullness = 0.0f;
    for (int i = 0; i < nseg; i++) {
      fullness += float(bs->pro_spacing.xvals[i] + bs->pro_spacing.yvals[i]) / (2.0f * nseg);
    }
  }
  else {
    if (bs->pro_super_r == PRO_LINE_R) {
      fullness = 0.0f;
    }
    else if (bs->pro_super_r == PRO_CIRCLE_R && nseg > 0 && nseg <= circle_fullness_segs) {
      fullness = circle_fullness[nseg - 1];
    }
    else {
      if (nseg % 2 == 0) {
        fullness = 2.4506f * bs->params.shape - 0.00000300f * nseg - 0.6266f;
      }
      else {
        fullness = 2.3635f * bs->params.shape + 0.000152f * nseg - 0.6060f;
      }
    }
  }
  return fullness;
}

/**
 * Fills the ProfileSpacing struct with the 2D coordinates for the profile's vertices.
 * The superellipse used for multi-segment profiles does not have a closed-form way
 * to generate evenly spaced points along an arc. We use an expensive search procedure
 * to find the parameter values that lead to bp->seg even chords.
 * We also want spacing for a number of segments that is a power of 2 >= bp->seg (but at least 4).
 * Use doubles because otherwise we cannot come close to float precision for final results.
 *
 * \param pro_spacing: The struct to fill. Changes depending on whether there needs
 * to be a separate miter profile.
 */
static void set_profile_spacing(BevelState *bs, ProfileSpacing *pro_spacing, bool custom)
{
  int segments = bs->params.segments;

  if (segments <= 1) {
    pro_spacing->seg_2 = 0;
    return;
  }

  int seg_2 = std::max(power_of_2_max_i(bs->params.segments), 4);
  bs->pro_spacing.seg_2 = seg_2;

  /**
   * Helper lambda: linearly resamples the pre-sampled custom profile at `n` evenly spaced
   * intervals.  The input `src` has `segments + 1` entries (from the caller who built it at
   * the node-eval resolution).  When `n != segments` we linearly interpolate to re-sample at
   * the new resolution (typically `seg_2`, a power-of-two used during subdivision).
   */
  auto fill_from_custom = [&](Array<double> &r_xvals, Array<double> &r_yvals, int n) {
    const Span<float2> src = bs->params.custom_profile_samples.as_span();
    const int src_n = int(src.size()) - 1; /* Number of source segments. */
    r_xvals = Array<double>(n + 1);
    r_yvals = Array<double>(n + 1);
    for (const int i : IndexRange(n + 1)) {
      /* Map output index i in [0, n] → source parameter t in [0, src_n]. */
      const float t = float(i) * float(src_n) / float(n);
      const int lo = std::min(int(t), src_n - 1);
      const int hi = lo + 1;
      const float f = t - float(lo);
      const float x = (1.0f - f) * src[lo].x + f * src[hi].x;
      const float y = (1.0f - f) * src[lo].y + f * src[hi].y;
      /* The GN curve uses (x=position-along-strip, y=profile-height) directly,
       * with x going from 0 (strip start) to 1 (strip end).  Unlike the legacy
       * CurveProfile widget (which stores path[0] at x=1 and path[last] at x=0,
       * requiring an x↔y swap), the GN curve's coordinate space already matches
       * what calculate_profile_segments expects, so no swap is needed. */
      r_xvals[i] = double(x);
      r_yvals[i] = double(y);
    }
  };

  /* Sample the seg_2 segments used during vertex mesh subdivision. */
  if (seg_2 != segments) {
    if (custom) {
      fill_from_custom(pro_spacing->xvals_2, pro_spacing->yvals_2, seg_2);
    }
    else {
      pro_spacing->xvals_2 = Array<double>(seg_2 + 1);
      pro_spacing->yvals_2 = Array<double>(seg_2 + 1);
      find_even_superellipse_chords(
          seg_2, bs->pro_super_r, pro_spacing->xvals_2, pro_spacing->yvals_2);
    }
  }

  /* Sample the input number of segments. */
  if (custom) {
    fill_from_custom(pro_spacing->xvals, pro_spacing->yvals, segments);
  }
  else {
    pro_spacing->xvals = Array<double>(segments + 1);
    pro_spacing->yvals = Array<double>(segments + 1);
    find_even_superellipse_chords(
        segments, bs->pro_super_r, pro_spacing->xvals, pro_spacing->yvals);
  }

  if (seg_2 == segments) {
    pro_spacing->xvals_2 = pro_spacing->xvals;
    pro_spacing->yvals_2 = pro_spacing->yvals;
  }
}

/* -------------------------------------------------------------------- */
/** \name Profile parameter setup and evaluation
 * \{ */

/* Find the closest point (`projco`) on edge `e_idx` to the line through `co_a` and `co_b`.
 * Mirrors BMesh's #project_to_edge. */
static float3 project_to_edge(const ExtendableMesh &emesh,
                              int e_idx,
                              const float3 &co_a,
                              const float3 &co_b)
{
  const int2 everts = emesh.edge_verts(e_idx);
  float3 projco, otherco;
  if (!math::isect_line_line(emesh.vert_position(everts[0]),
                             emesh.vert_position(everts[1]),
                             co_a,
                             co_b,
                             projco,
                             otherco))
  {
    return emesh.vert_position(everts[0]);
  }
  return projco;
}

/**
 * Sets `profile.start/middle/end/plane_co/plane_no/proj_dir/super_r` for a #BoundVert.
 * Ported from BMesh's #set_profile_params; edge access uses #ExtendableMesh instead of BMesh.
 */
static void set_profile_params(const BevelState &state, const BevVert *bv, BoundVert *bndv)
{
  bool do_linear_interp = true;
  const EdgeHalf *e = bndv->ebev;
  Profile &pro = bndv->profile;
  const ExtendableMesh &emesh = state.emesh;

  const float3 start = bndv->nv.co;
  const float3 end = bndv->next->nv.co;

  if (e) {
    do_linear_interp = false;
    pro.super_r = state.pro_super_r;
    /* Projection direction is along the beveled edge. */
    const int2 everts = emesh.src_edges[e->e];
    pro.proj_dir = emesh.vert_position(everts[0]) - emesh.vert_position(everts[1]);
    if (e->is_rev) {
      pro.proj_dir = -pro.proj_dir;
    }
    pro.proj_dir = math::normalize(pro.proj_dir);

    /* Middle = closest point on the edge line to the segment start-end. */
    pro.middle = project_to_edge(emesh, e->e, start, end);

    pro.start = start;
    pro.end = end;

    float3 d1 = math::normalize(float3(pro.middle) - start);
    float3 d2 = math::normalize(float3(pro.middle) - end);
    pro.plane_no = math::normalize(math::cross(d1, d2));

    if (geom::nearly_parallel(d1, d2)) {
      /* Start, middle, end are collinear. */
      const float3 v_co = emesh.src_positions[bv->v];
      pro.middle = v_co;

      if (e->prev->is_bev && e->next->is_bev && bv->selcount >= 3) {
        const int2 eprev_verts = emesh.src_edges[e->prev->e];
        const int2 enext_verts = emesh.src_edges[e->next->e];
        float3 d3 = math::normalize(emesh.vert_position(eprev_verts[0]) -
                                    emesh.vert_position(eprev_verts[1]));
        float3 d4 = math::normalize(emesh.vert_position(enext_verts[0]) -
                                    emesh.vert_position(enext_verts[1]));
        if (geom::nearly_parallel(d3, d4)) {
          pro.middle = math::midpoint(start, end);
          do_linear_interp = true;
        }
        else {
          const float3 co3 = start + d3;
          const float3 co4 = end + d4;
          float3 meetco;
          float3 isect2;
          if (math::isect_line_line(start, co3, end, co4, meetco, isect2) != 0) {
            pro.middle = meetco;
          }
          else {
            pro.middle = math::midpoint(start, end);
            do_linear_interp = true;
          }
        }
      }
      pro.end = end;
      d1 = math::normalize(float3(pro.middle) - start);
      d2 = math::normalize(float3(pro.middle) - end);
      pro.plane_no = math::normalize(math::cross(d1, d2));
      if (geom::nearly_parallel(d1, d2)) {
        do_linear_interp = true;
      }
      else {
        pro.plane_co = v_co;
        pro.proj_dir = pro.plane_no;
      }
    }
    pro.plane_co = start;
  }
  else if (bndv->is_arc_start) {
    pro.start = start;
    pro.end = end;
    pro.super_r = PRO_CIRCLE_R;
    pro.plane_co = float3(0.0f);
    pro.plane_no = float3(0.0f);
    pro.proj_dir = float3(0.0f);
    do_linear_interp = false;
  }
  else if (state.params.affect_type == BevelAffect::Vertices) {
    pro.start = start;
    pro.middle = emesh.src_positions[bv->v];
    pro.end = end;
    pro.super_r = state.pro_super_r;
    pro.plane_co = float3(0.0f);
    pro.plane_no = float3(0.0f);
    pro.proj_dir = float3(0.0f);
    do_linear_interp = false;
  }

  if (do_linear_interp) {
    pro.super_r = PRO_LINE_R;
    pro.start = start;
    pro.end = end;
    pro.middle = math::midpoint(start, end);
    pro.plane_co = float3(0.0f);
    pro.plane_no = float3(0.0f);
    pro.proj_dir = float3(0.0f);
  }
}

/**
 * Adjusts the profile plane of `bndv->profile` so that it contains the
 * plane through `bndv->profile.start`, `bndv->profile.end`, and `bmvert_co`.
 * Mirrors BMesh's #move_profile_plane. Sets `special_params = true` to prevent
 * #calculate_vm_profiles from resetting the parameters.
 * Currently used only in #build_boundary_terminal_edge.
 */
static void move_profile_plane(BoundVert *bndv, const float3 bmvert_co)
{
  Profile &pro = bndv->profile;

  /* Only do this if projecting, and start, end, and proj_dir are not coplanar. */
  if (math::is_zero(float3(pro.proj_dir))) {
    return;
  }

  const float3 d1 = math::normalize(bmvert_co - float3(pro.start));
  const float3 d2 = math::normalize(bmvert_co - float3(pro.end));
  float3 no = math::cross(d1, d2);
  float3 no2 = math::cross(d1, float3(pro.proj_dir));
  float3 no3 = math::cross(d2, float3(pro.proj_dir));

  float l1, l2, l3;
  no = math::normalize_and_get_length(no, l1);
  no2 = math::normalize_and_get_length(no2, l2);
  no3 = math::normalize_and_get_length(no3, l3);

  if (l1 > geom::BEVEL_EPSILON_BIG && l2 > geom::BEVEL_EPSILON_BIG && l3 > geom::BEVEL_EPSILON_BIG)
  {
    const float dot2 = math::dot(no, no2);
    const float dot3 = math::dot(no, no3);
    if (math::abs(dot2) < (1.0f - geom::BEVEL_EPSILON_BIG) &&
        math::abs(dot3) < (1.0f - geom::BEVEL_EPSILON_BIG))
    {
      pro.plane_no = no;
    }
  }

  /* Parameters are now non-default; prevent recalculation later. */
  pro.special_params = true;
}

/**
 * Adjusts the profile planes for the two #BoundVert instances involved in a weld.
 * Moves the plane to the one most likely to contain the profile projection intersections.
 * Sets `special_params = true` on both to prevent recalculation.
 * Mirrors BMesh's #move_weld_profile_planes.
 */
static void move_weld_profile_planes(BoundVert *bndv1, BoundVert *bndv2, const float3 v_co)
{
  /* Only do this if projecting. */
  if (math::is_zero(float3(bndv1->profile.proj_dir)) ||
      math::is_zero(float3(bndv2->profile.proj_dir)))
  {
    return;
  }
  const float3 d1 = v_co - float3(bndv1->nv.co);
  const float3 d2 = v_co - float3(bndv2->nv.co);
  float3 no = math::cross(d1, d2);
  float l1;
  no = math::normalize_and_get_length(no, l1);

  float3 no2 = math::cross(d1, float3(bndv1->profile.proj_dir));
  float l2;
  no2 = math::normalize_and_get_length(no2, l2);
  float3 no3 = math::cross(d2, float3(bndv2->profile.proj_dir));
  float l3;
  no3 = math::normalize_and_get_length(no3, l3);

  if (l1 != 0.0f && (l2 != 0.0f || l3 != 0.0f)) {
    const float dot1 = math::abs(math::dot(no, no2));
    const float dot2 = math::abs(math::dot(no, no3));
    if (math::abs(dot1 - 1.0f) > geom::BEVEL_EPSILON_D) {
      bndv1->profile.plane_no = no;
    }
    if (math::abs(dot2 - 1.0f) > geom::BEVEL_EPSILON_D) {
      bndv2->profile.plane_no = no;
    }
  }

  bndv1->profile.special_params = true;
  bndv2->profile.special_params = true;
}

/**
 * Fills `r_prof_co` with 3D positions for each segment point of the profile,
 * mapped from the 2D superellipse using the `map` matrix and projected along `proj_dir`.
 */
static void calculate_profile_segments(const Profile &pro,
                                       const float4x4 &map,
                                       const bool use_map,
                                       const bool reversed,
                                       const int ns,
                                       const double *xvals,
                                       const double *yvals,
                                       MutableSpan<float3> r_prof_co)
{
  for (int k = 0; k <= ns; k++) {
    float3 co;
    if (k == 0) {
      co = pro.start;
    }
    else if (k == ns) {
      co = pro.end;
    }
    else {
      if (use_map) {
        const float3 p = {
            reversed ? float(yvals[ns - k]) : float(xvals[k]),
            reversed ? float(xvals[ns - k]) : float(yvals[k]),
            0.0f,
        };
        co = math::transform_point(map, p);
      }
      else {
        co = math::interpolate(pro.start, pro.end, float(k) / float(ns));
      }
    }
    /* Project onto the profile plane along proj_dir. */
    if (!math::is_zero(float3(pro.proj_dir))) {
      const float3 co2 = co + float3(pro.proj_dir);
      r_prof_co[k] =
          math::isect_line_plane(co, co2, float3(pro.plane_co), float3(pro.plane_no)).value_or(co);
    }
    else {
      r_prof_co[k] = co;
    }
  }
}

/**
 * Computes the `prof_co` (and optionally `prof_co_2`) arrays for `bndv->profile`,
 * applying the superellipse (or custom profile) 2D-to-3D mapping and projection.
 * No-op when `params.segments == 1`.
 */
static void calculate_profile(BevelState &state, BoundVert *bndv, bool reversed, bool /*miter*/)
{
  Profile &pro = bndv->profile;
  const ProfileSpacing &pro_spacing = state.pro_spacing;

  if (state.params.segments <= 1) {
    return;
  }

  const bool need_2 = (state.params.segments != pro_spacing.seg_2);

  if (pro.prof_co.is_empty()) {
    pro.prof_co = Array<float3>(state.params.segments + 1);
    if (need_2) {
      pro.prof_co_2 = Array<float3>(pro_spacing.seg_2 + 1);
    }
    /* prof_co_2 alias is set below after prof_co is filled. */
  }

  /* Mirror the BMesh logic: skip the map only for a superellipse line profile.
   * When a custom profile is active the xvals/yvals in pro_spacing already encode the
   * custom shape, but they still need to be mapped through the unit-square-to-3D transform,
   * so use_map must be true in that case. */
  bool use_map;
  float4x4 map;
  if (state.params.custom_profile_samples.is_empty() && pro.super_r == PRO_LINE_R) {
    use_map = false;
  }
  else {
    use_map = geom::make_unit_square_map(pro.start, pro.middle, pro.end, map);
  }

  calculate_profile_segments(pro,
                             map,
                             use_map,
                             reversed,
                             state.params.segments,
                             pro_spacing.xvals.data(),
                             pro_spacing.yvals.data(),
                             pro.prof_co.as_mutable_span());
  if (need_2) {
    calculate_profile_segments(pro,
                               map,
                               use_map,
                               reversed,
                               pro_spacing.seg_2,
                               pro_spacing.xvals_2.data(),
                               pro_spacing.yvals_2.data(),
                               pro.prof_co_2.as_mutable_span());
  }
  else {
    /* When seg_2 == segments, prof_co_2 is a copy of the now-filled prof_co. */
    pro.prof_co_2 = pro.prof_co;
  }
}

/**
 * Returns the 3D coordinate of profile point `i` out of `nseg` for `pro`.
 * When `nseg == params.segments`, indexes into `prof_co`;
 * otherwise uses the higher-resolution `prof_co_2` with sub-sampling.
 */
static float3 get_profile_point(const BevelState &state, const Profile *pro, int i, int nseg)
{
  if (state.params.segments == 1) {
    return (i == 0 ? pro->start : pro->end);
  }
  if (nseg == state.params.segments) {
    BLI_assert(!pro->prof_co.is_empty());
    return pro->prof_co[i];
  }
  BLI_assert(is_power_of_2_i(nseg) && nseg <= state.pro_spacing.seg_2);
  const int subsample_spacing = state.pro_spacing.seg_2 / nseg;
  return pro->prof_co_2[i * subsample_spacing];
}

/**
 * Sets profile parameters for all #BoundVert entries of `vm` and computes
 * their profile coordinate arrays. This is the last step before vertex creation.
 */
static void calculate_vm_profiles(BevelState &state, BevVert *bv, VMesh *vm)
{
  BoundVert *bndv = vm->boundstart;
  do {
    if (!bndv->profile.special_params) {
      set_profile_params(state, bv, bndv);
    }
    bool miter_profile = false;
    bool reverse_profile = false;
    if (!state.params.custom_profile_samples.is_empty()) {
      /* Use the miter profile spacing struct if the default is filled with the custom profile. */
      miter_profile = (bndv->is_arc_start || bndv->is_patch_start);
      /* Don't bother reversing the profile if it's a miter profile. */
      reverse_profile = !bndv->is_profile_start && !miter_profile;
    }
    calculate_profile(state, bndv, reverse_profile, miter_profile);
  } while ((bndv = bndv->next) != vm->boundstart);
}

/** \} */

}  // namespace profile

float3 BevelState::face_center(const int f) const
{
  BLI_assert(f >= 0 && f < this->face_centers.size());
  return this->face_centers[f];
}

void BevelState::initialize_profile_data()
{
  const float psr = -std::numbers::ln2_v<float> /
                    std::log(math::sqrt(this->params.shape > 0 ? this->params.shape : 1e-20f));
  this->pro_super_r = psr;

  if (this->params.shape >= 0.950f) {
    this->pro_super_r = profile::PRO_SQUARE_R;
  }
  else if (abs(psr - profile::PRO_CIRCLE_R) < 1e-4f) {
    this->pro_super_r = profile::PRO_CIRCLE_R;
  }
  else if (abs(psr - profile::PRO_LINE_R) < 1e-4f) {
    this->pro_super_r = profile::PRO_LINE_R;
  }
  else if (abs(psr) < 1e-4f) {
    this->pro_super_r = profile::PRO_SQUARE_IN_R;
  }

  profile::set_profile_spacing(
      this, &this->pro_spacing, !this->params.custom_profile_samples.is_empty());

  if (this->params.segments > 1) {
    this->pro_spacing.fullness = profile::find_profile_fullness(this);
  }
}

void BevelState::uv_init()
{
  this->uv_layer_info.init(this->emesh.mesh);
  if (this->params.segments % 2 != 0) {
    this->uv_layer_info.find_components(this->emesh);
  }

  this->uv_vert_maps.clear();
  this->uv_vert_maps.resize(this->uv_layer_info.uv_maps.size());

  /* Allocate per-UV-attr storage for new corner UV values. */
  this->emesh.init_uv_storage(int(this->uv_layer_info.uv_maps.size()));
}

namespace construct {

/* Forward declaration -- defined in the ADJ vmesh subdivision section below. */
static VMesh adj_vmesh(BevelState &state, BevVert *bv);

/* -------------------------------------------------------------------- */
/** \name Representative face / edge selection
 *
 * Mirrors the BMesh `choose_rep_face`, `boundvert_rep_face`, and `frep_for_center_poly`
 * functions used to select which original face should be the "example" for new bevel faces.
 * The same 6-criterion lexicographic tie-breaking rule is used.
 * \{ */

/* === Helpers for is_bad_uv_poly === */

/**
 * Returns true if `co` is inside or on the boundary of face `f` when both are
 * projected to 2D along the face's dominant axis.
 * Mirrors #BM_face_point_inside_test.
 */
static bool face_point_inside_test(const ExtendableMesh &emesh, const int f, const float3 co)
{
  const float3 no = emesh.src_face_normals[f];
  const float3x3 axis_mat = math::axis_dominant_to_m3(no);

  /* Project the test point. */
  const float2 co_2d = float2(axis_mat * co);

  /* Project every corner of the face. */
  const OffsetIndices faces = emesh.src_faces;
  const Span<float3> positions = emesh.src_positions;
  const Span<int> corner_verts = emesh.src_corner_verts;
  const Span<int> face_verts = corner_verts.slice(faces[f]);
  Array<float2, 16> projverts(face_verts.size());
  for (const int i : face_verts.index_range()) {
    const int vert = face_verts[i];
    projverts[i] = float2(axis_mat * positions[vert]);
  }

  return math::isect_point_poly(co_2d, projverts);
}

/**
 * Sets `*r_e1` and `*r_e2` to the two original edges of face `f` that are
 * incident on original vertex `v_idx`.
 * Mirrors BMesh's #get_incident_edges.
 */
static void get_incident_edges(
    const ExtendableMesh &emesh, const int f, const int v_idx, int *r_e1, int *r_e2)
{
  *r_e1 = -1;
  *r_e2 = -1;
  if (f < 0) {
    return;
  }
  const Span<int2> edges = emesh.src_edges;
  const OffsetIndices faces = emesh.src_faces;
  const Span<int> corner_edges = emesh.src_corner_edges;
  for (const int e : corner_edges.slice(faces[f])) {
    const int2 &ev = edges[e];
    if (ev[0] == v_idx || ev[1] == v_idx) {
      if (*r_e1 < 0) {
        *r_e1 = e;
      }
      else if (*r_e2 < 0) {
        *r_e2 = e;
      }
    }
  }
}

/**
 * Find which #BoundVert positions of `bv` are inside face `f` when both are
 * projected to 2D.  At most 3 can be interior (the maximum between two edges
 * including miters).
 * Returns the number of internal vertices found.
 * Mirrors BMesh's #find_face_internal_boundverts.
 */
static int find_face_internal_boundverts(const ExtendableMesh &emesh,
                                         const BevVert *bv,
                                         const int f,
                                         BoundVert *(r_internal[3]))
{
  if (f < 0) {
    r_internal[0] = r_internal[1] = r_internal[2] = nullptr;
    return 0;
  }
  int n_internal = 0;
  VMesh *vm = bv->vmesh.get();
  BLI_assert(vm != nullptr);
  BoundVert *v = vm->boundstart;
  do {
    if (face_point_inside_test(emesh, f, float3(v->nv.co))) {
      r_internal[n_internal++] = v;
      if (n_internal == 3) {
        break;
      }
    }
  } while ((v = v->next) != vm->boundstart);
  for (int i = n_internal; i < 3; i++) {
    r_internal[i] = nullptr;
  }
  return n_internal;
}

/**
 * Project BoundVert positions snapped to the two incident edges of face `f` on
 * `bv->v`, compute the 2D area of the resulting polygon.
 * BoundVerts that are already inside `f` are used as-is.
 * Mirrors BMesh's #projected_boundary_area.
 */
static float projected_boundary_area(const BevelState &state, BevVert *bv, const int f)
{
  const ExtendableMesh &emesh = state.emesh;
  VMesh *vm = bv->vmesh.get();
  BLI_assert(vm != nullptr);

  const float3 no = emesh.src_face_normals[f];
  const float3x3 axis_mat = math::axis_dominant_to_m3(no);

  int e1 = -1, e2 = -1;
  get_incident_edges(emesh, f, bv->v, &e1, &e2);
  BLI_assert(e1 >= 0 && e2 >= 0);

  BoundVert *unsnapped[3];
  find_face_internal_boundverts(emesh, bv, f, unsnapped);

  const Span<float3> positions = emesh.src_positions;
  const int2 &ev1 = emesh.src_edges[e1];
  const int2 &ev2 = emesh.src_edges[e2];
  const float3 e1v1 = positions[ev1[0]];
  const float3 e1v2 = positions[ev1[1]];
  const float3 e2v1 = positions[ev2[0]];
  const float3 e2v2 = positions[ev2[1]];

  const int count = vm->count;
  Array<float2, 16> proj_co(count);
  BoundVert *v = vm->boundstart;
  int i = 0;
  do {
    const float3 co = v->nv.co;
    if (ELEM(v, unsnapped[0], unsnapped[1], unsnapped[2])) {
      proj_co[i] = float2(axis_mat * co);
    }
    else {
      const float3 snap1 = math::closest_to_line_segment(co, e1v1, e1v2);
      const float3 snap2 = math::closest_to_line_segment(co, e2v1, e2v2);
      const float d1_sq = math::dist_squared_to_line_segment(co, e1v1, e1v2);
      const float d2_sq = math::dist_squared_to_line_segment(co, e2v1, e2v2);
      proj_co[i] = float2(axis_mat * (d1_sq <= d2_sq ? snap1 : snap2));
    }
    ++i;
  } while ((v = v->next) != vm->boundstart);

  return area_poly_v2(reinterpret_cast<const float (*)[2]>(proj_co.data()), count);
}

/**
 * Returns true if choosing face `frep` as the representative for `bv`'s center
 * polygon would result in a degenerate (near-zero area) UV polygon.
 * Mirrors BMesh's #is_bad_uv_poly.
 */
static bool is_bad_uv_poly(const BevelState &state, BevVert *bv, const int frep)
{
  BLI_assert(bv->vmesh != nullptr);
  const float area = projected_boundary_area(state, bv, frep);
  return area < 1e-4f; /* BEVEL_EPSILON_BIG */
}

/**
 * Choose the best representative face index from `faces` (original face indices, -1 = skip).
 *
 * Tie-breaking criteria (lower value wins at each priority):
 *   0. UV-connected-component id (`uv_layer_info.face_component`; 0 when no UV layers).
 *   1. Reserved for a future "face selected" attribute; always 0.0f for now.
 *   2. Material index (`material_index` face attribute; 0 when absent).
 *   3. Higher z-coordinate of face center (stored negated so "higher wins" = lower value).
 *   4. Lower x-coordinate of face center.
 *   5. Lower y-coordinate of face center.
 *
 * Returns -1 when every candidate is -1.
 */
static int choose_rep_face(const BevelState &state, const Span<int> faces)
{
  constexpr int VEC_VALUE_LEN = 6;

  const int nfaces = int(faces.size());
  if (nfaces == 0) {
    return -1;
  }

  /* Read optional per-face attributes once. */
  const uv::UVMapInfo &uvi = state.uv_layer_info;
  const Mesh &mesh = state.emesh.mesh;
  const bke::AttributeAccessor attrs = mesh.attributes();
  VArraySpan<int> mat_span;
  {
    bke::AttributeReader<int> mat_reader = attrs.lookup<int>("material_index",
                                                             bke::AttrDomain::Face);
    if (mat_reader) {
      mat_span = VArraySpan<int>(mat_reader.varray);
    }
  }

  /* Build score vectors for each non-null candidate. */
  Array<float[VEC_VALUE_LEN]> value_vecs(nfaces);
  Array<bool> still_viable(nfaces, false);
  int num_viable = 0;

  for (int fi = 0; fi < nfaces; fi++) {
    const int f = faces[fi];
    if (f < 0 || f >= mesh.faces_num) {
      continue;
    }
    still_viable[fi] = true;
    num_viable++;

    int vi = 0;
    /* 0: UV-island component. */
    value_vecs[fi][vi++] = (!uvi.face_component.is_empty()) ? float(uvi.face_component[f]) : 0.0f;
    /* 1: Selected-face placeholder (always 0; no face-selection concept in mesh path). */
    value_vecs[fi][vi++] = 0.0f;
    /* 2: Material index. */
    value_vecs[fi][vi++] = mat_span.is_empty() ? 0.0f : float(mat_span[f]);
    /* 3–5: Face center coordinates.  Lower z wins (matches BMesh's un-negated cent[2]). */
    const float3 cent = state.face_center(f);
    value_vecs[fi][vi++] = cent.z;
    value_vecs[fi][vi++] = cent.x;
    value_vecs[fi][vi++] = cent.y;
    BLI_assert(vi == VEC_VALUE_LEN);
  }

  if (num_viable == 0) {
    return -1;
  }

  /* Lexicographic elimination: find unique minimum at each criterion. */
  int best_fi = -1;
  for (int vi = 0; num_viable > 1 && vi < VEC_VALUE_LEN; vi++) {
    for (int fi = 0; fi < nfaces; fi++) {
      if (!still_viable[fi] || fi == best_fi) {
        continue;
      }
      if (best_fi == -1) {
        best_fi = fi;
        continue;
      }
      if (value_vecs[fi][vi] < value_vecs[best_fi][vi]) {
        best_fi = fi;
        /* Eliminate all previous viable candidates. */
        for (int j = fi - 1; j >= 0; j--) {
          if (still_viable[j]) {
            still_viable[j] = false;
            num_viable--;
          }
        }
      }
      else if (value_vecs[fi][vi] > value_vecs[best_fi][vi]) {
        still_viable[fi] = false;
        num_viable--;
      }
    }
  }
  if (best_fi == -1) {
    best_fi = 0;
  }
  return faces[best_fi];
}

/**
 * Return a good representative face for faces created around/near BoundVert `v`.
 * Mirrors BMesh's #boundvert_rep_face.
 * Face indices are original mesh indices; -1 means none.
 * If `r_fother` is non-null, a secondary candidate (or -1) is stored there.
 */
static int boundvert_rep_face(const BoundVert *v, int *r_fother)
{
  int frep;
  int frep2 = -1;

  if (v->ebev) {
    frep = v->ebev->fprev;
    if (v->efirst->fprev != frep) {
      frep2 = v->efirst->fprev;
    }
  }
  else if (v->efirst) {
    frep = v->efirst->fprev;
    if (frep >= 0) {
      if (v->elast->fnext != frep) {
        frep2 = v->elast->fnext;
      }
      else if (v->efirst->fnext != frep) {
        frep2 = v->efirst->fnext;
      }
      else if (v->elast->fprev != frep) {
        frep2 = v->elast->fprev;
      }
    }
    else if (v->efirst->fnext >= 0) {
      frep = v->efirst->fnext;
      if (v->elast->fnext != frep) {
        frep2 = v->elast->fnext;
      }
    }
    else if (v->elast->fprev >= 0) {
      frep = v->elast->fprev;
    }
  }
  else if (v->prev->elast) {
    frep = v->prev->elast->fnext;
    if (v->next->efirst) {
      if (frep >= 0) {
        frep2 = v->next->efirst->fprev;
      }
      else {
        frep = v->next->efirst->fprev;
      }
    }
  }
  else {
    frep = -1;
  }

  if (r_fother) {
    *r_fother = frep2;
  }
  return frep;
}

/**
 * Returns the edge (either `e1` or `e2`) whose line segment is closest to `co`.
 * Mirrors BMesh's #find_closer_edge.
 */
static int find_closer_edge(const ExtendableMesh &emesh,
                            const float3 &co,
                            const int e1,
                            const int e2)
{
  BLI_assert(e1 >= 0 && e2 >= 0);
  const int2 ev1 = emesh.edge_verts(e1);
  const int2 ev2 = emesh.edge_verts(e2);
  const float dsq1 = math::dist_squared_to_line_segment(
      co, emesh.vert_position(ev1[0]), emesh.vert_position(ev1[1]));
  const float dsq2 = math::dist_squared_to_line_segment(
      co, emesh.vert_position(ev2[0]), emesh.vert_position(ev2[1]));
  return (dsq1 <= dsq2) ? e1 : e2;
}

/**
 * Returns the edge to snap to for the center-polygon vertex at BoundVert index `i`.
 * Mirrors BMesh's #snap_edge_for_center_vmesh_vert.
 * `eprev` / `enext` are the bevel edges for BoundVert `i-1` / `i` respectively (-1 = none).
 */
static int snap_edge_for_center_vmesh_vert(const int i,
                                           const int n_bndv,
                                           const int eprev,
                                           const int enext,
                                           const Span<int> bndv_rep_faces,
                                           const int center_frep,
                                           const Span<bool> frep_beats_next)
{
  const int previ = (i + n_bndv - 1) % n_bndv;
  const int nexti = (i + 1) % n_bndv;
  if (frep_beats_next[previ] && bndv_rep_faces[previ] == center_frep) {
    return eprev;
  }
  if (!frep_beats_next[i] && bndv_rep_faces[nexti] == center_frep) {
    return enext;
  }
  return -1;
}

/**
 * Fills `r_snap_edges[4]` with per-corner snap-edge indices for the ring quad at (i, j, k).
 * Mirrors BMesh's #snap_edges_for_vmesh_vert.
 * Only meaningful for odd segment counts; for even ns all entries are set to -1.
 */
static void snap_edges_for_vmesh_vert(const int i,
                                      const int j,
                                      const int k,
                                      const int ns,
                                      const int ns2,
                                      const int n_bndv,
                                      const int eprev,
                                      const int enext,
                                      const int enextnext,
                                      const Span<int> bndv_rep_faces,
                                      const int center_frep,
                                      const Span<bool> frep_beats_next,
                                      int r_snap_edges[4])
{
  BLI_assert(0 <= i && i < n_bndv && 0 <= j && j < ns2 && 0 <= k && k <= ns2);
  for (int corner = 0; corner < 4; corner++) {
    r_snap_edges[corner] = -1;
    if (ns % 2 == 0) {
      continue;
    }
    const int previ = (i + n_bndv - 1) % n_bndv;
    /* jj and kk are the j and k indices for this corner. */
    const int jj = corner < 2 ? j : j + 1;
    const int kk = ELEM(corner, 0, 3) ? k : k + 1;
    if (jj < ns2 && kk < ns2) {
      /* No snap. */
    }
    else if (jj < ns2 && kk == ns2) {
      if (!frep_beats_next[i]) {
        r_snap_edges[corner] = enext;
      }
    }
    else if (jj < ns2 && kk == ns2 + 1) {
      if (frep_beats_next[i]) {
        r_snap_edges[corner] = enext;
      }
    }
    else if (jj == ns2 && kk < ns2) {
      if (frep_beats_next[previ]) {
        r_snap_edges[corner] = eprev;
      }
    }
    else if (jj == ns2 && kk == ns2) {
      r_snap_edges[corner] = snap_edge_for_center_vmesh_vert(
          i, n_bndv, eprev, enext, bndv_rep_faces, center_frep, frep_beats_next);
    }
    else if (jj == ns2 && kk == ns2 + 1) {
      const int nexti = (i + 1) % n_bndv;
      r_snap_edges[corner] = snap_edge_for_center_vmesh_vert(
          nexti, n_bndv, enext, enextnext, bndv_rep_faces, center_frep, frep_beats_next);
    }
  }
}

/**
 * Pick a good representative face for the center polygon of `bv`.
 * Collects one candidate per beveled edge (choosing from fprev/fnext), eliminates
 * duplicates, then calls #choose_rep_face on the shortlist.
 * Mirrors BMesh's #frep_for_center_poly.
 */
static int frep_for_center_poly(const BevelState &state, const BevVert *bv)
{
  const bool consider_all_faces = (bv->selcount == 1 || state.affect_vertices_odd);

  /* Collect candidates (at most edgecount, one per edge). */
  Array<int> fchoices(bv->edgecount, -1);
  int fcount = 0;
  int any_f = -1;

  for (int i = 0; i < bv->edgecount; i++) {
    const EdgeHalf &e = bv->edges[i];
    if (!e.is_bev && !consider_all_faces) {
      continue;
    }
    const int candidates[2] = {e.fprev, e.fnext};
    const int bmf = choose_rep_face(state, Span<int>(candidates, 2));
    if (bmf < 0) {
      continue;
    }
    if (any_f < 0) {
      any_f = bmf;
    }
    /* Skip duplicates. */
    bool already_there = false;
    for (int j = fcount - 1; j >= 0; j--) {
      if (fchoices[j] == bmf) {
        already_there = true;
        break;
      }
    }
    if (!already_there) {
      if (state.uv_layer_info.has_uv_maps) {
        /* Skip candidates that would produce a degenerate UV polygon. */
        if (is_bad_uv_poly(state, const_cast<BevVert *>(bv), bmf)) {
          continue;
        }
      }
      fchoices[fcount++] = bmf;
    }
  }

  if (fcount == 0) {
    return any_f;
  }
  return choose_rep_face(state, fchoices.as_span().take_front(fcount));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VMesh face builders
 * \{ */

/**
 * Creates the center polygon (or ngon) face for `bv` from the first boundary vertex of each
 * arc (and intermediate arc verts for multi-segment bevels). Returns the new face index,
 * or -1 when degenerate (fewer than 3 verts).
 */
static int bevel_build_poly(BevelState &state, BevVert *bv)
{
  VMesh *vm = bv->vmesh.get();
  const int ns = vm->seg;

  /* Gather per-boundvert data for UV snap: frep, its two incident edges, and unsnapped verts. */
  const ExtendableMesh &emesh = state.emesh;
  const int frep = frep_for_center_poly(state, bv);
  int frep_e1 = -1, frep_e2 = -1;
  BoundVert *frep_unsnapped[3] = {nullptr, nullptr, nullptr};
  if (bv->any_seam && frep >= 0) {
    get_incident_edges(emesh, frep, bv->v, &frep_e1, &frep_e2);
    find_face_internal_boundverts(emesh, bv, frep, frep_unsnapped);
  }

  Vector<int, 32> verts;
  Vector<int, 32> corner_reps;
  Vector<int, 32> corner_snaps;

  BoundVert *bndv = vm->boundstart;
  do {
    const int bndv_i = bndv->index;
    /* Lambda to append one corner's data. */
    auto append_corner = [&](int vert_idx) {
      verts.append(vert_idx);
      if (frep >= 0 && bv->any_seam) {
        /* With a seam-adjacent frep: snap to the closer incident edge, unless this
         * boundvert is in frep_unsnapped (internal to the face). */
        corner_reps.append(frep);
        const bool is_unsnapped = ELEM(
            bndv, frep_unsnapped[0], frep_unsnapped[1], frep_unsnapped[2]);
        if (is_unsnapped || frep_e1 < 0 || frep_e2 < 0) {
          corner_snaps.append(-1);
        }
        else {
          const float3 co = emesh.vert_position(vert_idx);
          corner_snaps.append(find_closer_edge(emesh, co, frep_e1, frep_e2));
        }
      }
      else {
        /* No seam: each boundvert uses its own rep face, no snapping. */
        corner_reps.append(boundvert_rep_face(bndv, nullptr));
        corner_snaps.append(-1);
      }
    };
    (void)bndv_i;

    append_corner(geom::mesh_vert(vm, bndv_i, 0, 0)->v);
    if (bndv->ebev && ns > 1) {
      for (int k = 1; k < ns; k++) {
        /* Profile-arc intermediate verts: use the same snap rules as the boundvert corner. */
        const int pv = geom::mesh_vert(vm, bndv_i, 0, k)->v;
        verts.append(pv);
        if (frep >= 0 && bv->any_seam) {
          corner_reps.append(frep);
          if (frep_e1 >= 0 && frep_e2 >= 0) {
            const float3 co = emesh.vert_position(pv);
            /* For profile-arc intermediates, snap according to which half they're in
             * (mirrors BMesh lines 6353-6362). */
            const int snap_e = (k < ns / 2) ? -1 : find_closer_edge(emesh, co, frep_e1, frep_e2);
            corner_snaps.append(snap_e);
          }
          else {
            corner_snaps.append(-1);
          }
        }
        else {
          corner_reps.append(boundvert_rep_face(bndv, nullptr));
          corner_snaps.append(-1);
        }
      }
    }
  } while ((bndv = bndv->next) != vm->boundstart);

  if (verts.size() < 3) {
    return -1;
  }
  const int new_face = state.emesh.face_create(verts.as_span(), frep);
  state.emesh.face_set_corner_reps(new_face, corner_reps.as_span(), corner_snaps.as_span());
#ifdef BEVEL_DEBUG
  {
    fmt::println("bevel_build_poly: bv->v={} any_seam={} frep={} ns={} n_verts={}",
                 bv->v,
                 bv->any_seam,
                 frep,
                 ns,
                 int(verts.size()));
    for (int ci = 0; ci < int(verts.size()); ci++) {
      fmt::println(
          "  corner[{}] v={} rep={} snap={}", ci, verts[ci], corner_reps[ci], corner_snaps[ci]);
    }
  }
#endif
  return new_face;
}

/**
 * Builds a triangle-fan for `bv` (M_TRI_FAN) by collecting the boundary ring vertices
 * (same as #bevel_build_poly) and then fanning triangles from the last ring vertex,
 * mirroring BMesh's approach of splitting from `BM_FACE_FIRST_LOOP(f)->prev->v`.
 */
static void bevel_build_trifan(BevelState &state, BevVert *bv)
{
  VMesh *vm = bv->vmesh.get();
  const int ns = vm->seg;
  BLI_assert(ns == 1 || bv->selcount == 1);

  /* Build the same per-corner data as bevel_build_poly. */
  const ExtendableMesh &emesh = state.emesh;
  const int frep = (bv->any_seam) ? frep_for_center_poly(state, bv) : -1;
  int frep_e1 = -1, frep_e2 = -1;
  BoundVert *frep_unsnapped[3] = {nullptr, nullptr, nullptr};
  if (bv->any_seam && frep >= 0) {
    get_incident_edges(emesh, frep, bv->v, &frep_e1, &frep_e2);
    find_face_internal_boundverts(emesh, bv, frep, frep_unsnapped);
  }

  /* Collect all verts/reps/snaps for the fan polygon, then split into tris. */
  Vector<int, 32> ring;
  Vector<int, 32> ring_reps;
  Vector<int, 32> ring_snaps;

  BoundVert *bndv = vm->boundstart;
  do {
    const int bndv_i = bndv->index;
    auto append_ring_corner = [&](int vert_idx) {
      ring.append(vert_idx);
      if (frep >= 0 && bv->any_seam) {
        ring_reps.append(frep);
        const bool is_unsnapped = ELEM(
            bndv, frep_unsnapped[0], frep_unsnapped[1], frep_unsnapped[2]);
        if (is_unsnapped || frep_e1 < 0 || frep_e2 < 0) {
          ring_snaps.append(-1);
        }
        else {
          const float3 co = emesh.vert_position(vert_idx);
          ring_snaps.append(find_closer_edge(emesh, co, frep_e1, frep_e2));
        }
      }
      else {
        ring_reps.append(boundvert_rep_face(bndv, nullptr));
        ring_snaps.append(-1);
      }
    };
    append_ring_corner(geom::mesh_vert(vm, bndv_i, 0, 0)->v);
    if (bndv->ebev && ns > 1) {
      for (int k = 1; k < ns; k++) {
        ring.append(geom::mesh_vert(vm, bndv_i, 0, k)->v);
        ring_reps.append(frep >= 0 && bv->any_seam ? frep : boundvert_rep_face(bndv, nullptr));
        ring_snaps.append(-1);
      }
    }
  } while ((bndv = bndv->next) != vm->boundstart);

  if (ring.size() < 3) {
    return;
  }

  /* Mirror BMesh: the fan apex is the last loop vertex of the polygon built by bevel_build_poly,
   * which is BM_FACE_FIRST_LOOP(f)->prev->v — i.e. the last element in the ring. */
  const int n = int(ring.size());
  const int v_fan_idx = n - 1;
  const int v_fan = ring[v_fan_idx];
  for (int i = 0; i + 1 < v_fan_idx; i++) {
    const int tri[3] = {v_fan, ring[i], ring[i + 1]};
    const int tri_reps[3] = {ring_reps[v_fan_idx], ring_reps[i], ring_reps[i + 1]};
    const int tri_snaps[3] = {ring_snaps[v_fan_idx], ring_snaps[i], ring_snaps[i + 1]};
    /* Use frep as the face example only when there is a seam-adjacent rep face, mirroring
     * BMesh's bev_create_ngon call in bevel_build_poly which passes repface only when
     * bv->any_seam is true. Otherwise use the representative face of the leading edge. */
    const int face_example = (frep >= 0) ? frep : tri_reps[1];
    const int new_face = state.emesh.face_create(Span<int>(tri, 3), face_example);
    state.emesh.face_set_corner_reps(new_face, Span<int>(tri_reps, 3), Span<int>(tri_snaps, 3));
  }
}

/**
 * M_NONE with two boundary verts and vertex bevel: places intermediate profile
 * verts and, if the original vertex had no adjacent faces, creates the connecting edges.
 */
static void bevel_vert_two_edges(BevelState &state, BevVert *bv)
{
  VMesh *vm = bv->vmesh.get();
  BLI_assert(vm->count == 2 && state.params.affect_type == BevelAffect::Vertices);

  const int ns = vm->seg;
  const int v1 = geom::mesh_vert(vm, 0, 0, 0)->v;
  const int v2 = geom::mesh_vert(vm, 1, 0, 0)->v;

  if (ns > 1) {
    /* Set up a linear profile from v1 to v2 through the original vert. */
    BoundVert *bndv0 = vm->boundstart;
    Profile &pro = bndv0->profile;
    pro.super_r = state.pro_super_r;
    pro.start = geom::mesh_vert(vm, 0, 0, 0)->co;
    pro.end = geom::mesh_vert(vm, 1, 0, 0)->co;
    pro.middle = state.emesh.src_positions[bv->v];
    pro.plane_co = float3(0.0f);
    pro.plane_no = float3(0.0f);
    pro.proj_dir = float3(0.0f);
    profile::calculate_profile(state, bndv0, false, false);

    for (int k = 1; k < ns; k++) {
      const float3 co = profile::get_profile_point(state, &pro, k, ns);
      const int nv = state.emesh.vert_create(co, bv->v);
      geom::mesh_vert(vm, 0, 0, k)->co = co;
      geom::mesh_vert(vm, 0, 0, k)->v = nv;
    }
    geom::mesh_vert(vm, 0, 0, ns)->co = geom::mesh_vert(vm, 1, 0, 0)->co;
    geom::mesh_vert(vm, 0, 0, ns)->v = v2;

    /* Mirror to the second BoundVert arc (reversed). */
    for (int k = 1; k < ns; k++) {
      geom::mesh_vert(vm, 1, 0, ns - k)->co = geom::mesh_vert(vm, 0, 0, k)->co;
      geom::mesh_vert(vm, 1, 0, ns - k)->v = geom::mesh_vert(vm, 0, 0, k)->v;
    }
  }

  /* Create edges between successive arc verts if the original vertex had no faces. */
  for (int k = 0; k < ns; k++) {
    const int va = geom::mesh_vert(vm, 0, 0, k)->v;
    const int vb = geom::mesh_vert(vm, 0, 0, k + 1)->v;
    if (va >= 0 && vb >= 0) {
      state.emesh.edge_create(va, vb);
    }
  }
  (void)v1;
}

/**
 * Builds the ADJ (grid-fill) face mesh for `bv` (M_ADJ).
 * Vertex coordinates must already be set in `vm->mesh`; this function creates the
 * quad faces (and center ngon for odd segment counts).
 * Each face receives a representative original face example chosen by the same
 * lexicographic tie-breaking rule as BMesh's #bevel_build_rings.
 */
static void bevel_build_rings(BevelState &state, BevVert *bv)
{
  VMesh *vm = bv->vmesh.get();
  const int n_bndv = vm->count;
  const int ns = vm->seg;
  const int ns2 = ns / 2;
  const int odd = ns % 2;
  BLI_assert(n_bndv >= 3 && ns > 1);

  /* Compute the representative face for each BoundVert sector. */
  Array<int> bndv_rep_faces(n_bndv, -1);
  {
    BoundVert *bv_iter = vm->boundstart;
    do {
      bndv_rep_faces[bv_iter->index] = boundvert_rep_face(bv_iter, nullptr);
    } while ((bv_iter = bv_iter->next) != vm->boundstart);
  }

  /* For odd segment counts, pre-compute which rep wins the center-line tie-break
   * and the center polygon representative, mirroring BMesh's frep_beats_next logic. */
  Array<bool> frep_beats_next;
  int center_frep = -1;
  /* Per-bndv center-vertex snap edges accumulated during the ring quad loop. */
  Array<int> center_snap_edges(n_bndv, -1);
  if (odd && state.params.affect_type != BevelAffect::Vertices) {
    frep_beats_next = Array<bool>(n_bndv, false);
    center_frep = frep_for_center_poly(state, bv);
    for (int i = 0; i < n_bndv; i++) {
      const int inext = (i + 1) % n_bndv;
      const int candidates[2] = {bndv_rep_faces[i], bndv_rep_faces[inext]};
      const int winner = choose_rep_face(state, Span<int>(candidates, 2));
      frep_beats_next[i] = (winner == bndv_rep_faces[i]);
    }
  }

  BoundVert *bndv = vm->boundstart;
  do {
    const int i = bndv->index;
    const int inext = bndv->next->index;
    const int iprev = bndv->prev->index;
    const int f = bndv_rep_faces[i];
    const int f2 = bndv_rep_faces[inext];
    const int fc = (odd && state.params.affect_type != BevelAffect::Vertices) ?
                       (frep_beats_next[i] ? f : f2) :
                       -1;

    /* Bevel-edge indices for snap purposes (-1 when not applicable). */
    const EdgeHalf *ebev = (state.params.affect_type != BevelAffect::Vertices) ? bndv->ebev :
                                                                                 bndv->efirst;
    const EdgeHalf *ebev_prev = (state.params.affect_type != BevelAffect::Vertices) ?
                                    bndv->prev->ebev :
                                    bndv->prev->efirst;
    const EdgeHalf *ebev_next = (state.params.affect_type != BevelAffect::Vertices) ?
                                    bndv->next->ebev :
                                    bndv->next->efirst;
    const int bme = ebev ? ebev->e : -1;
    const int bmeprev = ebev_prev ? ebev_prev->e : -1;
    const int bmenext = ebev_next ? ebev_next->e : -1;

    for (int j = 0; j < ns2; j++) {
      for (int k = 0; k < ns2 + odd; k++) {
        /* Quad with lower-left corner at (i, j, k). */
        const int va = geom::mesh_vert(vm, i, j, k)->v;
        const int vb = geom::mesh_vert(vm, i, j, k + 1)->v;
        const int vc = geom::mesh_vert(vm, i, j + 1, k + 1)->v;
        const int vd = geom::mesh_vert(vm, i, j + 1, k)->v;
        if (va < 0 || vb < 0 || vc < 0 || vd < 0) {
          continue;
        }

        /* Choose face rep and per-corner snap edges, mirroring BMesh's bevel_build_rings
         * (lines 6063–6127 of bmesh_bevel.cc). */
        int face_rep = f;
        int se[4] = {-1, -1, -1, -1};
        /* Per-corner face reps (-1 = use face_rep fallback). */
        int cr[4] = {-1, -1, -1, -1};

        if (state.params.affect_type == BevelAffect::Vertices) {
          /* Vertex bevel: all corners start as f2. */
          face_rep = f2;
          if (j < k) {
            if (k == ns2 && j == ns2 - 1) {
              se[2] = bmenext;
              se[3] = bme;
            }
          }
          else if (j == k) {
            se[0] = se[2] = bme;
            if (!ebev || !ebev->is_seam) {
              /* Non-seam diagonal: corner 3 uses f instead of f2.
               * Mirrors BMesh bevel_build_rings line 6077: fr[3] = f. */
              cr[3] = f;
            }
          }
        }
        else {
          /* Edge bevel: default all to f. */
          face_rep = f;
          if (odd) {
            /* Compute snap edges for odd-segment ring quads. */
            const int b1 = (ebev_prev && ebev_prev->is_seam) ? bmeprev : -1;
            const int b2 = (ebev && ebev->is_seam) ? bme : -1;
            const int b3 = (ebev_next && ebev_next->is_seam) ? bmenext : -1;
            snap_edges_for_vmesh_vert(i,
                                      j,
                                      k,
                                      ns,
                                      ns2,
                                      n_bndv,
                                      b1,
                                      b2,
                                      b3,
                                      bndv_rep_faces.as_span(),
                                      center_frep,
                                      frep_beats_next.as_span(),
                                      se);
            if (k == ns2) {
              if (!ebev || ebev->is_seam) {
                face_rep = fc;
              }
              else {
                /* Non-seam center-column: corners 0,3 use f, corners 1,2 use f2.
                 * Mirrors BMesh bevel_build_rings lines 6105-6106:
                 *   fr[0] = fr[3] = f; fr[1] = fr[2] = f2. */
                cr[1] = cr[2] = f2;
              }
              if (j == ns2 - 1) {
                /* Record center-vert snap for the center ngon. */
                center_snap_edges[i] = se[3];
              }
            }
          }
          else {
            /* Even-segment ring quads: snap adjacent to center line. */
            if (k == ns2 - 1) {
              se[1] = bme;
            }
            if (j == ns2 - 1 && ebev_prev) {
              se[3] = bmeprev;
            }
            se[2] = (se[1] >= 0) ? se[1] : se[3];
          }
        }

        const int quad[4] = {va, vb, vc, vd};
        const int new_face = state.emesh.face_create(Span<int>(quad, 4), face_rep);
        /* Apply per-corner snap edges and face reps when any are non-default. */
        const bool any_snaps = se[0] >= 0 || se[1] >= 0 || se[2] >= 0 || se[3] >= 0;
        const bool any_corner_reps = cr[0] >= 0 || cr[1] >= 0 || cr[2] >= 0 || cr[3] >= 0;
        if (any_snaps || any_corner_reps) {
          state.emesh.face_set_corner_reps(new_face,
                                           any_corner_reps ? Span<int>(cr, 4) : Span<int>{},
                                           any_snaps ? Span<int>(se, 4) : Span<int>{});
        }
      }
    }
    (void)inext;
    (void)iprev;
    (void)fc;
  } while ((bndv = bndv->next) != vm->boundstart);

  /* Center ngon for odd segment count. */
  if (odd) {
    Vector<int, 16> center_verts_vec;
    Vector<int, 16> center_reps_vec;
    Vector<int, 16> center_snaps_vec;

    /* For edge bevel, collect the per-bndv center-vertex data recorded during the ring
     * quad loop above. For vertex bevel, fall back to build_center_ngon style logic. */
    if (state.params.affect_type != BevelAffect::Vertices) {
      /* center_vert_snap_edge and center_face_interp were accumulated per bndv. */
      const bool have_seam = bv->any_seam;
      const int cfrep = center_frep;
      bndv = vm->boundstart;
      do {
        const int ci = bndv->index;
        center_verts_vec.append(geom::mesh_vert(vm, ci, ns2, ns2)->v);
        center_reps_vec.append(have_seam ? cfrep : bndv_rep_faces[ci]);
        center_snaps_vec.append(center_snap_edges[ci]);
      } while ((bndv = bndv->next) != vm->boundstart);
    }
    else {
      /* Vertex bevel center poly: same as build_center_ngon. */
      bndv = vm->boundstart;
      do {
        center_verts_vec.append(geom::mesh_vert(vm, bndv->index, ns2, ns2)->v);
        center_reps_vec.append(-1);
        center_snaps_vec.append(-1);
      } while ((bndv = bndv->next) != vm->boundstart);
    }

    if (center_verts_vec.size() >= 3) {
      const int cface = state.emesh.face_create(center_verts_vec.as_span(), center_frep);
      state.emesh.face_set_corner_reps(
          cface, center_reps_vec.as_span(), center_snaps_vec.as_span());
    }
  }

  /* Tag the vmesh center-column edges as #NewEdgeKind::MidEdge. */
  bndv = vm->boundstart;
  do {
    const int i = bndv->index;
    for (int j = 0; j < ns2; j++) {
      const int va = geom::mesh_vert(vm, i, j, ns2)->v;
      const int vb = geom::mesh_vert(vm, i, j + 1, ns2)->v;
      if (va >= 0 && vb >= 0) {
        const int mid_edge = state.emesh.find_edge(va, vb);
        if (mid_edge >= 0) {
          state.emesh.tag_edge_kind(mid_edge, NewEdgeKind::MidEdge);
        }
      }
    }
  } while ((bndv = bndv->next) != vm->boundstart);
}

/* -------------------------------------------------------------------- */
/** \name Face rebuild
 * \{ */

/**
 * Returns the number of EdgeHalf steps CCW from `e1` to `e2` around the same BevVert ring.
 * Mirrors BMesh's #count_ccw_edges_between.
 */
static int count_ccw_edges_between(const EdgeHalf *e1, const EdgeHalf *e2)
{
  int count = 0;
  const EdgeHalf *e = e1;
  do {
    if (e == e2) {
      break;
    }
    e = e->next;
    count++;
  } while (e != e1);
  return count;
}

/**
 * Rebuilds face `f_idx` of `emesh` by replacing beveled vertex corners with
 * the corresponding boundary ring arcs from each #BevVert's #VMesh.
 * Returns the new face index if rebuilt, or -1 otherwise.
 * Mirrors BMesh's #bev_rebuild_polygon.
 */
static int bev_rebuild_polygon(BevelState &state, const int f_idx)
{
  const ExtendableMesh &emesh = state.emesh;
  const IndexRange corners = emesh.src_faces[f_idx];
  if (corners.size() < 3) {
    return false;
  }

  bool do_rebuild = false;
  /* New vertex indices for the rebuilt face. */
  Vector<int, 32> vv;
  /* Representative original vertex for each vv entry.
   * For non-beveled vertices this equals vv[i] (the original vertex).
   * For VMesh arc vertices this equals bv->v (the original beveled
   * vertex whose VMesh produced the arc). Used to identify the
   * best-matching original edge example for each rebuilt edge. */
  Vector<int, 32> orig_v;
  /* Original edge example for the segment from vv[i] to vv[i+1].
   * Mirrors BMesh's `ee[]` array in #bev_rebuild_polygon.
   * For arc edges within a beveled vertex, this is the original beveled
   * edge (e->e). For non-beveled vertex edges, this is the face loop's
   * edge (e_prev_idx). -1 if no suitable example is known. */
  Vector<int, 32> orig_e;

  const int sz = int(corners.size());
  for (int ci = 0; ci < sz; ci++) {
    const int c = corners[ci];
    const int v_idx = emesh.src_corner_verts[c];
    const int e_idx = emesh.src_corner_edges[c];
    const int c_prev = corners[(ci + sz - 1) % sz];
    const int e_prev_idx = emesh.src_corner_edges[c_prev];

    BevVert *bv = state.vert_hash.lookup_default(v_idx, nullptr);
    if (bv && bv->vmesh) {
      /* This corner's vertex is a beveled vertex.
       * Replace it with the arc of new boundary ring vertices. */
      EdgeHalf *e = find_edge_half_for_edge(bv, e_idx);
      EdgeHalf *eprev = find_edge_half_for_edge(bv, e_prev_idx);
      BLI_assert(e && eprev);

      /* The original beveled edge used as the example for arc segments,
       * matching BMesh's `bme = e->e` and `ee.append(bme)`. */
      const int bme = e->e;

      /* Determine CCW vs CW traversal (matching BMesh go_ccw logic). */
      bool go_ccw;
      if (e->prev == eprev) {
        if (eprev->prev == e) {
          /* Valence-2 vertex: break tie using face membership. */
          go_ccw = (e->fnext != f_idx);
        }
        else {
          go_ccw = true;
        }
      }
      else if (eprev->prev == e) {
        go_ccw = false;
      }
      else {
        /* Non-contiguous ordering: pick the shorter arc. */
        go_ccw = count_ccw_edges_between(eprev, e) < count_ccw_edges_between(e, eprev);
      }

      VMesh *vm = bv->vmesh.get();
      BoundVert *vstart;
      BoundVert *vend;
      if (go_ccw) {
        vstart = eprev->rightv;
        vend = e->leftv;
        /* profile_index > 0 handling is a TODO (miters not yet implemented). */
      }
      else {
        vstart = eprev->leftv;
        vend = e->rightv;
      }
      BLI_assert(vstart && vend);

      /* Emit the starting corner vertex. */
      vv.append(geom::mesh_vert(vm, vstart->index, 0, 0)->v);
      orig_v.append(bv->v);
      orig_e.append(bme);

      BoundVert *v = vstart;
      while (v != vend) {
        if (go_ccw) {
          const int i = v->index;
          for (int k = 1; k <= vm->seg; k++) {
            const int nv = geom::mesh_vert(vm, i, 0, k)->v;
            if (nv >= 0) {
              vv.append(nv);
              orig_v.append(bv->v);
              orig_e.append(bme);
            }
          }
          v = v->next;
        }
        else {
          const int i = v->prev->index;
          for (int k = vm->seg - 1; k >= 0; k--) {
            const int nv = geom::mesh_vert(vm, i, 0, k)->v;
            if (nv >= 0) {
              vv.append(nv);
              orig_v.append(bv->v);
              orig_e.append(bme);
            }
          }
          v = v->prev;
        }
      }
      do_rebuild = true;
    }
    else {
      /* Non-beveled vertex: keep as-is. */
      vv.append(v_idx);
      orig_v.append(v_idx);
      orig_e.append(e_idx);
    }
  }

  if (do_rebuild && vv.size() >= 3) {
    /* Pre-create edges with example original edges so that face_create's internal
     * edge_create calls inherit the correct example via the dedup lookup.
     * Only needed for cross-vertex edges (ov_a != ov_b). */
    const int n = int(vv.size());
    for (int i = 0; i < n; i++) {
      const int ov_a = orig_v[i];
      const int ov_b = orig_v[(i + 1) % n];
      if (ov_a != ov_b) {
        const int example_edge = emesh.find_edge(ov_a, ov_b);
        if (example_edge >= 0 && example_edge < emesh.mesh.edges_num) {
          state.emesh.edge_create(vv[i], vv[(i + 1) % n], example_edge);
        }
      }
    }

    const int new_face_idx = state.emesh.face_create(vv.as_span(), f_idx);

    /* Post-hoc: set example edges for ALL rebuilt edges, mirroring BMesh's
     * `BM_elem_attrs_copy(bm, ee[k], bme_new)` in #bev_rebuild_polygon.
     * This is necessary because arc edges within a beveled vertex's boundary ring
     * may already have been created (during vertex mesh construction) with example=-1.
     * The edge_set_example call overwrites the example, propagating attributes like
     * bevel_weight_edge and uv_seam from the original beveled edge. */
    for (int i = 0; i < n; i++) {
      const int ov_a = orig_v[i];
      const int ov_b = orig_v[(i + 1) % n];
      int example_edge;
      if (ov_a != ov_b) {
        example_edge = emesh.find_edge(ov_a, ov_b);
        if (example_edge < 0 || example_edge >= emesh.mesh.edges_num) {
          example_edge = -1;
        }
      }
      else {
        /* Arc edge within the same beveled vertex: use orig_e (the beveled edge). */
        example_edge = orig_e[i];
      }
      if (example_edge >= 0) {
        const int new_edge = state.emesh.find_edge(vv[i], vv[(i + 1) % n]);
        if (new_edge >= 0) {
          state.emesh.edge_set_example(new_edge, example_edge);
        }
      }
    }

    /* Corner-segment seam/sharp fixup, matching BMesh's #bev_rebuild_polygon (lines 7316-7332).
     * When consecutive edges in the rebuilt face share the same original edge (i.e. they are
     * adjacent arc segments within the same beveled vertex), undo seam/sharp if it is not
     * contiguous with the previous original edge in the face ring.
     *
     * Specifically: if orig_e[k] == orig_e[k+1] (corner segment), and orig_e[k] has seam=True
     * but the previous *different* original edge (bme_prev) does NOT, then clear seam on the
     * new edge.  This prevents arc edges from spuriously inheriting seam from the beveled edge
     * when the seam is not supposed to continue around that side of the vertex. */
    {
      const bke::AttributeAccessor src_attrs_check = emesh.mesh.attributes();
      const bke::AttributeReader<bool> seam_reader = src_attrs_check.lookup<bool>(
          "uv_seam", bke::AttrDomain::Edge);
      const bke::AttributeReader<bool> sharp_reader = src_attrs_check.lookup<bool>(
          "sharp_edge", bke::AttrDomain::Edge);

      auto orig_has_seam = [&](int e_idx) -> bool {
        if (!seam_reader || e_idx < 0 || e_idx >= int(emesh.mesh.edges_num)) {
          return false;
        }
        return bool(seam_reader.varray[e_idx]);
      };
      auto orig_has_sharp = [&](int e_idx) -> bool {
        if (!sharp_reader || e_idx < 0 || e_idx >= int(emesh.mesh.edges_num)) {
          return false;
        }
        return bool(sharp_reader.varray[e_idx]);
      };

      int bme_prev_idx = orig_e[(n - 1) % n];
      for (int k = 0; k < n; k++) {
        const int oe_k = orig_e[k];
        const int new_edge = state.emesh.find_edge(vv[k], vv[(k + 1) % n]);
        if (new_edge < 0 || oe_k < 0) {
          bme_prev_idx = oe_k;
          continue;
        }

        if (k < n - 1 && oe_k == orig_e[k + 1]) {
          /* Corner segment: oe_k == oe_{k+1}. */
          if (orig_has_seam(oe_k) && !orig_has_seam(bme_prev_idx)) {
            state.emesh.edge_set_seam_override(new_edge, 0);
          }
          /* For sharp: reverse test (smooth flag is inverted relative to sharp). */
          if (orig_has_sharp(oe_k) && !orig_has_sharp(bme_prev_idx)) {
            state.emesh.edge_set_sharp_override(new_edge, 0);
          }
        }
        else {
          bme_prev_idx = oe_k;
        }
      }
    }

    return new_face_idx;
  }
  return -1;
}

/**
 * For each original face that touches at least one beveled vertex, rebuild it.
 * Mirrors #bevel_rebuild_existing_polygons from the BMesh system.
 */
static void bevel_rebuild_existing_polygons(BevelState &state,
                                            Vector<int> &r_rebuilt_orig_faces,
                                            int &r_rebuilt_face_0)
{
  const ExtendableMesh &emesh = state.emesh;
  const int orig_faces_num = emesh.mesh.faces_num;

  /* Track which original faces have already been rebuilt to avoid processing them twice
   * when multiple beveled vertices share a face. */
  Array<bool> rebuilt(orig_faces_num, false);
  r_rebuilt_face_0 = -1;

  state.bevel_affected_vertices.foreach_index([&](const int v) {
    for (const int c : emesh.src_vert_to_corner[v]) {
      const int f_idx = emesh.src_corner_to_face[c];
      if (f_idx < orig_faces_num && !rebuilt[f_idx]) {
        const int new_f = bev_rebuild_polygon(state, f_idx);
        if (new_f >= 0) {
          rebuilt[f_idx] = true;
          r_rebuilt_orig_faces.append(f_idx);
          if (f_idx == 0) {
            r_rebuilt_face_0 = new_f;
          }
        }
      }
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edge polygon construction
 * \{ */

/**
 * Returns the #EdgeHalf in `bv` whose beveled edge index equals `edge_index`,
 * or `nullptr` if not found.
 */
static EdgeHalf *find_edge_half_for_edge(BevVert *bv, const int edge_index)
{
  for (int i = 0; i < bv->edgecount; i++) {
    if (bv->edges[i].e == edge_index) {
      return &bv->edges[i];
    }
  }
  return nullptr;
}

/**
 * Returns true when `bv` is a "weld cross": 4 edges, 2 beveled, opposite each other.
 * Matches BMesh's #bevvert_is_weld_cross.
 */
static bool bevvert_is_weld_cross(const BevVert *bv)
{
  return (bv->edgecount == 4 && bv->selcount == 2 &&
          ((bv->edges[0].is_bev && bv->edges[2].is_bev) ||
           (bv->edges[1].is_bev && bv->edges[3].is_bev)));
}

/**
 * Builds the `nseg` quad strips of F_EDGE faces along a single beveled edge `edge_index`.
 * Mirrors BMesh's #bevel_build_edge_polygons.
 *
 * Diagram (bme = the original beveled edge):
 * \code
 *      bme->v1
 *     / | \
 *   v1--|--v4
 *   |   |   |
 *   v2--|--v3
 *     \ | /
 *      bme->v2
 * \endcode
 * `v1`/`v4` are the leftv/rightv BoundVerts on bv1 (the e1-\>leftv side),
 * `v2`/`v3` are the rightv/leftv BoundVerts on bv2.
 */
static void bevel_build_edge_polygons(BevelState &state, const int edge_index)
{
  const ExtendableMesh &emesh = state.emesh;
  const int2 edge_verts = emesh.edge_verts(edge_index);
  const int v1_idx = edge_verts[0];
  const int v2_idx = edge_verts[1];

  /* Skip non-manifold edges (those with != 2 adjacent faces). */
  const GroupedSpan<int> edge_faces = emesh.src_edge_to_face;
  if (edge_index >= emesh.mesh.edges_num || edge_faces[edge_index].size() != 2) {
    return;
  }

  BevVert *bv1 = state.vert_hash.lookup_default(v1_idx, nullptr);
  BevVert *bv2 = state.vert_hash.lookup_default(v2_idx, nullptr);
  if (!bv1 || !bv2) {
    return;
  }

  EdgeHalf *e1 = find_edge_half_for_edge(bv1, edge_index);
  EdgeHalf *e2 = find_edge_half_for_edge(bv2, edge_index);
  if (!e1 || !e2) {
    return;
  }

  const int nseg = e1->seg;
  BLI_assert(nseg > 0 && nseg == e2->seg);

  /* Corner boundary-ring vertex indices at both endpoints. */
  const int i1 = e1->leftv->index;
  const int i2 = e2->leftv->index;
  VMesh *vm1 = bv1->vmesh.get();
  VMesh *vm2 = bv2->vmesh.get();

  /* The two adjacent original faces for this edge.
   * Strips on the left half (k <= mid) use f1; strips on the right half use f2. */
  const int f1 = e1->fprev; /* -1 on a boundary edge. */
  const int f2 = e1->fnext; /* -1 on a boundary edge. */

  const int odd = nseg % 2;
  const int mid = nseg / 2;

  /* For odd nseg with a seam at e1, choose the winner once via #choose_rep_face.
   * Also compute center_adj_k: the strip index adjacent to the center strip,
   * on the other UV-island side.  This mirrors BMesh exactly. */
  int f_choice = -1;
  int center_adj_k = -1;
  if (odd && e1->is_seam) {
    const int candidates[2] = {f1, f2};
    f_choice = choose_rep_face(state, Span<int>(candidates, 2));
    if (nseg > 1) {
      center_adj_k = (f_choice == f1) ? mid + 2 : mid;
    }
  }

  /* Starting vertices at k=0 on bv1's end and k=nseg on bv2's end.
   * bv2's ring is traversed in reverse (nseg→0), matching BMesh's use of e2->rightv as the
   * starting corner (e2->rightv corresponds to mesh_vert(vm2, i2, 0, nseg)).
   * Save the first-strip boundary verts (v1/v2 in the BMesh diagram) so that after the loop we
   * can look up the first outer long edge and set its example. */
  const int v_bme1 = geom::mesh_vert(vm1, i1, 0, 0)->v;    /* v1 in BMesh diagram. */
  const int v_bme2 = geom::mesh_vert(vm2, i2, 0, nseg)->v; /* v2 in BMesh diagram. */
  int v_prev_1 = v_bme1;
  int v_prev_2 = v_bme2;

  for (int k = 1; k <= nseg; k++) {
    /* Next boundary verts along the ring: vm1 goes forward (k), vm2 goes backward (nseg-k). */
    const int v_next_1 = geom::mesh_vert(vm1, i1, 0, k)->v;        /* v4 in BMesh diagram. */
    const int v_next_2 = geom::mesh_vert(vm2, i2, 0, nseg - k)->v; /* v3 in BMesh diagram. */

    /* Choose face rep and per-corner face reps / snap edges, mirroring BMesh's
     * #bevel_build_edge_polygons exactly.
     *
     * The quad winds:  [0]=v_prev_1, [1]=v_prev_2, [2]=v_next_2, [3]=v_next_1. */
    int face_rep;
    int corner_reps[4] = {-1, -1, -1, -1};
    int corner_snaps[4] = {-1, -1, -1, -1};

    if (odd && k == mid + 1) {
      /* Center strip of an odd-segment bevel. */
      if (e1->is_seam) {
        /* Straddles a seam: interpolate in f_choice and snap the verts on
         * the non-chosen side to bme for interpolation purposes. */
        face_rep = f_choice;
        if (f_choice == f1) {
          corner_snaps[2] = corner_snaps[3] = edge_index;
        }
        else {
          corner_snaps[0] = corner_snaps[1] = edge_index;
        }
        corner_reps[0] = corner_reps[1] = corner_reps[2] = corner_reps[3] = f_choice;
      }
      else {
        /* Straddles but not a seam: interpolate left half in f1, right half in f2.
         * f_choice is -1 here; face_rep uses f1 but per-corner reps override it. */
        face_rep = f1;
        corner_reps[0] = corner_reps[1] = f1;
        corner_reps[2] = corner_reps[3] = f2;
      }
    }
    else if (odd && k == center_adj_k && e1->is_seam) {
      /* The strip adjacent to the center one, in another UV island.
       * Snap the edge near the seam to bme to match what happens in the bevel rings. */
      if (k == mid) {
        face_rep = f1;
        corner_snaps[2] = corner_snaps[3] = edge_index;
      }
      else {
        face_rep = f2;
        corner_snaps[0] = corner_snaps[1] = edge_index;
      }
    }
    else if (!odd && k == mid) {
      /* Left poly that touches an even center line on right. */
      face_rep = f1;
      corner_snaps[2] = corner_snaps[3] = edge_index;
    }
    else if (!odd && k == mid + 1) {
      /* Right poly that touches an even center line on left. */
      face_rep = f2;
      corner_snaps[0] = corner_snaps[1] = edge_index;
    }
    else {
      /* Doesn't cross or touch the center line, so interpolate in appropriate f1 or f2. */
      face_rep = (k <= mid) ? f1 : f2;
    }

    /* The quad winds as: v_prev_1 -> v_prev_2 -> v_next_2 -> v_next_1. */
    const int quad[4] = {v_prev_1, v_prev_2, v_next_2, v_next_1};
    const int new_face = state.emesh.face_create(Span<int>(quad, 4), face_rep);
    state.emesh.tag_last_face(NewFaceKind::EdgeFace);

    /* Tag the mid-ring edge: the shared boundary between quads k and k+1 at k == mid,
     * i.e. the ring at index mid in the strip.  Only applies when nseg >= 2.
     * For even nseg the mid boundary is between k==mid and k==mid+1 quads.
     * For odd nseg the center strip straddles the mid, so tag at k == mid (the boundary
     * between mid and the center quad). */
    if (nseg >= 2 && k == mid) {
      const int mid_edge = state.emesh.find_edge(v_next_1, v_next_2);
      if (mid_edge >= 0) {
        state.emesh.tag_edge_kind(mid_edge, NewEdgeKind::MidEdge);
      }
    }

    /* Set per-corner face reps and snap edges when any are non-default. */
    const bool any_corner_reps = corner_reps[0] >= 0 || corner_reps[1] >= 0 ||
                                 corner_reps[2] >= 0 || corner_reps[3] >= 0;
    const bool any_corner_snaps = corner_snaps[0] >= 0 || corner_snaps[1] >= 0 ||
                                  corner_snaps[2] >= 0 || corner_snaps[3] >= 0;
    if (any_corner_reps || any_corner_snaps) {
      state.emesh.face_set_corner_reps(new_face,
                                       any_corner_reps ? Span<int>(corner_reps, 4) : Span<int>{},
                                       any_corner_snaps ? Span<int>(corner_snaps, 4) :
                                                          Span<int>{});
    }
#ifdef BEVEL_DEBUG
    {
      fmt::println(
          "bevel_build_edge_polygons: edge={} k={}/{} f1={} f2={} is_seam={} face_rep={} "
          "verts=[{},{},{},{}] creps=[{},{},{},{}] csnaps=[{},{},{},{}]",
          edge_index,
          k,
          nseg,
          f1,
          f2,
          e1->is_seam,
          face_rep,
          quad[0],
          quad[1],
          quad[2],
          quad[3],
          corner_reps[0],
          corner_reps[1],
          corner_reps[2],
          corner_reps[3],
          corner_snaps[0],
          corner_snaps[1],
          corner_snaps[2],
          corner_snaps[3]);
    }
#endif

    v_prev_1 = v_next_1;
    v_prev_2 = v_next_2;
  }

  /* Copy edge attributes to the first and last "long" edges of the strip (those that run
   * parallel to the original beveled edge), mirroring BMesh's post-loop
   * `BM_elem_attrs_copy(bm, bme, bme1)` / `BM_elem_attrs_copy(bm, bme, bme2)`.
   * After the loop: v_prev_1 = v4, v_prev_2 = v3 (BMesh diagram).
   * The first outer edge is between v1 (v_bme1) and v2 (v_bme2).
   * The last outer edge is between v3 (v_prev_2) and v4 (v_prev_1). */
  const int outer_edge1 = state.emesh.find_edge(v_bme1, v_bme2);
  const int outer_edge2 = state.emesh.find_edge(v_prev_2, v_prev_1);
  if (outer_edge1 >= 0) {
    state.emesh.edge_set_example(outer_edge1, edge_index);
    state.emesh.tag_edge_kind(outer_edge1, NewEdgeKind::OuterEdge);
    state.outer_edge_src_indices.append(edge_index);
  }
  if (outer_edge2 >= 0) {
    state.emesh.edge_set_example(outer_edge2, edge_index);
    state.emesh.tag_edge_kind(outer_edge2, NewEdgeKind::OuterEdge);
    if (outer_edge2 != outer_edge1) {
      /* Only record once if both outer edges are the same (nseg == 1 degenerate case). */
      state.outer_edge_src_indices.append(edge_index);
    }
  }

  /* If either end is a "weld cross", want continuity of edge attributes across
   * the boundary arc edges.  Mirrors BMesh's #weld_cross_attrs_copy:
   * For a weld-cross vertex (4 edges, 2 beveled, the beveled pair opposite each other),
   * find the adjacent non-beveled edges (bme_prev, bme_next).  If their seam status
   * disagrees, clear seam on all arc edges.  Then set the example for each arc edge
   * to bme_prev so that other attributes (bevel weight, sharp, etc.) propagate. */
  auto weld_cross_attrs_copy = [&](BevVert *bv, VMesh *vm, int vmindex, EdgeHalf *e) {
    int e_prev = -1;
    int e_next = -1;
    for (int i = 0; i < 4; i++) {
      if (&bv->edges[i] == e) {
        e_prev = bv->edges[(i + 3) % 4].e;
        e_next = bv->edges[(i + 1) % 4].e;
        break;
      }
    }
    BLI_assert(e_prev >= 0 && e_next >= 0);

    /* Want seams to cross only if that way on both sides.
     * Look up the `uv_seam` edge attribute via the attribute API. */
    const bke::AttributeAccessor attrs = emesh.mesh.attributes();
    const bke::AttributeReader<bool> seam_reader = attrs.lookup<bool>("uv_seam",
                                                                      bke::AttrDomain::Edge);
    const bool prev_seam = seam_reader ? bool(seam_reader.varray[e_prev]) : false;
    const bool next_seam = seam_reader ? bool(seam_reader.varray[e_next]) : false;
    const bool disable_seam = (prev_seam != next_seam);

    for (int i = 0; i < nseg; i++) {
      const int va = geom::mesh_vert(vm, vmindex, 0, i)->v;
      const int vb = geom::mesh_vert(vm, vmindex, 0, i + 1)->v;
      const int arc_edge = state.emesh.find_edge(va, vb);
      if (arc_edge >= 0) {
        state.emesh.edge_set_example(arc_edge, e_prev);
        /* TODO: if disable_seam is true, the seam attribute on the arc edge should be
         * cleared after attribute propagation.  This requires post-processing support
         * not yet available.  Similarly for enable_smooth (sharp disagree). */
        (void)disable_seam;
      }
    }
  };

  if (bevvert_is_weld_cross(bv1)) {
    weld_cross_attrs_copy(bv1, vm1, i1, e1);
  }
  if (bevvert_is_weld_cross(bv2)) {
    weld_cross_attrs_copy(bv2, vm2, i2, e2);
  }
}

/** \} */

static BoundVert *pipe_test(const BevelState &state, BevVert *bv);
static VMesh pipe_adj_vmesh(BevelState &state, BevVert *bv, BoundVert *vpipe);
static VMesh square_out_adj_vmesh(BevelState &state, BevVert *bv);

/**
 * Main vmesh builder for a single beveled vertex.
 * Allocates the #NewVert grid, creates boundary vertices in #ExtendableMesh,
 * computes profile coordinates, then dispatches to the appropriate per-kind builder.
 */
static void build_vmesh(BevelState &state, BevVert *bv)
{
  VMesh *vm = bv->vmesh.get();
  const int n = vm->count;
  const int ns = vm->seg;
  const int ns2 = ns / 2;

  /* Allocate the grid. */
  vm->mesh = Array<NewVert>(n * (ns2 + 1) * (ns + 1), NewVert{-1, float3(0.0f)});

  /* Detect the weld case: exactly two beveled edges welding together. */
  const bool weld = (bv->selcount == 2) && (vm->count == 2);
  BoundVert *weld1 = nullptr;
  BoundVert *weld2 = nullptr;

  /* Create mesh vertices for each BoundVert's (i, 0, 0) position. */
  BoundVert *bndv = vm->boundstart;
  do {
    const int i = bndv->index;
    geom::mesh_vert(vm, i, 0, 0)->co = bndv->nv.co;
    geom::mesh_vert(vm, i, 0, 0)->v = state.emesh.vert_create(float3(bndv->nv.co), bv->v);
    bndv->nv.v = geom::mesh_vert(vm, i, 0, 0)->v;

    if (weld && bndv->ebev) {
      if (!weld1) {
        weld1 = bndv;
      }
      else {
        weld2 = bndv;
      }
    }
  } while ((bndv = bndv->next) != vm->boundstart);

  /* For the weld case, set profile parameters and move the profile planes before
   * calculate_vm_profiles runs (which skips BoundVerts with special_params == true).
   * Mirrors BMesh's build_vmesh (BMesh lines 6484-6486). */
  if (weld && weld1 && weld2) {
    profile::set_profile_params(state, bv, weld1);
    profile::set_profile_params(state, bv, weld2);
    profile::move_weld_profile_planes(weld1, weld2, state.emesh.src_positions[bv->v]);
  }

  /* Calculate profiles for non-ADJ kinds; ADJ computes its own via adj_vmesh. */
  profile::calculate_vm_profiles(state, bv, vm);

  /* Fill boundary arc verts (j=0, k=1..ns-1) for non-ADJ. */
  bndv = vm->boundstart;
  do {
    const int i = bndv->index;
    /* Last arc vert shares with the next BoundVert's first. */
    geom::mesh_vert(vm, i, 0, ns)->co = bndv->next->nv.co;
    geom::mesh_vert(vm, i, 0, ns)->v = bndv->next->nv.v;

    if (vm->mesh_kind != MeshKind::Adj) {
      for (int k = 1; k < ns; k++) {
        if (bndv->ebev) {
          const float3 co = profile::get_profile_point(state, &bndv->profile, k, ns);
          geom::mesh_vert(vm, i, 0, k)->co = co;
          if (!weld) {
            geom::mesh_vert(vm, i, 0, k)->v = state.emesh.vert_create(co, bv->v);
          }
        }
        else if (n == 2 && !bndv->ebev) {
          /* Non-beveled side of a weld: mirror from the other BoundVert. */
          geom::mesh_vert(vm, i, 0, k)->co = geom::mesh_vert(vm, 1 - i, 0, ns - k)->co;
          geom::mesh_vert(vm, i, 0, k)->v = geom::mesh_vert(vm, 1 - i, 0, ns - k)->v;
        }
      }
    }
  } while ((bndv = bndv->next) != vm->boundstart);

  /* Weld case: build a blended profile between the two weld BoundVerts. */
  if (weld) {
    vm->mesh_kind = MeshKind::None;
    for (int k = 1; k < ns; k++) {
      const float3 &v_w1 = geom::mesh_vert(vm, weld1->index, 0, k)->co;
      const float3 &v_w2 = geom::mesh_vert(vm, weld2->index, 0, ns - k)->co;
      float3 co;
      /* TODO: handle BEVEL_PROFILE_CUSTOM weld blending. */
      if (weld1->profile.super_r == profile::PRO_LINE_R &&
          weld2->profile.super_r != profile::PRO_LINE_R)
      {
        co = v_w2;
      }
      else if (weld2->profile.super_r == profile::PRO_LINE_R &&
               weld1->profile.super_r != profile::PRO_LINE_R)
      {
        co = v_w1;
      }
      else {
        co = (v_w1 + v_w2) * 0.5f;
      }
      const int nv = state.emesh.vert_create(co, bv->v);
      geom::mesh_vert(vm, weld1->index, 0, k)->co = co;
      geom::mesh_vert(vm, weld1->index, 0, k)->v = nv;
    }
    for (int k = 1; k < ns; k++) {
      geom::mesh_vert(vm, weld2->index, 0, ns - k)->co =
          geom::mesh_vert(vm, weld1->index, 0, k)->co;
      geom::mesh_vert(vm, weld2->index, 0, ns - k)->v = geom::mesh_vert(vm, weld1->index, 0, k)->v;
    }
  }

  switch (vm->mesh_kind) {
    case MeshKind::None:
      if (n == 2 && state.params.affect_type == BevelAffect::Vertices) {
        bevel_vert_two_edges(state, bv);
      }
      break;
    case MeshKind::Poly:
      bevel_build_poly(state, bv);
      break;
    case MeshKind::TriFan:
      bevel_build_trifan(state, bv);
      break;
    case MeshKind::Adj: {
      /* Compute the ADJ interior coordinates via cubic subdivision. */
      VMesh vm_adj;
      BoundVert *vpipe = pipe_test(state, bv);
      if (state.pro_super_r == profile::PRO_SQUARE_R && bv->selcount >= 3 && (ns % 2 == 0) &&
          state.params.custom_profile_samples.is_empty())
      {
        vm_adj = square_out_adj_vmesh(state, bv);
      }
      else if (vpipe) {
        vm_adj = pipe_adj_vmesh(state, bv, vpipe);
      }
      else {
        vm_adj = adj_vmesh(state, bv);
      }
      /* Copy final positions into vm->mesh and create ExtendableMesh verts. */
      for (int i = 0; i < n; i++) {
        for (int j = 0; j <= ns2; j++) {
          for (int k = 0; k <= ns; k++) {
            if (j == 0 && (k == 0 || k == ns)) {
              continue; /* Boundary corners already created. */
            }
            if (!geom::is_canon(vm, i, j, k)) {
              continue;
            }
            const float3 co = geom::mesh_vert(&vm_adj, i, j, k)->co;
            const int nv = state.emesh.vert_create(co, bv->v);
            geom::mesh_vert(vm, i, j, k)->co = co;
            geom::mesh_vert(vm, i, j, k)->v = nv;
          }
        }
      }
      geom::vmesh_copy_equiv_verts(vm);
      bevel_build_rings(state, bv);
      break;
    }
    case MeshKind::Cutoff:
      /* TODO: implement M_CUTOFF. */
      break;
  }
}

/** \} */

/**
 * For each UV map, group the corners of vertex `v` into buckets of corners whose UV
 * coordinates coincide (within #STD_UV_CONNECT_LIMIT). Store the result in
 * `state.uv_vert_maps[i][v]`, one entry per UV layer.
 *
 * This is the Mesh equivalent of the BMesh #determine_uv_vert_connectivity function.
 * Corners play the role of BMesh loops; #uv::UVMapInfo::layers supplies the UV values.
 */
static void determine_uv_vert_connectivity(BevelState &state, const int v)
{
  const int num_uv_layers = int(state.uv_layer_info.uv_maps.size());
  BLI_assert(int(state.uv_vert_maps.size()) == num_uv_layers);

  for (int i = 0; i < num_uv_layers; i++) {
    const Span<float2> uv_vals = state.uv_layer_info.uv_maps[i].values;
    Vector<UVVertBucket> uv_vert_buckets;

    for (const int c : state.emesh.src_vert_to_corner[v]) {
      const float2 &luv = uv_vals[c];
      bool is_overlap_found = false;
      for (UVVertBucket &bucket : uv_vert_buckets) {
        for (const int c2 : bucket) {
          if (compare_v2v2(luv, uv_vals[c2], STD_UV_CONNECT_LIMIT)) {
            bucket.add(c);
            is_overlap_found = true;
            break;
          }
        }
        if (is_overlap_found) {
          break;
        }
      }
      if (!is_overlap_found) {
        uv_vert_buckets.append(UVVertBucket{c});
      }
    }

    BLI_assert(state.uv_vert_maps[i].contains(v) == false);
    state.uv_vert_maps[i].add_new(v, uv_vert_buckets);
  }
}

/* -------------------------------------------------------------------- */
/** \name ADJ vmesh subdivision helpers
 * \{ */

/* Allocates a VMesh with a zeroed NewVert grid of size count*(seg/2+1)*(seg+1). */
static VMesh new_adj_vmesh(int count, int seg, BoundVert *bounds)
{
  VMesh vm;
  vm.count = count;
  vm.seg = seg;
  vm.boundstart = bounds;
  vm.mesh = Array<NewVert>(count * (seg / 2 + 1) * (seg + 1), NewVert{-1, float3(0.0f)});
  vm.mesh_kind = MeshKind::Adj;
  return vm;
}

/* Fills frac[0..ns] with cumulative arc-length fractions along ring 0 of vmesh row i. */
static void fill_vmesh_fracs(VMesh *vm, MutableSpan<float> frac, int i)
{
  const int ns = vm->seg;
  frac[0] = 0.0f;
  float total = 0.0f;
  for (int k = 0; k < ns; k++) {
    total += math::distance(geom::mesh_vert(vm, i, 0, k)->co,
                            geom::mesh_vert(vm, i, 0, k + 1)->co);
    frac[k + 1] = total;
  }
  if (total > 0.0f) {
    for (int k = 1; k <= ns; k++) {
      frac[k] /= total;
    }
  }
  else {
    frac[ns] = 1.0f;
  }
}

/* Fills frac[0..ns] with cumulative arc-length fractions along bndv's profile. */
static void fill_profile_fracs(const BevelState &state,
                               const BoundVert *bndv,
                               MutableSpan<float> frac,
                               const int ns)
{
  frac[0] = 0.0f;
  float total = 0.0f;
  float3 co = bndv->nv.co;
  for (int k = 0; k < ns; k++) {
    const float3 nextco = profile::get_profile_point(state, &bndv->profile, k + 1, ns);
    total += math::distance(co, nextco);
    frac[k + 1] = total;
    co = nextco;
  }
  if (total > 0.0f) {
    for (int k = 1; k <= ns; k++) {
      frac[k] /= total;
    }
  }
  else {
    frac[ns] = 1.0f;
  }
}

/* Returns index i such that frac[i] <= f <= frac[i+1], and sets r_rest to the remainder. */
static int interp_range(const Span<float> frac, const int n, const float f, float *r_rest)
{
  for (int i = 0; i < n; i++) {
    if (f <= frac[i + 1]) {
      float rest = f - frac[i];
      *r_rest = (rest == 0.0f) ? 0.0f : rest / (frac[i + 1] - frac[i]);
      if (i == n - 1 && *r_rest == 1.0f) {
        i = n;
        *r_rest = 0.0f;
      }
      return i;
    }
  }
  *r_rest = 0.0f;
  return n;
}

/* Re-samples vm_in to produce a VMesh with nseg boundary segments. */
static VMesh interp_vmesh(const BevelState &state, VMesh &vm_in, int nseg)
{
  const int n_bndv = vm_in.count;
  const int ns_in = vm_in.seg;
  const int nseg2 = nseg / 2;
  const int odd = nseg % 2;
  VMesh vm_out = new_adj_vmesh(n_bndv, nseg, vm_in.boundstart);

  Array<float, 8> prev_frac(ns_in + 1);
  Array<float, 8> frac(ns_in + 1);
  Array<float, 8> new_frac(nseg + 1);
  Array<float, 8> prev_new_frac(nseg + 1);

  fill_vmesh_fracs(&vm_in, prev_frac, n_bndv - 1);
  BoundVert *bndv = vm_in.boundstart;
  fill_profile_fracs(state, bndv->prev, prev_new_frac, nseg);

  for (int i = 0; i < n_bndv; i++) {
    fill_vmesh_fracs(&vm_in, frac, i);
    fill_profile_fracs(state, bndv, new_frac, nseg);
    for (int j = 0; j <= nseg2 - 1 + odd; j++) {
      for (int k = 0; k <= nseg2; k++) {
        float restk, restkprev;
        int k_in = interp_range(frac, ns_in, new_frac[k], &restk);
        int k_in_prev = interp_range(prev_frac, ns_in, prev_new_frac[nseg - j], &restkprev);
        int j_in = ns_in - k_in_prev;
        float restj = -restkprev;
        if (restj > -geom::BEVEL_EPSILON_D) {
          restj = 0.0f;
        }
        else {
          j_in--;
          restj = 1.0f + restj;
        }
        float3 co;
        if (restj < geom::BEVEL_EPSILON_D && restk < geom::BEVEL_EPSILON_D) {
          co = geom::mesh_vert_canon(&vm_in, i, j_in, k_in)->co;
        }
        else {
          const int j0inc = (restj < geom::BEVEL_EPSILON_D || j_in == ns_in) ? 0 : 1;
          const int k0inc = (restk < geom::BEVEL_EPSILON_D || k_in == ns_in) ? 0 : 1;
          float3 quad[4];
          quad[0] = geom::mesh_vert_canon(&vm_in, i, j_in, k_in)->co;
          quad[1] = geom::mesh_vert_canon(&vm_in, i, j_in, k_in + k0inc)->co;
          quad[2] = geom::mesh_vert_canon(&vm_in, i, j_in + j0inc, k_in + k0inc)->co;
          quad[3] = geom::mesh_vert_canon(&vm_in, i, j_in + j0inc, k_in)->co;
          interp_bilinear_quad_v3(reinterpret_cast<float (*)[3]>(quad), restk, restj, co);
        }
        geom::mesh_vert(&vm_out, i, j, k)->co = co;
      }
    }
    bndv = bndv->next;
    prev_frac = frac;
    prev_new_frac = new_frac;
  }
  if (!odd) {
    geom::mesh_vert(&vm_out, 0, nseg2, nseg2)->co = geom::vmesh_center(&vm_in);
  }
  geom::vmesh_copy_equiv_verts(&vm_out);
  return vm_out;
}

/**
 * One step of Catmull-Clark-like cubic subdivision (Levin 1999).
 * `vm_in.seg` must be even and >= 2. Returns a new VMesh with doubled resolution.
 */
static VMesh cubic_subdiv(const BevelState &state, VMesh &vm_in)
{
  const int n_boundary = vm_in.count;
  const int ns_in = vm_in.seg;
  const int ns_in2 = ns_in / 2;
  BLI_assert(ns_in % 2 == 0);
  const int ns_out = 2 * ns_in;
  VMesh vm_out = new_adj_vmesh(n_boundary, ns_out, vm_in.boundstart);

  /* Adjust even boundary vertices. */
  for (int i = 0; i < n_boundary; i++) {
    geom::mesh_vert(&vm_out, i, 0, 0)->co = geom::mesh_vert(&vm_in, i, 0, 0)->co;
    for (int k = 1; k < ns_in; k++) {
      float3 co = geom::mesh_vert(&vm_in, i, 0, k)->co;
      /* Smooth boundary (not for custom profile). */
      if (state.params.custom_profile_samples.is_empty()) {
        float3 acc = geom::mesh_vert(&vm_in, i, 0, k - 1)->co +
                     geom::mesh_vert(&vm_in, i, 0, k + 1)->co;
        acc += co * -2.0f;
        co += acc * (-1.0f / 6.0f);
      }
      geom::mesh_vert_canon(&vm_out, i, 0, 2 * k)->co = co;
    }
  }

  /* Adjust odd boundary vertices from profile. */
  BoundVert *bndv = vm_out.boundstart;
  for (int i = 0; i < n_boundary; i++) {
    for (int k = 1; k < ns_out; k += 2) {
      float3 co = profile::get_profile_point(state, &bndv->profile, k, ns_out);
      if (state.params.custom_profile_samples.is_empty()) {
        float3 acc = geom::mesh_vert_canon(&vm_out, i, 0, k - 1)->co +
                     geom::mesh_vert_canon(&vm_out, i, 0, k + 1)->co;
        acc += co * -2.0f;
        co += acc * (-1.0f / 6.0f);
      }
      geom::mesh_vert_canon(&vm_out, i, 0, k)->co = co;
    }
    bndv = bndv->next;
  }
  geom::vmesh_copy_equiv_verts(&vm_out);

  /* Copy adjusted boundary back into vm_in. */
  for (int i = 0; i < n_boundary; i++) {
    for (int k = 0; k < ns_in; k++) {
      geom::mesh_vert(&vm_in, i, 0, k)->co = geom::mesh_vert(&vm_out, i, 0, 2 * k)->co;
    }
  }
  geom::vmesh_copy_equiv_verts(&vm_in);

  /* New face vertices. */
  for (int i = 0; i < n_boundary; i++) {
    for (int j = 0; j < ns_in2; j++) {
      for (int k = 0; k < ns_in2; k++) {
        geom::mesh_vert(&vm_out, i, 2 * j + 1, 2 * k + 1)->co = geom::avg4(
            geom::mesh_vert(&vm_in, i, j, k),
            geom::mesh_vert(&vm_in, i, j, k + 1),
            geom::mesh_vert(&vm_in, i, j + 1, k),
            geom::mesh_vert(&vm_in, i, j + 1, k + 1));
      }
    }
  }

  /* New vertical edge vertices. */
  for (int i = 0; i < n_boundary; i++) {
    for (int j = 0; j < ns_in2; j++) {
      for (int k = 1; k <= ns_in2; k++) {
        geom::mesh_vert(&vm_out, i, 2 * j + 1, 2 * k)->co = geom::avg4(
            geom::mesh_vert(&vm_in, i, j, k),
            geom::mesh_vert(&vm_in, i, j + 1, k),
            geom::mesh_vert_canon(&vm_out, i, 2 * j + 1, 2 * k - 1),
            geom::mesh_vert_canon(&vm_out, i, 2 * j + 1, 2 * k + 1));
      }
    }
  }

  /* New horizontal edge vertices. */
  for (int i = 0; i < n_boundary; i++) {
    for (int j = 1; j < ns_in2; j++) {
      for (int k = 0; k < ns_in2; k++) {
        geom::mesh_vert(&vm_out, i, 2 * j, 2 * k + 1)->co = geom::avg4(
            geom::mesh_vert(&vm_in, i, j, k),
            geom::mesh_vert(&vm_in, i, j, k + 1),
            geom::mesh_vert_canon(&vm_out, i, 2 * j - 1, 2 * k + 1),
            geom::mesh_vert_canon(&vm_out, i, 2 * j + 1, 2 * k + 1));
      }
    }
  }

  /* New interior vertices (not on boundary). */
  constexpr float gamma_interior = 0.25f;
  constexpr float beta_interior = -gamma_interior;
  for (int i = 0; i < n_boundary; i++) {
    for (int j = 1; j < ns_in2; j++) {
      for (int k = 1; k <= ns_in2; k++) {
        const float3 co1 = geom::avg4(geom::mesh_vert_canon(&vm_out, i, 2 * j, 2 * k - 1),
                                      geom::mesh_vert_canon(&vm_out, i, 2 * j, 2 * k + 1),
                                      geom::mesh_vert_canon(&vm_out, i, 2 * j - 1, 2 * k),
                                      geom::mesh_vert_canon(&vm_out, i, 2 * j + 1, 2 * k));
        const float3 co2 = geom::avg4(geom::mesh_vert_canon(&vm_out, i, 2 * j - 1, 2 * k - 1),
                                      geom::mesh_vert_canon(&vm_out, i, 2 * j + 1, 2 * k - 1),
                                      geom::mesh_vert_canon(&vm_out, i, 2 * j - 1, 2 * k + 1),
                                      geom::mesh_vert_canon(&vm_out, i, 2 * j + 1, 2 * k + 1));
        const float3 co = co1 + co2 * beta_interior +
                          geom::mesh_vert(&vm_in, i, j, k)->co * gamma_interior;
        geom::mesh_vert(&vm_out, i, 2 * j, 2 * k)->co = co;
      }
    }
  }

  geom::vmesh_copy_equiv_verts(&vm_out);

  /* Special center vertex (Sabin modification). */
  const float gamma_c = geom::sabin_gamma(n_boundary);
  const float beta_c = -gamma_c;
  float3 co1(0.0f), co2(0.0f);
  for (int i = 0; i < n_boundary; i++) {
    co1 += geom::mesh_vert(&vm_out, i, ns_in, ns_in - 1)->co;
    co2 += geom::mesh_vert(&vm_out, i, ns_in - 1, ns_in - 1)->co;
    co2 += geom::mesh_vert(&vm_out, i, ns_in - 1, ns_in + 1)->co;
  }
  const float3 co = co1 * (1.0f / float(n_boundary)) +
                    co2 * (beta_c / (2.0f * float(n_boundary))) +
                    geom::mesh_vert(&vm_in, 0, ns_in2, ns_in2)->co * gamma_c;
  for (int i = 0; i < n_boundary; i++) {
    geom::mesh_vert(&vm_out, i, ns_in, ns_in)->co = co;
  }

  /* Restore final profile boundary. */
  bndv = vm_out.boundstart;
  for (int i = 0; i < n_boundary; i++) {
    const int inext = (i + 1) % n_boundary;
    for (int k = 0; k <= ns_out; k++) {
      const float3 pco = profile::get_profile_point(state, &bndv->profile, k, ns_out);
      geom::mesh_vert(&vm_out, i, 0, k)->co = pco;
      if (k >= ns_in && k < ns_out) {
        geom::mesh_vert(&vm_out, inext, ns_out - k, 0)->co = pco;
      }
    }
    bndv = bndv->next;
  }

  return vm_out;
}

/**
 * Snaps `co` to lie on the superellipsoid `x^r + y^r + z^r = 1`.
 * When `r == PRO_CIRCLE_R` normalizes the vector; for the square cases snaps
 * to the nearest axis-aligned face. Only used for cube corner special cases.
 */
static void snap_to_superellipsoid(float3 &co, const float super_r, const bool midline)
{
  const float r = super_r;
  if (r == profile::PRO_CIRCLE_R) {
    co = math::normalize(co);
    return;
  }

  float a = math::max(0.0f, co[0]);
  float b = math::max(0.0f, co[1]);
  float c = math::max(0.0f, co[2]);
  float x = a, y = b, z = c;
  if (ELEM(r, profile::PRO_SQUARE_R, profile::PRO_SQUARE_IN_R)) {
    BLI_assert(math::abs(z) < geom::BEVEL_EPSILON_D);
    z = 0.0f;
    x = math::min(1.0f, x);
    y = math::min(1.0f, y);
    if (r == profile::PRO_SQUARE_R) {
      const float dx = 1.0f - x;
      const float dy = 1.0f - y;
      if (dx < dy) {
        x = 1.0f;
        y = midline ? 1.0f : y;
      }
      else {
        y = 1.0f;
        x = midline ? 1.0f : x;
      }
    }
    else {
      if (x < y) {
        x = 0.0f;
        y = midline ? 0.0f : y;
      }
      else {
        y = 0.0f;
        x = midline ? 0.0f : x;
      }
    }
  }
  else {
    const float rinv = 1.0f / r;
    if (a == 0.0f) {
      if (b == 0.0f) {
        x = 0.0f;
        y = 0.0f;
        z = powf(c, rinv);
      }
      else {
        x = 0.0f;
        y = powf(1.0f / (1.0f + powf(c / b, r)), rinv);
        z = c * y / b;
      }
    }
    else {
      x = powf(1.0f / (1.0f + powf(b / a, r) + powf(c / a, r)), rinv);
      y = b * x / a;
      z = c * x / a;
    }
  }
  co[0] = x;
  co[1] = y;
  co[2] = z;
}

/**
 * Builds a 4x4 matrix that maps the unit cube (with vertices at ±1) to the
 * tetrahedron formed by `va`, `vb`, `vc` (the three boundary verts) and `vd`
 * (the original beveled vertex). Same as BMesh's #make_unit_cube_map.
 */
static float4x4 make_unit_cube_map(const float3 &va,
                                   const float3 &vb,
                                   const float3 &vc,
                                   const float3 &vd)
{
  float4x4 mat;
  /* Columns of the 4x4 column-major matrix. */
  mat[0] = float4((va - vb - vc + vd) * 0.5f, 0.0f);
  mat[1] = float4((vb - va - vc + vd) * 0.5f, 0.0f);
  mat[2] = float4((vc - va - vb + vd) * 0.5f, 0.0f);
  mat[3] = float4((va + vb + vc - vd) * 0.5f, 1.0f);
  return mat;
}

/**
 * Special case for cube corner when `r == PRO_SQUARE_R` (outward straight sides).
 * Builds the VMesh analytically by setting each canonical grid point to lie on the axis-aligned
 * face of the octant cube (`co[i] = 1`, the other two coords ramp linearly).
 * Mirrors BMesh's #make_cube_corner_square.
 */
static VMesh make_cube_corner_square(const int nseg)
{
  const int ns2 = nseg / 2;

  /* Build 3 BoundVerts at unit-axis corners. */
  BoundVert bvs[3] = {};
  for (int i = 0; i < 3; i++) {
    bvs[i].next = &bvs[(i + 1) % 3];
    bvs[i].prev = &bvs[(i + 2) % 3];
    bvs[i].index = i;
    bvs[i].nv.co[i] = 1.0f;
  }

  VMesh vm = new_adj_vmesh(3, nseg, &bvs[0]);

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j <= ns2; j++) {
      for (int k = 0; k <= ns2; k++) {
        if (!geom::is_canon(&vm, i, j, k)) {
          continue;
        }
        float3 co;
        co[i] = 1.0f;
        co[(i + 1) % 3] = float(k) * 2.0f / float(nseg);
        co[(i + 2) % 3] = float(j) * 2.0f / float(nseg);
        geom::mesh_vert(&vm, i, j, k)->co = co;
      }
    }
  }
  geom::vmesh_copy_equiv_verts(&vm);
  return vm;
}

/**
 * Special case for cube corner when `r == PRO_SQUARE_IN_R` (inward straight sides).
 * Mostly a three-way weld with a triangle in the middle for odd nseg.
 * Mirrors BMesh's #make_cube_corner_square_in.
 */
static VMesh make_cube_corner_square_in(const int nseg)
{
  const int ns2 = nseg / 2;
  const int odd = nseg % 2;

  BoundVert bvs[3] = {};
  for (int i = 0; i < 3; i++) {
    bvs[i].next = &bvs[(i + 1) % 3];
    bvs[i].prev = &bvs[(i + 2) % 3];
    bvs[i].index = i;
    bvs[i].nv.co[i] = 1.0f;
  }

  VMesh vm = new_adj_vmesh(3, nseg, &bvs[0]);

  const float b = odd ? 2.0f / (2.0f * float(ns2) + float(M_SQRT2)) : 2.0f / float(nseg);

  for (int i = 0; i < 3; i++) {
    for (int k = 0; k <= ns2; k++) {
      float3 co(0.0f);
      co[i] = 1.0f - float(k) * b;
      geom::mesh_vert(&vm, i, 0, k)->co = co;
      co[i] = 0.0f;
      co[(i + 1) % 3] = 1.0f - float(k) * b;
      geom::mesh_vert(&vm, i, 0, nseg - k)->co = co;
    }
  }
  return vm;
}

/**
 * Builds the canonical unit-simplex vmesh for the cube corner case by:
 *   1. constructing a seg=2 seed with profile-parameterized boundary midpoints,
 *   2. iteratively doubling via #cubic_subdiv until seg >= nseg,
 *   3. resampling to nseg via #interp_vmesh,
 *   4. snapping every grid point to the superellipsoid `x^r + y^r + z^r = 1`.
 * Equivalent to BMesh's #make_cube_corner_adj_vmesh.
 *
 * The `PRO_SQUARE_R` and `PRO_SQUARE_IN_R` cases are handled analytically by
 * #make_cube_corner_square / #make_cube_corner_square_in because the subdivision
 * + snap pipeline would trip the `z == 0` assertion in #snap_to_superellipsoid
 * (the square cases assume a 2-D cross-section, not a 3-D superellipsoid).
 */
static VMesh make_cube_corner_adj_vmesh(BevelState &state)
{
  const float r = state.pro_super_r;
  const int nseg = state.params.segments;

  /* Short-circuit for square profiles: the superellipsoid snap path below assumes z ≈ 0
   * only for the general superellipse case; for the two square extremes BMesh calls
   * dedicated helpers that never invoke snap_to_superellipsoid. */
  if (state.params.custom_profile_samples.is_empty()) {
    if (r == profile::PRO_SQUARE_R) {
      return make_cube_corner_square(nseg);
    }
    if (r == profile::PRO_SQUARE_IN_R) {
      return make_cube_corner_square_in(nseg);
    }
  }

  /* Create 3 BoundVerts for the unit simplex corners (1,0,0), (0,1,0), (0,0,1).
   * They are stack-allocated here and remain live for the entire subdivision.
   * cubic_subdiv and interp_vmesh access bndv->profile via vm->boundstart, so
   * these must not go out of scope until vm1 is returned. */
  BoundVert unit_bv[3] = {};

  for (int i = 0; i < 3; i++) {
    float3 co_start(0.0f);
    float3 co_end(0.0f);
    float3 co_mid(0.0f);
    co_start[i] = 1.0f;
    co_end[(i + 1) % 3] = 1.0f;
    co_mid[i] = 1.0f;
    co_mid[(i + 1) % 3] = 1.0f;

    /* Circular-list links. */
    unit_bv[i].next = &unit_bv[(i + 1) % 3];
    unit_bv[i].prev = &unit_bv[(i + 2) % 3];
    unit_bv[i].index = i;
    unit_bv[i].nv.co = co_start;

    /* Set up the profile for the arc from corner i to corner i+1. */
    Profile &pro = unit_bv[i].profile;
    pro.start = co_start;
    pro.end = co_end;
    pro.middle = co_mid;
    pro.plane_co = co_start;
    pro.plane_no = math::normalize(math::cross(co_start, co_end));
    pro.proj_dir = pro.plane_no;
    pro.super_r = r;
    pro.height = 0.0f;
    pro.special_params = false;

    /* Build the 2D→3D map and fill prof_co / prof_co_2. */
    float4x4 map;
    const bool use_map = (r != profile::PRO_LINE_R) &&
                         geom::make_unit_square_map(pro.start, pro.middle, pro.end, map);

    const ProfileSpacing &ps = state.pro_spacing;
    pro.prof_co = Array<float3>(nseg + 1);
    profile::calculate_profile_segments(pro,
                                        map,
                                        use_map,
                                        false,
                                        nseg,
                                        ps.xvals.data(),
                                        ps.yvals.data(),
                                        pro.prof_co.as_mutable_span());

    const bool need_2 = (nseg != ps.seg_2);
    if (need_2) {
      pro.prof_co_2 = Array<float3>(ps.seg_2 + 1);
      profile::calculate_profile_segments(pro,
                                          map,
                                          use_map,
                                          false,
                                          ps.seg_2,
                                          ps.xvals_2.data(),
                                          ps.yvals_2.data(),
                                          pro.prof_co_2.as_mutable_span());
    }
    else {
      /* prof_co_2 is a copy of the now-filled prof_co (seg_2 == nseg). */
      pro.prof_co_2 = pro.prof_co;
    }
  }

  /* Build the seg=2 seed vmesh using the unit-simplex BoundVerts as the boundary ring. */
  VMesh vm0 = new_adj_vmesh(3, 2, &unit_bv[0]);

  for (int i = 0; i < 3; i++) {
    geom::mesh_vert(&vm0, i, 0, 0)->co = unit_bv[i].nv.co;
    /* Sample the profile midpoint at k=1 out of seg=2. */
    const float3 pt = profile::get_profile_point(state, &unit_bv[i].profile, 1, 2);
    geom::mesh_vert(&vm0, i, 0, 1)->co = pt;
  }

  /* Center vertex: place it on the (1,1,1) diagonal scaled by 1/sqrt(3),
   * then adjust slightly based on super_r to match BMesh. */
  float3 cen(float(M_SQRT1_3));
  if (nseg > 2) {
    if (r > 1.5f) {
      cen *= 1.4f;
    }
    else if (r < 0.75f) {
      cen *= 0.6f;
    }
  }
  geom::mesh_vert(&vm0, 0, 1, 1)->co = cen;
  geom::vmesh_copy_equiv_verts(&vm0);

  /* Subdivide until seg >= nseg, then resample. */
  VMesh vm1 = std::move(vm0);
  while (vm1.seg < nseg) {
    VMesh next = cubic_subdiv(state, vm1);
    vm1 = std::move(next);
  }
  if (vm1.seg != nseg) {
    VMesh resampled = interp_vmesh(state, vm1, nseg);
    vm1 = std::move(resampled);
  }

  /* Snap every grid point onto the superellipsoid `x^r + y^r + z^r = 1`. */
  const int ns2 = nseg / 2;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j <= ns2; j++) {
      for (int k = 0; k <= nseg; k++) {
        snap_to_superellipsoid(geom::mesh_vert(&vm1, i, j, k)->co, r, false);
      }
    }
  }
  return vm1;
}

/**
 * Copy whichever of `a` and `b` is closer to `v` into `r`.
 */
static float3 closer_v3_v3v3v3(const float3 &a, const float3 &b, const float3 &v)
{
  return (math::distance_squared(a, v) <= math::distance_squared(b, v)) ? a : b;
}

/**
 * Identify if the given vertex configures a 'pipe' (3 or 4 edges, collinear pairs).
 * Return the boundary vert whose ebev is one of the pipe edges, and
 * whose next boundary vert has a beveled, non-pipe edge.
 */
static BoundVert *pipe_test(const BevelState &state, BevVert *bv)
{
  VMesh *vm = bv->vmesh.get();
  if (vm->count < 3 || vm->count > 4 || bv->selcount < 3 || bv->selcount > 4) {
    return nullptr;
  }

  const float3 bv_co = state.emesh.src_positions[bv->v];

  /* Find v1, v2, v3 all with beveled edges, where v1 and v3 have collinear edges. */
  EdgeHalf *epipe = nullptr;
  BoundVert *v1 = vm->boundstart;
  float3 dir1, dir3;
  do {
    BoundVert *v2 = v1->next;
    BoundVert *v3 = v2->next;
    if (v1->ebev && v2->ebev && v3->ebev) {
      const int other_v1 = geom::edge_other_vert(state.emesh, v1->ebev->e, bv->v);
      const int other_v3 = geom::edge_other_vert(state.emesh, v3->ebev->e, bv->v);
      const float3 co_v1 = state.emesh.src_positions[other_v1];
      const float3 co_v3 = state.emesh.src_positions[other_v3];

      dir1 = math::normalize(bv_co - co_v1);
      dir3 = math::normalize(co_v3 - bv_co);
      if (float(math::angle_between(dir1, dir3)) < geom::BEVEL_EPSILON_ANG) {
        epipe = v1->ebev;
        break;
      }
    }
  } while ((v1 = v1->next) != vm->boundstart);

  if (!epipe) {
    return nullptr;
  }

  /* Check face planes: all should have normals perpendicular to epipe. */
  for (int i = 0; i < bv->edgecount; i++) {
    EdgeHalf *e = &bv->edges[i];
    if (e->fnext >= 0) {
      const float3 face_no = state.emesh.src_face_normals[e->fnext];
      if (math::abs(math::dot(dir1, face_no)) > geom::BEVEL_EPSILON_BIG) {
        return nullptr;
      }
    }
  }
  return v1;
}

/**
 * Snap co to the closest point on the profile for vpipe projected onto the plane
 * containing co with normal in the direction of edge vpipe->ebev.
 */
static float3 snap_to_pipe_profile(
    const BevelState &state, BevVert *bv, BoundVert *vpipe, bool midline, const float3 &co)
{
  Profile *pro = &vpipe->profile;
  EdgeHalf *e = vpipe->ebev;

  if (math::is_equal(pro->start, pro->end, geom::BEVEL_EPSILON_D)) {
    return pro->start;
  }

  /* Get a plane with the normal pointing along the beveled edge. */
  const int other_v = geom::edge_other_vert(state.emesh, e->e, bv->v);
  const float3 v_co = state.emesh.src_positions[bv->v];
  const float3 other_co = state.emesh.src_positions[other_v];
  const float3 edir = v_co - other_co;
  const float4 plane = math::plane_from_point_normal(co, edir);

  const float3 start_plane = math::closest_to_plane(plane, pro->start);
  const float3 end_plane = math::closest_to_plane(plane, pro->end);
  const float3 middle_plane = math::closest_to_plane(plane, pro->middle);

  float4x4 m;
  if (geom::make_unit_square_map(start_plane, middle_plane, end_plane, m)) {
    bool invert_ok;
    const float4x4 minv = math::invert(m, invert_ok);
    if (invert_ok) {
      /* Transform co and project it onto superellipse. */
      float3 p = math::transform_point(minv, co);
      snap_to_superellipsoid(p, pro->super_r, midline);
      return math::transform_point(m, p);
    }
  }
  /* Planar case: just snap to line start_plane--end_plane. */
  return math::closest_to_line_segment(co, start_plane, end_plane);
}

/**
 * See pipe_test for conditions that make 'pipe'; vpipe is the return value from that.
 * We want to make an ADJ mesh but then snap the vertices to the profile in a plane
 * perpendicular to the pipes.
 */
static VMesh pipe_adj_vmesh(BevelState &state, BevVert *bv, BoundVert *vpipe)
{
  /* Some unnecessary overhead running this subdivision with custom profile snapping later on. */
  VMesh vm = adj_vmesh(state, bv);

  /* Now snap all interior coordinates to be on the epipe profile. */
  const int n_bndv = bv->vmesh->count;
  const int ns = bv->vmesh->seg;
  const int half_ns = ns / 2;
  const int ipipe1 = vpipe->index;
  const int ipipe2 = vpipe->next->next->index;

  for (int i = 0; i < n_bndv; i++) {
    for (int j = 1; j <= half_ns; j++) {
      for (int k = 0; k <= half_ns; k++) {
        if (!geom::is_canon(&vm, i, j, k)) {
          continue;
        }
        /* With a custom profile just copy the shape of the profile at each ring. */
        if (!state.params.custom_profile_samples.is_empty()) {
          /* Find both profile vertices that correspond to this point. */
          float3 *profile_point_pipe1, *profile_point_pipe2;
          float f;
          if (ELEM(i, ipipe1, ipipe2)) {
            if (n_bndv == 3 && i == ipipe1) {
              /* This part of the vmesh is the triangular corner between the two pipe profiles. */
              const int ring = std::max(j, k);
              profile_point_pipe2 = &geom::mesh_vert(&vm, i, 0, ring)->co;
              profile_point_pipe1 = &geom::mesh_vert(&vm, i, ring, 0)->co;
              /* End profile index increases with k on one side and j on the other. */
              f = ((k < j) ? std::min(j, k) : ((2.0f * ring) - j)) / (2.0f * ring);
            }
            else {
              /* This is part of either pipe profile boundvert area in the 4-way intersection. */
              profile_point_pipe1 = &geom::mesh_vert(&vm, i, 0, k)->co;
              profile_point_pipe2 =
                  &geom::mesh_vert(&vm, (i == ipipe1) ? ipipe2 : ipipe1, 0, ns - k)->co;
              f = float(j) / float(ns); /* The ring index brings us closer to the other side. */
            }
          }
          else {
            /* The profile vertices are on both ends of each of the side profile's rings. */
            profile_point_pipe1 = &geom::mesh_vert(&vm, i, j, 0)->co;
            profile_point_pipe2 = &geom::mesh_vert(&vm, i, j, ns)->co;
            f = float(k) / float(ns); /* Ring runs along the pipe, so segment is used here. */
          }

          /* Place the vertex by interpolating between the two profile points using the factor. */
          geom::mesh_vert(&vm, i, j, k)->co = math::interpolate(
              *profile_point_pipe1, *profile_point_pipe2, f);
        }
        else {
          /* A tricky case is for the 'square' profiles and an even nseg: we want certain
           * vertices to snap to the midline on the pipe, not just to one plane or the other. */
          const bool even = (ns % 2) == 0;
          const bool midline = even && k == half_ns &&
                               ((i == 0 && j == half_ns) || ELEM(i, ipipe1, ipipe2));
          geom::mesh_vert(&vm, i, j, k)->co = snap_to_pipe_profile(
              state, bv, vpipe, midline, geom::mesh_vert(&vm, i, j, k)->co);
        }
      }
    }
  }
  return vm;
}

/**
 * Special case of VMesh when profile == 1 and there are 3 or more beveled edges.
 * We want the effect of parallel offset lines (n/2 of them)
 * on each side of the center, for even n.
 * Wherever they intersect with each other between two successive beveled edges,
 * those intersections are part of the vmesh rings.
 * We have to move the boundary edges too -- the usual method is to make one profile plane between
 * successive BoundVerts, but for the effect we want here, there will be two planes,
 * one on each side of the original edge.
 * At the moment, this is not called for odd number of segments, though code does something if it
 * is.
 */
static VMesh square_out_adj_vmesh(BevelState &state, BevVert *bv)
{
  const int n_bndv = bv->vmesh->count;
  const int ns = bv->vmesh->seg;
  const int ns2 = ns / 2;
  const int odd = ns % 2;
  float ns2inv = 1.0f / float(ns2);
  VMesh vm = new_adj_vmesh(n_bndv, ns, bv->vmesh->boundstart);
  const int clstride = ns2 + 1;
  Array<float3, 8> centerline(clstride * n_bndv);
  Array<bool, 8> cset(n_bndv, false);

  const float3 bv_co = state.emesh.src_positions[bv->v];

  /* Find on_edge, place on bndv[i]'s elast where offset line would meet,
   * taking min-distance-to bv->v with position where next sector's offset line would meet. */
  BoundVert *bndv = vm.boundstart;
  for (int i = 0; i < n_bndv; i++) {
    const float3 bndco = bndv->nv.co;
    EdgeHalf *e1 = bndv->efirst;
    EdgeHalf *e2 = bndv->elast;
    AngleKind ang_kind = ANGLE_STRAIGHT;
    if (e1 && e2) {
      ang_kind = edges_angle_kind(state.emesh, e1, e2, bv->v);
    }
    if (bndv->is_patch_start) {
      centerline[clstride * i] = math::midpoint(bndv->nv.co, bndv->next->nv.co);
      cset[i] = true;
      bndv = bndv->next;
      i++;
      centerline[clstride * i] = math::midpoint(bndv->nv.co, bndv->next->nv.co);
      cset[i] = true;
      bndv = bndv->next;
      i++;
      /* Leave cset[i] where it was - probably false, unless i == n - 1. */
    }
    else if (bndv->is_arc_start) {
      e1 = bndv->efirst;
      e2 = bndv->next->efirst;
      centerline[clstride * i] = bndv->profile.middle;
      bndv = bndv->next;
      cset[i] = true;
      i++;
      /* Leave cset[i] where it was - probably false, unless i == n - 1. */
    }
    else if (ang_kind == ANGLE_SMALLER) {
      const int e1_other_v = geom::edge_other_vert(state.emesh, e1->e, bv->v);
      const int e2_other_v = geom::edge_other_vert(state.emesh, e2->e, bv->v);
      const float3 e1_other_co = state.emesh.src_positions[e1_other_v];
      const float3 e2_other_co = state.emesh.src_positions[e2_other_v];
      const float3 dir1 = float3(bv_co) - e1_other_co;
      const float3 dir2 = float3(bv_co) - e2_other_co;
      const float3 co1 = float3(bndco) + dir1;
      const float3 co2 = float3(bndco) + dir2;
      /* Intersect e1 with line through bndv parallel to e2 to get v1co. */
      float3 meet1, meet2;
      int ikind = math::isect_line_line(
          float3(bv_co), e1_other_co, float3(bndco), co2, meet1, meet2);
      float3 v1co;
      bool v1set;
      if (ikind == 0) {
        v1set = false;
      }
      else {
        /* If the lines are skew (ikind == 2), want meet1 which is on e1. */
        v1co = meet1;
        v1set = true;
      }
      /* Intersect e2 with line through bndv parallel to e1 to get v2co. */
      ikind = math::isect_line_line(float3(bv_co), e2_other_co, float3(bndco), co1, meet1, meet2);
      float3 v2co;
      bool v2set;
      if (ikind == 0) {
        v2set = false;
      }
      else {
        v2set = true;
        v2co = meet1;
      }

      /* We want on_edge[i] to be min dist to bv->v of v2co and the v1co of next iteration. */
      float3 &on_edge_cur = centerline[clstride * i];
      int iprev = (i == 0) ? n_bndv - 1 : i - 1;
      float3 &on_edge_prev = centerline[clstride * iprev];
      if (v2set) {
        if (cset[i]) {
          on_edge_cur = closer_v3_v3v3v3(on_edge_cur, v2co, bv_co);
        }
        else {
          on_edge_cur = v2co;
          cset[i] = true;
        }
      }
      if (v1set) {
        if (cset[iprev]) {
          on_edge_prev = closer_v3_v3v3v3(on_edge_prev, v1co, bv_co);
        }
        else {
          on_edge_prev = v1co;
          cset[iprev] = true;
        }
      }
    }
    bndv = bndv->next;
  }
  /* Maybe not everything was set by the previous loop. */
  bndv = vm.boundstart;
  for (int i = 0; i < n_bndv; i++) {
    if (!cset[i]) {
      float3 &on_edge_cur = centerline[clstride * i];
      EdgeHalf *e1 = bndv->next->efirst;
      float3 co1 = bndv->nv.co;
      float3 co2 = bndv->next->nv.co;
      if (e1) {
        const int e1_other_v = geom::edge_other_vert(state.emesh, e1->e, bv->v);
        const float3 e1_other_co = state.emesh.src_positions[e1_other_v];
        if (bndv->prev->is_arc_start && bndv->next->is_arc_start) {
          float3 meet1, meet2;
          int ikind = math::isect_line_line(bv_co, e1_other_co, co1, co2, meet1, meet2);
          if (ikind != 0) {
            on_edge_cur = meet1;
            cset[i] = true;
          }
        }
        else {
          if (bndv->prev->is_arc_start) {
            on_edge_cur = math::closest_to_line_segment(co1, bv_co, e1_other_co);
          }
          else {
            on_edge_cur = math::closest_to_line_segment(co2, bv_co, e1_other_co);
          }
          cset[i] = true;
        }
      }
      if (!cset[i]) {
        on_edge_cur = math::midpoint(co1, co2);
        cset[i] = true;
      }
    }
    bndv = bndv->next;
  }

  /* Fill in rest of center-lines by interpolation. */
  const float3 co2 = bv_co;
  bndv = vm.boundstart;
  for (int i = 0; i < n_bndv; i++) {
    if (odd) {
      float ang = 0.5f * float(math::angle_between(math::normalize(bndv->nv.co - co2),
                                                   math::normalize(bndv->next->nv.co - co2)));
      float finalfrac;
      if (ang > geom::BEVEL_SMALL_ANG) {
        /* finalfrac is the length along arms of isosceles triangle with top angle 2*ang
         * such that the base of the triangle is 1.
         * This is used in interpolation along center-line in odd case.
         * To avoid too big a drop from bv, cap finalfrac a 0.8 arbitrarily */
        finalfrac = 0.5f / sinf(ang);
        finalfrac = std::min(finalfrac, 0.8f);
      }
      else {
        finalfrac = 0.8f;
      }
      ns2inv = 1.0f / (ns2 + finalfrac);
    }

    const float3 co1 = centerline[clstride * i];
    for (int j = 1; j <= ns2; j++) {
      centerline[clstride * i + j] = math::interpolate(co1, co2, float(j) * ns2inv);
    }
    bndv = bndv->next;
  }

  /* Coords of edges and mid or near-mid line. */
  bndv = vm.boundstart;
  for (int i = 0; i < n_bndv; i++) {
    const float3 co1 = bndv->nv.co;
    const float3 &prev_cl = centerline[clstride * (i == 0 ? n_bndv - 1 : i - 1)];
    for (int j = 0; j < ns2 + odd; j++) {
      geom::mesh_vert(&vm, i, j, 0)->co = math::interpolate(co1, prev_cl, float(j) * ns2inv);
    }
    const float3 &cur_cl = centerline[clstride * i];
    for (int k = 1; k <= ns2; k++) {
      geom::mesh_vert(&vm, i, 0, k)->co = math::interpolate(co1, cur_cl, float(k) * ns2inv);
    }
    bndv = bndv->next;
  }
  if (!odd) {
    geom::mesh_vert(&vm, 0, ns2, ns2)->co = bv_co;
  }
  geom::vmesh_copy_equiv_verts(&vm);

  /* Fill in interior points by interpolation from edges to center-lines. */
  bndv = vm.boundstart;
  for (int i = 0; i < n_bndv; i++) {
    int im1 = (i == 0) ? n_bndv - 1 : i - 1;
    for (int j = 1; j < ns2 + odd; j++) {
      for (int k = 1; k <= ns2; k++) {
        float3 meet1, meet2;
        int ikind = math::isect_line_line(geom::mesh_vert(&vm, i, 0, k)->co,
                                          centerline[clstride * im1 + k],
                                          geom::mesh_vert(&vm, i, j, 0)->co,
                                          centerline[clstride * i + j],
                                          meet1,
                                          meet2);
        if (ikind == 0) {
          /* How can this happen? fall back on interpolation in one direction if it does. */
          geom::mesh_vert(&vm, i, j, k)->co = math::interpolate(geom::mesh_vert(&vm, i, 0, k)->co,
                                                                centerline[clstride * im1 + k],
                                                                float(j) * ns2inv);
        }
        else if (ikind == 1) {
          geom::mesh_vert(&vm, i, j, k)->co = meet1;
        }
        else {
          geom::mesh_vert(&vm, i, j, k)->co = math::midpoint(meet1, meet2);
        }
      }
    }
    bndv = bndv->next;
  }

  geom::vmesh_copy_equiv_verts(&vm);
  return vm;
}

/**
 * Tests whether `bv` is a good candidate for the tri-corner cube-corner special case.
 * Returns 1 when it qualifies (3-vert, equal offsets, ~90° corner angles),
 * 0 when the count is 3 but other conditions are not met,
 * and -1 when it definitely should not use this path.
 */
static int tri_corner_test(const BevelState &state, const BevVert *bv)
{
  /* Custom profiles and vertex-only mode skip this path. */
  if (state.params.affect_type == BevelAffect::Vertices ||
      !state.params.custom_profile_samples.is_empty())
  {
    return -1;
  }
  if (bv->vmesh->count != 3) {
    return 0;
  }

  const float offset = bv->edges[0].offset_l;
  int in_plane_e = 0;
  float totang = 0.0f;
  const ExtendableMesh &emesh = state.emesh;

  for (int i = 0; i < bv->edgecount; i++) {
    const EdgeHalf &e = bv->edges[i];
    /* Compute the signed dihedral angle of this edge from its two adjacent face normals. */
    float ang = 0.0f;
    if (e.fprev >= 0 && e.fnext >= 0) {
      const float3 no_prev = emesh.src_face_normals[e.fprev];
      const float3 no_next = emesh.src_face_normals[e.fnext];
      ang = float(math::angle_between(no_prev, no_next));
      /* Negate for concave (the dihedral is > π). */
      if (math::dot(math::cross(no_prev, no_next),
                    emesh.src_positions[bv->v] - state.face_center(e.fprev)) < 0.0f)
      {
        ang = -ang;
      }
    }

    const float absang = math::abs(ang);
    if (absang <= float(M_PI_4)) {
      in_plane_e++;
    }
    else if (absang >= 3.0f * float(M_PI_4)) {
      return -1;
    }

    if (e.is_bev && math::abs(e.offset_l - offset) > geom::BEVEL_EPSILON_D) {
      return -1;
    }
    totang += ang;
  }

  if (in_plane_e != bv->edgecount - 3) {
    return -1;
  }
  const float angdiff = math::abs(math::abs(totang) - 3.0f * float(M_PI_2));
  if ((state.pro_super_r == profile::PRO_SQUARE_R && angdiff > float(M_PI) / 16.0f) ||
      (angdiff > float(M_PI_4)))
  {
    return -1;
  }
  if (bv->edgecount != 3 || bv->selcount != 3) {
    return 0;
  }
  return 1;
}

/**
 * Builds the ADJ vmesh for a tri-corner bevel using the cube-corner superellipsoid snap approach.
 * Equivalent to BMesh's #tri_corner_adj_vmesh.
 */
static VMesh tri_corner_adj_vmesh(BevelState &state, BevVert *bv)
{
  BoundVert *bndv = bv->vmesh->boundstart;
  const float3 co0 = bndv->nv.co;
  bndv = bndv->next;
  const float3 co1 = bndv->nv.co;
  bndv = bndv->next;
  const float3 co2 = bndv->nv.co;

  const float3 v_co = state.emesh.src_positions[bv->v];
  const float4x4 mat = make_unit_cube_map(co0, co1, co2, v_co);

  VMesh vm = make_cube_corner_adj_vmesh(state);
  /* Set the correct BoundVert ring (the canonical helper builds with nullptr). */
  vm.boundstart = bv->vmesh->boundstart;

  const int ns = vm.seg;
  const int ns2 = ns / 2;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j <= ns2; j++) {
      for (int k = 0; k <= ns; k++) {
        geom::mesh_vert(&vm, i, j, k)->co = math::transform_point(
            mat, geom::mesh_vert(&vm, i, j, k)->co);
      }
    }
  }
  return vm;
}

/**
 * Builds the general ADJ vmesh by starting from a seed mesh with seg=2,
 * then iteratively doubling resolution via #cubic_subdiv and
 * resampling to the target with #interp_vmesh.
 * Dispatches to #tri_corner_adj_vmesh for the cube-corner special case.
 */
static VMesh adj_vmesh(BevelState &state, BevVert *bv)
{
  const int n_bndv = bv->vmesh->count;
  const int nseg = bv->vmesh->seg;

  /* Same as the bevel of 3 edges of a vertex in a cube: use the superellipsoid snap path. */
  if (n_bndv == 3 && tri_corner_test(state, bv) != -1 &&
      state.pro_super_r != profile::PRO_SQUARE_IN_R)
  {
    return tri_corner_adj_vmesh(state, bv);
  }

  VMesh vm0 = new_adj_vmesh(n_bndv, 2, bv->vmesh->boundstart);

  /* Seed mesh: boundary from BoundVert coords, mid-arc from profile at k=1. */
  float3 center(0.0f);
  BoundVert *bndv = vm0.boundstart;
  for (int i = 0; i < n_bndv; i++) {
    geom::mesh_vert(&vm0, i, 0, 0)->co = bndv->nv.co;
    const float3 pt = profile::get_profile_point(state, &bndv->profile, 1, 2);
    geom::mesh_vert(&vm0, i, 0, 1)->co = pt;
    center += float3(bndv->nv.co);
    bndv = bndv->next;
  }
  center /= float(n_bndv);

  /* Center vertex position using fullness. */
  const float3 v_co = state.emesh.src_positions[bv->v];
  const float3 center_dir = v_co - center;
  if (math::length_squared(center_dir) > geom::BEVEL_EPSILON_SQ) {
    const float fullness = state.pro_spacing.fullness;
    geom::mesh_vert(&vm0, 0, 1, 1)->co = center + center_dir * fullness;
  }
  else {
    geom::mesh_vert(&vm0, 0, 1, 1)->co = center;
  }
  geom::vmesh_copy_equiv_verts(&vm0);

  /* Subdivide until seg >= nseg.
   * Use do..while so that cubic_subdiv always runs at least once, matching bmesh_bevel.cc.
   * This is necessary because the seed mesh's center vertex is placed using the fullness
   * heuristic, which can be > 1.0 for high shape values, overshooting the original vertex.
   * The first cubic_subdiv step applies the Sabin-formula correction that pulls the center
   * back toward the bevel boundary regardless of nseg. */
  VMesh vm1 = std::move(vm0);
  do {
    VMesh next = cubic_subdiv(state, vm1);
    vm1 = std::move(next);
  } while (vm1.seg < nseg);
  if (vm1.seg != nseg) {
    VMesh resampled = interp_vmesh(state, vm1, nseg);
    vm1 = std::move(resampled);
  }
  return vm1;
}

/** \} */

/* Construction around the vertex. */
static void bevel_vert_construct(BevelState &state, int v)
{
  int nsel = 0;
  int tot_edges = 0;
  int tot_wire = 0;
  int first_e = -1;

  const ExtendableMesh &emesh = state.emesh;

  /* Current bevel does nothing if only one edge into a vertex (handled below).
   *
   * Gather input selected edges.
   * Only bevel selected edges that have exactly two incident faces.
   * Want edges to be ordered so that they share faces.
   * There may be one or more chains of shared faces broken by
   * gaps where there are no faces.
   * Want to ignore wire edges completely for edge beveling.
   * TODO: make following work when more than one gap. */

  for (const int e : emesh.src_vert_to_edge[v]) {
    int face_count = emesh.src_edge_to_face[e].size();

    bool is_selected = (state.params.affect_type != BevelAffect::Vertices &&
                        state.selection.contains(e));
    if (is_selected) {
      BLI_assert(face_count == 2);
      nsel++;
      if (first_e == -1) {
        first_e = e;
      }
    }
    if (face_count == 1) {
      first_e = e;
    }
    if (face_count > 0 || state.params.affect_type == BevelAffect::Vertices) {
      tot_edges++;
    }
    if (face_count == 0) {
      tot_wire++;
    }
  }

  if (first_e == -1 && !emesh.src_vert_to_edge[v].is_empty()) {
    first_e = emesh.src_vert_to_edge[v].first();
  }

  if ((nsel == 0 && state.params.affect_type != BevelAffect::Vertices) ||
      (tot_edges < 2 && state.params.affect_type == BevelAffect::Vertices))
  {
    return;
  }

  state.bev_verts.append({});
  BevVert *bv = &state.bev_verts.last();
  bv->v = v;
  bv->edgecount = tot_edges;
  bv->selcount = nsel;
  bv->wirecount = tot_wire;
  /* bv->offset holds the representative slide distance for this vertex.
   * In vertex bevel mode, offsets[0] is a per-vertex array, so index by v.
   * In edge bevel mode, offsets[0] is a per-edge array; use the first selected
   * edge as an approximation (exact when all edges share a uniform offset). */
  if (state.params.affect_type == BevelAffect::Vertices) {
    bv->offset = state.params.offsets[0][v];
  }
  else {
    bv->offset = (first_e != -1) ? state.params.offsets[0][first_e] : 1.0f;
  }

  bv->edges = Array<EdgeHalf>(tot_edges);

  if (tot_wire > 0) {
    bv->wire_edges = Array<int>(tot_wire);
    int i = 0;
    for (const int e : emesh.src_vert_to_edge[v]) {
      if (emesh.src_edge_to_face[e].is_empty()) {
        bv->wire_edges[i++] = e;
      }
    }
  }

  bv->vmesh = std::make_unique<VMesh>();
  bv->vmesh->seg = state.params.segments;

  const bool is_edge_bevel = (state.params.affect_type != BevelAffect::Vertices);
  find_bevel_edge_order(emesh, bv, first_e, is_edge_bevel);

  for (int i = 0; i < tot_edges; i++) {
    EdgeHalf *eh = &bv->edges[i];
    int e = eh->e;
    bool is_selected = (state.params.affect_type != BevelAffect::Vertices &&
                        state.selection.contains(e));
    if (is_selected) {
      eh->is_bev = true;
      eh->seg = state.params.segments;
    }

    const int2 edge_verts = emesh.edge_verts(e);
    eh->is_rev = (edge_verts[1] == v);
  }

  if (tot_edges > 1) {
    /* Test if the winding order of edges around the vertex is CCW as seen from the
     * vertex normal side. If not, reverse the order and swap fprev/fnext on each EdgeHalf. */
    int ccw_test_sum = 0;
    for (int i = 0; i < tot_edges; i++) {
      ccw_test_sum += bev_ccw_test(
          emesh, bv->edges[i].e, bv->edges[(i + 1) % tot_edges].e, bv->edges[i].fnext);
    }
    if (ccw_test_sum < 0) {
      for (int i = 0; i <= (tot_edges / 2) - 1; i++) {
        std::swap(bv->edges[i], bv->edges[tot_edges - i - 1]);
        std::swap(bv->edges[i].fprev, bv->edges[i].fnext);
        std::swap(bv->edges[tot_edges - i - 1].fprev, bv->edges[tot_edges - i - 1].fnext);
      }
      if (tot_edges % 2 == 1) {
        int i = tot_edges / 2;
        std::swap(bv->edges[i].fprev, bv->edges[i].fnext);
      }
    }
  }

  /* Link the next/prev ring and compute offsets and seam flags for each EdgeHalf. */
  for (int i = 0; i < tot_edges; i++) {
    EdgeHalf *eh = &bv->edges[i];
    eh->next = &bv->edges[(i + 1) % tot_edges];
    eh->prev = &bv->edges[(i + tot_edges - 1) % tot_edges];

    if (eh->is_bev) {
      /* Determine which end of the edge contains this vertex.
       * `edges()[e][0]` is the source end and `[1]` is the destination end.
       * Left and right offsets are selected from the corresponding index pair.
       * Internally we use the convention that "left" and "right" are as seen
       * looking towards the vertex of the edge half.
       * The external convention, maybe more intuitive to users, is to
       * stand looking from source to destination (on the positive normal side),
       * so we switch "left" and "right" at the source end.
       */
      const int2 &ev = emesh.src_edges[eh->e];
      const bool at_src = (ev[0] == v);
      if (at_src) {
        eh->offset_l = state.params.offsets[1][eh->e];
        eh->offset_r = state.params.offsets[0][eh->e];
      }
      else {
        eh->offset_l = state.params.offsets[2][eh->e];
        eh->offset_r = state.params.offsets[3][eh->e];
      }
      eh->offset_l_spec = eh->offset_l;
      eh->offset_r_spec = eh->offset_r;
    }
    else if (state.params.affect_type == BevelAffect::Vertices) {
      /* Vertex bevel: all edges incident to the selected vertex slide by the same amount.
       * offsets[0] is a per-vertex array; index by v to get the slide distance for this vertex.
       * offset_r is not used in vertex bevel mode. */
      eh->offset_l = eh->offset_l_spec = state.params.offsets[0][v];
      eh->offset_r = eh->offset_r_spec = 0.0f;
    }
    else {
      eh->offset_l = eh->offset_l_spec = 0.0f;
      eh->offset_r = eh->offset_r_spec = 0.0f;
    }

    /* An edge half is a seam when its two adjacent faces have discontinuous UV data,
     * or when one of those faces is absent (boundary edge). Mirrors the BMesh logic
     * in #bev_vert_construct. */
    if (eh->fprev != -1 && eh->fnext != -1) {
      eh->is_seam = !state.uv_layer_info.contig_uv_maps_across_edge(
          emesh, eh->e, eh->fprev, eh->fnext);
    }
    else {
      eh->is_seam = true;
    }
  }

  state.vert_hash.add_new(v, bv);
}

/* -------------------------------------------------------------------- */
/** \name Output mesh assembly
 * \{ */

/**
 * Assembles a new output #Mesh from the #ExtendableMesh, which holds both the
 * original mesh elements (some marked as killed) and the newly created elements.
 *
 * Index convention in #ExtendableMesh:
 *   - original elements:  indices 0 .. mesh.Xnum - 1
 *   - new elements:       indices mesh.Xnum .. mesh.Xnum + newX - 1
 *
 * Mirrors the `build_mesh` function from the TRY1 reference implementation,
 * adapted for the new index convention (no negated indices).
 */
/**
 * Interpolate a UV value for vertex position `dst_co` from the corners of original face `f_src`.
 * Mirrors the relevant part of #BM_loop_interp_from_face.
 */
static float2 interp_uv_from_face(const ExtendableMesh &emesh,
                                  const Span<float2> uv_vals,
                                  const int f_src,
                                  const float3 dst_co)
{
  const OffsetIndices src_faces = emesh.src_faces;
  const Span<int> corner_verts = emesh.src_corner_verts;
  const Span<float3> positions = emesh.src_positions;
  const IndexRange face_corners = src_faces[f_src];

  const float3 no = emesh.src_face_normals[f_src];
  const float3x3 axis_mat = math::axis_dominant_to_m3(no);

  /* Project face corners to 2D. */
  Array<float2, 16> cos_2d(face_corners.size());
  const Span<int> face_verts = corner_verts.slice(face_corners);
  for (const int i : face_corners.index_range()) {
    const int vert = face_verts[i];
    cos_2d[i] = float2(axis_mat * positions[vert]);
  }

  /* Project destination point to 2D. */
  const float2 co_2d = float2(axis_mat * dst_co);

  /* Compute mean-value interpolation weights. */
  Array<float, 16> w(face_corners.size());
  math::interp_weights_poly(w, cos_2d, co_2d);

  /* Weighted sum of UV values. */
  float2 result(0.0f);
  for (const int i : face_corners.index_range()) {
    result += w[i] * uv_vals[face_corners[i]];
  }
  return result;
}

/**
 * For every new corner, interpolate UV values from the corner's face representative and
 * store the results in `emesh.new_corner_uvs_`.
 * Mirrors the per-loop UV interpolation done inside BMesh's #bev_create_ngon.
 */
static void fill_new_corner_uvs(BevelState &state)
{
  const int num_uv_layers = int(state.uv_layer_info.uv_maps.size());
  if (num_uv_layers == 0) {
    return;
  }
  ExtendableMesh &emesh = state.emesh;
  const int n_new_faces = emesh.new_faces_num();
  const Span<int> new_face_exs = emesh.new_face_examples();
  /* Build OffsetIndices over the new-face offset array. */
  const OffsetIndices new_faces(emesh.new_face_offsets());
  const Span<int> new_corner_verts = emesh.new_corner_verts();
  const Span<int> new_corner_face_reps = emesh.new_corner_face_reps();
  const Span<int> new_corner_snap_edges = emesh.new_corner_snap_edges();

  for (int nf = 0; nf < n_new_faces; nf++) {
    /* Face-level fallback representative face. */
    const int face_fallback = new_face_exs[nf];
    const IndexRange new_corners = new_faces[nf];
    for (int uv_i = 0; uv_i < num_uv_layers; uv_i++) {
      const Span<float2> uv_vals = state.uv_layer_info.uv_maps[uv_i].values;
      MutableSpan<float2> dst_uvs = emesh.new_corner_uvs(uv_i);
      for (const int nc : new_corners) {
        /* Use the per-corner face rep if set; otherwise fall back to the face-level one. */
        const int f_src = (new_corner_face_reps[nc] >= 0) ? new_corner_face_reps[nc] :
                                                            face_fallback;
        if (f_src < 0 || f_src >= emesh.mesh.faces_num) {
          continue;
        }
        /* Use emesh.vert_position() so new bevel vertices are correctly resolved. */
        float3 co = emesh.vert_position(new_corner_verts[nc]);
        /* If a snap edge is set, project the position onto that edge before interpolating.
         * This mirrors BMesh's snap_edge_arr logic in #bev_create_ngon: UVs are sampled from
         * the point on the original edge nearest to the new vertex, ensuring correct UV
         * continuity across the seam at the center strip of an odd-segment bevel. */
        const int snap_e = new_corner_snap_edges[nc];
        if (snap_e >= 0) {
          const int2 ev = emesh.edge_verts(snap_e);
          const float3 ep0 = emesh.vert_position(ev[0]);
          const float3 ep1 = emesh.vert_position(ev[1]);
          co = math::closest_to_line_segment(co, ep0, ep1);
        }
        dst_uvs[nc] = interp_uv_from_face(emesh, uv_vals, f_src, co);
      }
    }
  }
}

/**
 * Average UV values at shared seam vertices.
 *
 * Mirrors what BMesh achieves via #update_uv_vert_map + #bevel_merge_uvs:
 *
 * 1. **Source corners at original vertices** — average source-corner UV values within each
 *    pre-built #UVVertBucket (same as before).
 *
 * 2. **New corners at original vertices** — should not exist for correctly built bevel geometry
 *    (bevel vertices are killed before output), but handled for safety via source-bucket lookup.
 *
 * 3. **New corners at new (profile-arc) vertices** — look up the arc vertex's parent bevel
 *    vertex via #vert_bev_origin, then use that parent's UV bucket structure to group new
 *    corners.  New corners whose face representative's source corner at the parent vertex
 *    belongs to the same UV bucket get averaged together.  This correctly handles both
 *    seam-free meshes (one bucket: all corners merge) and seam meshes (multiple buckets:
 *    corners from different UV islands remain separate).
 */
static void merge_uvs(BevelState &state)
{
  const int num_uv_layers = int(state.uv_layer_info.uv_maps.size());
  if (num_uv_layers == 0) {
    return;
  }

  ExtendableMesh &emesh = state.emesh;
  const Map<int, Vector<int>> &vert_nc_map = emesh.vert_to_new_corners();
  const Map<int, int> &bev_origin = emesh.vert_bev_origin();

  /* Build a flat array: nc_face_rep[nc] = representative original face index for new corner nc.
   * Used in pass 3 to determine which UV bucket a new corner belongs to. */
  const int n_new_corners = int(emesh.new_corner_verts().size());
  Array<int> nc_face_rep(n_new_corners, -1);
  {
    const OffsetIndices new_faces(emesh.new_face_offsets());
    const Span<int> new_face_exs = emesh.new_face_examples();
    for (const int nf : IndexRange(emesh.new_faces_num())) {
      const int f_src = new_face_exs[nf];
      for (const int nc : new_faces[nf]) {
        nc_face_rep[nc] = f_src;
      }
    }
  }

  /* Build a map: (original_face, original_vertex) → source corner index.
   * Used to look up which bucket a new corner's representative face corner at origin_v is in. */
  const OffsetIndices src_faces = emesh.src_faces;
  const Span<int> src_corner_verts = emesh.src_corner_verts;
  /* key = face_idx * mesh.verts_num + vert_idx; value = source corner index. */
  Map<int64_t, int> face_vert_to_src_corner;
  face_vert_to_src_corner.reserve(emesh.mesh.corners_num);
  for (const int f : IndexRange(emesh.mesh.faces_num)) {
    for (const int sc : src_faces[f]) {
      const int64_t key = int64_t(f) * emesh.mesh.verts_num + src_corner_verts[sc];
      face_vert_to_src_corner.add_new(key, sc);
    }
  }

  for (int uv_i = 0; uv_i < num_uv_layers; uv_i++) {
    MutableSpan<float2> src_uv_vals = state.uv_layer_info.uv_maps[uv_i].values.as_mutable_span();
    MutableSpan<float2> new_uv_vals = emesh.new_corner_uvs(uv_i);

    /* --- Pass 1: source corners at original bevel vertices ------------------- */
    for (auto item : state.uv_vert_maps[uv_i].items()) {
      Vector<UVVertBucket> &uv_vert_buckets = item.value;
      for (UVVertBucket &bucket : uv_vert_buckets) {
        if (bucket.size() <= 1) {
          continue;
        }
        float2 avg(0.0f);
        for (const int src_c : bucket) {
          avg += src_uv_vals[src_c];
        }
        avg /= float(bucket.size());
        for (const int src_c : bucket) {
          src_uv_vals[src_c] = avg;
        }
      }
    }

    /* --- Pass 3: new (profile-arc) vertices ---------------------------------- */
    for (const auto &[nv, new_corners] : vert_nc_map.items()) {
      if (nv < emesh.mesh.verts_num) {
        continue;
      }
      if (new_corners.size() <= 1) {
        continue;
      }

      /* Find the parent bevel vertex for this arc vertex. */
      const int *origin_v_ptr = bev_origin.lookup_ptr(nv);
      if (origin_v_ptr == nullptr) {
        continue;
      }
      const int origin_v = *origin_v_ptr;

      /* Look up the UV buckets for the parent vertex on this layer. */
      const Vector<UVVertBucket> *buckets = state.uv_vert_maps[uv_i].lookup_ptr(origin_v);
      if (buckets == nullptr) {
        continue;
      }

      /* For each new corner, determine which bucket its face representative's source corner
       * at origin_v belongs to.  Use a simple scan since bucket count is tiny (≤ valence). */
      for (const UVVertBucket &bucket : *buckets) {
        /* Collect new corners in this bucket: their face rep has a source corner at origin_v
         * that is in this bucket. */
        Vector<int> matching;
        for (const int nc : new_corners) {
          const int f_rep = nc_face_rep[nc];
          if (f_rep < 0 || f_rep >= emesh.mesh.faces_num) {
            continue;
          }
          const int64_t key = int64_t(f_rep) * emesh.mesh.verts_num + origin_v;
          const int *src_c_ptr = face_vert_to_src_corner.lookup_ptr(key);
          if (src_c_ptr == nullptr) {
            continue;
          }
          if (bucket.contains(*src_c_ptr)) {
            matching.append(nc);
          }
        }
        if (matching.size() <= 1) {
          continue;
        }
        float2 avg(0.0f);
        for (const int nc : matching) {
          avg += new_uv_vals[nc];
        }
        avg /= float(matching.size());
        for (const int nc : matching) {
          new_uv_vals[nc] = avg;
        }
      }
    }
  }
}

/* Extend edge data (seam, sharp, bevel_weight) to boundary arc edges.
 * Mirrors BMesh's #bevel_extend_edge_data / #bevel_extend_edge_data_ex.
 *
 * For each BevVert, walk the BoundVert ring.  When a BoundVert has seam_len > 0
 * (or sharp_len > 0), the boundary arc edges for the next `seam_len` BoundVerts
 * need to be marked.  We do this by setting their example edge to the original
 * beveled edge that has the corresponding property (seam, bevel_weight, etc.).
 *
 * BMesh does this by directly setting BM_ELEM_SEAM / BM_ELEM_SMOOTH flags.
 * In the GN system, we use edge_set_example to propagate all edge attributes
 * from the original edge that carries the flag. */
static void bevel_extend_edge_data(BevelState &state)
{
  for (auto bv_entry : state.vert_hash.items()) {
    BevVert *bv = bv_entry.value;
    if (!bv || !bv->vmesh) {
      continue;
    }
    VMesh *vm = bv->vmesh.get();
    if (vm->mesh_kind == MeshKind::TriFan || bv->selcount < 2) {
      continue;
    }

    /* Find an original edge with the seam property to use as example.
     * This is the edge e such that e->rightv == bcur (the BoundVert with seam_len > 0).
     * In #check_edge_data_seam_sharp_edges, seam_len is set on e->rightv,
     * where e is the EdgeHalf that HAS the seam property.  So the example edge is e->e. */
    auto find_example_edge_for_flag = [&](BoundVert *bcur, bool is_seam_flag) -> int {
      for (int ei = 0; ei < bv->edgecount; ei++) {
        EdgeHalf *eh = &bv->edges[ei];
        if (eh->rightv == bcur) {
          return eh->e;
        }
      }
      (void)is_seam_flag;
      return -1;
    };

    /* Helper: walk boundary arcs from `bcur` for `extend_len` BoundVerts,
     * setting the example edge on each arc and spoke edge. */
    auto extend_arcs = [&](BoundVert *&bcur, int extend_len, int example) {
      const int idx_end = bcur->index + extend_len;
      for (int i = bcur->index; i < idx_end; i++) {
        for (int k = 0; k < vm->seg; k++) {
          const int va = geom::mesh_vert(vm, i % vm->count, 0, k)->v;
          const int vb = geom::mesh_vert(vm, i % vm->count, 0, k + 1)->v;
          if (va >= 0 && vb >= 0) {
            const int arc_edge = state.emesh.find_edge(va, vb);
            if (arc_edge >= 0 && example >= 0) {
              state.emesh.edge_set_example(arc_edge, example);
            }
          }
        }
        const int va = geom::mesh_vert(vm, i % vm->count, 0, vm->seg)->v;
        const int vb = geom::mesh_vert(vm, (i + 1) % vm->count, 0, 0)->v;
        if (va >= 0 && vb >= 0) {
          const int spoke_edge = state.emesh.find_edge(va, vb);
          if (spoke_edge >= 0 && example >= 0) {
            state.emesh.edge_set_example(spoke_edge, example);
          }
        }
        bcur = bcur->next;
      }
    };

    /* Process seam extension. */
    BoundVert *bcur = vm->boundstart;
    BoundVert *start = bcur;
    do {
      const int extend_len = bcur->seam_len;
      if (extend_len > 0) {
        if (!vm->boundstart->seam_len && start == vm->boundstart) {
          start = bcur;
        }
        const int example = find_example_edge_for_flag(bcur, true);
        extend_arcs(bcur, extend_len, example);
      }
      else {
        bcur = bcur->next;
      }
    } while (bcur != start);

    /* Process sharp extension (analogous, using sharp_len). */
    bcur = vm->boundstart;
    start = bcur;
    do {
      const int extend_len = bcur->sharp_len;
      if (extend_len > 0) {
        if (!vm->boundstart->sharp_len && start == vm->boundstart) {
          start = bcur;
        }
        const int example = find_example_edge_for_flag(bcur, false);
        extend_arcs(bcur, extend_len, example);
      }
      else {
        bcur = bcur->next;
      }
    } while (bcur != start);
  }
}

static std::optional<Mesh *> build_output_mesh(const BevelState &state,
                                               const bke::AttributeFilter &attribute_filter)
{
  const ExtendableMesh &emesh = state.emesh;
  const Mesh &src_mesh = emesh.mesh;

  /* Collect surviving original element masks from the kill arrays. */
  IndexMaskMemory memory;
  const IndexMask src_survive_verts = IndexMask::from_bools(emesh.kill_verts_array(), memory)
                                          .complement(IndexMask(src_mesh.verts_num), memory);
  const IndexMask src_survive_edges = IndexMask::from_bools(emesh.kill_edges_array(), memory)
                                          .complement(IndexMask(src_mesh.edges_num), memory);
  const IndexMask src_survive_faces = IndexMask::from_bools(emesh.kill_faces_array(), memory)
                                          .complement(IndexMask(src_mesh.faces_num), memory);

  /* Build old→new index maps for surviving original elements; -1 for killed entries. */
  Array<int> src_vert_map(src_mesh.verts_num, -1);
  index_mask::build_reverse_map(src_survive_verts, src_vert_map.as_mutable_span());
  Array<int> src_edge_map(src_mesh.edges_num, -1);
  index_mask::build_reverse_map(src_survive_edges, src_edge_map.as_mutable_span());

  /* Count surviving corners: only corners in surviving original faces are kept. */
  const Span<float3> src_positions = src_mesh.vert_positions();
  const Span<int2> src_edges = src_mesh.edges();
  const OffsetIndices<int> src_faces = src_mesh.faces();
  const Span<int> src_corner_verts = src_mesh.corner_verts();
  const Span<int> src_corner_edges = src_mesh.corner_edges();

  const int n_surv_verts = src_survive_verts.size();
  const int n_surv_edges = src_survive_edges.size();
  const int n_surv_faces = src_survive_faces.size();
  const int n_surv_corners = offset_indices::sum_group_sizes(src_faces, src_survive_faces);

  const Span<float3> new_positions = emesh.new_vert_positions();
  const Span<int2> new_edge_data = emesh.new_edges();
  const Span<int> new_face_offs = emesh.new_face_offsets(); /* size = new_faces_num + 1 */
  const Span<int> new_corner_verts = emesh.new_corner_verts();
  const Span<int> new_corner_edges = emesh.new_corner_edges();
  const int n_new_verts = int(new_positions.size());
  const int n_new_edges = int(new_edge_data.size());
  const int n_new_faces = emesh.new_faces_num();
  const int n_new_corners = int(new_corner_verts.size());

  Mesh *dst = BKE_mesh_new_nomain(n_surv_verts + n_new_verts,
                                  n_surv_edges + n_new_edges,
                                  n_surv_faces + n_new_faces,
                                  n_surv_corners + n_new_corners);
  BKE_mesh_copy_parameters_for_eval(dst, &src_mesh);

  MutableSpan<float3> dst_positions = dst->vert_positions_for_write();
  MutableSpan<int2> dst_edges = dst->edges_for_write();
  MutableSpan<int> dst_face_offsets = dst->face_offsets_for_write();
  MutableSpan<int> dst_corner_verts = dst->corner_verts_for_write();
  MutableSpan<int> dst_corner_edges = dst->corner_edges_for_write();

  /* Maps any combined (original + new) vertex index to the destination index. */
  auto mixed_vert_map = [&](const int v) -> int {
    if (v < src_mesh.verts_num) {
      return src_vert_map[v];
    }
    return n_surv_verts + (v - src_mesh.verts_num);
  };
  /* Maps any combined (original + new) edge index to the destination index. */
  auto mixed_edge_map = [&](const int e) -> int {
    if (e < src_mesh.edges_num) {
      return src_edge_map[e];
    }
    return n_surv_edges + (e - src_mesh.edges_num);
  };

  /* Surviving original vert positions and new positions. */
  array_utils::gather(
      src_positions, src_survive_verts, dst_positions.take_front(src_survive_verts.size()));
  dst_positions.take_back(new_positions.size()).copy_from(new_positions);

  /* Surviving original edges (vertex pairs remapped to destination indices) and new edges. */
  src_survive_edges.foreach_index([&](const int64_t src_e, const int64_t dst_e) {
    dst_edges[dst_e][0] = src_vert_map[src_edges[src_e][0]];
    dst_edges[dst_e][1] = src_vert_map[src_edges[src_e][1]];
  });
  for (const int ne : IndexRange(n_new_edges)) {
    dst_edges[n_surv_edges + ne][0] = mixed_vert_map(new_edge_data[ne][0]);
    dst_edges[n_surv_edges + ne][1] = mixed_vert_map(new_edge_data[ne][1]);
  }

  /* Face offsets. */
  offset_indices::gather_selected_offsets(
      src_faces, src_survive_faces, dst_face_offsets.take_front(src_survive_faces.size() + 1));
  for (const int nf : IndexRange(n_new_faces)) {
    dst_face_offsets[n_surv_faces + nf] = n_surv_corners + new_face_offs[nf];
  }
  /* Sentinel at the end (Blender stores offsets as face_offsets[face_num] = corners_num). */
  if (!dst_face_offsets.is_empty()) {
    dst_face_offsets[n_surv_faces + n_new_faces] = n_surv_corners + new_face_offs[n_new_faces];
  }

  const OffsetIndices<int> dst_faces(dst_face_offsets);

  /* Corner data. */
  src_survive_faces.foreach_index(
      [&](const int64_t src_f, const int64_t dst_f) {
        const IndexRange src_face = src_faces[src_f];
        const IndexRange dst_face = dst_faces[dst_f];
        for (const int i : src_face.index_range()) {
          dst_corner_verts[dst_face[i]] = src_vert_map[src_corner_verts[src_face[i]]];
          dst_corner_edges[dst_face[i]] = src_edge_map[src_corner_edges[src_face[i]]];
        }
      },
      exec_mode::grain_size(512));
  for (const int nc : IndexRange(n_new_corners)) {
    dst_corner_verts[n_surv_corners + nc] = mixed_vert_map(new_corner_verts[nc]);
    dst_corner_edges[n_surv_corners + nc] = mixed_edge_map(new_corner_edges[nc]);
  }

  const bke::AttributeAccessor src_attrs = src_mesh.attributes();
  bke::MutableAttributeAccessor dst_attrs = dst->attributes_for_write();
  const VectorSet<StringRefNull> uv_names = src_mesh.uv_map_names();

  const Span<int> vert_src_by_dst = emesh.new_vert_examples();
  const IndexMask verts_new_with_src = array_utils::indices_non_negative(
      vert_src_by_dst.index_range(), vert_src_by_dst, memory);
  const IndexMask verts_new_no_src = verts_new_with_src.complement(vert_src_by_dst.index_range(),
                                                                   memory);

  const Span<int> edge_src_by_dst = emesh.new_edge_examples();
  const IndexMask edges_new_with_src = array_utils::indices_non_negative(
      edge_src_by_dst.index_range(), edge_src_by_dst, memory);
  const IndexMask edges_new_no_src = edges_new_with_src.complement(edge_src_by_dst.index_range(),
                                                                   memory);

  const Span<int> face_src_by_dst = emesh.new_face_examples();
  const IndexMask faces_new_with_src = array_utils::indices_non_negative(
      face_src_by_dst.index_range(), face_src_by_dst, memory);
  const IndexMask face_new_no_src = faces_new_with_src.complement(face_src_by_dst.index_range(),
                                                                  memory);

  const Span<int> corner_src_by_dst = emesh.new_corner_examples();
  const IndexMask corners_new_with_src = array_utils::indices_non_negative(
      corner_src_by_dst.index_range(), corner_src_by_dst, memory);
  const IndexMask corners_new_no_src = corners_new_with_src.complement(
      corner_src_by_dst.index_range(), memory);

  src_attrs.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    if (ELEM(iter.name, "position", ".edge_verts", ".corner_vert", ".corner_edge")) {
      return;
    }
    if (uv_names.contains(iter.name)) {
      return;
    }
    if (attribute_filter.allow_skip(iter.name)) {
      return;
    }
    const GVArray src = *iter.get();
    const CPPType &type = src.type();
    const CommonVArrayInfo src_info = src.common_info();
    if (src_info.type == CommonVArrayInfo::Type::Single) {
      const GPointer value(src.type(), src_info.data);
      dst_attrs.add(iter.name, iter.domain, iter.data_type, bke::AttributeInitValue(value));
      return;
    }
    bke::GSpanAttributeWriter dst = dst_attrs.lookup_or_add_for_write_only_span(
        iter.name, iter.domain, iter.data_type);
    switch (iter.domain) {
      case bke::AttrDomain::Point: {
        GMutableSpan surv_values = dst.span.take_front(n_surv_verts);
        GMutableSpan new_values = dst.span.take_back(n_new_verts);
        array_utils::gather(src, src_survive_verts, surv_values);
        bke::attribute_math::gather(src, vert_src_by_dst, verts_new_with_src, new_values);
        type.fill_assign_indices(type.default_value(), new_values.data(), verts_new_no_src);
        break;
      }
      case bke::AttrDomain::Edge: {
        GMutableSpan surv_values = dst.span.take_front(n_surv_edges);
        GMutableSpan new_values = dst.span.take_back(n_new_edges);
        array_utils::gather(src, src_survive_edges, surv_values);
        bke::attribute_math::gather(src, edge_src_by_dst, edges_new_with_src, new_values);
        type.fill_assign_indices(type.default_value(), new_values.data(), edges_new_no_src);
        break;
      }
      case bke::AttrDomain::Face: {
        GMutableSpan surv_values = dst.span.take_front(n_surv_faces);
        GMutableSpan new_values = dst.span.take_back(n_new_faces);
        array_utils::gather(src, src_survive_faces, surv_values);
        bke::attribute_math::gather(src, face_src_by_dst, faces_new_with_src, new_values);
        type.fill_assign_indices(type.default_value(), new_values.data(), face_new_no_src);
        break;
      }
      case bke::AttrDomain::Corner: {
        const GVArraySpan src_span(src);
        GMutableSpan surv_values = dst.span.take_front(n_surv_corners);
        GMutableSpan new_values = dst.span.take_back(n_new_corners);
        bke::attribute_math::gather_group_to_group(
            src_faces, dst_faces, src_survive_faces, src_span, surv_values);
        bke::attribute_math::gather(src_span, corner_src_by_dst, corners_new_with_src, new_values);
        type.fill_assign_indices(type.default_value(), new_values.data(), corners_new_no_src);
        break;
      }
      default:
        BLI_assert_unreachable();
        break;
    }

    dst.finish();
  });

  /* Apply per-edge seam/sharp overrides, matching BMesh's post-copy fixup
   * that undoes seam/smooth on corner segments where those flags are not contiguous.
   * Only apply if the attribute already exists (created by gather_attributes above). */

  if (bke::SpanAttributeWriter<bool> seam_writer = dst_attrs.lookup_for_write_span<bool>(
          "uv_seam"))
  {
    MutableSpan<bool> seam_span = seam_writer.span;
    for (const int ni : IndexRange(n_new_edges)) {
      const int8_t ov = emesh.new_edge_seam_overrides()[ni];
      if (ov >= 0) {
        seam_span[n_surv_edges + ni] = (ov == 1);
      }
    }
    seam_writer.finish();
  }

  if (bke::SpanAttributeWriter<bool> sharp_writer = dst_attrs.lookup_for_write_span<bool>(
          "sharp_edge"))
  {
    MutableSpan<bool> sharp_span = sharp_writer.span;
    for (const int ni : IndexRange(n_new_edges)) {
      const int8_t ov = emesh.new_edge_sharp_overrides()[ni];
      if (ov >= 0) {
        sharp_span[n_surv_edges + ni] = (ov == 1);
      }
    }
    sharp_writer.finish();
  }

  /* Write precomputed UV values for new corners into the output mesh. */
  for (const int i : state.uv_layer_info.uv_maps.index_range()) {
    const StringRef name = state.uv_layer_info.uv_maps[i].name;
    const VArraySpan src_uvs = *src_attrs.lookup<float2>(name, bke::AttrDomain::Corner);
    bke::SpanAttributeWriter dst_uvs = dst_attrs.lookup_or_add_for_write_only_span<float2>(
        name, bke::AttrDomain::Corner);
    if (!dst_uvs) {
      continue;
    }
    bke::attribute_math::gather_group_to_group(
        src_faces, dst_faces, src_survive_faces, src_uvs, dst_uvs.span.take_front(n_surv_corners));
    array_utils::copy(emesh.new_corner_uvs(i), dst_uvs.span.take_back(n_new_corners));
    dst_uvs.finish();
  }

  /* Write #BevelAttributeOutputs fields into the output mesh.
   *
   * Each output is guarded by the optional attribute ID: if the GN output socket is not
   * connected, `get_output_anonymous_attribute_id_if_needed` returns `std::nullopt` and no
   * memory is allocated or written here.
   *
   * - vertex_face_id : Face domain — true for every new face tagged VERTEX_FACE.
   * - edge_face_id   : Face domain — true for every new face tagged EDGE_FACE.
   * - outer_edge_id  : Edge domain — true for the two outermost edges of each bevel strip.
   * - mid_edge_id    : Edge domain — true for the mid-ring edge of each strip (nseg >= 2 only). */
  const BevelAttributeOutputs &ao = state.params.attribute_outputs;

  if (ao.vertex_face_id || ao.edge_face_id) {
    const Span<NewFaceKind> face_kinds = emesh.new_face_kinds();
    if (ao.vertex_face_id) {
      bke::SpanAttributeWriter<bool> writer = dst_attrs.lookup_or_add_for_write_span<bool>(
          *ao.vertex_face_id, bke::AttrDomain::Face);
      for (const int nf : IndexRange(n_new_faces)) {
        writer.span[n_surv_faces + nf] = (face_kinds[nf] == NewFaceKind::VertexFace);
      }
      writer.finish();
    }
    if (ao.edge_face_id) {
      bke::SpanAttributeWriter<bool> writer = dst_attrs.lookup_or_add_for_write_span<bool>(
          *ao.edge_face_id, bke::AttrDomain::Face);
      for (const int nf : IndexRange(n_new_faces)) {
        writer.span[n_surv_faces + nf] = (face_kinds[nf] == NewFaceKind::EdgeFace);
      }
      writer.finish();
    }
  }

  if (ao.outer_edge_id) {
    bke::SpanAttributeWriter<bool> writer = dst_attrs.lookup_or_add_for_write_span<bool>(
        *ao.outer_edge_id, bke::AttrDomain::Edge);
    /* Outer edges are surviving original edges, identified via src_edge_map. */
    for (const int se : state.outer_edge_src_indices) {
      const int dst_e = src_edge_map[se];
      if (dst_e >= 0) {
        writer.span[dst_e] = true;
      }
    }
    /* Also check new edges tagged as OUTER_EDGE (can happen when the outer edge is new). */
    const Span<NewEdgeKind> edge_kinds = emesh.new_edge_kinds();
    for (const int ne : IndexRange(n_new_edges)) {
      if (edge_kinds[ne] == NewEdgeKind::OuterEdge) {
        writer.span[n_surv_edges + ne] = true;
      }
    }
    writer.finish();
  }

  if (ao.mid_edge_id) {
    bke::SpanAttributeWriter<bool> writer = dst_attrs.lookup_or_add_for_write_span<bool>(
        *ao.mid_edge_id, bke::AttrDomain::Edge);
    const Span<NewEdgeKind> edge_kinds = emesh.new_edge_kinds();
    for (const int ne : IndexRange(n_new_edges)) {
      if (edge_kinds[ne] == NewEdgeKind::MidEdge) {
        writer.span[n_surv_edges + ne] = true;
      }
    }
    writer.finish();
  }

  BLI_assert(bke::mesh_is_valid(*dst));
  return dst;
}

/** \} */

}  // namespace construct

std::optional<Mesh *> mesh_bevel(const Mesh &src_mesh,
                                 const IndexMask &selection,
                                 const BevelParameters &params,
                                 const bke::AttributeFilter &attribute_filter)
{
  auto all_non_positive = [](const Span<float> span) {
    return std::ranges::all_of(span, [](float value) { return value <= 0.0f; });
  };
  if (all_non_positive(params.offsets[0]) && all_non_positive(params.offsets[1]) &&
      all_non_positive(params.offsets[2]) && all_non_positive(params.offsets[3]))
  {
    return std::nullopt;
  }

#ifdef DEBUG_TIME
  fmt::println("BEVEL NODE starts");
  const timeit::TimePoint start_time = timeit::Clock::now();
#endif
  BevelState state(src_mesh, params, selection);
  if (state.bevel_affected_vertices.size() == 0) {
    return std::nullopt;
  }
  state.initialize_profile_data();
  state.uv_init();

  state.bev_verts.reserve(state.bevel_affected_vertices.size());
#ifdef DEBUG_TIME
  const timeit::TimePoint init_time = timeit::Clock::now();
  fmt::println("BEVEL NODE initialization, {:.4} ms", (init_time - start_time).count() / 1.0e6f);
#endif

  /* Phase 1: construct BevVerts and build initial boundaries. */
  state.bevel_affected_vertices.foreach_index([&](const int v) {
    construct::bevel_vert_construct(state, v);
    BevVert *bv = state.vert_hash.lookup_default(v, nullptr);
    if (bv) {
      construct::build_boundary(state, bv, true);
    }
  });

#ifdef DEBUG_TIME
  const timeit::TimePoint vert_and_boundaries_time = timeit::Clock::now();
  fmt::println("BEVEL NODE bevel construct and build boundaries,{:.4} ms",
               (vert_and_boundaries_time - init_time).count() / 1.0e6f);
#endif

  /* Maintain consistent orientations for the asymmetrical custom profiles. */
  if (!params.custom_profile_samples.is_empty() && params.affect_type != BevelAffect::Vertices) {
    state.selection.foreach_index(
        [&](const int e) { construct::regularize_profile_orientation(state, e); });
  }

#ifdef DEBUG_TIME
  const timeit::TimePoint adjust_time = timeit::Clock::now();
  fmt::println("BEVEL NODE adjust offsets, {:.4} ms",
               (adjust_time - vert_and_boundaries_time).count() / 1.0e6f);
#endif

  /* Phase 3: UV connectivity and vmesh construction (depends on final BoundVert positions). */
  state.bevel_affected_vertices.foreach_index([&](const int v) {
    BevVert *bv = state.vert_hash.lookup_default(v, nullptr);
    if (!bv) {
      return;
    }
    construct::determine_uv_vert_connectivity(state, v);
    /* Record the number of new faces before build_vmesh so that all faces it creates can
     * be tagged as #NewFaceKind::VERTEX_FACE for the #BevelAttributeOutputs output field. */
    const int faces_before = state.emesh.new_faces_num();
    construct::build_vmesh(state, bv);
    const int faces_after = state.emesh.new_faces_num();
    MutableSpan<NewFaceKind> kinds = state.emesh.new_face_kinds();
    for (int nf = faces_before; nf < faces_after; nf++) {
      kinds[nf] = NewFaceKind::VertexFace;
    }
  });

#ifdef DEBUG_TIME
  const timeit::TimePoint vmesh_and_uv_time = timeit::Clock::now();
  fmt::println("BEVEL NODE vmesh and uv connectivity, {:.4} ms",
               (vmesh_and_uv_time - vert_and_boundaries_time).count() / 1.0e6f);
#endif

  /* Build edge-strip polygons along each beveled edge. */
  if (params.affect_type != BevelAffect::Vertices) {
    state.selection.foreach_index(
        [&](const int e) { construct::bevel_build_edge_polygons(state, e); });
  }

#ifdef DEBUG_TIME
  const timeit::TimePoint edge_time = timeit::Clock::now();
  fmt::println("BEVEL NODE edge mesh, {:.4} ms", (edge_time - vmesh_and_uv_time).count() / 1.0e6f);
#endif

  /* Rebuild original faces that touch beveled vertices. */
  Vector<int> rebuilt_orig_faces;
  int rebuilt_face_0 = -1;
  construct::bevel_rebuild_existing_polygons(state, rebuilt_orig_faces, rebuilt_face_0);

#ifdef DEBUG_TIME
  const timeit::TimePoint face_time = timeit::Clock::now();
  fmt::println("BEVEL NODE face mesh, {:.4} ms", (face_time - edge_time).count() / 1.0e6f);
#endif

  /* Kill the original faces that were rebuilt, mirroring BMesh's deferred kill pattern. */
  for (const int f : rebuilt_orig_faces) {
    state.emesh.face_kill(f);
  }

  /* Kill original beveled vertices. */
  state.bevel_affected_vertices.foreach_index([&](const int v) { state.emesh.vert_kill(v); });

  /* Interpolate UV values for new corners, then merge at seam vertices. */
  if (state.uv_layer_info.has_uv_maps) {
    construct::fill_new_corner_uvs(state);
#ifdef BEVEL_DEBUG
    {
      /* After fill, before merge: dump per-corner UV values for all new faces. */
      const OffsetIndices new_faces(state.emesh.new_face_offsets());
      const Span<int> ncv = state.emesh.new_corner_verts();
      for (int nf = 0; nf < state.emesh.new_faces_num(); nf++) {
        const IndexRange nc_range = new_faces[nf];
        fmt::println("new_face[{}] (corners {}-{}):",
                     nf + state.emesh.mesh.faces_num,
                     nc_range.start(),
                     nc_range.last());
        for (int uv_i = 0; uv_i < int(state.uv_layer_info.layers.size()); uv_i++) {
          const Span<float2> new_uv = state.emesh.new_corner_uvs(uv_i);
          for (const int nc : nc_range) {
            fmt::println("  uv_i={} corner={} v={} uv=({:.5f},{:.5f})",
                         uv_i,
                         nc,
                         ncv[nc],
                         new_uv[nc].x,
                         new_uv[nc].y);
          }
        }
      }
    }
#endif
#ifndef BEVEL_DEBUG_SKIP_MERGE_UVS
    construct::merge_uvs(state);
#else
    fmt::println("merge_uvs SKIPPED (BEVEL_DEBUG_SKIP_MERGE_UVS)");
#endif
  }

  construct::bevel_extend_edge_data(state);

#ifdef DEBUG_TIME
  const timeit::TimePoint uv_edge_data_time = timeit::Clock::now();
  fmt::println("BEVEL NODE uvs and edge data, {:.4} ms",
               (uv_edge_data_time - face_time).count() / 1.0e6f);
#endif

  std::optional<Mesh *> ans = construct::build_output_mesh(state, attribute_filter);

#ifdef DEBUG_TIME
  const timeit::TimePoint end_time = timeit::Clock::now();
  fmt::println("BEVEL NODE construct output mesh, {:.4} ms",
               (end_time - uv_edge_data_time).count() / 1.0e6f);
  fmt::println("BEVEL NODE total, {:5} ms", (end_time - start_time).count() / 1.0e6f);
#endif

  return ans;
}

}  // namespace blender::geometry
