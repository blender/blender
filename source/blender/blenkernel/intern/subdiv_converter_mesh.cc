/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bke
 */

#include "subdiv_converter.hh"

#include <cstring>

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

struct ConverterStorage {
  Settings settings;
  const Mesh *mesh;
  Span<float3> vert_positions;
  Span<int2> edges;
  OffsetIndices<int> faces;
  Span<int> corner_verts;
  Span<int> corner_edges;

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
   */
  int *manifold_vertex_index;
  /* Indexed by vertex index from mesh, corresponds to whether this vertex has
   * infinite sharpness due to non-manifold topology.
   */
  BitVector<> infinite_sharp_vertices_map;
  /* Reverse mapping to above. */
  int *manifold_vertex_index_reverse;
  int *manifold_edge_index_reverse;
  /* Number of non-loose elements. */
  int num_manifold_vertices;
  int num_manifold_edges;
};

static OpenSubdiv_SchemeType get_scheme_type(const OpenSubdiv_Converter *converter)
{
#if BUGGY_SIMPLE_SCHEME_WORKAROUND
  (void)converter;
  return OSD_SCHEME_CATMARK;
#else
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
  if (storage->settings.is_simple) {
    return OSD_SCHEME_BILINEAR;
  }
  else {
    return OSD_SCHEME_CATMARK;
  }
#endif
}

static OpenSubdiv_VtxBoundaryInterpolation get_vtx_boundary_interpolation(
    const OpenSubdiv_Converter *converter)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
  return OpenSubdiv_VtxBoundaryInterpolation(
      converter_vtx_boundary_interpolation_from_settings(&storage->settings));
}

static OpenSubdiv_FVarLinearInterpolation get_fvar_linear_interpolation(
    const OpenSubdiv_Converter *converter)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
  return OpenSubdiv_FVarLinearInterpolation(
      converter_fvar_linear_from_settings(&storage->settings));
}

static bool specifies_full_topology(const OpenSubdiv_Converter * /*converter*/)
{
  return false;
}

static int get_num_edges(const OpenSubdiv_Converter *converter)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
  return storage->num_manifold_edges;
}

static int get_num_vertices(const OpenSubdiv_Converter *converter)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
  return storage->num_manifold_vertices;
}

static void get_face_vertices(const OpenSubdiv_Converter *converter,
                              int manifold_face_index,
                              int *manifold_face_vertices)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
  const IndexRange face = storage->faces[manifold_face_index];
  for (int i = 0; i < face.size(); i++) {
    const int vert = storage->corner_verts[face[i]];
    manifold_face_vertices[i] = storage->manifold_vertex_index[vert];
  }
}

static void get_edge_vertices(const OpenSubdiv_Converter *converter,
                              int manifold_edge_index,
                              int *manifold_edge_vertices)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
  const int edge_index = storage->manifold_edge_index_reverse[manifold_edge_index];
  const int2 &edge = storage->edges[edge_index];
  manifold_edge_vertices[0] = storage->manifold_vertex_index[edge[0]];
  manifold_edge_vertices[1] = storage->manifold_vertex_index[edge[1]];
}

static float get_edge_sharpness(const OpenSubdiv_Converter *converter, int manifold_edge_index)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
#if BUGGY_SIMPLE_SCHEME_WORKAROUND
  if (storage->settings.is_simple) {
    return 10.0f;
  }
#endif
  if (storage->cd_edge_crease.is_empty()) {
    return 0.0f;
  }
  const int edge_index = storage->manifold_edge_index_reverse[manifold_edge_index];
  return crease_to_sharpness(storage->cd_edge_crease[edge_index]);
}

static bool is_infinite_sharp_vertex(const OpenSubdiv_Converter *converter,
                                     int manifold_vertex_index)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
#if BUGGY_SIMPLE_SCHEME_WORKAROUND
  if (storage->settings.is_simple) {
    return true;
  }
#endif
  if (storage->infinite_sharp_vertices_map.is_empty()) {
    return false;
  }
  const int vertex_index = storage->manifold_vertex_index_reverse[manifold_vertex_index];
  return storage->infinite_sharp_vertices_map[vertex_index];
}

static float get_vertex_sharpness(const OpenSubdiv_Converter *converter, int manifold_vertex_index)
{
  ConverterStorage *storage = static_cast<ConverterStorage *>(converter->user_data);
  if (storage->cd_vertex_crease.is_empty()) {
    return 0.0f;
  }
  const int vertex_index = storage->manifold_vertex_index_reverse[manifold_vertex_index];
  return crease_to_sharpness(storage->cd_vertex_crease[vertex_index]);
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
  const int num_vert = mesh->verts_num;
  /* Initialize memory required for the operations. */
  if (storage->loop_uv_indices == nullptr) {
    storage->loop_uv_indices = MEM_malloc_arrayN<int>(size_t(mesh->corners_num),
                                                      "loop uv vertex index");
  }
  UvVertMap *uv_vert_map = BKE_mesh_uv_vert_map_create(storage->faces,
                                                       storage->corner_verts,
                                                       uv_map,
                                                       num_vert,
                                                       blender::float2(STD_UV_CONNECT_LIMIT),
                                                       true);
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
  MEM_SAFE_FREE(user_data->loop_uv_indices);
  MEM_freeN(user_data->manifold_vertex_index);
  MEM_freeN(user_data->manifold_vertex_index_reverse);
  MEM_freeN(user_data->manifold_edge_index_reverse);
  MEM_delete(user_data);
}

static void init_functions(OpenSubdiv_Converter *converter)
{
  converter->getSchemeType = get_scheme_type;
  converter->getVtxBoundaryInterpolation = get_vtx_boundary_interpolation;
  converter->getFVarLinearInterpolation = get_fvar_linear_interpolation;
  converter->specifiesFullTopology = specifies_full_topology;

  converter->getNumEdges = get_num_edges;
  converter->getNumVertices = get_num_vertices;

  converter->getFaceVertices = get_face_vertices;
  converter->getFaceEdges = nullptr;

  converter->getEdgeVertices = get_edge_vertices;
  converter->getNumEdgeFaces = nullptr;
  converter->getEdgeFaces = nullptr;
  converter->getEdgeSharpness = get_edge_sharpness;

  converter->getNumVertexEdges = nullptr;
  converter->getVertexEdges = nullptr;
  converter->getNumVertexFaces = nullptr;
  converter->getVertexFaces = nullptr;
  converter->isInfiniteSharpVertex = is_infinite_sharp_vertex;
  converter->getVertexSharpness = get_vertex_sharpness;

  converter->getNumUVLayers = get_num_uv_layers;
  converter->precalcUVLayer = precalc_uv_layer;
  converter->finishUVLayer = finish_uv_layer;
  converter->getNumUVCoordinates = get_num_uvs;
  converter->getFaceCornerUVIndex = get_face_corner_uv_index;

  converter->freeUserData = free_user_data;
}

static void initialize_manifold_index_array(const BitSpan not_used_map,
                                            const int num_elements,
                                            int **r_indices,
                                            int **r_indices_reverse,
                                            int *r_num_manifold_elements)
{
  int *indices = nullptr;
  if (r_indices != nullptr) {
    indices = MEM_malloc_arrayN<int>(size_t(num_elements), "manifold indices");
  }
  int *indices_reverse = nullptr;
  if (r_indices_reverse != nullptr) {
    indices_reverse = MEM_malloc_arrayN<int>(size_t(num_elements), "manifold indices reverse");
  }
  int offset = 0;
  for (int i = 0; i < num_elements; i++) {
    if (not_used_map.is_empty() || !not_used_map[i]) {
      if (indices != nullptr) {
        indices[i] = i - offset;
      }
      if (indices_reverse != nullptr) {
        indices_reverse[i - offset] = i;
      }
    }
    else {
      if (indices != nullptr) {
        indices[i] = -1;
      }
      offset++;
    }
  }
  if (r_indices != nullptr) {
    *r_indices = indices;
  }
  if (r_indices_reverse != nullptr) {
    *r_indices_reverse = indices_reverse;
  }
  *r_num_manifold_elements = num_elements - offset;
}

static void initialize_manifold_indices(ConverterStorage *storage)
{
  const Mesh *mesh = storage->mesh;
  const bke::LooseVertCache &loose_verts = mesh->verts_no_face();
  const bke::LooseEdgeCache &loose_edges = mesh->loose_edges();
  initialize_manifold_index_array(loose_verts.is_loose_bits,
                                  mesh->verts_num,
                                  &storage->manifold_vertex_index,
                                  &storage->manifold_vertex_index_reverse,
                                  &storage->num_manifold_vertices);
  initialize_manifold_index_array(loose_edges.is_loose_bits,
                                  mesh->edges_num,
                                  nullptr,
                                  &storage->manifold_edge_index_reverse,
                                  &storage->num_manifold_edges);
  /* Initialize infinite sharp mapping. */
  if (loose_edges.count > 0) {
    const Span<int2> edges = storage->edges;
    storage->infinite_sharp_vertices_map.resize(mesh->verts_num, false);
    for (int edge_index = 0; edge_index < mesh->edges_num; edge_index++) {
      if (loose_edges.is_loose_bits[edge_index]) {
        const int2 edge = edges[edge_index];
        storage->infinite_sharp_vertices_map[edge[0]].set();
        storage->infinite_sharp_vertices_map[edge[1]].set();
      }
    }
  }
}

static void init_user_data(OpenSubdiv_Converter *converter,
                           const Settings *settings,
                           const Mesh *mesh)
{
  ConverterStorage *user_data = MEM_new<ConverterStorage>(__func__);
  user_data->settings = *settings;
  user_data->mesh = mesh;
  user_data->vert_positions = mesh->vert_positions();
  user_data->edges = mesh->edges();
  user_data->faces = mesh->faces();
  user_data->corner_verts = mesh->corner_verts();
  user_data->corner_edges = mesh->corner_edges();
  if (settings->use_creases) {
    const AttributeAccessor attributes = mesh->attributes();
    user_data->cd_vertex_crease = *attributes.lookup<float>("crease_vert", AttrDomain::Point);
    user_data->cd_edge_crease = *attributes.lookup<float>("crease_edge", AttrDomain::Edge);
  }
  user_data->uv_map_names = mesh->uv_map_names();
  user_data->loop_uv_indices = nullptr;
  initialize_manifold_indices(user_data);
  converter->user_data = user_data;
}

void converter_init_for_mesh(OpenSubdiv_Converter *converter,
                             const Settings *settings,
                             const Mesh *mesh)
{
  converter->faces = mesh->faces();
  init_functions(converter);
  init_user_data(converter, settings, mesh);
}

}  // namespace blender::bke::subdiv
