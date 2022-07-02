/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"

#include "BLI_array.hh"
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

/* While Mesh uses the term 'poly' for polygon, most of Blender uses the term 'face',
 * so we'll go with 'face' in this code except in the final to/from mesh routines.
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
};

MeshTopology::MeshTopology(const Mesh &mesh) : mesh_(mesh)
{
  timeit::ScopedTimer t("MeshTopology construction");
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

  /* Debug printing on std::cout. */
  void print() const;
};

class BevelData {
  Array<VertexCap> bevel_vert_caps_;

 public:
  MeshTopology topo;

  BevelData(const Mesh &mesh) : topo(mesh)
  {
  }
  ~BevelData()
  {
  }

  void init_caps_from_vertex_selection(const IndexMask selection);
};

/* Construct and return the VertexCap for vertex vert. */
static VertexCap construct_cap(const int vert, const MeshTopology &topo)
{
  Span<int> incident_edges = topo.vert_edges(vert);
  const int num_edges = incident_edges.size();
  if (num_edges == 0) {
    return VertexCap(vert, Span<int>(), Span<int>());
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
      return VertexCap(vert, ordered_edges.as_span(), ordered_faces.as_span());
    }
  }
  std::cout << "to implement: VertexCap for non-manifold edges\n";
  BLI_assert(false);
  return VertexCap();
}

void VertexCap::print() const
{
  std::cout << "cap at v" << vert << ": ";
  for (const int i : edges_.index_range()) {
    std::cout << "e" << edges_[i] << " ";
    if (faces_[i] == -1) {
      std::cout << "<gap> ";
    }
    else {
      std::cout << "f" << faces_[i] << " ";
    }
  }
  std::cout << "\n";
}

void BevelData::init_caps_from_vertex_selection(const IndexMask selection)
{
  bevel_vert_caps_.reinitialize(selection.size());
  threading::parallel_for(selection.index_range(), 1024, [&](const IndexRange range) {
    for (const int i : range) {
      bevel_vert_caps_[i] = construct_cap(selection[i], topo);
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
  bdata.init_caps_from_vertex_selection(selection);
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
