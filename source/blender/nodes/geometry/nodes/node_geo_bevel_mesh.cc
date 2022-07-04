/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"

#include "BLI_array.hh"
#include "BLI_math_vec_types.hh"
#include "BLI_math_vector.hh"
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
  b.add_output<decl::Geometry>("Mesh");
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "mode", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryBevelMesh *data = MEM_cnew<NodeGeometryBevelMesh>(__func__);
  data->mode = GEO_NODE_BEVEL_MESH_EDGES;
  node->storage = data;
}

static void node_update(bNodeTree *UNUSED(ntree), bNode *UNUSED(node))
{
}

/* MeshTopology encapsulates data needed to answer topological queries about a mesh,
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

  float3 vert_co(int v) const
  {
    return float3(mesh_.mvert[v].co);
  }

  int edge_v1(int e) const
  {
    return mesh_.medge[e].v1;
  }

  int edge_v2(int e) const
  {
    return mesh_.medge[e].v2;
  }

  float3 edge_dir_from_vert(int e, int v) const;
  float3 edge_dir_from_vert_normalized(int e, int v) const;
};

MeshTopology::MeshTopology(const Mesh &mesh) : mesh_(mesh)
{
  // timeit::ScopedTimer t("MeshTopology construction");
  BKE_mesh_vert_edge_map_create(
      &vert_edge_map_, &vert_edge_map_mem_, mesh.medge, mesh.totvert, mesh.totedge);
  BKE_mesh_edge_poly_map_create(&edge_poly_map_,
                                &edge_poly_map_mem_,
                                mesh.medge,
                                mesh.totedge,
                                mesh.mpoly,
                                mesh.totpoly,
                                mesh.mloop,
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
  const MPoly &mpoly = mesh_.mpoly[f];
  const int loopstart = mpoly.loopstart;
  const int loopend = mpoly.loopstart + mpoly.totloop - 1;
  for (int l = loopstart; l <= loopend; l++) {
    const MLoop &mloop = mesh_.mloop[l];
    if (mloop.e == e) {
      if (mloop.v == v) {
        /* The other edge with vertex v is the preceding (incoming) edge. */
        MLoop &mloop_prev = l == loopstart ? mesh_.mloop[loopend] : mesh_.mloop[l - 1];
        return mloop_prev.e;
      }
      else {
        /* The other edge with vertex v is the next (outgoing) edge, which should have vertex v. */
        MLoop &mloop_next = l == loopend ? mesh_.mloop[loopstart] : mesh_.mloop[l + 1];
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
  const MPoly &mpoly = mesh_.mpoly[f];
  const int loopstart = mpoly.loopstart;
  const int loopend = mpoly.loopstart + mpoly.totloop - 1;
  for (int l = loopstart; l <= loopend; l++) {
    const MLoop &mloop = mesh_.mloop[l];
    if (mloop.e == e0) {
      const MLoop &mloop_next = l == loopend ? mesh_.mloop[loopstart] : mesh_.mloop[l + 1];
      return mloop_next.e == e1;
    }
  }
  return false;
}

float3 MeshTopology::edge_dir_from_vert(int e, int v) const
{
  const MEdge &medge = mesh_.medge[e];
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

/* A Vertex Cap consists of a vertex in a mesh and an CCW ordering of
 * alternating edges and faces around it, as viewed from the face's
 * normal side. Some faces may be missing (i.e., gaps).
 * (If there are other edges and faces attached to the vertex that
 * don't fit into this pattern, they need to go into other Vertex Caps
 * or ignored, for the sake of beveling.)
 */
class VertexCap {
  Array<int> edges_;
  Array<int> faces_;  // face_[i] is between edges i and i+1

 public:
  /* The vertex (as index into a mesh) that the cap is around. */
  int vert;

  VertexCap() : vert(-1)
  {
  }
  VertexCap(int vert, Span<int> edges, Span<int> faces) : edges_(edges), faces_(faces), vert(vert)
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
  Span<int> edges() const
  {
    return edges_.as_span();
  }

  /* Faces in CCW order (viewed from top) around the cap. -1 means a gap. */
  Span<int> faces() const
  {
    return faces_.as_span();
  }

  /* The ith edge. */
  int edge(int i) const
  {
    return edges_[i];
  }
  /* The edge after the ith edge (with wraparound). */
  int next_edge(int i) const
  {
    return i < edges_.size() - 1 ? edges_[i + 1] : edges_[0];
  }
  /* The edge before the ith edge (with wraparound). */
  int prev_edge(int i) const
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
};

/* Construct and return the VertexCap for vertex vert. */
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
      this->edges_ = ordered_edges;
      this->faces_ = ordered_faces;
      return;
    }
  }
  std::cout << "to implement: VertexCap for non-manifold edges\n";
}

static std::ostream &operator<<(std::ostream &os, const VertexCap &cap)
{
  os << "cap at v" << cap.vert << ": ";
  for (const int i : cap.edges().index_range()) {
    os << "e" << cap.edge(i) << " ";
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

/* The different types of BoundaryVerts (see below). */
typedef enum eBoundaryVertType {
  BV_ON_EDGE = 0,
  BV_ON_FACE = 1,
  BV_ABOVE_FACE = 2,
  BV_OTHER = 3,
} eBoundaryVertType;

static const char *bv_type_name[4] = {"on_edge", "on_face", "above_face", "other"};

/* A BoundaryVert is a vertex placed somewhere around a vertex involved
 * in a bevel. BoundaryVerts will be joined with line or arcs (depending on the
 * number of segments in the bevel).
 */
class BoundaryVert {
 public:
  /* The position of the Boundary Vertex. */
  float3 co;
  /* If the type references an edge or a face.
   * the index of the corresponding edge or face in the VertexCap. */
  int vc_index;
  /* Mesh index of this vertex in the output mesh. */
  int mesh_index;
  /* The type of this Boundary Vertex. */
  eBoundaryVertType type;

  BoundaryVert() : co(0.0, 0.0, 0.0), vc_index(-1), mesh_index(-1), type(BV_OTHER)
  {
  }
};

static std::ostream &operator<<(std::ostream &os, const BoundaryVert &bv)
{
  os << "bv{" << bv_type_name[bv.type] << " "
     << "vc#=" << bv.vc_index << " "
     << "mesh#" << bv.mesh_index << " "
     << "co=" << bv.co << "}";
  return os;
}

/* The different types of BoundaryEdges (see below). */
typedef enum eBoundaryEdgeType {
  BE_UNBEVELED = 0,
  BE_BEVELED = 1,
  BE_FACE_BEVEL_BOTH = 2,
  BE_FACE_BEVEL_LEFT = 3,
  BE_FACE_BEVEL_RIGHT = 4,
  BE_OTHER = 5,
} eBoundaryEdgeType;

static const char *be_type_name[6] = {
    "unbev", "bev", "facebev_both", "facebev_l", "facebev_r", "other"};

/* A BoundaryEdge is one end of an edge, attached to a vertex in a VertexCap.
 * This data describes how it is involved in beveling, and how it is attached
 * to BoundaryVerts.
 * Note: when the descriptors "left" and "right" are used to refer to sides of
 * edges, these are to be taken as left and right when looking down the edge
 * towards the VertexCap's vertex.
 */
class BoundaryEdge {
 public:
  /* The mesh index of the edge. */
  int edge;
  /* Where it is found in the list of edges in the VertexCap. */
  int vc_index;
  /* The boundary vertex index where the edge is attached,
   * only used for BE_UNBEVELED and BE_FACE_BEVEL_* types. */
  int bv_index;
  /* The boundary vertex index where the left half of a BE_BEVELED,
   * BE_FACE_BEVEL_BOTH, or BE_FACE_BEVEL_LEFT attached. */
  int bv_left_index;
  /* The boundary vertex index where the left half of a BE_BEVELED,
   * BE_FACE_BEVEL_BOTH, or BE_FACE_BEVEL_RIGHT attached. */
  int bv_right_index;
  /* The type of this BoundaryEdge. */
  eBoundaryEdgeType type;

  BoundaryEdge()
      : edge(-1), vc_index(-1), bv_index(-1), bv_left_index(-1), bv_right_index(-1), type(BE_OTHER)
  {
  }
};

static std::ostream &operator<<(std::ostream &os, const BoundaryEdge &be)
{
  os << "be{" << be_type_name[be.type] << " "
     << "edge=" << be.edge << " "
     << "vc#=" << be.vc_index << " "
     << "bv#=" << be.bv_index << " "
     << "bvl#=" << be.bv_left_index << " "
     << "bvr#=" << be.bv_right_index << "}";
  return os;
}

class BevelVertexData {
  VertexCap vertex_cap_;
  Array<BoundaryVert> boundary_vert_;
  Array<BoundaryEdge> boundary_edge_;

 public:
  BevelVertexData()
  {
  }
  ~BevelVertexData()
  {
  }

  void construct_vertex_cap(int vert, const MeshTopology &topo)
  {
    vertex_cap_.init_from_topo(vert, topo);
  }

  void construct_vertex_bevel(int vert, float amount, const MeshTopology &topo);

  const VertexCap &vertex_cap() const
  {
    return vertex_cap_;
  }

  Span<BoundaryVert> boundary_verts() const
  {
    return boundary_vert_.as_span();
  }

  Span<BoundaryEdge> boundary_edges() const
  {
    return boundary_edge_.as_span();
  }
};

static std::ostream &operator<<(std::ostream &os, const BevelVertexData &bvd)
{
  const VertexCap &vc = bvd.vertex_cap();
  os << "bevel vertex data for vertex " << vc.vert << "\n";
  os << vc;
  Span<BoundaryVert> bvs = bvd.boundary_verts();
  os << "boundary verts:\n";
  for (const int i : bvs.index_range()) {
    os << "[" << i << "] " << bvs[i] << "\n";
  }
  Span<BoundaryEdge> bes = bvd.boundary_edges();
  os << "boundary edges:\n";
  for (const int i : bes.index_range()) {
    os << "[" << i << "] " << bes[i] << "\n";
  }
  return os;
}

/* Calculate the BevelVertexData for one vertex, `vert`, by the given `amount`.
 * This doesn't calculate limits to the bevel caused by collisions with vertex bevels
 * at adjacent vertices; that needs to done after all of these are calculated,
 * so that this operation can be done in parallel with all other vertex constructions.
 */
void BevelVertexData::construct_vertex_bevel(int vert, float amount, const MeshTopology &topo)
{
  construct_vertex_cap(vert, topo);

  const int num_edges = vertex_cap().size();

  /* There will be one boundary vertex on each edge attached to `vert`. */
  boundary_edge_.reinitialize(num_edges);
  boundary_vert_.reinitialize(num_edges);

  const float3 vert_co = topo.vert_co(vertex_cap().vert);
  for (const int i : IndexRange(num_edges)) {
    BoundaryVert &bv = boundary_vert_[i];
    bv.type = BV_ON_EDGE;
    bv.vc_index = i;
    BoundaryEdge &be = boundary_edge_[i];
    be.edge = vertex_cap().edge(i);
    be.type = BE_UNBEVELED;
    be.bv_index = i;
    be.vc_index = i;

    /* Set the position of the boundary vertex by sliding at distance `amount` along the edge. */
    float3 dir = topo.edge_dir_from_vert_normalized(be.edge, vert);
    bv.co = vert_co + amount * dir;
  }
}

class BevelData {
  Array<BevelVertexData> bevel_vert_data_;

 public:
  MeshTopology topo;

  BevelData(const Mesh &mesh) : topo(mesh)
  {
  }
  ~BevelData()
  {
  }

  void calculate_vertex_bevels(const IndexMask to_bevel, VArray<float> amounts);

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

/* Calculate the BevelData for a vertex bevel of all specified vertices of the mesh.
 * `to_bevel` gives the mesh indices of vertices to be beveled.
 * `amounts` should have (virtual) length that matches the number of vertices in the mesh,
 * and gives, per vertex, the magnitude of the bevel at that vertex.
 */
void BevelData::calculate_vertex_bevels(const IndexMask to_bevel, VArray<float> amounts)
{
  BLI_assert(amounts.size() == topo.num_verts());

  bevel_vert_data_.reinitialize(to_bevel.size());
  threading::parallel_for(to_bevel.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int vert = to_bevel[i];
      bevel_vert_data_[i].construct_vertex_bevel(vert, amounts[vert], topo);
    }
  });
}

static void bevel_mesh_vertices(MeshComponent &component,
                                const Field<bool> &selection_field,
                                const Field<float> &amount_field)
{
  Mesh &mesh = *component.get_for_write();
  int orig_vert_size = mesh.totvert;
  GeometryComponentFieldContext context(component, ATTR_DOMAIN_POINT);
  FieldEvaluator evaluator{context, orig_vert_size};
  evaluator.set_selection(selection_field);
  evaluator.add(amount_field);
  evaluator.evaluate();
  VArray<float> amounts = evaluator.get_evaluated<float>(0);
  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  BevelData bdata(mesh);
  bdata.calculate_vertex_bevels(selection, amounts);
  bdata.print("After calculate_vertex_bevels");  // DEBUG
}

static void bevel_mesh_edges(MeshComponent &UNUSED(component),
                             const Field<bool> &UNUSED(selection_field),
                             const Field<float> &UNUSED(amount_field))
{
}

static void bevel_mesh_faces(MeshComponent &UNUSED(component),
                             const Field<bool> &UNUSED(selection_field),
                             const Field<float> &UNUSED(amount_field))
{
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  Field<float> amount_field = params.extract_input<Field<float>>("Amount");
  const NodeGeometryBevelMesh &storage = node_storage(params.node());
  GeometryNodeBevelMeshMode mode = static_cast<GeometryNodeBevelMeshMode>(storage.mode);

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (geometry_set.has_mesh()) {
      MeshComponent &component = geometry_set.get_component_for_write<MeshComponent>();
      switch (mode) {
        case GEO_NODE_BEVEL_MESH_VERTICES:
          bevel_mesh_vertices(component, selection_field, amount_field);
          break;
        case GEO_NODE_BEVEL_MESH_EDGES:
          bevel_mesh_edges(component, selection_field, amount_field);
          break;
        case GEO_NODE_BEVEL_MESH_FACES:
          bevel_mesh_faces(component, selection_field, amount_field);
          break;
      }
      BLI_assert(BKE_mesh_is_valid(component.get_for_write()));
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
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  node_type_storage(
      &ntype, "NodeGeometryBevelMesh", node_free_standard_storage, node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
