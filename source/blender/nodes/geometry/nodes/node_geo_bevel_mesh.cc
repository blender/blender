/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"

#include "BLI_array.hh"
#include "BLI_hash.hh"
#include "BLI_map.hh"
#include "BLI_math_vector.hh"
#include "BLI_mesh_inset.hh"
#include "BLI_set.hh"
#include "BLI_sort.hh"
#include "BLI_task.hh"
#include "BLI_timeit.hh"
#include "BLI_vector.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

#include <algorithm>

namespace blender::nodes::node_geo_bevel_mesh_cc {

NODE_STORAGE_FUNCS(NodeGeometryBevelMesh)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh").supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).supports_field().hide_value();
  b.add_input<decl::Float>(N_("Amount")).default_value(1.0f).supports_field();
  b.add_input<decl::Float>(N_("Slope"))
      .default_value(0.0f)
      .supports_field()
      .description(N_("Face inset will raise up with this slope"))
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_BEVEL_MESH_FACES; });
  b.add_input<decl::Bool>(N_("Use Regions"))
      .default_value(false)
      .description(N_("Combine adjacent faces into regions and inset regions as a whole"))
      .make_available([](bNode &node) { node_storage(node).mode = GEO_NODE_BEVEL_MESH_FACES; });
  b.add_output<decl::Geometry>("Mesh").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryBevelMesh *data = MEM_cnew<NodeGeometryBevelMesh>(__func__);
  data->mode = GEO_NODE_BEVEL_MESH_EDGES;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometryBevelMesh &storage = node_storage(*node);
  bNodeSocket *slope_socket = nodeFindSocket(node, SOCK_IN, "Slope");
  nodeSetSocketAvailability(ntree, slope_socket, storage.mode == GEO_NODE_BEVEL_MESH_FACES);
  bNodeSocket *use_regions_socket = nodeFindSocket(node, SOCK_IN, "Use Regions");
  nodeSetSocketAvailability(ntree, use_regions_socket, storage.mode == GEO_NODE_BEVEL_MESH_FACES);
}

/** MeshTopology encapsulates data needed to answer topological queries about a mesh,
 * such as "which edges are adjacent to a given vertex?".
 * While Mesh uses the term 'poly' for polygon, most of Blender uses the term 'face',
 * so we'll go with 'face' in this code except in the final to/from mesh routines.
 * This structure will also give some basic access to information about the Mesh elements
 * themselves, in order to keep open the possibility that this code could be adapted
 * for use with BMesh at some point in the future.
 */
class MeshTopology {
  MeshElemMap *vert_edge_map_;
  int *vert_edge_map_mem_;
  MeshElemMap *edge_poly_map_;
  int *edge_poly_map_mem_;
  const Mesh &mesh_;

 public:
  MeshTopology(const Mesh &mesh);
  ~MeshTopology();

  /* Edges adjacent to vertex v. */
  Span<int> vert_edges(int v) const
  {
    const MeshElemMap &m = vert_edge_map_[v];
    return Span<int>{m.indices, m.count};
  }

  /* Faces adjacent to edge e. */
  Span<int> edge_faces(int e) const
  {
    const MeshElemMap &m = edge_poly_map_[e];
    return Span<int>{m.indices, m.count};
  }

  /* Does edge e have exactly two adjacent faces? */
  bool edge_is_manifold(int e) const
  {
    return edge_poly_map_[e].count == 2;
  }

  /* What is the other manifold face (i.e., not f) attached to edge e?
   * Edge e must be manifold and f must be one of the incident faces. */
  int edge_other_manifold_face(int e, int f) const;

  /* What is the other edge of f (i.e., not e) attached to vertex v.
   * Face f must contain e, and e must have v as one of its vertices. */
  int face_other_edge_at_vert(int f, int v, int e) const;

  /* Is edge e1 the successor of e0 when going around face f? */
  bool edge_is_successor_in_face(int e0, int e1, int f) const;

  int num_verts() const
  {
    return mesh_.totvert;
  }
  int num_edges() const
  {
    return mesh_.totedge;
  }
  int num_faces() const
  {
    return mesh_.totpoly;
  }

  blender::float3 vert_co(int v) const
  {
    return mesh_.vert_positions()[v];
  }

  int edge_v1(int e) const
  {
    return mesh_.edges()[e].v1;
  }

  int edge_v2(int e) const
  {
    return mesh_.edges()[e].v2;
  }

  float3 edge_dir_from_vert(int e, int v) const;
  float3 edge_dir_from_vert_normalized(int e, int v) const;
};

MeshTopology::MeshTopology(const Mesh &mesh) : mesh_(mesh)
{
  // timeit::ScopedTimer t("MeshTopology construction");
  BKE_mesh_vert_edge_map_create(
      &vert_edge_map_, &vert_edge_map_mem_, BKE_mesh_edges(&mesh), mesh.totvert, mesh.totedge);
  BKE_mesh_edge_poly_map_create(&edge_poly_map_,
                                &edge_poly_map_mem_,
                                mesh.totedge,
                                BKE_mesh_polys(&mesh),
                                mesh.totpoly,
                                BKE_mesh_loops(&mesh),
                                mesh.totloop);
}

MeshTopology::~MeshTopology()
{
  MEM_freeN(vert_edge_map_);
  MEM_freeN(vert_edge_map_mem_);
  MEM_freeN(edge_poly_map_);
  MEM_freeN(edge_poly_map_mem_);
}

int MeshTopology::edge_other_manifold_face(int e, int f) const
{
  const MeshElemMap &m = edge_poly_map_[e];
  BLI_assert(m.count == 2);
  if (m.indices[0] == f) {
    return m.indices[1];
  }
  BLI_assert(m.indices[1] == f);
  return m.indices[0];
}

int MeshTopology::face_other_edge_at_vert(int f, int v, int e) const
{
  const MPoly &mpoly = mesh_.polys()[f];
  Span<MLoop> loops = mesh_.loops();
  const int loopstart = mpoly.loopstart;
  const int loopend = mpoly.loopstart + mpoly.totloop - 1;
  for (int l = loopstart; l <= loopend; l++) {
    const MLoop &mloop = loops[l];
    if (mloop.e == e) {
      if (mloop.v == v) {
        /* The other edge with vertex v is the preceding (incoming) edge. */
        const MLoop &mloop_prev = l == loopstart ? loops[loopend] : loops[l - 1];
        return mloop_prev.e;
      }
      else {
        /* The other edge with vertex v is the next (outgoing) edge, which should have vertex v. */
        const MLoop &mloop_next = l == loopend ? loops[loopstart] : loops[l + 1];
        BLI_assert(mloop_next.v == v);
        return mloop_next.e;
      }
    }
  }
  /* If didn't return in the loop, then there is no edge e with vertex v in face f. */
  BLI_assert_unreachable();
  return -1;
}

bool MeshTopology::edge_is_successor_in_face(const int e0, const int e1, const int f) const
{
  const MPoly &mpoly = mesh_.polys()[f];
  const int loopstart = mpoly.loopstart;
  const int loopend = mpoly.loopstart + mpoly.totloop - 1;
  Span<MLoop> loops = mesh_.loops();
  for (int l = loopstart; l <= loopend; l++) {
    const MLoop &mloop = loops[l];
    if (mloop.e == e0) {
      const MLoop &mloop_next = l == loopend ? loops[loopstart] : loops[l + 1];
      return mloop_next.e == e1;
    }
  }
  return false;
}

float3 MeshTopology::edge_dir_from_vert(int e, int v) const
{
  const MEdge &medge = mesh_.edges()[e];
  if (medge.v1 == v) {
    return vert_co(medge.v2) - vert_co(medge.v1);
  }
  else {
    BLI_assert(medge.v2 == v);
    return vert_co(medge.v1) - vert_co(medge.v2);
  }
}

float3 MeshTopology::edge_dir_from_vert_normalized(int e, int v) const
{
  return math::normalize(edge_dir_from_vert(e, v));
}

/** Canon
 * CanonVertPair is a pair of vertex indices in canonical order (first index <= second index).
 * This is suitable for a key to look up edges by.
 */
class CanonVertPair {
 public:
  int v1;
  int v2;

  CanonVertPair(int a, int b)
  {
    if (a < b) {
      v1 = a;
      v2 = b;
    }
    else {
      v1 = b;
      v2 = a;
    }
  }

  uint64_t hash() const
  {
    return get_default_hash_2(v1, v2);
  }

  friend bool operator==(const CanonVertPair a, const CanonVertPair b);
};

bool operator==(const CanonVertPair a, const CanonVertPair b){
  return a.v1 == b.v1 && a.v2 == b.v2;
}

/** IndexAlloc allocates sequential integers, starting from a given start value. */
class IndexAlloc {
  int start_;
  int first_free_;

 public:
  IndexAlloc(int start) : start_(start), first_free_(start)
  {
  }

  int alloc()
  {
    return first_free_++;
  }
  int start() const
  {
    return start_;
  }
  int allocated_size() const
  {
    return first_free_ - start_;
  }
};

/** MeshDelta represents a delta to a Mesh: additions and deletions
 * of Mesh elements.
 * New elements will get index numbers starting at the end of the current
 * range of the elements in the base mesh, `mesh_`.
 * There is also a fast method for finding an edge, if any, joining two
 * vertices (either in the base mesh, or in the added vertices, or mixed).
 */
class MeshDelta {
  const Mesh &mesh_;
  const MeshTopology &topo_;
  IndexAlloc vert_alloc_;
  IndexAlloc edge_alloc_;
  IndexAlloc poly_alloc_;
  IndexAlloc loop_alloc_;
  Set<int> vert_deletes_;
  Set<int> edge_deletes_;
  Set<int> poly_deletes_;
  Set<int> loop_deletes_;
  Vector<float3> new_vert_positions_;
  Vector<MEdge> new_edges_;
  Vector<MPoly> new_polys_;
  Vector<MLoop> new_loops_;
  Vector<int> new_vert_rep_;
  Vector<int> new_edge_rep_;
  Vector<int> new_poly_rep_;
  Vector<int> new_loop_rep_;
  /* Lookup map for added edges. */
  Map<CanonVertPair, int> new_edge_map_;

 public:
  MeshDelta(const Mesh &mesh, const MeshTopology &topo);

  /* In the following, `rep` is the index of the old mesh element to base attributes on.  */
  int new_vert(const float3 &co, int rep);
  int new_edge(int v1, int v2, int rep);
  int new_loop(int v, int e, int rep);
  int new_face(int loopstart, int totloop, int rep);
  /* A new face from a span of vertex indices and edge indices (-1 means: make a new edge). */
  int new_face(Span<int> verts, Span<int> edges, int rep);
  /* Find the edge either in mesh or new_edges_, or make a new one if not there. */
  int find_or_add_edge(int v1, int v2, int rep);

  void delete_vert(int v)
  {
    vert_deletes_.add(v);
  }
  void delete_edge(int e)
  {
    edge_deletes_.add(e);
  }
  void delete_face(int f);

  void get_edge_verts(int edge, int *r_v1, int *r_v2)
  {
    const MEdge &medge = new_edges_[edge - edge_alloc_.start()];
    *r_v1 = medge.v1;
    *r_v2 = medge.v2;
  }

  /* Return a new Mesh, the result of applying delta to mesh_. */
  Mesh *apply_delta_to_mesh(GeometrySet &geometry_set, const MeshComponent &in_component, const AnonymousAttributePropagationInfo &propagation_info);

  void print(const std::string &label) const;
};

MeshDelta::MeshDelta(const Mesh &mesh, const MeshTopology &topo)
    : mesh_(mesh),
      topo_(topo),
      vert_alloc_(mesh_.totvert),
      edge_alloc_(mesh_.totedge),
      poly_alloc_(mesh_.totpoly),
      loop_alloc_(mesh_.totloop)
{
}

int MeshDelta::new_vert(const float3 &co, int rep)
{
  int v = vert_alloc_.alloc();
  new_vert_positions_.append(co);
  new_vert_rep_.append(rep);
  return v;
}

int MeshDelta::new_edge(int v1, int v2, int rep)
{
  int e = edge_alloc_.alloc();
  MEdge medge;
  medge.v1 = v1;
  medge.v2 = v2;
  new_edges_.append(medge);
  new_edge_rep_.append(rep);
  new_edge_map_.add_new(CanonVertPair(v1, v2), e);
  return e;
}

int MeshDelta::find_or_add_edge(int v1, int v2, int rep)
{
  /* First see if (v1, v2) is an existing edge in the base mesh. */
  if (v1 < mesh_.totvert && v2 < mesh_.totvert) {
    for (int i : topo_.vert_edges(v1)) {
      const MEdge &e = mesh_.edges()[i];
      if ((e.v1 == v1 && e.v2 == v2) || (e.v1 == v2 && e.v2 == v1)) {
        return i;
      }
    }
  }
  /* Look inside the new edge map to see if it is there. */
  int e = new_edge_map_.lookup_default(CanonVertPair(v1, v2), -1);
  if (e != -1) {
    return e;
  }
  return new_edge(v1, v2, rep);
}

int MeshDelta::new_loop(int v, int e, int rep)
{
  int l = loop_alloc_.alloc();
  MLoop mloop;
  mloop.v = v;
  mloop.e = e;
  new_loops_.append(mloop);
  new_loop_rep_.append(rep);
  return l;
}

int MeshDelta::new_face(int loopstart, int totloop, int rep)
{
  int f = poly_alloc_.alloc();
  MPoly mpoly;
  mpoly.loopstart = loopstart;
  mpoly.totloop = totloop;
  new_polys_.append(mpoly);
  new_poly_rep_.append(rep);
  return f;
}

int MeshDelta::new_face(Span<int> verts, Span<int> edges, int rep)
{
  const int n = verts.size();
  int lfirst = -1;
  for (const int i : IndexRange(n)) {
    const int v1 = verts[i];
    int e = -1;
    if (edges.size() != 0) {
      e = edges[i];
    }
    if (e == -1) {
      const int v2 = verts[(i + 1) % n];
      e = new_edge(v1, v2, -1);
    }
    int l = new_loop(v1, e, -1);
    if (i == 0) {
      lfirst = l;
    }
  }
  return new_face(lfirst, n, rep);
}

/** Delete the MPoly and the loops.
 * The edges and vertices need to be deleted elsewhere, if necessary.
 */
void MeshDelta::delete_face(int f)
{
  poly_deletes_.add(f);
  BLI_assert(f >= 0 && f < mesh_.totpoly);
  const MPoly &mpoly = mesh_.polys()[f];
  for (int l = mpoly.loopstart; l < mpoly.loopstart + mpoly.totloop; l++) {
    loop_deletes_.add(l);
  }
}

#if 1
/* For debugging. */
static std::ostream &operator<<(std::ostream &os, const Mesh *mesh)
{
  os << "Mesh, totvert=" << mesh->totvert << " totedge=" << mesh->totedge
     << " totpoly=" << mesh->totpoly << " totloop=" << mesh->totloop << "\n";
  for (int v : IndexRange(mesh->totvert)) {
    os << "v" << v << " at (" << mesh->vert_positions()[v][0] << "," << mesh->vert_positions()[v][1] << ","
       << mesh->vert_positions()[v][2] << ")\n";
  }
  for (int e : IndexRange(mesh->totedge)) {
    os << "e" << e << " = (v" << mesh->edges()[e].v1 << ", v" << mesh->edges()[e].v2 << ")\n";
  }
  for (int p : IndexRange(mesh->totpoly)) {
    os << "p" << p << " at loopstart l" << mesh->polys()[p].loopstart << " with "
       << mesh->polys()[p].totloop << " loops\n";
  }
  for (int l : IndexRange(mesh->totloop)) {
    os << "l" << l << " = (v" << mesh->loops()[l].v << ", e" << mesh->loops()[l].e << ")\n";
  }
  return os;
}
#endif

/** Initialze a vector keeps of ints in [0,total) that are not in the deletes set. */
static void init_keeps_from_delete_set(Vector<int> &keeps, const Set<int> &deletes, int total)
{
  keeps.reserve(total);
  for (int i : IndexRange(total)) {
    if (!deletes.contains(i)) {
      keeps.append(i);
    }
  }
}

/** Create a map from old indices to new indices, given that only the elemnts
 * in `keeps` will be kept, and moved into a contigouus range.
 * Old indices that don't get kept yield a map value of -1.
 */
static void init_map_from_keeps(Array<int> &map, const Vector<int> &keeps)
{
  map.fill(-1);
  for (int i : keeps.index_range()) {
    map[keeps[i]] = i;
  }
}

/** Copy the vertices whose indices are in `src_verts_map` from `src` to a continous range in
 * `dst`. The `src_verts_map` takes old vertex indices to new ones.
 */
static void copy_vert_positions_based_on_map(Span<float3> src,
                                       MutableSpan<float3> dst,
                                       Span<int> src_verts_map)
{
  for (const int src_v_index : src_verts_map.index_range()) {
    const int i_dst = src_v_index;
    const int i_src = src_verts_map[src_v_index];
    dst[i_dst] = src[i_src];
  }
}

/** Copy from `src` to `dst`. */
static void copy_vert_positions(Span<float3> src, MutableSpan<float3> dst)
{
  for (const int i : src.index_range()) {
    dst[i] = src[i];
  }
}

/** Copy the edges whose indices are in `src_edges_map` from `src` to a continous range in
 * `dst`. While doing so, use `vertex_map` to map the vertex indices within the edges.
 */
static void copy_mapped_edges_based_on_map(Span<MEdge> src,
                                           MutableSpan<MEdge> dst,
                                           Span<int> src_edges_map,
                                           Span<int> vertex_map)
{
  for (const int src_e_index : src_edges_map.index_range()) {
    const int i_dst = src_e_index;
    const int i_src = src_edges_map[src_e_index];
    const MEdge &e_src = src[i_src];
    MEdge &e_dst = dst[i_dst];

    e_dst = e_src;
    e_dst.v1 = vertex_map[e_src.v1];
    e_dst.v2 = vertex_map[e_src.v2];
    BLI_assert(e_dst.v1 != -1 && e_dst.v2 != -1);
  }
}

/** Copy the edges from `src` to `dst`, mapping the vertex indices in those
 * edges using the `vmapfn` function.
 */
static void copy_mapped_edges(Span<MEdge> src,
                              MutableSpan<MEdge> dst,
                              std::function<int(int)> vmapfn)
{
  for (const int i : src.index_range()) {
    const MEdge &e_src = src[i];
    MEdge &e_dst = dst[i];

    e_dst = e_src;
    e_dst.v1 = vmapfn(e_src.v1);
    e_dst.v2 = vmapfn(e_src.v2);
    BLI_assert(e_dst.v1 != -1 && e_dst.v2 != -1);
  }
}

/** Copy the loops whose indices are in `src_loops_map` from `src` to a continous range in
 * `dst`. While doing so, use `vertex_map` to map the vertex indices within the loops,
 * and `edge_map` to map the edge indices within the loops.
 */
static void copy_mapped_loops_based_on_map(Span<MLoop> src,
                                           MutableSpan<MLoop> dst,
                                           Span<int> src_loops_map,
                                           Span<int> vertex_map,
                                           Span<int> edge_map)
{
  for (const int src_l_index : src_loops_map.index_range()) {
    const int i_dst = src_l_index;
    const int i_src = src_loops_map[src_l_index];
    const MLoop &l_src = src[i_src];
    MLoop &l_dst = dst[i_dst];

    l_dst.v = vertex_map[l_src.v];
    l_dst.e = edge_map[l_src.e];
    BLI_assert(l_dst.v != -1 && l_dst.e != -1);
  }
}

/** Copy the loops from `src` to `dst`, mapping the vertex indices in those
 * edges using the `vmapfn` function, and similarly for edge indices using `emapfn`.
 */
static void copy_mapped_loops(Span<MLoop> src,
                              MutableSpan<MLoop> dst,
                              std::function<int(int)> vmapfn,
                              std::function<int(int)> emapfn)
{
  for (const int i : src.index_range()) {
    const MLoop &l_src = src[i];
    MLoop &l_dst = dst[i];

    l_dst.e = emapfn(l_src.e);
    l_dst.v = vmapfn(l_src.v);
    BLI_assert(l_dst.v != -1 && l_dst.e != -1);
  }
}

/** Copy the polys whose indices are in `src_polys_map` from `src` to a continous range in
 * `dst`. While doing so, use `loop_map` to map the loop indices within the polys.
 */
static void copy_mapped_polys_based_on_map(Span<MPoly> src,
                                           MutableSpan<MPoly> dst,
                                           Span<int> src_polys_map,
                                           Span<int> loop_map)
{
  for (const int src_p_index : src_polys_map.index_range()) {
    const int i_dst = src_p_index;
    const int i_src = src_polys_map[src_p_index];
    const MPoly &p_src = src[i_src];
    MPoly &p_dst = dst[i_dst];

    p_dst = p_src;
    p_dst.loopstart = loop_map[p_src.loopstart];
    BLI_assert(p_dst.loopstart != -1);
  }
}

/** Copy the polys from `src` to `dst`, mapping the loop indices in those
 * polys using the `lmapfn` function.
 */
static void copy_mapped_polys(Span<MPoly> src,
                              MutableSpan<MPoly> dst,
                              std::function<int(int)> lmapfn)
{
  for (const int i : src.index_range()) {
    const MPoly &p_src = src[i];
    MPoly &p_dst = dst[i];

    p_dst = p_src;
    p_dst.loopstart = lmapfn(p_src.loopstart);
    BLI_assert(p_dst.loopstart != -1);
  }
}

/* Copy all entries in `data` that have indices that are in `mask` to be contiguous
 * at the beginning of `r_data`.
 */
template<typename T>
static void copy_data_based_on_mask(Span<T> data, MutableSpan<T> r_data, IndexMask mask)
{
  for (const int i_out : mask.index_range()) {
    r_data[i_out] = data[mask[i_out]];
  }
}

template<typename T>
static void copy_data_based_on_map(Span<T> src, MutableSpan<T> dst, Span<int> index_map)
{
  for (const int i_src : index_map.index_range()) {
    const int i_dst = index_map[i_src];
    if (i_dst != -1) {
      dst[i_dst] = src[i_src];
    }
  }
}

/**
 * For each attribute with a domain equal to `domain`, copy the parts of that attribute which lie
 * from the `in_component` as mapped by `mapfn` to `result_component`.
 * If the map result is -1, use the default value for the attribute.
 */
static void copy_attributes_based_on_fn(Map<AttributeIDRef, AttributeKind> &attributes,
                                        const GeometryComponent &in_component,
                                        GeometryComponent &result_component,
                                        const eAttrDomain domain,
                                        std::function<int(int)> mapfn)
{
  const AttributeAccessor src_attributes = *in_component.attributes();
  MutableAttributeAccessor dst_attributes = *result_component.attributes_for_write();

  for (Map<AttributeIDRef, AttributeKind>::Item entry : attributes.items()) {
    const AttributeIDRef attribute_id = entry.key;
    GAttributeReader src_attribute = src_attributes.lookup(attribute_id);
    if (!src_attribute) {
      continue;
    }

    /* Only copy if it is on a domain we want. */
    if (domain != src_attribute.domain) {
      continue;
    }
    const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(
        src_attribute.varray.type());

    GSpanAttributeWriter dst_attribute = dst_attributes.lookup_or_add_for_write_only_span(
        attribute_id, domain, data_type);

    if (!dst_attribute) {
      continue;
    }

    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      VArraySpan<T> span{src_attribute.varray.typed<T>()};
      MutableSpan<T> out_span = dst_attribute.span.typed<T>();
      for (const int i : out_span.index_range()) {
        const int src_i = mapfn(i);
        /* The unmapped entries of `out_span` have been initialized to the default value. */
        if (src_i != -1) {
          out_span[i] = span[src_i];
        }
      }
    });
    dst_attribute.finish();
  }
}

Mesh *MeshDelta::apply_delta_to_mesh(GeometrySet &geometry_set, const MeshComponent &in_component, const AnonymousAttributePropagationInfo &propagation_info)
{
  constexpr int dbglevel = 0;
  if (dbglevel > 0) {
    std::cout << "\nApply delta to mesh\n";
    this->print("final delta");
  }
  /* The keep_... vectors hold the indices of elements in the original mesh to keep. */
  Vector<int> keep_vertices;
  Vector<int> keep_edges;
  Vector<int> keep_polys;
  Vector<int> keep_loops;
  init_keeps_from_delete_set(keep_vertices, vert_deletes_, mesh_.totvert);
  init_keeps_from_delete_set(keep_edges, edge_deletes_, mesh_.totedge);
  init_keeps_from_delete_set(keep_polys, poly_deletes_, mesh_.totpoly);
  init_keeps_from_delete_set(keep_loops, loop_deletes_, mesh_.totloop);

  /* The vertex_map Array says, for vertex v, what index it maps to in the output mesh, with -1
   * if not mapped. Similarly for the other ..._map Arrays.
   */
  Array<int> vertex_map(mesh_.totvert);
  Array<int> edge_map(mesh_.totedge);
  Array<int> poly_map(mesh_.totpoly);
  Array<int> loop_map(mesh_.totloop);
  init_map_from_keeps(vertex_map, keep_vertices);
  init_map_from_keeps(edge_map, keep_edges);
  init_map_from_keeps(poly_map, keep_polys);
  init_map_from_keeps(loop_map, keep_loops);

  Map<AttributeIDRef, AttributeKind> attributes;
  geometry_set.gather_attributes_for_propagation(
      {GEO_COMPONENT_TYPE_MESH}, GEO_COMPONENT_TYPE_MESH, false, propagation_info, attributes);

  int out_totvert = keep_vertices.size() + new_vert_positions_.size();
  int out_totedge = keep_edges.size() + new_edges_.size();
  int out_totpoly = keep_polys.size() + new_polys_.size();
  int out_totloop = keep_loops.size() + new_loops_.size();

  Span<float3> mesh_vert_positions = mesh_.vert_positions();
  Span<MEdge> mesh_edges = mesh_.edges();
  Span<MLoop> mesh_loops = mesh_.loops();
  Span<MPoly> mesh_polys = mesh_.polys();

  Mesh *mesh_out = BKE_mesh_new_nomain_from_template(
      &mesh_, out_totvert, out_totedge, out_totloop, out_totpoly);

  MeshComponent out_component;
  out_component.replace(mesh_out, GeometryOwnershipType::Editable);

  MutableSpan<float3> mesh_out_vert_positions = mesh_out->vert_positions_for_write();
  MutableSpan<MEdge> mesh_out_edges = mesh_out->edges_for_write();
  MutableSpan<MLoop> mesh_out_loops = mesh_out->loops_for_write();
  MutableSpan<MPoly> mesh_out_polys = mesh_out->polys_for_write();

  /* Copy the kept elements to the new mesh, mapping the internal vertex, edge, and loop
   * indices in each of those elements to their new positions.
   */

  copy_vert_positions_based_on_map(mesh_vert_positions, mesh_out_vert_positions, keep_vertices);
  copy_mapped_edges_based_on_map(mesh_edges, mesh_out_edges, keep_edges, vertex_map);
  copy_mapped_loops_based_on_map(mesh_loops, mesh_out_loops, keep_loops, vertex_map, edge_map);
  copy_mapped_polys_based_on_map(mesh_polys, mesh_out_polys, keep_polys, loop_map);

  /* Copy in the added elements, mapping the internal vertex, edge, and loop
   * indices in each of those elements (which may be old elements, now in new positions,
   * or new elements) to their new positions.
   */
  std::function<int(int)> vmapfn = [&](int v) -> int {
    if (v < mesh_.totvert) {
      return vertex_map[v];
    }
    else {
      return v - mesh_.totvert + keep_vertices.size();
    }
  };
  std::function<int(int)> emapfn = [&](int e) -> int {
    if (e < mesh_.totedge) {
      return edge_map[e];
    }
    else {
      return e - mesh_.totedge + keep_edges.size();
    }
  };
  std::function<int(int)> lmapfn = [&](int l) -> int {
    if (l < mesh_.totloop) {
      return loop_map[l];
    }
    else {
      return l - mesh_.totloop + keep_loops.size();
    }
  };

  copy_vert_positions(new_vert_positions_.as_span(), mesh_out_vert_positions.drop_front(keep_vertices.size()));
  copy_mapped_edges(new_edges_.as_span(), mesh_out_edges.drop_front(keep_edges.size()), vmapfn);
  copy_mapped_loops(
      new_loops_.as_span(), mesh_out_loops.drop_front(keep_loops.size()), vmapfn, emapfn);
  copy_mapped_polys(new_polys_.as_span(), mesh_out_polys.drop_front(keep_polys.size()), lmapfn);

  /* Copy attributes, either from mapped kept ones, or mapped representatives.
   * The map function needs to say, for an argument new element index, what old
   * element index is to be used to copy that attribute from. Or -1 to indicate that
   * it is copied from nowhere, but instead should have the default value for that attribute.
   */
  std::function<int(int)> vrepmapfn = [&](int v) -> int {
    if (v < keep_vertices.size()) {
      return keep_vertices[v];
    }
    else {
      return new_vert_rep_[v - keep_vertices.size()];
    }
  };
  std::function<int(int)> erepmapfn = [&](int e) -> int {
    if (e < keep_edges.size()) {
      return keep_edges[e];
    }
    else {
      return new_edge_rep_[e - keep_edges.size()];
    }
  };
  std::function<int(int)> prepmapfn = [&](int p) -> int {
    if (p < keep_polys.size()) {
      return keep_polys[p];
    }
    else {
      return new_poly_rep_[p - keep_polys.size()];
    }
  };
  std::function<int(int)> lrepmapfn = [&](int l) -> int {
    if (l < keep_loops.size()) {
      return keep_loops[l];
    }
    else {
      return new_loop_rep_[l - keep_loops.size()];
    }
  };

  copy_attributes_based_on_fn(
      attributes, in_component, out_component, ATTR_DOMAIN_POINT, vrepmapfn);
  copy_attributes_based_on_fn(
      attributes, in_component, out_component, ATTR_DOMAIN_EDGE, erepmapfn);
  copy_attributes_based_on_fn(
      attributes, in_component, out_component, ATTR_DOMAIN_FACE, prepmapfn);
  copy_attributes_based_on_fn(
      attributes, in_component, out_component, ATTR_DOMAIN_CORNER, lrepmapfn);

  /* Fix coordinates of new vertices. */
  for (const int v : new_vert_positions_.index_range()) {
    mesh_out->vert_positions_for_write()[v + keep_vertices.size()] = new_vert_positions_[v];
  }

  BKE_mesh_runtime_clear_cache(mesh_out);
  if (dbglevel > 0) {
    std::cout << "\nFinal Mesh\n" << mesh_out;
  }
  return mesh_out;
}

void MeshDelta::print(const std::string &label) const
{
  if (label.size() > 0) {
    std::cout << label << " ";
  }
  std::cout << "MeshDelta\n";
  std::cout << "new vertices:\n";
  const int voff = vert_alloc_.start();
  for (int i : new_vert_positions_.index_range()) {
    const float3 &mv = new_vert_positions_[i];
    std::cout << "v" << voff + i << ": (" << mv[0] << "," << mv[1] << "," << mv[2]
              << ")\n";
  }
  std::cout << "new edeges:\n";
  const int eoff = edge_alloc_.start();
  for (int i : new_edges_.index_range()) {
    const MEdge &me = new_edges_[i];
    std::cout << "e" << eoff + i << ": v1=" << me.v1 << " v2=" << me.v2 << "\n";
  }
  std::cout << "new loops:\n";
  const int loff = loop_alloc_.start();
  for (int i : new_loops_.index_range()) {
    const MLoop &ml = new_loops_[i];
    std::cout << "l" << loff + i << ": v=" << ml.v << " e=" << ml.e << "\n";
  }
  std::cout << "new faces:\n";
  const int poff = poly_alloc_.start();
  for (int i : new_polys_.index_range()) {
    const MPoly &mp = new_polys_[i];
    std::cout << "f" << poff + i << "; loopstart=" << mp.loopstart << " totloop=" << mp.totloop
              << "\n";
  }
  /* For deleted sets, go through all and printed deleted ones, in order to get ascending order.
   */
  std::cout << "deleted vertices:\n";
  for (int i : IndexRange(mesh_.totvert)) {
    if (vert_deletes_.contains(i)) {
      std::cout << i << " ";
    }
    if ((i > 0 && (i % 50) == 0)) {
      std::cout << "\n";
    }
  }
  std::cout << "\n";
  std::cout << "deleted edges:\n";
  for (int i : IndexRange(mesh_.totedge)) {
    if (edge_deletes_.contains(i)) {
      std::cout << i << " ";
    }
    if ((i > 0 && (i % 50) == 0)) {
      std::cout << "\n";
    }
  }
  std::cout << "\n";
  std::cout << "deleted faces:\n";
  for (int i : IndexRange(mesh_.totpoly)) {
    if (poly_deletes_.contains(i)) {
      std::cout << i << " ";
    }
    if ((i > 0 && (i % 50) == 0)) {
      std::cout << "\n";
    }
  }
  std::cout << "\n";
  std::cout << "deleted loops:\n";
  for (int i : IndexRange(mesh_.totloop)) {
    if (loop_deletes_.contains(i)) {
      std::cout << i << " ";
    }
    if ((i > 0 && (i % 50) == 0)) {
      std::cout << "\n";
    }
  }
  std::cout << "\n";
}

class VertexCap;

/** A HalfEdge represents one end of a Mesh Edge.
 * There is an VertexCap (see following) that the HalfEdge is
 * connected to, and a position within the ordered edges in
 * that VertexCap, that will be recorded here.
 */
class HalfEdge {
 public:
  /* Index of the edge in the Mesh. */
  int mesh_index{-1};
  /* Index of this HalfEdge in the edges of a VertexCap. */
  int cap_index{-1};
  VertexCap *cap{nullptr};

  HalfEdge()
  {
  }
};

/** A Vertex Cap consists of a vertex in a mesh and an CCW ordering of
 * alternating edges and faces around it, as viewed from the face's
 * normal side. Some faces may be missing (i.e., gaps).
 * (If there are other edges and faces attached to the vertex that
 * don't fit into this pattern, they need to go into other Vertex Caps
 * or ignored, for the sake of beveling.)
 */
class VertexCap {
  /* The HalfEdges in CCW order around the cap. */
  Array<HalfEdge> edges_;
  /* The mesh indices of the faces: ace_[i] is between edges i and i+1; -1 means no face. */
  Array<int> faces_;

 public:
  /* The vertex (as index into a mesh) that the cap is around. */
  int vert;

  VertexCap() : vert(-1)
  {
  }

  /* Initialize for vertex v, given a mesh topo. */
  void init_from_topo(const int vert, const MeshTopology &topo);

  /* The number of edges around the cap. */
  int size() const
  {
    return edges_.size();
  }

  /* Edges in CCW order (viewed from top) around the cap. */
  Span<HalfEdge> edges() const
  {
    return edges_.as_span();
  }

  /* Faces in CCW order (viewed from top) around the cap. -1 means a gap. */
  Span<int> faces() const
  {
    return faces_.as_span();
  }

  /* The ith edge. */
  const HalfEdge &edge(int i) const
  {
    return edges_[i];
  }
  /* The edge after the ith edge (with wraparound). */
  const HalfEdge &next_edge(int i) const
  {
    return i < edges_.size() - 1 ? edges_[i + 1] : edges_[0];
  }
  /* The edge before the ith edge (with wraparound). */
  const HalfEdge &prev_edge(int i) const
  {
    return i > 1 ? edges_[i - 1] : edges_.last();
  }

  /* The face returned may be -1, meaning "gap". */
  /* The face betwen edge(i) and next_edge(i). */
  int face(int i) const
  {
    return faces_[i];
  }
  /* The face between edge(i) and prev_edge(i). */
  int prev_face(int i) const
  {
    return i > 1 ? faces_[i - 1] : faces_.last();
  }
  /* True if there is a gap between edges i and next_edge(i). */
  bool is_gap(int i) const
  {
    return face(i) == -1;
  }

  /* Return the HalfEdge that is for the Mesh edge with index mesh_index. */
  const HalfEdge *half_edge_for_edge(int mesh_index) const;
};

/** Construct and return the VertexCap for vertex `vert`. */
void VertexCap::init_from_topo(const int vert, const MeshTopology &topo)
{
  this->vert = vert;
  Span<int> incident_edges = topo.vert_edges(vert);
  const int num_edges = incident_edges.size();
  if (num_edges == 0) {
    return;
  }

  /* First check for the most common case: a complete manifold cap:
   * That is, each edge is incident on exactly two faces and the
   * edge--face--edge--...--face chain forms a single cycle.
   */
  bool all_edges_manifold = true;
  for (const int e : incident_edges) {
    if (!topo.edge_is_manifold(e)) {
      all_edges_manifold = false;
      break;
    }
  }
  if (all_edges_manifold) {
    bool is_manifold_cap = true;
    Array<int> ordered_edges(num_edges, -1);
    Array<int> ordered_faces(num_edges, -1);
    Set<int, 16> used_edges;
    Set<int, 16> used_faces;

    int next_edge = incident_edges[0];
    for (int slot = 0; slot < num_edges; slot++) {
      /* Invariant: ordered_edges and ordered_faces are filled
       * up to slot-1 with a valid sequence for the cap, and
       * next_edge is a valid continuation edge but we don't
       * yet know if it has already been used.
       */
      ordered_edges[slot] = next_edge;
      used_edges.add_new(next_edge);
      /* Find a face attached to next_edge that is not yet used. */
      int next_face;
      if (slot == 0) {
        next_face = topo.edge_faces(next_edge)[0];
      }
      else {
        const int prev_face = ordered_faces[slot - 1];
        next_face = topo.edge_other_manifold_face(next_edge, prev_face);
      }
      if (used_faces.contains(next_face)) {
        is_manifold_cap = false;
        break;
      }
      ordered_faces[slot] = next_face;
      next_edge = topo.face_other_edge_at_vert(next_face, vert, next_edge);
      if (slot < num_edges - 1 && used_edges.contains(next_edge)) {
        is_manifold_cap = false;
        break;
      }
    }
    is_manifold_cap = is_manifold_cap && next_edge == ordered_edges[0];
    if (is_manifold_cap) {
      /* Check if cap is oriented properly, and fix it if not.
       * A pair of successive edges in ordered_edges should be going CW
       * in the face in between. For now, just check the first pair.
       */
      if (num_edges > 1) {
        if (topo.edge_is_successor_in_face(ordered_edges[0], ordered_edges[1], ordered_faces[0])) {
          /* They are in the wrong orientation, so we need to reverse.
           * To make interleaving of edges and faces work out, reverse only 1..end of edges
           * and reverse all of faces.
           */
          std::reverse(ordered_edges.begin() + 1, ordered_edges.end());
          std::reverse(ordered_faces.begin(), ordered_faces.end());
        }
      }
      edges_.reinitialize(ordered_edges.size());
      for (const int i : ordered_edges.index_range()) {
        HalfEdge &he = edges_[i];
        he.cap = this;
        he.cap_index = i;
        he.mesh_index = ordered_edges[i];
      }
      faces_ = ordered_faces;
      return;
    }
  }
  std::cout << "to implement: VertexCap for non-manifold edges\n";
}

const HalfEdge *VertexCap::half_edge_for_edge(int mesh_index) const
{
  for (const HalfEdge &he : edges_) {
    if (he.mesh_index == mesh_index) {
      return &he;
    }
  }
  return nullptr;
}

static std::ostream &operator<<(std::ostream &os, const HalfEdge &he)
{
  os << "e" << he.mesh_index;
  if (he.cap != nullptr) {
    os << "@(v" << he.cap->vert << " pos " << he.cap_index << ")";
  }
  return os;
}

static std::ostream &operator<<(std::ostream &os, const VertexCap &cap)
{
  os << "cap at v" << cap.vert << ": ";
  for (const int i : cap.edges().index_range()) {
    os << "e" << cap.edge(i).mesh_index << " ";
    if (cap.face(i) == -1) {
      os << "<gap> ";
    }
    else {
      os << "f" << cap.face(i) << " ";
    }
  }
  os << "\n";
  return os;
}

/** A VertexMesh contains the structure of the new mesh around a bevel-involved vertex.
 * It uses vertex and face indices in an implicit MeshDelta.
 * For now, we only need to keep track of the "boundary", the vertices that are on the
 * outside of the vertex mesh -- the ones that edge meshes and other faces will attach to.
 * We also need the edges between those vertices, so that we share them when reconstructing faces.
 */
class VertexMesh {
  /* The MeshDelta vertex indices of the boundary of the mesh. */
  Vector<int> boundary_vert_;
  /* The MeshDelta edge indices of the edge starting at the corresponding boundary vert. */
  Vector<int> boundary_edge_;

 public:
  VertexMesh()
  {
  }

  void append_boundary_vert(int v)
  {
    boundary_vert_.append(v);
  }

  void append_boundary_edge(int e)
  {
    boundary_edge_.append(e);
  }

  int boundary_size() const
  {
    return boundary_vert_.size();
  }

  Span<int> boundary_verts_as_span() const
  {
    return boundary_vert_.as_span();
  }

  int boundary_vert(int i) const
  {
    return boundary_vert_[i];
  }

  int next_boundary_vert(int i) const
  {
    return boundary_vert_[(i + 1) % boundary_vert_.size()];
  }

  int next_boundary_pos(int i) const
  {
    return (i + 1) % boundary_vert_.size();
  }

  int prev_boundary_pos(int i) const
  {
    return (i + boundary_vert_.size() - 1) % boundary_vert_.size();
  }

  Span<int> boundary_edges_as_span() const
  {
    return boundary_edge_.as_span();
  }

  int boundary_edge(int i) const
  {
    return boundary_edge_[i];
  }

  /* Find and return the boundary position containing \a v. Return -1 if not found. */
  int boundary_pos_for_vert(int v) const;
};

int VertexMesh::boundary_pos_for_vert(int v) const
{
  for (int i : boundary_vert_.index_range()) {
    if (boundary_vert_[i] == v) {
      return i;
    }
  }
  return -1;
}

static std::ostream &operator<<(std::ostream &os, const VertexMesh &vm)
{
  os << "Vertex Mesh\nboundary:";
  for (const int v : vm.boundary_verts_as_span()) {
    os << " " << v;
  }
  os << "\n";
  return os;
}

/** A BevelEdge holds the two ends (HalfEdges) of an edge that is to be beveled.
 * The underlying edge has a direction, and he1 is the HalfEdge at the source end,
 * while he2 is the HalfEdge at the destination end.
 */
class BevelEdge {
  /* Index in mesh of the underlying edge. */
  int edge_;

 public:
  /* Source end HalfEdge. */
  HalfEdge *he1;
  /* Destination end HalfEdge. */
  HalfEdge *he2;

  BevelEdge() : edge_(-1), he1(nullptr), he2(nullptr)
  {
  }

  BevelEdge(int edge, HalfEdge *he1, HalfEdge *he2) : edge_(edge), he1(he1), he2(he2)
  {
  }

  int edge() const
  {
    return edge_;
  }
};

/** BevelVertexData holds the data used to bevel around a vertex. */
class BevelVertexData {
  /* The vertex cap for the vertex. */
  VertexCap vertex_cap_;
  /* The vertex mesh for the vertex. */
  VertexMesh vertex_mesh_;
  /* Map from vertex_cap_ edge position to the attachement point on the boundary
   * in vertex_mesh_. If the corresponding edge is beveled, this will be the attachment
   * point for the left side (looking at the vertex) of the edge mesh.
   */
  Array<int> cap_pos_to_boundary_pos_;

 public:
  BevelVertexData()
  {
  }
  ~BevelVertexData()
  {
  }

  /* Initialize the vertex cap data for \a vert. */
  void construct_vertex_cap(int vert, const MeshTopology &topo)
  {
    vertex_cap_.init_from_topo(vert, topo);
  }

  /* Construct the vertex mesh for this vert, for a vertex bevel by \a amount. Assume the vertex
   * cap is already built. */
  void construct_vertex_mesh_for_vertex_bevel(float amount,
                                              const MeshTopology &topo,
                                              MeshDelta &mesh_delta);

  const VertexCap &vertex_cap() const
  {
    return vertex_cap_;
  }

  const VertexMesh &vertex_mesh() const
  {
    return vertex_mesh_;
  }

  const int cap_pos_to_boundary_pos(int cap_pos) const
  {
    return cap_pos_to_boundary_pos_[cap_pos];
  }

  const int beveled_vert() const
  {
    return vertex_cap_.vert;
  }
};

static std::ostream &operator<<(std::ostream &os, const BevelVertexData &bvd)
{
  const VertexCap &vc = bvd.vertex_cap();
  os << "bevel vertex data for vertex " << vc.vert << "\n";
  os << vc;
  os << bvd.vertex_mesh();
  return os;
}

/** Pick a face to be a representative for a beveled vertex. */
static int face_rep_for_beveled_vert(const BevelVertexData &bvd)
{
  /* For now: just pick the first face we find. */
  for (const int f : bvd.vertex_cap().faces()) {
    if (f != -1) {
      return f;
    }
  }
  return -1;
}

/** Construct the vertex mesh for \a vert, for a vertex bevel by \a amount. Assume the vertex cap
 * is already built.
 * TODO: deal with collisions.
 */
void BevelVertexData::construct_vertex_mesh_for_vertex_bevel(float amount,
                                                             const MeshTopology &topo,
                                                             MeshDelta &mesh_delta)
{
  const VertexCap &cap = vertex_cap();
  const int num_edges = cap.size();
  const int vert = vertex_cap().vert;
  const float3 vert_co = topo.vert_co(vertex_cap().vert);
  cap_pos_to_boundary_pos_.reinitialize(num_edges);
  for (const int i : IndexRange(num_edges)) {
    const HalfEdge &he = cap.edge(i);
    /* Set the position of the boundary vertex by sliding at distance `amount` along the edge. */
    float3 dir = topo.edge_dir_from_vert_normalized(he.mesh_index, vert);
    float3 new_co = vert_co + amount * dir;
    int newv = mesh_delta.new_vert(new_co, vert);
    cap_pos_to_boundary_pos_[i] = i;
    vertex_mesh_.append_boundary_vert(newv);
  }
  /* All of the edges are new, so can pass an empty span for the edges. */
  mesh_delta.new_face(
      vertex_mesh_.boundary_verts_as_span(), Span<int>(), face_rep_for_beveled_vert(*this));
}

/** BevelSpec holds the data the specifies what the user wants beveled.
 * There will be a derived class for each type of bevel.
 */
class BevelSpec {
 public:
  /* Are we beveling vertices, edges, or faces? */
  GeometryNodeBevelMeshMode bevel_mode;
  /* A mask over the elements of the beveled type, saying what is to bovel. */
  IndexMask to_bevel;

  BevelSpec(GeometryNodeBevelMeshMode mode, IndexMask to_bevel)
      : bevel_mode(mode), to_bevel(to_bevel)
  {
  }

  virtual void dump_spec() = 0;
};

class VertexBevelSpec : public BevelSpec {
 public:
  /* Indexed by Mesh vertex index, the amount to slide along all edges
   * attached to the vertex. */
  VArray<float> amount;

  VertexBevelSpec(IndexMask to_bevel, VArray<float> amount)
      : BevelSpec{GEO_NODE_BEVEL_MESH_VERTICES, to_bevel}, amount(amount)
  {
  }

  void dump_spec();
};

void VertexBevelSpec::dump_spec()
{
  std::cout << "VertexBevelSpec\n";
  for (const int v : to_bevel.index_range()) {
    if (to_bevel[v]) {
      std::cout << v << ": " << amount[v] << "\n";
    }
  }
}

class FaceBevelSpec : public BevelSpec {
 public:
  /* Indexed by Mesh poly index, the amount to inset the face by. */
  VArray<float> amount;
  /* Indexed by Mesh poly index, the slope to follow when insetting the face. */
  VArray<float> slope;
  bool use_regions;

  FaceBevelSpec(IndexMask to_bevel, VArray<float> amount, VArray<float> slope, bool use_regions)
      : BevelSpec{GEO_NODE_BEVEL_MESH_FACES, to_bevel},
        amount(amount),
        slope(slope),
        use_regions(use_regions)
  {
  }

  void dump_spec();
};

void FaceBevelSpec::dump_spec()
{
  std::cout << "FaceBevelSpec\n";
  if (use_regions) {
    std::cout << "use regions\n";
  }
  for (const int f : to_bevel.index_range()) {
    if (to_bevel[f]) {
      std::cout << f << ": " << amount[f] << ", slope=" << slope[f] << "\n";
    }
  }
}

class EdgeBevelSpec : public BevelSpec {
 public:
  /* Indexed by Mesh edge index, the amounts to bevel the edge.
   * `left_amount[0]` is the left side amount for the source end and
   * `right_amount[0]` is the right side amount for the source end,
   * where "left" and "right" mean: those sides as you look
   * along the edge to the source.
   * Similarly, the 1-indexed elements of those arrays are for the
   * destination end, with left and right as you look towards the dest end.
   */
  VArray<float> left_amount[2];
  VArray<float> right_amount[2];

  EdgeBevelSpec(IndexMask to_bevel, VArray<float> amount)
      : BevelSpec(GEO_NODE_BEVEL_MESH_EDGES, to_bevel),
        left_amount{amount, amount},
        right_amount{amount, amount}
  {
  }

  EdgeBevelSpec(IndexMask to_bevel,
                VArray<float> src_left_amount,
                VArray<float> src_right_amount,
                VArray<float> dst_left_amount,
                VArray<float> dst_right_amount)
      : BevelSpec(GEO_NODE_BEVEL_MESH_EDGES, to_bevel),
        left_amount{src_left_amount, dst_left_amount},
        right_amount{src_right_amount, dst_right_amount}
  {
  }

  void dump_spec();
};

void EdgeBevelSpec::dump_spec()
{
  std::cout << "EdgeBevelSpec\n";
  for (const int e : to_bevel.index_range()) {
    if (to_bevel[e]) {
      std::cout << e << ": ";
      if (left_amount[0] == right_amount[0] && left_amount[0] == left_amount[1] &&
          left_amount[1] == right_amount[1]) {
        std::cout << left_amount[0][e] << "\n";
      }
      else {
        std::cout << "0(" << left_amount[0][e] << ", " << right_amount[0][e] << ") 1("
                  << left_amount[1][e] << ", " << right_amount[1][e] << ")\n";
      }
    }
  }
}

/** BevelData holds the global data needed for a bevel. */
class BevelData {
  /* The specification of the bevel. */
  const BevelSpec *spec_;
  /* The original Mesh. */
  const Mesh *mesh_;
  /* Topology for mesh_. */
  const MeshTopology *topo_;
  /* Will accumulate delta from mesh_ to desired answer. */
  MeshDelta mesh_delta_;
  /* BevelVertexData for just the affected vertices. */
  Array<BevelVertexData> bevel_vert_data_;
  /* A map from mesh vertex index to index in bevel_vert_data_.
   * If we wanted  more speed at expense of space, we could also use
   * an Array of size equal to the number of mesh vertices here.
   */
  Map<int, int> vert_to_bvd_index_;
  /* All the BevelEdges, when edge beveling. */
  Array<BevelEdge> bevel_edge_;
  /* Map from mesh edge indiex inot bevel_edge_. */
  Map<int, int> edge_to_bevel_edge_;

 public:
  BevelData(const Mesh *mesh, const BevelSpec *spec, const MeshTopology *topo)
      : spec_(spec), mesh_(mesh), topo_(topo), mesh_delta_(*mesh, *topo)
  {
  }
  ~BevelData()
  {
  }

  /* Calculate vertex bevels based on spec_, with answer in mesh_delta_. */
  void calculate_vertex_bevel();
  /* Calculate edge bevels based on spec_, with answer in mesh_delta_. */
  void calculate_edge_bevel();
  /* Calculate face bevels based on spec_, with answer in mesh_delta_. */
  void calculate_face_bevel();

  /* Return the Mesh that is the result of applying mesh_delta_ to mesh_. */
  Mesh *get_output_mesh(GeometrySet geometry_set, const MeshComponent &component, const AnonymousAttributePropagationInfo &propagation_info);

 private:
  /* Initial calculation of vertex bevels. */
  void calculate_vertex_bevels(const IndexMask to_bevel, VArray<float> amounts);

  /* Calculation of edge bevels. */
  void calculate_edge_bevels(const IndexMask to_bevel, VArray<float> amounts);

  /* Sets up internal Map for fast access to the BevelVertexData for a given mesh vert. */
  void setup_vert_map();

  /* What is the BevelVertexData for mesh vertex `vert`? May return nullptr if `vert` isn't
   * involved in beveling. */
  BevelVertexData *bevel_vertex_data(int vert)
  {
    int slot = vert_to_bvd_index_.lookup_default(vert, -1);
    if (slot != -1) {
      return &bevel_vert_data_[slot];
    }
    return nullptr;
  }

  /* Return all faces affected by bevel, in a deterministic order.
   * Assume the bevel_vert_data_ has been initialized.
   */
  Array<int> affected_faces() const;

  /* Find and return the input and output vertices in the new mesh that will replace vertex \a v
   * when in a face with incoming edge \a e.
   */
  std::pair<int, int> attach_verts_for_beveled_vert(int v, int e, const BevelVertexData *bvd);

  /* Allocate in mesh_delta_ and return a new MLoop for an edge from v1 to v2. Make the MEdge if
   * needed. */
  int new_loop_for_vert_pair(int v1, int v2);

  /* Allocate in mesh_delta_ new MLoops for all int edges from vi to vo in the boundary given in
   * bvd. If bvd is null, does nothing. */
  std::pair<int, int> new_loops_for_beveled_vert(const BevelVertexData *bvd, int vi, int vo);

  void print(const std::string &label) const;
};

void BevelData::print(const std::string &label) const
{
  if (label.size() > 0) {
    std::cout << label << " ";
  }
  std::cout << "BevelData\n";
  for (const BevelVertexData &bvd : bevel_vert_data_.as_span()) {
    std::cout << bvd;
  }
}

/** Make a transation map from mesh vertex index to indices in bevel_vert_data_. */
void BevelData::setup_vert_map()
{
  vert_to_bvd_index_.reserve(bevel_vert_data_.size());
  for (const int i : bevel_vert_data_.index_range()) {
    vert_to_bvd_index_.add_new(bevel_vert_data_[i].vertex_cap().vert, i);
  }
}

/* Return all faces affected by bevel, in a deterministic order.
 * Assume the bevel_vert_data_ has been initialized.
 */
Array<int> BevelData::affected_faces() const
{
  Set<int> need_face;
  for (const BevelVertexData &bvd : bevel_vert_data_) {
    for (const int f : bvd.vertex_cap().faces()) {
      need_face.add(f);
    }
  }
  int n = need_face.size();
  Array<int> ans(n);
  int i = 0;
  for (const int f : need_face) {
    ans[i++] = f;
  }
  /* Sort the answer to make the result deterministic run-to-run. */
  std::sort(ans.begin(), ans.end());
  return ans;
}

/* Find and return the input and output vertices in the new mesh that will replace vertex \a v
 * when in a face with incoming edge \a e. If \a v is beveled then \a bvd is its BevelVertexData.
 */
std::pair<int, int> BevelData::attach_verts_for_beveled_vert(int v,
                                                             int e,
                                                             const BevelVertexData *bvd)
{
  if (bvd == nullptr) {
    return std::pair<int, int>(v, v);
  }
  const HalfEdge *he = bvd->vertex_cap().half_edge_for_edge(e);
  BLI_assert(he != nullptr);
  const HalfEdge *he_next = &bvd->vertex_cap().next_edge(he->cap_index);
  int vin_pos = bvd->cap_pos_to_boundary_pos(he_next->cap_index);
  int vout_pos = bvd->cap_pos_to_boundary_pos(he->cap_index);
  int vin = bvd->vertex_mesh().boundary_vert(vin_pos);
  int vout = bvd->vertex_mesh().boundary_vert(vout_pos);
  return std::pair<int, int>(vin, vout);
}

/* Allocate in mesh_delta_ new MLoop for an edge from v1 to v2. Make the MEdge if needed. */
int BevelData::new_loop_for_vert_pair(int v1, int v2)
{
  /* TODO: something about representative edges and loops. */
  int e = mesh_delta_.find_or_add_edge(v1, v2, -1);
  return mesh_delta_.new_loop(v1, e, -1);
}

/* Allocate in mesh_delta_ new MLoops for all int edges from vi to vo in the boundary given in bvd.
 * If bvd is null, does nothing.
 * Return a pair of (index of first loop allocate, number allocated).
 */
std::pair<int, int> BevelData::new_loops_for_beveled_vert(const BevelVertexData *bvd,
                                                          int vi,
                                                          int vo)
{
  int lfirst = -1;
  int count = 0;
  if (vi == vo || bvd == nullptr) {
    return std::pair<int, int>(lfirst, count);
  }
  const int vi_boundary_pos = bvd->vertex_mesh().boundary_pos_for_vert(vi);
  int pos = vi_boundary_pos;
  const VertexMesh &vmesh = bvd->vertex_mesh();
  for (;;) {
    int pos_prev = vmesh.prev_boundary_pos(pos);
    int v_prev = vmesh.boundary_vert(pos_prev);
    int v = vmesh.boundary_vert(pos);
    int l = new_loop_for_vert_pair(v, v_prev);
    if (pos == vi_boundary_pos) {
      lfirst = l;
    }
    count++;
    if (v_prev == vo) {
      break;
    }
    pos = pos_prev;
    BLI_assert(v_prev != vi); /* Shouldn't wrap around. */
  }
  return std::pair<int, int>(lfirst, count);
}

/** Calculate the vertex bevels and leave the result in the mesh_delta_ member.
 * Assume that the `spec_` member has been set up as a `VertexBevelSpec` to
 * specify how the vertex bevel is to be done.
 */
void BevelData::calculate_vertex_bevel()
{
  const VertexBevelSpec *spec = dynamic_cast<const VertexBevelSpec *>(spec_);
  bevel_vert_data_.reinitialize(spec_->to_bevel.size());
  const IndexMask &to_bevel = spec_->to_bevel;
  threading::parallel_for(to_bevel.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int vert = to_bevel[i];
      bevel_vert_data_[i].construct_vertex_cap(vert, *topo_);
    }
  });

  setup_vert_map();

  /* Make the polygons that will replace the beveled vertices. Since this allocates new elements
   * in mesh_delta_, need to do this serially.
   * Also delete the vertices and the original edges attached to them.
   */
  for (BevelVertexData &bvd : bevel_vert_data_) {
    /* Allocate vertices for the boundary vertices. */
    const int vert = bvd.beveled_vert();
    const float amount = spec->amount[vert];
    bvd.construct_vertex_mesh_for_vertex_bevel(amount, *topo_, mesh_delta_);

    /* Delete the beveled vertex, which is now being replaced.
     * TODO: only do this if there is no extra stuff attached to it.
     */
    mesh_delta_.delete_vert(vert);
    /* We also delete any edges that were using that vertex. */
    for (int e : topo_->vert_edges(bvd.vertex_cap().vert)) {
      mesh_delta_.delete_edge(e);
    }
  }

  /* Reconstruct all faces that involve a beveled vertex. */
  Span<MPoly> polys = mesh_->polys();
  Span<MLoop> loops = mesh_->loops();
  Array<int> faces_to_reconstruct = affected_faces();
  for (const int f : faces_to_reconstruct) {
    const MPoly &mpoly = polys[f];
    /* Need new loops in the recontruction because they must be contiguous in the new face. */
    int lfirst = -1;
    int num_loops = 0;
    for (int l = mpoly.loopstart; l < mpoly.loopstart + mpoly.totloop; l++) {
      const MLoop &mloop = loops[l];
      int lnext = l < mpoly.loopstart + mpoly.totloop - 1 ? l + 1 : mpoly.loopstart;
      const MLoop &mloop_next = loops[lnext];
      int v1 = mloop.v;
      int v2 = mloop_next.v;
      const BevelVertexData *bvd1 = bevel_vertex_data(v1);
      const BevelVertexData *bvd2 = bevel_vertex_data(v2);
      int v1i, v1o, v2i, v2o;
      std::tie(v1i, v1o) = attach_verts_for_beveled_vert(v1, mloop.e, bvd1);
      std::tie(v2i, v2o) = attach_verts_for_beveled_vert(v2, mloop_next.e, bvd2);
      if (lfirst == -1) {
        int cnt;
        std::tie(lfirst, cnt) = new_loops_for_beveled_vert(bvd1, v1i, v1o);
        num_loops += cnt;
      }
      /* Invariant here: have made new loops for everything in previous part of mpoly,
       * and up to and including mloop.v (if unbeveled) or the part of the boundary
       * for mloop.v that replaces mloop.v. num_loops is the number of loops made so far. */
      int lnew = new_loop_for_vert_pair(v1o, v2i);
      if (lfirst == -1) {
        lfirst = lnew;
      }
      num_loops++;
      /* IF we are back tot he beginning, the following was already done. */
      if (l == mpoly.loopstart + mpoly.totloop - 1) {
        break;
      }
      std::pair<int, int> lnew_and_cnt = new_loops_for_beveled_vert(bvd2, v2i, v2o);
      num_loops += lnew_and_cnt.second;
    }
    mesh_delta_.new_face(lfirst, num_loops, f);
    /* Delete the old face (which also deletes its loops). */
    mesh_delta_.delete_face(f);
  }
}

void BevelData::calculate_edge_bevel()
{
  const EdgeBevelSpec *spec = dynamic_cast<const EdgeBevelSpec *>(spec_);
  const IndexMask &to_bevel = spec->to_bevel;
  bevel_edge_.reinitialize(to_bevel.size());
  Set<int> need_vert;
  for (const int e : to_bevel) {
    need_vert.add(topo_->edge_v1(e));
    need_vert.add(topo_->edge_v2(e));
  }
  const int tot_need_vert = need_vert.size();
  bevel_vert_data_.reinitialize(tot_need_vert);
  /* We want the vertices in a deterministic order, so loop through edges again, removing from
   * need_vert as we go. */
  int bvd_index = 0;
  for (const int e : to_bevel) {
    for (const int i : IndexRange(2)) {
      const int v = i == 0 ? topo_->edge_v1(e) : topo_->edge_v2(e);
      if (need_vert.contains(v)) {
        bevel_vert_data_[bvd_index].construct_vertex_cap(v, *topo_);
        need_vert.remove(v);
        bvd_index++;
      }
    }
  }
}

void BevelData::calculate_face_bevel()
{
  const FaceBevelSpec *spec = dynamic_cast<const FaceBevelSpec *>(spec_);
  if (spec->use_regions) {
    std::cout << "TODO: Implement use_regions";
  }
  Span<MPoly> faces = mesh_->polys();
  Span<float3> vert_positions = mesh_->vert_positions();
  Span<MLoop> loops = mesh_->loops();
  const IndexMask &to_bevel = spec_->to_bevel;
  for (const int i : to_bevel.index_range()) {
    const int face_index = to_bevel[i];
    const MPoly &face = faces[face_index];
    const int n = face.totloop;
    Array<float3> vert_co(n);
    Array<int> mi_vert_to_mesh_vert(n);
    Vector<int> face_vert;
    for (const int l : IndexRange(n)) {
      const MLoop &loop = loops[face.loopstart + l];
      const int v_index = loop.v;
      vert_co[l] = vert_positions[v_index];
      mi_vert_to_mesh_vert[l] = v_index;
      face_vert.append(l);
    }
    Array<Vector<int>> mi_faces(1);
    mi_faces[0] = face_vert;
    meshinset::MeshInset_Input mi_input;
    mi_input.vert = vert_co.as_span();
    mi_input.face = mi_faces.as_span();
    mi_input.contour = mi_faces.as_span();
    mi_input.inset_amount = spec->amount[face_index];
    mi_input.slope = spec->slope[face_index];
    meshinset::MeshInset_Result mi_result = meshinset::mesh_inset_calc(mi_input);
    /* Mapping from the result output vert indices to mesh indices. */
    Array<int> mr_vert_to_mesh_vert(mi_result.vert.size());
    for (const int i : mi_result.vert.index_range()) {
      if (mi_result.orig_vert[i] != -1) {
        mr_vert_to_mesh_vert[i] = mi_vert_to_mesh_vert[mi_result.orig_vert[i]];
      }
      else {
        mr_vert_to_mesh_vert[i] = mesh_delta_.new_vert(mi_result.vert[i], 0);  // TODO: better rep!
      }
    }
    /* Construct the output faces. */
    for (const int out_face_index : mi_result.face.index_range()) {
      Vector<int> &mr_face = mi_result.face[out_face_index];
      const int m = mr_face.size();
      int lfirst = -1;
      for (const int i : IndexRange(m)) {
        int v = mr_vert_to_mesh_vert[mr_face[i]];
        int v_next = mr_vert_to_mesh_vert[mr_face[(i + 1) % m]];
        int e = mesh_delta_.find_or_add_edge(v, v_next, 0);  // TODO: better rep!
        int l = mesh_delta_.new_loop(v, e, 0);               // TODO: better rep!
        if (lfirst == -1) {
          lfirst = l;
        }
      }
      mesh_delta_.new_face(lfirst, m, face_index);  // TODO: better rep!
    }
    /* The following also deletes the loops. The edges in the original faces should have all been
     * reused. */
    mesh_delta_.delete_face(face_index);
  }
}

Mesh *BevelData::get_output_mesh(GeometrySet geometry_set, const MeshComponent &component, const AnonymousAttributePropagationInfo &propagation_info)
{
  return mesh_delta_.apply_delta_to_mesh(geometry_set, component, propagation_info);
}

static Mesh *bevel_mesh_vertices(GeometrySet geometry_set,
                                 const MeshComponent &component,
                                 const Field<bool> &selection_field,
                                 const Field<float> &amount_field,
                                 const AnonymousAttributePropagationInfo &propagation_info)
{
  const Mesh &mesh = *component.get_for_read();
  int orig_vert_size = mesh.totvert;
  bke::MeshFieldContext context{mesh, ATTR_DOMAIN_POINT};
  FieldEvaluator evaluator{context, orig_vert_size};
  evaluator.set_selection(selection_field);
  evaluator.add(amount_field);
  evaluator.evaluate();
  VArray<float> amounts = evaluator.get_evaluated<float>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  VertexBevelSpec spec(selection, amounts);
  MeshTopology topo(mesh);
  BevelData bdata(&mesh, &spec, &topo);
  bdata.calculate_vertex_bevel();
  return bdata.get_output_mesh(geometry_set, component, propagation_info);
}

static Mesh *bevel_mesh_edges(GeometrySet geometry_set,
                              const MeshComponent &component,
                              const Field<bool> selection_field,
                              const Field<float> amount_field,
                              const AnonymousAttributePropagationInfo &propagation_info)
{
  const Mesh &mesh = *component.get_for_read();
  int orig_edge_size = mesh.totedge;
  bke::MeshFieldContext context(mesh, ATTR_DOMAIN_EDGE);
  FieldEvaluator evaluator(context, orig_edge_size);
  evaluator.set_selection(selection_field);
  evaluator.add(amount_field);
  evaluator.evaluate();
  VArray<float> amounts = evaluator.get_evaluated<float>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  EdgeBevelSpec spec(selection, amounts);
  MeshTopology topo(mesh);
  BevelData bdata(&mesh, &spec, &topo);
  bdata.calculate_edge_bevel();
  return bdata.get_output_mesh(geometry_set, component, propagation_info);
}

static Mesh *bevel_mesh_faces(GeometrySet geometry_set,
                              const MeshComponent &component,
                              const Field<bool> &selection_field,
                              const Field<float> &amount_field,
                              const Field<float> &slope_field,
                              bool use_regions,
                              const AnonymousAttributePropagationInfo &propagation_info)
{
  const Mesh &mesh = *component.get_for_read();
  bke::MeshFieldContext context{mesh, ATTR_DOMAIN_FACE};
  FieldEvaluator evaluator{context, mesh.totpoly};
  evaluator.set_selection(selection_field);
  evaluator.add(amount_field);
  evaluator.add(slope_field);
  evaluator.evaluate();
  VArray<float> amounts = evaluator.get_evaluated<float>(0);
  VArray<float> slopes = evaluator.get_evaluated<float>(1);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  FaceBevelSpec spec(selection, amounts, slopes, use_regions);
  MeshTopology topo(mesh);
  BevelData bdata(&mesh, &spec, &topo);
  bdata.calculate_face_bevel();
  return bdata.get_output_mesh(geometry_set, component, propagation_info);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float> amount_field = params.extract_input<Field<float>>("Amount");
  const NodeGeometryBevelMesh &storage = node_storage(params.node());
  GeometryNodeBevelMeshMode mode = static_cast<GeometryNodeBevelMeshMode>(storage.mode);
  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info("Mesh");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_mesh()) {
      const MeshComponent &component = *geometry_set.get_component_for_read<MeshComponent>();
      Mesh *mesh_out = nullptr;
      switch (mode) {
        case GEO_NODE_BEVEL_MESH_VERTICES:
          mesh_out = bevel_mesh_vertices(geometry_set, component, selection_field, amount_field, propagation_info);
          break;
        case GEO_NODE_BEVEL_MESH_EDGES: {
          mesh_out = bevel_mesh_edges(geometry_set, component, selection_field, amount_field, propagation_info);
        } break;
        case GEO_NODE_BEVEL_MESH_FACES: {
          Field<float> slope_field = params.extract_input<Field<float>>("Slope");
          bool use_regions = params.get_input<bool>("Use Regions");
          mesh_out = bevel_mesh_faces(
              geometry_set, component, selection_field, amount_field, slope_field, use_regions, propagation_info);
        } break;
      }
      BLI_assert(BKE_mesh_is_valid(mesh_out));
      geometry_set.replace_mesh(mesh_out);
    }
  });

  params.set_output("Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_bevel_mesh_cc

void register_node_type_geo_bevel_mesh()
{
  namespace file_ns = blender::nodes::node_geo_bevel_mesh_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_BEVEL_MESH, "Bevel Mesh", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  node_type_storage(
      &ntype, "NodeGeometryBevelMesh", node_free_standard_storage, node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
