/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute_math.hh"
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

  float3 vert_co(int v) const
  {
    return float3(mesh_.verts()[v].co);
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
                                BKE_mesh_edges(&mesh),
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

/** A Vertex Cap consists of a vertex in a mesh and an CCW ordering of
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

/** The different types of BoundaryEdges (see below). */
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

/** A BoundaryEdge is one end of an edge, attached to a vertex in a VertexCap.
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
  /* The index of this edge, if unbeveled, in output mesh. */
  int mesh_index;
  /* The type of this BoundaryEdge. */
  eBoundaryEdgeType type;

  BoundaryEdge()
      : edge(-1),
        vc_index(-1),
        bv_index(-1),
        bv_left_index(-1),
        bv_right_index(-1),
        mesh_index(-1),
        type(BE_OTHER)
  {
  }
};

/** A BoundaryConnector has the vertices and edges in the output mesh
 * of the connection between two successive BoundaryVerts.
 */
class BoundaryConnector {
 public:
  /* Temporary: for now, just one edge. Will eventually be array
   * of vertices with intervening edges. */
  int edge;

  BoundaryConnector() : edge(-1)
  {
  }

  BoundaryConnector(int e) : edge(e)
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
     << "bvr#=" << be.bv_right_index << " "
     << "eout=" << be.mesh_index << "}";
  return os;
}

/** BevelVertexData holds the data used to bevel around a vertex. */
class BevelVertexData {
  VertexCap vertex_cap_;
  Array<BoundaryVert> boundary_vert_;
  Array<BoundaryEdge> boundary_edge_;
  /* boundary_conn_[i] goes from boundary_vert_[i] to the following one. */
  Array<BoundaryConnector> boundary_conn_;

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

  const int beveled_vert() const
  {
    return vertex_cap_.vert;
  }

  Span<BoundaryVert> boundary_verts() const
  {
    return boundary_vert_.as_span();
  }

  MutableSpan<BoundaryVert> mutable_boundary_verts()
  {
    return boundary_vert_.as_mutable_span();
  }

  Span<BoundaryEdge> boundary_edges() const
  {
    return boundary_edge_.as_span();
  }

  const BoundaryVert &boundary_vert(int boundary_vert_pos) const
  {
    return boundary_vert_[boundary_vert_pos];
  }

  const BoundaryVert &next_boundary_vert(int boundary_vert_pos) const
  {
    int n = (boundary_vert_pos + 1) % boundary_vert_.size();
    return boundary_vert_[n];
  }

  void set_boundary_connection(int boundary_vert_pos, const BoundaryConnector conn)
  {
    boundary_conn_[boundary_vert_pos] = conn;
  }

  const int boundary_connector_edge(int boundary_vert_pos, int edge_index) const
  {
    BLI_assert(edge_index == 0);  // Temporary
    return boundary_conn_[boundary_vert_pos].edge;
  }

  /* Find the BoundaryEdge for `edge`, returning nullptr if not found. */
  BoundaryEdge *find_boundary_edge(int edge) const;
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

/** Calculate the `BevelVertexData` for one vertex, `vert`, by the given `amount`.
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
  boundary_conn_.reinitialize(num_edges);

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

BoundaryEdge *BevelVertexData::find_boundary_edge(int edge) const
{
  for (int i : boundary_edge_.index_range()) {
    if (boundary_edge_[i].edge == edge) {
      /* There's no non-const rvalue subscripting in Array. */
      BoundaryEdge *be = const_cast<BoundaryEdge *>(&boundary_edge_[i]);
      return be;
    }
  }
  return nullptr;
}

/** BevelData holds the global data needed for a bevel. */
class BevelData {
  /* BevelVertexData for just the affected vertices. */
  Array<BevelVertexData> bevel_vert_data_;
  /* A map from mesh vertex index to index in bevel_vert_data_.
   * If we wanted  more speed at expense of space, we could also use
   * an Array of size equal to the number of mesh vertices here.
   */
  Map<int, int> vert_to_bvd_index_;

 public:
  MeshTopology topo;

  BevelData(const Mesh &mesh) : topo(mesh)
  {
  }
  ~BevelData()
  {
  }

  /* Initial calculation of position of boundary and edge attachments for vertex bevel. */
  void calculate_vertex_bevels(const IndexMask to_bevel, VArray<float> amounts);

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

  Span<BevelVertexData> beveled_vertices_data() const
  {
    return bevel_vert_data_.as_span();
  }

  MutableSpan<BevelVertexData> mutable_beveled_vertices_data()
  {
    return bevel_vert_data_.as_mutable_span();
  }

  void print(const std::string &label) const;
};

/** Make a transation map from mesh vertex index to indices in bevel_vert_data_. */
void BevelData::setup_vert_map()
{
  vert_to_bvd_index_.reserve(bevel_vert_data_.size());
  for (const int i : bevel_vert_data_.index_range()) {
    vert_to_bvd_index_.add_new(bevel_vert_data_[i].vertex_cap().vert, i);
  }
}

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

/** Calculate the BevelData for a vertex bevel of all specified vertices of the mesh.
 * `to_bevel` gives the mesh indices of vertices to be beveled.
 * `amounts` should have (virtual) length that matches the number of vertices in the mesh,
 * and gives, per vertex, the magnitude of the bevel at that vertex.
 */
void BevelData::calculate_vertex_bevels(const IndexMask to_bevel, VArray<float> amounts)
{
  // BLI_assert(amounts.size() == topo.num_verts());

  bevel_vert_data_.reinitialize(to_bevel.size());
  threading::parallel_for(to_bevel.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      const int vert = to_bevel[i];
      bevel_vert_data_[i].construct_vertex_bevel(vert, amounts[vert], topo);
    }
  });
  setup_vert_map();
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
 */
class MeshDelta {
  const Mesh &mesh_;
  IndexAlloc vert_alloc_;
  IndexAlloc edge_alloc_;
  IndexAlloc poly_alloc_;
  IndexAlloc loop_alloc_;
  Set<int> vert_deletes_;
  Set<int> edge_deletes_;
  Set<int> poly_deletes_;
  Set<int> loop_deletes_;
  Vector<MVert> new_verts_;
  Vector<MEdge> new_edges_;
  Vector<MPoly> new_polys_;
  Vector<MLoop> new_loops_;
  Vector<int> new_vert_rep_;
  Vector<int> new_edge_rep_;
  Vector<int> new_poly_rep_;
  Vector<int> new_loop_rep_;

 public:
  MeshDelta(const Mesh &mesh);

  /* In the following, `rep` is the index of the old mesh element to base attributes on.  */
  int new_vert(const float3 &co, int rep);
  int new_edge(int v1, int v2, int rep);
  int new_loop(int v, int e, int rep);
  int new_face(int loopstart, int totloop, int rep);

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
  Mesh *apply_delta_to_mesh(GeometrySet &geometry_set, const MeshComponent &in_component);

  void print(const std::string &label) const;
};

MeshDelta::MeshDelta(const Mesh &mesh)
    : mesh_(mesh),
      vert_alloc_(mesh_.totvert),
      edge_alloc_(mesh_.totedge),
      poly_alloc_(mesh_.totpoly),
      loop_alloc_(mesh_.totloop)
{
}

int MeshDelta::new_vert(const float3 &co, int rep)
{
  int v = vert_alloc_.alloc();
  MVert mvert;
  copy_v3_v3(mvert.co, co);
  mvert.flag = 0;
  mvert.bweight = 0;
  new_verts_.append(mvert);
  new_vert_rep_.append(rep);
  return v;
}

int MeshDelta::new_edge(int v1, int v2, int rep)
{
  int e = edge_alloc_.alloc();
  MEdge medge;
  medge.v1 = v1;
  medge.v2 = v2;
  medge.crease = 0;
  medge.bweight = 0;
  medge.flag = ME_EDGEDRAW;
  new_edges_.append(medge);
  new_edge_rep_.append(rep);
  return e;
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
  mpoly.flag = 0;
  new_polys_.append(mpoly);
  new_poly_rep_.append(rep);
  return f;
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

#if 0
/* For debugging. */
static std::ostream &operator<<(std::ostream &os, const Mesh *mesh)
{
  os << "Mesh, totvert=" << mesh->totvert << " totedge=" << mesh->totedge
     << " totpoly=" << mesh->totpoly << " totloop=" << mesh->totloop << "\n";
  for (int v : IndexRange(mesh->totvert)) {
    os << "v" << v << " at (" << mesh->verts()[v].co[0] << "," << mesh->verts()[v].co[1] << ","
       << mesh->verts()[v].co[2] << ")\n";
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
static void copy_vertices_based_on_map(Span<MVert> src,
                                       MutableSpan<MVert> dst,
                                       Span<int> src_verts_map)
{
  for (const int src_v_index : src_verts_map.index_range()) {
    const int i_dst = src_v_index;
    const int i_src = src_verts_map[src_v_index];
    dst[i_dst] = src[i_src];
  }
}

/** Copy from `src` to `dst`. */
static void copy_vertices(Span<MVert> src, MutableSpan<MVert> dst)
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

Mesh *MeshDelta::apply_delta_to_mesh(GeometrySet &geometry_set, const MeshComponent &in_component)
{
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
      {GEO_COMPONENT_TYPE_MESH}, GEO_COMPONENT_TYPE_MESH, false, attributes);

  int out_totvert = keep_vertices.size() + new_verts_.size();
  int out_totedge = keep_edges.size() + new_edges_.size();
  int out_totpoly = keep_polys.size() + new_polys_.size();
  int out_totloop = keep_loops.size() + new_loops_.size();

  Span<MVert> mesh_verts = mesh_.verts();
  Span<MEdge> mesh_edges = mesh_.edges();
  Span<MLoop> mesh_loops = mesh_.loops();
  Span<MPoly> mesh_polys = mesh_.polys();

  Mesh *mesh_out = BKE_mesh_new_nomain_from_template(
      &mesh_, out_totvert, out_totedge, 0, out_totloop, out_totpoly);

  MeshComponent out_component;
  out_component.replace(mesh_out, GeometryOwnershipType::Editable);

  MutableSpan<MVert> mesh_out_verts = mesh_out->verts_for_write();
  MutableSpan<MEdge> mesh_out_edges = mesh_out->edges_for_write();
  MutableSpan<MLoop> mesh_out_loops = mesh_out->loops_for_write();
  MutableSpan<MPoly> mesh_out_polys = mesh_out->polys_for_write();

  /* Copy the kept elements to the new mesh, mapping the internal vertex, edge, and loop
   * indices in each of those elements to their new positions.
   */

  copy_vertices_based_on_map(mesh_verts, mesh_out_verts, keep_vertices);
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

  copy_vertices(new_verts_.as_span(), mesh_out_verts.drop_front(keep_vertices.size()));
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
  for (const int v : new_verts_.index_range()) {
    copy_v3_v3(mesh_out->verts_for_write()[v + keep_vertices.size()].co, new_verts_[v].co);
  }

  BKE_mesh_calc_edges_loose(mesh_out);
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
  for (int i : new_verts_.index_range()) {
    const MVert &mv = new_verts_[i];
    std::cout << "v" << voff + i << ": (" << mv.co[0] << "," << mv.co[1] << "," << mv.co[2]
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

/* This function is temporary, to test the MeshDelta functions. */
static Mesh *finish_vertex_bevel(BevelData &bd,
                                 const Mesh &mesh,
                                 GeometrySet geometry_set,
                                 const MeshComponent &component)
{
  MeshDelta mesh_delta(mesh);
  MutableSpan<BevelVertexData> beveled_bvds = bd.mutable_beveled_vertices_data();

  /* Make the polygons that will replace the beveled vertices. */
  for (BevelVertexData &bvd : beveled_bvds) {
    /* Allocate vertices for the boundary vertices. */
    MutableSpan<BoundaryVert> boundary_verts = bvd.mutable_boundary_verts();
    int n = boundary_verts.size();
    for (BoundaryVert &bv : boundary_verts) {
      bv.mesh_index = mesh_delta.new_vert(bv.co, bvd.beveled_vert());
    }
    /* Allocate the edges and loops for the polygon. */
    Array<int> edges(n);
    Array<int> loops(n);
    int lfirst = -1;
    int lprev = -1;
    for (int i : IndexRange(n)) {
      const BoundaryVert &bv = boundary_verts[i];
      const BoundaryVert &bv_next = boundary_verts[i == n - 1 ? 0 : i + 1];
      int v1 = bv.mesh_index;
      int v2 = bv_next.mesh_index;
      int e = mesh_delta.new_edge(v1, v2, -1);
      bvd.set_boundary_connection(i, BoundaryConnector(e));
      int l = mesh_delta.new_loop(v1, e, -1);
      if (i == 0) {
        lfirst = l;
      }
      lprev = l;
    }
    /* Now make the face. Assert that we allocated contiguous loop indices. */
    BLI_assert(lfirst != -1 && lprev == lfirst + n - 1);
    mesh_delta.new_face(lfirst, n, face_rep_for_beveled_vert(bvd));
    /* Delete the beveled vertex, which is now being replaced.
     * TODO: only do this if there is no extra stuff attached to it.
     */
    mesh_delta.delete_vert(bvd.vertex_cap().vert);
    /* We also delete any edges that were using that vertex. */
    for (int e : bd.topo.vert_edges(bvd.vertex_cap().vert)) {
      mesh_delta.delete_edge(e);
    }
  }

  /* Reconstruct all faces that involve a beveled vertex.
   * For now, go through all faces to see which ones are affected.
   * TODO: gather affected faces via connections to beveled vertices.
   */
  Span<MPoly> polys = mesh.polys();
  Span<MLoop> loops = mesh.loops();
  for (int f : IndexRange(mesh.totpoly)) {
    const MPoly &mpoly = polys[f];

    /* Are there any beveled vertices in f? */
    int any_affected_vert = false;
    for (int l = mpoly.loopstart; l < mpoly.loopstart + mpoly.totloop; l++) {
      const int v = loops[l].v;
      const BevelVertexData *bvd = bd.bevel_vertex_data(v);
      if (bvd != nullptr) {
        any_affected_vert = true;
        break;
      }
    }
    if (any_affected_vert) {
      /* We need to reconstruct f. We can't reuse unaffected loops since they won't be
       * contiguous.
       */
      int lfirst = -1;
      int totloop = 0;
      for (int l = mpoly.loopstart; l < mpoly.loopstart + mpoly.totloop; l++) {
        const MLoop &mloop = loops[l];
        const MLoop &mloop_next =
            loops[l == mpoly.loopstart + mpoly.totloop - 1 ? mpoly.loopstart : l + 1];
        int v1 = mloop.v;
        int v2 = mloop_next.v;
        int e = mloop.e;
        BevelVertexData *bvd1 = bd.bevel_vertex_data(v1);
        BevelVertexData *bvd2 = bd.bevel_vertex_data(v2);
        BoundaryEdge *be1 = bvd1 == nullptr ? nullptr : bvd1->find_boundary_edge(e);
        BoundaryEdge *be2 = bvd2 == nullptr ? nullptr : bvd2->find_boundary_edge(e);
        const BoundaryVert *bv1 = be1 == nullptr ? nullptr : &bvd1->boundary_vert(be1->bv_index);
        const BoundaryVert *bv2 = be2 == nullptr ? nullptr : &bvd2->boundary_vert(be2->bv_index);

        /* If v1 is beveled, we need to add the boundary connector from the next boundary vertex
         * CCW from bv1 (which is therefore the previous boundary vertex when going around our
         * current face) to bv1. This is the reverse of the connector from the current edge to
         * the next. Then after that, the new edge that replaces e. We assume the edge(s) for the
         * connector have already been made.
         */
        int lnew = -1;
        if (bvd1 != nullptr) {
          /* Temporary: for now assume only one edge in the connector. */
          int econn = bvd1->boundary_connector_edge(bv1->vc_index, 0);
          BLI_assert(econn != -1);
          int econn_v1, econn_v2;
          mesh_delta.get_edge_verts(econn, &econn_v1, &econn_v2);
          BLI_assert(econn_v1 == bv1->mesh_index);
          lnew = mesh_delta.new_loop(econn_v2, econn, l);
          if (l == mpoly.loopstart) {
            lfirst = lnew;
          }
          totloop++;
          /* Now we need an edge from bv1->mesh_index to either v2 (if v2 is not beveled)
           * or to bv2->mesh_index. But that edge may have been made already. If so,
           * we will find its mesh index in be2->mesh_index.
           * It is also possible we made the edge and stored it in be1->mesh_index,
           * while doing the adjacent face.
           */
          if (bvd2 == nullptr) {
            if (be1->mesh_index != -1) {
              e = be1->mesh_index;
            }
            else {
              e = mesh_delta.new_edge(bv1->mesh_index, v2, mloop.e);
            }
            lnew = mesh_delta.new_loop(bv1->mesh_index, e, l);
          }
          else {
            if (be1->mesh_index != -1) {
              e = be1->mesh_index;
            }
            else if (be2->mesh_index != -1) {
              e = be2->mesh_index;
            }
            else {
              e = mesh_delta.new_edge(bv1->mesh_index, bv2->mesh_index, mloop.e);
              be2->mesh_index = e;
            }
            lnew = mesh_delta.new_loop(bv1->mesh_index, e, l);
          }
          be1->mesh_index = e;
        }
        else if (bvd2 != nullptr) {
          /* v1 is not beveled and v2 is. */
          if (be2->mesh_index != -1) {
            e = be2->mesh_index;
          }
          else {
            e = mesh_delta.new_edge(v1, bv2->mesh_index, mloop.e);
            be2->mesh_index = e;
          }
          lnew = mesh_delta.new_loop(v1, e, l);
        }
        else {
          /* Neither v1 nor v2 is beveled, so we can use the existing e. */
          lnew = mesh_delta.new_loop(v1, e, l);
        }
        totloop++;

        if (lfirst == -1) {
          lfirst = lnew;
        }
      }
      mesh_delta.new_face(lfirst, totloop, f);
      /* Delete the old face (which also deletes its loops). */
      mesh_delta.delete_face(f);
    }
  }
  Mesh *mesh_out = mesh_delta.apply_delta_to_mesh(geometry_set, component);
  return mesh_out;
}

static Mesh *bevel_mesh_vertices(GeometrySet geometry_set,
                                 const MeshComponent &component,
                                 const Field<bool> &selection_field,
                                 const Field<float> &amount_field)
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

  BevelData bdata(mesh);
  bdata.calculate_vertex_bevels(selection, amounts);
  // bdata.print("After calculate_vertex_bevels");  // DEBUG
  return finish_vertex_bevel(bdata, mesh, geometry_set, component);
}

static void bevel_mesh_edges(const MeshComponent &UNUSED(component),
                             const Field<bool> &UNUSED(selection_field),
                             const Field<float> &UNUSED(amount_field))
{
}

static void bevel_mesh_faces(const MeshComponent &UNUSED(component),
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
      const MeshComponent &component = *geometry_set.get_component_for_read<MeshComponent>();
      Mesh *mesh_out = nullptr;
      switch (mode) {
        case GEO_NODE_BEVEL_MESH_VERTICES:
          mesh_out = bevel_mesh_vertices(geometry_set, component, selection_field, amount_field);
          break;
        case GEO_NODE_BEVEL_MESH_EDGES:
          bevel_mesh_edges(component, selection_field, amount_field);
          break;
        case GEO_NODE_BEVEL_MESH_FACES:
          bevel_mesh_faces(component, selection_field, amount_field);
          break;
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
  node_type_init(&ntype, file_ns::node_init);
  node_type_update(&ntype, file_ns::node_update);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  node_type_storage(
      &ntype, "NodeGeometryBevelMesh", node_free_standard_storage, node_copy_standard_storage);
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
