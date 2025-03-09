/* SPDX-FileCopyrightText: 2011-2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_OPENSUBDIV

#  include "subd/osd.h"

#  include "scene/attribute.h"
#  include "scene/camera.h"
#  include "scene/mesh.h"

/* Specialization of TopologyRefinerFactory for OsdMesh */

namespace OpenSubdiv::OPENSUBDIV_VERSION::Far {

using namespace ccl;

template<>
bool TopologyRefinerFactory<Mesh>::resizeComponentTopology(TopologyRefiner &refiner,
                                                           const Mesh &mesh)
{
  setNumBaseVertices(refiner, mesh.get_verts().size());
  setNumBaseFaces(refiner, mesh.get_num_subd_faces());

  for (int i = 0; i < mesh.get_num_subd_faces(); i++) {
    setNumBaseFaceVertices(refiner, i, mesh.get_subd_num_corners()[i]);
  }

  return true;
}

template<>
bool TopologyRefinerFactory<Mesh>::assignComponentTopology(TopologyRefiner &refiner,
                                                           const Mesh &mesh)
{
  const array<int> &subd_face_corners = mesh.get_subd_face_corners();
  const array<int> &subd_start_corner = mesh.get_subd_start_corner();
  const array<int> &subd_num_corners = mesh.get_subd_num_corners();

  for (int i = 0; i < mesh.get_num_subd_faces(); i++) {
    IndexArray face_verts = getBaseFaceVertices(refiner, i);

    const int start_corner = subd_start_corner[i];
    int *corner = &subd_face_corners[start_corner];

    for (int j = 0; j < subd_num_corners[i]; j++, corner++) {
      face_verts[j] = *corner;
    }
  }

  return true;
}

template<>
bool TopologyRefinerFactory<Mesh>::assignComponentTags(TopologyRefiner &refiner, const Mesh &mesh)
{
  /* Historical maximum crease weight used at Pixar, influencing the maximum in OpenSubDiv. */
  static constexpr float CREASE_SCALE = 10.0f;

  const size_t num_creases = mesh.get_subd_creases_weight().size();
  const size_t num_vertex_creases = mesh.get_subd_vert_creases().size();

  /* The last loop is over the vertices, so early exit to avoid iterating them needlessly. */
  if (num_creases == 0 && num_vertex_creases == 0) {
    return true;
  }

  for (int i = 0; i < num_creases; i++) {
    const Mesh::SubdEdgeCrease crease = mesh.get_subd_crease(i);
    const Index edge = findBaseEdge(refiner, crease.v[0], crease.v[1]);

    if (edge != INDEX_INVALID) {
      setBaseEdgeSharpness(refiner, edge, crease.crease * CREASE_SCALE);
    }
  }

  std::map<int, float> vertex_creases;

  for (size_t i = 0; i < num_vertex_creases; ++i) {
    const int vertex_idx = mesh.get_subd_vert_creases()[i];
    const float weight = mesh.get_subd_vert_creases_weight()[i];

    vertex_creases[vertex_idx] = weight * CREASE_SCALE;
  }

  for (int i = 0; i < mesh.get_verts().size(); i++) {
    float sharpness = 0.0f;
    const std::map<int, float>::const_iterator iter = vertex_creases.find(i);

    if (iter != vertex_creases.end()) {
      sharpness = iter->second;
    }

    const ConstIndexArray vert_edges = getBaseVertexEdges(refiner, i);

    if (vert_edges.size() == 2) {
      const float sharpness0 = refiner.getLevel(0).getEdgeSharpness(vert_edges[0]);
      const float sharpness1 = refiner.getLevel(0).getEdgeSharpness(vert_edges[1]);

      sharpness += min(sharpness0, sharpness1);
      sharpness = min(sharpness, CREASE_SCALE);
    }

    if (sharpness != 0.0f) {
      setBaseVertexSharpness(refiner, i, sharpness);
    }
  }

  return true;
}

template<>
bool TopologyRefinerFactory<Mesh>::assignFaceVaryingTopology(TopologyRefiner & /*refiner*/,
                                                             const Mesh & /*mesh*/)
{
  return true;
}

template<>
void TopologyRefinerFactory<Mesh>::reportInvalidTopology(TopologyError /*err_code*/,
                                                         const char * /*msg*/,
                                                         const Mesh & /*mesh*/)
{
}
}  // namespace OpenSubdiv::OPENSUBDIV_VERSION::Far

CCL_NAMESPACE_BEGIN

/* OsdData */

void OsdData::build_from_mesh(Mesh *mesh_)
{
  mesh = mesh_;

  /* type and options */
  const Sdc::SchemeType type = Sdc::SCHEME_CATMARK;

  Sdc::Options options;
  options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);

  /* create refiner */
  refiner.reset(Far::TopologyRefinerFactory<Mesh>::Create(
      *mesh, Far::TopologyRefinerFactory<Mesh>::Options(type, options)));

  /* adaptive refinement */
  const int max_isolation = 3;  // TODO: get from Blender
  refiner->RefineAdaptive(Far::TopologyRefiner::AdaptiveOptions(max_isolation));

  /* create patch table */
  Far::PatchTableFactory::Options patch_options;
  patch_options.endCapType = Far::PatchTableFactory::Options::ENDCAP_GREGORY_BASIS;

  patch_table.reset(Far::PatchTableFactory::Create(*refiner, patch_options));

  /* interpolate verts */
  const int num_refiner_verts = refiner->GetNumVerticesTotal();
  const int num_local_points = patch_table->GetNumLocalPoints();

  verts.resize(num_refiner_verts + num_local_points);
  for (int i = 0; i < mesh->get_verts().size(); i++) {
    verts[i].value = mesh->get_verts()[i];
  }

  OsdValue<float3> *src = verts.data();
  for (int i = 0; i < refiner->GetMaxLevel(); i++) {
    OsdValue<float3> *dest = src + refiner->GetLevel(i).GetNumVertices();
    Far::PrimvarRefiner(*refiner).Interpolate(i + 1, src, dest);
    src = dest;
  }

  if (num_local_points) {
    patch_table->ComputeLocalPointValues(verts.data(), &verts[num_refiner_verts]);
  }

  /* create patch map */
  patch_map = make_unique<Far::PatchMap>(*patch_table);
}

/* OsdPatch */

void OsdPatch::eval(float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, float v)
{
  const Far::PatchTable::PatchHandle *handle = osd_data->patch_map->FindPatch(
      patch_index, (double)u, (double)v);
  assert(handle);

  float p_weights[20];
  float du_weights[20];
  float dv_weights[20];
  osd_data->patch_table->EvaluateBasis(*handle, u, v, p_weights, du_weights, dv_weights);

  const Far::ConstIndexArray cv = osd_data->patch_table->GetPatchVertices(*handle);

  float3 du;
  float3 dv;
  if (P) {
    *P = zero_float3();
  }
  du = zero_float3();
  dv = zero_float3();

  for (int i = 0; i < cv.size(); i++) {
    const float3 p = osd_data->verts[cv[i]].value;

    if (P) {
      *P += p * p_weights[i];
    }
    du += p * du_weights[i];
    dv += p * dv_weights[i];
  }

  if (dPdu) {
    *dPdu = du;
  }
  if (dPdv) {
    *dPdv = dv;
  }
  if (N) {
    *N = cross(du, dv);

    const float t = len(*N);
    *N = (t != 0.0f) ? *N / t : make_float3(0.0f, 0.0f, 1.0f);
  }
}

CCL_NAMESPACE_END

#endif
