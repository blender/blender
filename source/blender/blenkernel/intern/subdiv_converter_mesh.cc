/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "subdiv_converter.hh"

#include <cstring>

#include "BLI_array_utils.hh"
#include "BLI_task.hh"

#include "BKE_attribute.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"
#include "BKE_subdiv.hh"

#include "MEM_guardedalloc.h"

#include "opensubdiv_converter_capi.hh"

namespace blender::bke::subdiv {

/* Enable work-around for non-working CPU evaluator when using bilinear scheme.
 * This forces Catmark scheme with all edges marked as infinitely sharp. */
#define BUGGY_SIMPLE_SCHEME_WORKAROUND 1

struct ConverterStorage : NonMovable {
  Settings settings;
  const Mesh *mesh;
  /* Edge vertices, either referencing Mesh::edges() or #edges_manifold. */
  Span<int2> edges;
  Array<int2> edges_manifold;

  OffsetIndices<int> faces;
  /* Vertex indices of face corners, using manifold vertex indices (see #manifold_vertex_index
   * below). Either referencing of Mesh::corner_verts()  #corner_verts_manifold. */
  Span<int> corner_verts;
  Array<int> corner_verts_manifold;

  /* Sharpness of every manifold edge and vertex, or empty when nothing is sharp. See
   * #OpenSubdiv_Converter::edge_sharpness. */
  Array<float> edge_sharpness;
  Array<float> vert_sharpness;

  VectorSet<StringRefNull> uv_map_names;

  /* CustomData layer for vertex sharpnesses. */
  VArraySpan<float> cd_vertex_crease;
  /* CustomData layer for edge sharpness. */
  VArraySpan<float> cd_edge_crease;
  /* Indexed by loop index, value denotes index of face-varying vertex
   * which corresponds to the UV coordinate.
   */
  int *loop_uv_indices;
  int num_uv_coordinates;
  /* Indexed by coarse mesh elements, gives index of corresponding element
   * with ignoring all non-manifold entities.
   *
   * NOTE: This isn't strictly speaking manifold, this is more like non-loose
   * geometry index. As in, index of element as if there were no loose edges
   * or vertices in the mesh.
   *
   * Left empty when the mapping is the identity for all elements that are kept, which is the
   * case whenever the removed (loose) elements are already a contiguous run at the end of the
   * array. See #initialize_manifold_index_map.
   */
  Array<int> manifold_verts;
  /* Reverse mapping to above. */
  Array<int> manifold_vert_to_orig;
  Array<int> manifold_edge_to_orig;
  /* Number of non-loose elements. */
  int num_manifold_vertices;
  int num_manifold_edges;
};

static OpenSubdiv_SchemeType get_scheme_type(const Settings &settings)
{
#if BUGGY_SIMPLE_SCHEME_WORKAROUND
  (void)settings;
  return OSD_SCHEME_CATMARK;
#else
  if (settings.is_simple) {
    return OSD_SCHEME_BILINEAR;
  }
  else {
    return OSD_SCHEME_CATMARK;
  }
#endif
}

static bool specifies_full_topology(const OpenSubdiv_Converter * /*converter*/)
{
  return false;
}

/* Both `original_to_manifold()` and `manifold_to_original()` treat an empty map as the identity
 * mapping. This is the case whenever the elements not passed to OpenSubdiv (loose vertices or
 * edges) are already a contiguous run at the end of the array, see
 * #initialize_manifold_index_map. */

static int original_to_manifold(const Array<int> &forward_map, const int original_index)
{
  return forward_map.is_empty() ? original_index : forward_map[original_index];
}

static int manifold_to_original(const Array<int> &reverse_map, const int manifold_index)
{
  return reverse_map.is_empty() ? manifold_index : reverse_map[manifold_index];
}

static int get_num_uv_layers(const OpenSubdiv_Converter *converter)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
  return storage->uv_map_names.size();
}

static void precalc_uv_layer(const OpenSubdiv_Converter *converter, const int layer_index)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
  const Mesh *mesh = storage->mesh;
  const StringRef name = storage->uv_map_names[layer_index];
  const bke::AttributeAccessor attributes = mesh->attributes();
  const VArraySpan uv_map = *attributes.lookup<float2>(name, bke::AttrDomain::Corner);
  const int num_vert = storage->num_manifold_vertices;
  /* Initialize memory required for the operations. */
  if (storage->loop_uv_indices == nullptr) {
    storage->loop_uv_indices = MEM_new_array_uninitialized<int>(size_t(mesh->corners_num),
                                                                "loop uv vertex index");
  }
  UvVertMap *uv_vert_map = BKE_mesh_uv_vert_map_create(
      storage->faces, storage->corner_verts, uv_map, num_vert, float2(STD_UV_CONNECT_LIMIT), true);
  /* NOTE: First UV vertex is supposed to be always marked as separate. */
  storage->num_uv_coordinates = -1;
  for (int vertex_index = 0; vertex_index < num_vert; vertex_index++) {
    const UvMapVert *uv_vert = BKE_mesh_uv_vert_map_get_vert(uv_vert_map, vertex_index);
    while (uv_vert != nullptr) {
      if (uv_vert->separate) {
        storage->num_uv_coordinates++;
      }
      const IndexRange face = storage->faces[uv_vert->face_index];
      const int global_loop_index = face.start() + uv_vert->loop_of_face_index;
      storage->loop_uv_indices[global_loop_index] = storage->num_uv_coordinates;
      uv_vert = uv_vert->next;
    }
  }
  /* So far this value was used as a 0-based index, actual number of UV
   * vertices is 1 more.
   */
  storage->num_uv_coordinates += 1;
  BKE_mesh_uv_vert_map_free(uv_vert_map);
}

static void finish_uv_layer(const OpenSubdiv_Converter * /*converter*/) {}

static int get_num_uvs(const OpenSubdiv_Converter *converter)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
  return storage->num_uv_coordinates;
}

static int get_face_corner_uv_index(const OpenSubdiv_Converter *converter,
                                    const int face_index,
                                    const int corner)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
  const IndexRange face = storage->faces[face_index];
  return storage->loop_uv_indices[face.start() + corner];
}

static void free_user_data(const OpenSubdiv_Converter *converter)
{
  ConverterStorage *user_data = static_cast<ConverterStorage *>(converter->user_data);
  MEM_SAFE_DELETE(user_data->loop_uv_indices);
  MEM_delete(user_data);
}

static void init_functions(OpenSubdiv_Converter *converter)
{
  converter->specifiesFullTopology = specifies_full_topology;

  converter->getFaceEdges = nullptr;

  converter->getNumEdgeFaces = nullptr;
  converter->getEdgeFaces = nullptr;

  converter->getNumVertexEdges = nullptr;
  converter->getVertexEdges = nullptr;
  converter->getNumVertexFaces = nullptr;
  converter->getVertexFaces = nullptr;

  converter->getNumUVLayers = get_num_uv_layers;
  converter->precalcUVLayer = precalc_uv_layer;
  converter->finishUVLayer = finish_uv_layer;
  converter->getNumUVCoordinates = get_num_uvs;
  converter->getFaceCornerUVIndex = get_face_corner_uv_index;

  converter->freeUserData = free_user_data;
}

/* Whether `unused_mask` (e.g. loose vertices/edges) is empty, or is exactly a contiguous section
 * of indices at the end. In that case every kept element already has the same index whether or not
 * the unused elements are removed, so no remapping is necessary. */
static bool unused_is_trailing_range(const IndexMask &unused_mask, const int num_elements)
{
  if (unused_mask.is_empty()) {
    return true;
  }
  const std::optional<IndexRange> range = unused_mask.to_range();
  return range.has_value() && range->one_after_last() == num_elements;
}

/* Builds the mapping between original and manifold (i.e. skipping `unused_mask`) indices for a
 * set of coarse mesh elements (vertices or edges). Returns the number of manifold elements. */
static int initialize_manifold_index_map(const IndexMask &unused_mask,
                                         const int num_elements,
                                         Array<int> *r_index_forward,
                                         Array<int> *r_index_reverse)
{
  const int num_manifold_elements = num_elements - unused_mask.size();
  if (unused_is_trailing_range(unused_mask, num_elements)) {
    return num_manifold_elements;
  }
  IndexMaskMemory memory;
  const IndexMask used_mask = unused_mask.complement(IndexRange(num_elements), memory);
  if (r_index_forward != nullptr) {
    r_index_forward->reinitialize(num_elements);
    index_mask::build_reverse_map<int>(used_mask, *r_index_forward);
  }
  if (r_index_reverse != nullptr) {
    r_index_reverse->reinitialize(num_manifold_elements);
    used_mask.to_indices<int>(*r_index_reverse);
  }
  return num_manifold_elements;
}

/* Builds the manifold-space edges array exposed as `converter->edges`: for each manifold edge
 * index, the pair of manifold vertex indices, skipping loose edges. */
static void initialize_manifold_edges(ConverterStorage &storage)
{
  const Span<int2> mesh_edges = storage.mesh->edges();
  if (storage.manifold_edge_to_orig.is_empty() && storage.manifold_verts.is_empty()) {
    storage.edges = mesh_edges.take_front(storage.num_manifold_edges);
    return;
  }
  storage.edges_manifold.reinitialize(storage.num_manifold_edges);
  const MutableSpan<int2> edges_manifold = storage.edges_manifold;
  threading::parallel_for(edges_manifold.index_range(), 4096, [&](const IndexRange range) {
    for (const int manifold_edge_index : range) {
      const int edge_index = manifold_to_original(storage.manifold_edge_to_orig,
                                                  manifold_edge_index);
      const int2 &edge = mesh_edges[edge_index];
      edges_manifold[manifold_edge_index] = int2(
          original_to_manifold(storage.manifold_verts, edge[0]),
          original_to_manifold(storage.manifold_verts, edge[1]));
    }
  });
  storage.edges = storage.edges_manifold;
}

static void initialize_manifold_indices(ConverterStorage &storage)
{
  const Mesh *mesh = storage.mesh;
  const IndexMask &loose_verts = mesh->verts_no_face();
  const IndexMask &loose_edges = mesh->loose_edges();

  storage.num_manifold_vertices = initialize_manifold_index_map(
      loose_verts, mesh->verts_num, &storage.manifold_verts, &storage.manifold_vert_to_orig);
  if (storage.manifold_verts.is_empty()) {
    storage.corner_verts = mesh->corner_verts();
  }
  else {
    const Span<int> mesh_corner_verts = mesh->corner_verts();
    storage.corner_verts_manifold.reinitialize(mesh_corner_verts.size());
    array_utils::gather(storage.manifold_verts.as_span(),
                        mesh_corner_verts,
                        storage.corner_verts_manifold.as_mutable_span());
    storage.corner_verts = storage.corner_verts_manifold;
  }

  storage.num_manifold_edges = initialize_manifold_index_map(
      loose_edges, mesh->edges_num, nullptr, &storage.manifold_edge_to_orig);
  initialize_manifold_edges(storage);
}

/* Fills `sharpness` with the sharpness of every manifold element, from the crease attribute of the
 * original elements. `manifold_to_orig` is empty when it is the identity mapping, in which case
 * the manifold elements are simply the first `sharpness.size()` original ones. */
static void gather_manifold_sharpness(const Span<float> creases,
                                      const Span<int> manifold_to_orig,
                                      const MutableSpan<float> sharpness)
{
  if (manifold_to_orig.is_empty()) {
    array_utils::copy(creases.take_front(sharpness.size()), sharpness);
  }
  else {
    array_utils::gather(creases, manifold_to_orig, sharpness);
  }
  threading::parallel_for(sharpness.index_range(), 4096, [&](const IndexRange range) {
    for (const int i : range) {
      sharpness[i] = crease_to_sharpness(sharpness[i]);
    }
  });
}

/* Builds the sharpness of every manifold edge, exposed as `converter->edge_sharpness`. Left empty
 * when no edge is sharp, which OpenSubdiv treats the same as all-zero sharpness. */
static void initialize_edge_sharpness(ConverterStorage &storage)
{
#if BUGGY_SIMPLE_SCHEME_WORKAROUND
  if (storage.settings.is_simple) {
    /* Emulate the bilinear scheme by making every edge infinitely sharp. */
    storage.edge_sharpness = Array<float>(storage.num_manifold_edges,
                                          OPENSUBDIV_SHARPNESS_INFINITE);
    return;
  }
#endif
  if (storage.cd_edge_crease.is_empty()) {
    return;
  }
  storage.edge_sharpness.reinitialize(storage.num_manifold_edges);
  gather_manifold_sharpness(
      storage.cd_edge_crease, storage.manifold_edge_to_orig, storage.edge_sharpness);
}

/* Builds the sharpness of every manifold vertex, exposed as `converter->vert_sharpness`. Besides
 * the vertex crease this includes the infinite sharpness given to vertices adjacent to a loose
 * edge, since OpenSubdiv expects non-manifold vertices to be sharp. Left empty when no vertex is
 * sharp. */
static void initialize_vert_sharpness(ConverterStorage &storage)
{
#if BUGGY_SIMPLE_SCHEME_WORKAROUND
  if (storage.settings.is_simple) {
    /* Emulate the bilinear scheme by making every vertex infinitely sharp. */
    storage.vert_sharpness = Array<float>(storage.num_manifold_vertices,
                                          OPENSUBDIV_SHARPNESS_INFINITE);
    return;
  }
#endif
  const Mesh *mesh = storage.mesh;
  const IndexMask &loose_edges = mesh->loose_edges();
  if (storage.cd_vertex_crease.is_empty() && loose_edges.is_empty()) {
    return;
  }

  storage.vert_sharpness.reinitialize(storage.num_manifold_vertices);
  const MutableSpan<float> sharpness = storage.vert_sharpness;
  if (storage.cd_vertex_crease.is_empty()) {
    sharpness.fill(0.0f);
  }
  else {
    gather_manifold_sharpness(storage.cd_vertex_crease, storage.manifold_vert_to_orig, sharpness);
  }

  if (loose_edges.is_empty()) {
    return;
  }

  /* Vertices adjacent to a loose edge are infinitely sharp, which overrides the vertex crease.
   * The map is indexed by the original vertex index, since such a vertex may itself be loose, in
   * which case it is not one of the manifold vertices at all. */
  BitVector<> infinite_sharp_verts(mesh->verts_num, false);
  const Span<int2> edges = mesh->edges();
  loose_edges.foreach_index([&](const int edge_index) {
    const int2 edge = edges[edge_index];
    infinite_sharp_verts[edge[0]].set();
    infinite_sharp_verts[edge[1]].set();
  });
  threading::parallel_for(sharpness.index_range(), 4096, [&](const IndexRange range) {
    for (const int manifold_vertex_index : range) {
      const int vertex_index = manifold_to_original(storage.manifold_vert_to_orig,
                                                    manifold_vertex_index);
      if (infinite_sharp_verts[vertex_index]) {
        sharpness[manifold_vertex_index] = OPENSUBDIV_SHARPNESS_INFINITE;
      }
    }
  });
}

static void init_user_data(OpenSubdiv_Converter *converter,
                           const Settings *settings,
                           const Mesh *mesh)
{
  ConverterStorage *user_data = MEM_new<ConverterStorage>(__func__);
  user_data->settings = *settings;
  user_data->mesh = mesh;
  user_data->faces = mesh->faces();
  if (settings->use_creases) {
    const AttributeAccessor attributes = mesh->attributes();
    user_data->cd_vertex_crease = *attributes.lookup<float>("crease_vert", AttrDomain::Point);
    user_data->cd_edge_crease = *attributes.lookup<float>("crease_edge", AttrDomain::Edge);
  }
  user_data->uv_map_names = mesh->uv_map_names();
  user_data->loop_uv_indices = nullptr;
  initialize_manifold_indices(*user_data);
  initialize_edge_sharpness(*user_data);
  initialize_vert_sharpness(*user_data);
  converter->user_data = user_data;
}

void converter_init_for_mesh(OpenSubdiv_Converter *converter,
                             const Settings *settings,
                             const Mesh *mesh)
{
  init_functions(converter);
  init_user_data(converter, settings, mesh);
  const auto &storage = *static_cast<ConverterStorage *>(converter->user_data);
  converter->verts_num = storage.num_manifold_vertices;
  converter->face_offsets = storage.mesh->face_offsets();
  converter->edges = storage.edges.cast<std::pair<int, int>>();
  converter->corner_verts = storage.corner_verts;
  converter->edge_sharpness = storage.edge_sharpness;
  converter->vert_sharpness = storage.vert_sharpness;
  converter->scheme_type = get_scheme_type(*settings);
  converter->vtx_boundary_interpolation = OpenSubdiv_VtxBoundaryInterpolation(
      converter_vtx_boundary_interpolation_from_settings(settings));
  converter->fvar_linear_interpolation = OpenSubdiv_FVarLinearInterpolation(
      converter_fvar_linear_from_settings(settings));
}

}  // namespace blender::bke::subdiv
