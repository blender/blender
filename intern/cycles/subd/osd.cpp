/* SPDX-FileCopyrightText: 2011-2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifdef WITH_OPENSUBDIV

#  include "subd/osd.h"

#  include "scene/attribute.h"
#  include "scene/mesh.h"

#  include "util/log.h"

/* Specialization of TopologyRefinerFactory for OsdMesh */

namespace OpenSubdiv::OPENSUBDIV_VERSION::Far {

using namespace ccl;

template<>
bool TopologyRefinerFactory<OsdMesh>::resizeComponentTopology(TopologyRefiner &refiner,
                                                              OsdMesh const &osd_mesh)
{
  const Mesh &mesh = osd_mesh.mesh;

  const int num_base_verts = mesh.get_num_subd_base_verts();
  const int num_base_faces = mesh.get_num_subd_faces();
  const int *subd_num_corners = mesh.get_subd_num_corners().data();

  setNumBaseVertices(refiner, num_base_verts);
  setNumBaseFaces(refiner, num_base_faces);

  for (int i = 0; i < num_base_faces; i++) {
    setNumBaseFaceVertices(refiner, i, subd_num_corners[i]);
  }

  return true;
}

template<>
bool TopologyRefinerFactory<OsdMesh>::assignComponentTopology(TopologyRefiner &refiner,
                                                              OsdMesh const &osd_mesh)
{
  const Mesh &mesh = osd_mesh.mesh;

  const int num_base_faces = mesh.get_num_subd_faces();

  const int *subd_face_corners = mesh.get_subd_face_corners().data();
  const int *subd_start_corner = mesh.get_subd_start_corner().data();
  const int *subd_num_corners = mesh.get_subd_num_corners().data();

  for (int i = 0; i < num_base_faces; i++) {
    IndexArray face_verts = getBaseFaceVertices(refiner, i);

    const int start_corner = subd_start_corner[i];
    const int *corner = &subd_face_corners[start_corner];

    for (int j = 0; j < subd_num_corners[i]; j++, corner++) {
      face_verts[j] = *corner;
    }
  }

  return true;
}

template<>
bool TopologyRefinerFactory<OsdMesh>::assignComponentTags(TopologyRefiner &refiner,
                                                          OsdMesh const &osd_mesh)
{
  const Mesh &mesh = osd_mesh.mesh;

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

  const int num_base_verts = mesh.get_num_subd_base_verts();

  for (int i = 0; i < num_base_verts; i++) {
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

template<typename T>
static void merge_smooth_fvar(const Mesh &mesh,
                              const Attribute &subd_attr,
                              OsdMesh::MergedFVar &merged_fvar,
                              vector<int> &merged_next,
                              vector<int> &merged_face_corners)
{
  const int num_base_verts = mesh.get_num_subd_base_verts();
  const int num_base_faces = mesh.get_num_subd_faces();
  const int *subd_face_corners = mesh.get_subd_face_corners().data();

  const T *values = reinterpret_cast<const T *>(subd_attr.data());

  merged_fvar.values.resize(num_base_verts * sizeof(T));

  // Merge identical corner values with the same vertex. The first value is stored at the vertex
  // index, and any different values are pushed backed onto the array. merged_next creates a
  // linked list between all values for the same vertex.
  const int state_uninitialized = 0;
  const int state_end = -1;
  merged_next.resize(num_base_verts, state_uninitialized);

  for (int f = 0, i = 0; f < num_base_faces; f++) {
    Mesh::SubdFace face = mesh.get_subd_face(f);

    for (int corner = 0; corner < face.num_corners; corner++) {
      int v = subd_face_corners[face.start_corner + corner];
      const T value = values[i++];

      if (merged_next[v] == state_uninitialized) {
        // First corner to initialize vertex.
        reinterpret_cast<T *>(merged_fvar.values.data())[v] = value;
        merged_next[v] = state_end;
        merged_face_corners.push_back(v);
      }
      else {
        // Find vertex with matching value, following linked list per vertex.
        int v_prev = v;
        for (; v != state_end; v_prev = v, v = merged_next[v]) {
          if (reinterpret_cast<T *>(merged_fvar.values.data())[v] == value) {
            // Matching value found, reuse merged vertex.
            merged_face_corners.push_back(v);
            break;
          }
        }

        if (v == state_end) {
          // Non-matching value, add new merged vertex and add to linked list.
          const int next = merged_next.size();
          merged_fvar.values.resize((next + 1) * sizeof(T));
          reinterpret_cast<T *>(merged_fvar.values.data())[next] = value;
          merged_next.push_back(state_end);
          merged_next[v_prev] = next;
          merged_face_corners.push_back(next);
        }
      }
    }
  }
}

template<>
bool TopologyRefinerFactory<OsdMesh>::assignFaceVaryingTopology(TopologyRefiner &refiner,
                                                                OsdMesh const &osd_mesh)
{
  const Mesh &mesh = osd_mesh.mesh;
  auto &merged_fvars = const_cast<OsdMesh &>(osd_mesh).merged_fvars;

  for (const Attribute &subd_attr : mesh.subd_attributes.attributes) {
    if (!osd_mesh.use_smooth_fvar(subd_attr)) {
      continue;
    }

    // Created merged FVar, for use in subdivide_attribute_corner_smooth.
    OsdMesh::MergedFVar merged_fvar{subd_attr};

    vector<int> merged_next;
    vector<int> merged_face_corners;

    if (subd_attr.element == ATTR_ELEMENT_CORNER_BYTE) {
      merge_smooth_fvar<uchar4>(mesh, subd_attr, merged_fvar, merged_next, merged_face_corners);
    }
    else if (Attribute::same_storage(subd_attr.type, TypeFloat)) {
      merge_smooth_fvar<float>(mesh, subd_attr, merged_fvar, merged_next, merged_face_corners);
    }
    else if (Attribute::same_storage(subd_attr.type, TypeFloat2)) {
      merge_smooth_fvar<float2>(mesh, subd_attr, merged_fvar, merged_next, merged_face_corners);
    }
    else if (Attribute::same_storage(subd_attr.type, TypeVector)) {
      merge_smooth_fvar<float3>(mesh, subd_attr, merged_fvar, merged_next, merged_face_corners);
    }
    else if (Attribute::same_storage(subd_attr.type, TypeFloat4)) {
      merge_smooth_fvar<float4>(mesh, subd_attr, merged_fvar, merged_next, merged_face_corners);
    }

    // Create FVar channel and topology for OpenUSD.
    merged_fvar.channel = createBaseFVarChannel(refiner, merged_next.size());

    const int num_base_faces = mesh.get_num_subd_faces();
    for (int f = 0, i = 0; f < num_base_faces; f++) {
      Far::IndexArray dst_face_uvs = getBaseFaceFVarValues(refiner, f, merged_fvar.channel);
      const int num_corners = dst_face_uvs.size();
      for (int corner = 0; corner < num_corners; corner++) {
        dst_face_uvs[corner] = merged_face_corners[i++];
      }
    }

    merged_fvars.push_back(std::move(merged_fvar));
  }

  return true;
}

template<>
void TopologyRefinerFactory<OsdMesh>::reportInvalidTopology(TopologyError /*err_code*/,
                                                            char const *msg,
                                                            OsdMesh const &osd_mesh)
{
  const Mesh &mesh = osd_mesh.mesh;
  LOG_WARNING << "Invalid subdivision topology for '" << mesh.name.c_str() << "': " << msg;
}
}  // namespace OpenSubdiv::OPENSUBDIV_VERSION::Far

CCL_NAMESPACE_BEGIN

/* OsdMesh */

Sdc::Options OsdMesh::sdc_options()
{
  Sdc::Options options;
  switch (mesh.get_subdivision_fvar_interpolation()) {
    case Mesh::SUBDIVISION_FVAR_LINEAR_NONE:
      options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_NONE);
      break;
    case Mesh::SUBDIVISION_FVAR_LINEAR_CORNERS_ONLY:
      options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_CORNERS_ONLY);
      break;
    case Mesh::SUBDIVISION_FVAR_LINEAR_CORNERS_PLUS1:
      options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_CORNERS_PLUS1);
      break;
    case Mesh::SUBDIVISION_FVAR_LINEAR_CORNERS_PLUS2:
      options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_CORNERS_PLUS2);
      break;
    case Mesh::SUBDIVISION_FVAR_LINEAR_BOUNDARIES:
      options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_BOUNDARIES);
      break;
    case Mesh::SUBDIVISION_FVAR_LINEAR_ALL:
      options.SetFVarLinearInterpolation(Sdc::Options::FVAR_LINEAR_ALL);
      break;
  }
  switch (mesh.get_subdivision_boundary_interpolation()) {
    case Mesh::SUBDIVISION_BOUNDARY_NONE:
      options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_NONE);
      break;
    case Mesh::SUBDIVISION_BOUNDARY_EDGE_ONLY:
      options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);
      break;
    case Mesh::SUBDIVISION_BOUNDARY_EDGE_AND_CORNER:
      options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER);
      break;
  }
  return options;
}

bool OsdMesh::use_smooth_fvar(const Attribute &attr) const
{
  return mesh.get_subdivision_fvar_interpolation() != Mesh::SUBDIVISION_FVAR_LINEAR_ALL &&
         attr.element == ATTR_ELEMENT_CORNER &&
         (attr.std == ATTR_STD_UV || (attr.flags & ATTR_SUBDIVIDE_SMOOTH_FVAR));
}

bool OsdMesh::use_smooth_fvar() const
{
  for (const Attribute &attr : mesh.subd_attributes.attributes) {
    if (use_smooth_fvar(attr)) {
      return true;
    }
  }

  return false;
}

/* OsdData */

void OsdData::build(OsdMesh &osd_mesh)
{
  /* create refiner */
  refiner.reset(Far::TopologyRefinerFactory<OsdMesh>::Create(
      osd_mesh,
      Far::TopologyRefinerFactory<OsdMesh>::Options(Sdc::SCHEME_CATMARK, osd_mesh.sdc_options())));

  /* adaptive refinement */
  const bool has_fvar = osd_mesh.use_smooth_fvar();
  const int max_isolation = 3;  // TODO: get from Blender

  Far::TopologyRefiner::AdaptiveOptions adaptive_options(max_isolation);
  adaptive_options.considerFVarChannels = has_fvar;
  adaptive_options.useInfSharpPatch = true;
  refiner->RefineAdaptive(adaptive_options);

  /* create patch table */
  Far::PatchTableFactory::Options patch_options;
  patch_options.endCapType = Far::PatchTableFactory::Options::ENDCAP_GREGORY_BASIS;
  patch_options.generateFVarTables = has_fvar;
  patch_options.generateFVarLegacyLinearPatches = false;
  patch_options.useInfSharpPatch = true;

  patch_table.reset(Far::PatchTableFactory::Create(*refiner, patch_options));

  /* interpolate verts */
  const int num_refiner_verts = refiner->GetNumVerticesTotal();
  const int num_local_points = patch_table->GetNumLocalPoints();
  const int num_base_verts = osd_mesh.mesh.get_num_subd_base_verts();
  const float3 *verts_data = osd_mesh.mesh.get_verts().data();

  refined_verts.resize(num_refiner_verts + num_local_points);
  for (int i = 0; i < num_base_verts; i++) {
    refined_verts[i].value = verts_data[i];
  }

  OsdValue<float3> *src = refined_verts.data();
  for (int i = 0; i < refiner->GetMaxLevel(); i++) {
    OsdValue<float3> *dest = src + refiner->GetLevel(i).GetNumVertices();
    Far::PrimvarRefiner(*refiner).Interpolate(i + 1, src, dest);
    src = dest;
  }

  if (num_local_points) {
    patch_table->ComputeLocalPointValues(refined_verts.data(), &refined_verts[num_refiner_verts]);
  }

  /* Create patch map */
  patch_map = make_unique<Far::PatchMap>(*patch_table);
}

/* OsdPatch */

void OsdPatch::eval(
    float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, const float v) const
{
  const Far::PatchTable::PatchHandle &handle = *osd_data.patch_map->FindPatch(
      patch_index, (double)u, (double)v);

  float p_weights[20], du_weights[20], dv_weights[20];
  osd_data.patch_table->EvaluateBasis(handle, u, v, p_weights, du_weights, dv_weights);

  const Far::ConstIndexArray cv = osd_data.patch_table->GetPatchVertices(handle);

  if (P) {
    *P = zero_float3();
  }
  float3 du = zero_float3();
  float3 dv = zero_float3();

  for (int i = 0; i < cv.size(); i++) {
    const float3 p = osd_data.refined_verts[cv[i]].value;

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
    *N = safe_normalize_fallback(cross(du, dv), make_float3(0.0f, 0.0f, 1.0f));
  }
}

CCL_NAMESPACE_END

#endif
