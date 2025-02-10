/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "scene/attribute.h"
#include "scene/camera.h"
#include "scene/mesh.h"

#include "subd/patch.h"
#include "subd/patch_table.h"
#include "subd/split.h"

#include "util/algorithm.h"

CCL_NAMESPACE_BEGIN

#ifdef WITH_OPENSUBDIV

CCL_NAMESPACE_END

#  include <opensubdiv/far/patchMap.h>
#  include <opensubdiv/far/patchTableFactory.h>
#  include <opensubdiv/far/primvarRefiner.h>
#  include <opensubdiv/far/topologyRefinerFactory.h>

/* specializations of TopologyRefinerFactory for ccl::Mesh */

namespace OpenSubdiv::OPENSUBDIV_VERSION::Far {
template<>
bool TopologyRefinerFactory<ccl::Mesh>::resizeComponentTopology(TopologyRefiner &refiner,
                                                                const ccl::Mesh &mesh)
{
  setNumBaseVertices(refiner, mesh.get_verts().size());
  setNumBaseFaces(refiner, mesh.get_num_subd_faces());

  for (int i = 0; i < mesh.get_num_subd_faces(); i++) {
    setNumBaseFaceVertices(refiner, i, mesh.get_subd_num_corners()[i]);
  }

  return true;
}

template<>
bool TopologyRefinerFactory<ccl::Mesh>::assignComponentTopology(TopologyRefiner &refiner,
                                                                const ccl::Mesh &mesh)
{
  const ccl::array<int> &subd_face_corners = mesh.get_subd_face_corners();
  const ccl::array<int> &subd_start_corner = mesh.get_subd_start_corner();
  const ccl::array<int> &subd_num_corners = mesh.get_subd_num_corners();

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
bool TopologyRefinerFactory<ccl::Mesh>::assignComponentTags(TopologyRefiner &refiner,
                                                            const ccl::Mesh &mesh)
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
    const ccl::Mesh::SubdEdgeCrease crease = mesh.get_subd_crease(i);
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

      sharpness += ccl::min(sharpness0, sharpness1);
      sharpness = ccl::min(sharpness, CREASE_SCALE);
    }

    if (sharpness != 0.0f) {
      setBaseVertexSharpness(refiner, i, sharpness);
    }
  }

  return true;
}

template<>
bool TopologyRefinerFactory<ccl::Mesh>::assignFaceVaryingTopology(TopologyRefiner & /*refiner*/,
                                                                  const ccl::Mesh & /*mesh*/)
{
  return true;
}

template<>
void TopologyRefinerFactory<ccl::Mesh>::reportInvalidTopology(TopologyError /*err_code*/,
                                                              const char * /*msg*/,
                                                              const ccl::Mesh & /*mesh*/)
{
}
}  // namespace OpenSubdiv::OPENSUBDIV_VERSION::Far

CCL_NAMESPACE_BEGIN

using namespace OpenSubdiv;

/* struct that implements OpenSubdiv's vertex interface */

template<typename T> struct OsdValue {
  T value;

  OsdValue() = default;

  void Clear(void * /*unused*/ = nullptr)
  {
    memset(&value, 0, sizeof(T));
  }

  void AddWithWeight(const OsdValue<T> &src, const float weight)
  {
    value += src.value * weight;
  }
};

template<> void OsdValue<uchar4>::AddWithWeight(const OsdValue<uchar4> &src, const float weight)
{
  for (int i = 0; i < 4; i++) {
    value[i] += (uchar)(src.value[i] * weight);
  }
}

/* class for holding OpenSubdiv data used during tessellation */

class OsdData {
  Mesh *mesh = nullptr;
  vector<OsdValue<float3>> verts;
  unique_ptr<Far::TopologyRefiner> refiner;
  unique_ptr<Far::PatchTable> patch_table;
  unique_ptr<Far::PatchMap> patch_map;

 public:
  OsdData() = default;

  void build_from_mesh(Mesh *mesh_)
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
    const int max_isolation = calculate_max_isolation();
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

  void subdivide_attribute(Attribute &attr)
  {
    const Far::PrimvarRefiner primvar_refiner(*refiner);

    if (attr.element == ATTR_ELEMENT_VERTEX) {
      const int num_refiner_verts = refiner->GetNumVerticesTotal();
      const int num_local_points = patch_table->GetNumLocalPoints();

      attr.resize(num_refiner_verts + num_local_points);
      attr.flags |= ATTR_FINAL_SIZE;

      char *src = attr.buffer.data();

      for (int i = 0; i < refiner->GetMaxLevel(); i++) {
        char *dest = src + refiner->GetLevel(i).GetNumVertices() * attr.data_sizeof();

        if (ccl::Attribute::same_storage(attr.type, TypeFloat)) {
          primvar_refiner.Interpolate(i + 1, (OsdValue<float> *)src, (OsdValue<float> *&)dest);
        }
        else if (ccl::Attribute::same_storage(attr.type, TypeFloat2)) {
          primvar_refiner.Interpolate(i + 1, (OsdValue<float2> *)src, (OsdValue<float2> *&)dest);
          // float3 is not interchangeable with float4 and so needs to be handled
          // separately
        }
        else if (ccl::Attribute::same_storage(attr.type, TypeFloat4)) {
          primvar_refiner.Interpolate(i + 1, (OsdValue<float4> *)src, (OsdValue<float4> *&)dest);
        }
        else {
          primvar_refiner.Interpolate(i + 1, (OsdValue<float3> *)src, (OsdValue<float3> *&)dest);
        }

        src = dest;
      }

      if (num_local_points) {
        if (ccl::Attribute::same_storage(attr.type, TypeFloat)) {
          patch_table->ComputeLocalPointValues(
              (OsdValue<float> *)attr.buffer.data(),
              (OsdValue<float> *)&attr.buffer[num_refiner_verts * attr.data_sizeof()]);
        }
        else if (ccl::Attribute::same_storage(attr.type, TypeFloat2)) {
          patch_table->ComputeLocalPointValues(
              (OsdValue<float2> *)attr.buffer.data(),
              (OsdValue<float2> *)&attr.buffer[num_refiner_verts * attr.data_sizeof()]);
        }
        else if (ccl::Attribute::same_storage(attr.type, TypeFloat4)) {
          // float3 is not interchangeable with float4 and so needs to be handled
          // separately
          patch_table->ComputeLocalPointValues(
              (OsdValue<float4> *)attr.buffer.data(),
              (OsdValue<float4> *)&attr.buffer[num_refiner_verts * attr.data_sizeof()]);
        }
        else {
          // float3 is not interchangeable with float4 and so needs to be handled
          // separately
          patch_table->ComputeLocalPointValues(
              (OsdValue<float3> *)attr.buffer.data(),
              (OsdValue<float3> *)&attr.buffer[num_refiner_verts * attr.data_sizeof()]);
        }
      }
    }
    else if (attr.element == ATTR_ELEMENT_CORNER || attr.element == ATTR_ELEMENT_CORNER_BYTE) {
      // TODO(mai): fvar interpolation
    }
  }

  int calculate_max_isolation()
  {
    /* loop over all edges to find longest in screen space */
    const Far::TopologyLevel &level = refiner->GetLevel(0);
    const SubdParams *subd_params = mesh->get_subd_params();
    const Transform objecttoworld = subd_params->objecttoworld;
    Camera *cam = subd_params->camera;

    float longest_edge = 0.0f;

    for (size_t i = 0; i < level.GetNumEdges(); i++) {
      const Far::ConstIndexArray verts = level.GetEdgeVertices(i);

      float3 a = mesh->get_verts()[verts[0]];
      float3 b = mesh->get_verts()[verts[1]];

      float edge_len;

      if (cam) {
        a = transform_point(&objecttoworld, a);
        b = transform_point(&objecttoworld, b);

        edge_len = len(a - b) / cam->world_to_raster_size((a + b) * 0.5f);
      }
      else {
        edge_len = len(a - b);
      }

      longest_edge = max(longest_edge, edge_len);
    }

    /* calculate isolation level */
    const int isolation = (int)(log2f(max(longest_edge / subd_params->dicing_rate, 1.0f)) + 1.0f);

    return min(isolation, 10);
  }

  friend struct OsdPatch;
  friend class Mesh;
};

/* ccl::Patch implementation that uses OpenSubdiv for eval */

struct OsdPatch : Patch {
  OsdData *osd_data;

  OsdPatch() = default;
  OsdPatch(OsdData *data) : osd_data(data) {}

  void eval(float3 *P, float3 *dPdu, float3 *dPdv, float3 *N, const float u, float v) override
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
};

#endif

void Mesh::tessellate(DiagSplit *split)
{
  /* reset the number of subdivision vertices, in case the Mesh was not cleared
   * between calls or data updates */
  num_subd_verts = 0;

#ifdef WITH_OPENSUBDIV
  OsdData osd_data;
  bool need_packed_patch_table = false;

  if (subdivision_type == SUBDIVISION_CATMULL_CLARK) {
    if (get_num_subd_faces()) {
      osd_data.build_from_mesh(this);
    }
  }
  else
#endif
  {
    /* force linear subdivision if OpenSubdiv is unavailable to avoid
     * falling into catmull-clark code paths by accident
     */
    subdivision_type = SUBDIVISION_LINEAR;

    /* force disable attribute subdivision for same reason as above */
    for (Attribute &attr : subd_attributes.attributes) {
      attr.flags &= ~ATTR_SUBDIVIDED;
    }
  }

  const int num_faces = get_num_subd_faces();

  Attribute *attr_vN = subd_attributes.find(ATTR_STD_VERTEX_NORMAL);
  float3 *vN = (attr_vN) ? attr_vN->data_float3() : nullptr;

  /* count patches */
  int num_patches = 0;
  for (int f = 0; f < num_faces; f++) {
    SubdFace face = get_subd_face(f);

    if (face.is_quad()) {
      num_patches++;
    }
    else {
      num_patches += face.num_corners;
    }
  }

  /* build patches from faces */
#ifdef WITH_OPENSUBDIV
  if (subdivision_type == SUBDIVISION_CATMULL_CLARK) {
    vector<OsdPatch> osd_patches(num_patches, &osd_data);
    OsdPatch *patch = osd_patches.data();

    for (int f = 0; f < num_faces; f++) {
      SubdFace face = get_subd_face(f);

      if (face.is_quad()) {
        patch->patch_index = face.ptex_offset;
        patch->from_ngon = false;
        patch->shader = face.shader;
        patch++;
      }
      else {
        for (int corner = 0; corner < face.num_corners; corner++) {
          patch->patch_index = face.ptex_offset + corner;
          patch->from_ngon = true;
          patch->shader = face.shader;
          patch++;
        }
      }
    }

    /* split patches */
    split->split_patches(osd_patches.data(), sizeof(OsdPatch));
  }
  else
#endif
  {
    vector<LinearQuadPatch> linear_patches(num_patches);
    LinearQuadPatch *patch = linear_patches.data();

    for (int f = 0; f < num_faces; f++) {
      SubdFace face = get_subd_face(f);

      if (face.is_quad()) {
        float3 *hull = patch->hull;
        float3 *normals = patch->normals;

        patch->patch_index = face.ptex_offset;
        patch->from_ngon = false;

        for (int i = 0; i < 4; i++) {
          hull[i] = verts[subd_face_corners[face.start_corner + i]];
        }

        if (face.smooth) {
          for (int i = 0; i < 4; i++) {
            normals[i] = vN[subd_face_corners[face.start_corner + i]];
          }
        }
        else {
          const float3 N = face.normal(this);
          for (int i = 0; i < 4; i++) {
            normals[i] = N;
          }
        }

        swap(hull[2], hull[3]);
        swap(normals[2], normals[3]);

        patch->shader = face.shader;
        patch++;
      }
      else {
        /* ngon */
        float3 center_vert = zero_float3();
        float3 center_normal = zero_float3();

        const float inv_num_corners = 1.0f / float(face.num_corners);
        for (int corner = 0; corner < face.num_corners; corner++) {
          center_vert += verts[subd_face_corners[face.start_corner + corner]] * inv_num_corners;
          center_normal += vN[subd_face_corners[face.start_corner + corner]] * inv_num_corners;
        }

        for (int corner = 0; corner < face.num_corners; corner++) {
          float3 *hull = patch->hull;
          float3 *normals = patch->normals;

          patch->patch_index = face.ptex_offset + corner;
          patch->from_ngon = true;

          patch->shader = face.shader;

          hull[0] =
              verts[subd_face_corners[face.start_corner + mod(corner + 0, face.num_corners)]];
          hull[1] =
              verts[subd_face_corners[face.start_corner + mod(corner + 1, face.num_corners)]];
          hull[2] =
              verts[subd_face_corners[face.start_corner + mod(corner - 1, face.num_corners)]];
          hull[3] = center_vert;

          hull[1] = (hull[1] + hull[0]) * 0.5;
          hull[2] = (hull[2] + hull[0]) * 0.5;

          if (face.smooth) {
            normals[0] =
                vN[subd_face_corners[face.start_corner + mod(corner + 0, face.num_corners)]];
            normals[1] =
                vN[subd_face_corners[face.start_corner + mod(corner + 1, face.num_corners)]];
            normals[2] =
                vN[subd_face_corners[face.start_corner + mod(corner - 1, face.num_corners)]];
            normals[3] = center_normal;

            normals[1] = (normals[1] + normals[0]) * 0.5;
            normals[2] = (normals[2] + normals[0]) * 0.5;
          }
          else {
            const float3 N = face.normal(this);
            for (int i = 0; i < 4; i++) {
              normals[i] = N;
            }
          }

          patch++;
        }
      }
    }

    /* split patches */
    split->split_patches(linear_patches.data(), sizeof(LinearQuadPatch));
  }

  /* interpolate center points for attributes */
  for (Attribute &attr : subd_attributes.attributes) {
#ifdef WITH_OPENSUBDIV
    if (subdivision_type == SUBDIVISION_CATMULL_CLARK && attr.flags & ATTR_SUBDIVIDED) {
      if (attr.element == ATTR_ELEMENT_CORNER || attr.element == ATTR_ELEMENT_CORNER_BYTE) {
        /* keep subdivision for corner attributes disabled for now */
        attr.flags &= ~ATTR_SUBDIVIDED;
      }
      else if (get_num_subd_faces()) {
        osd_data.subdivide_attribute(attr);

        need_packed_patch_table = true;
        continue;
      }
    }
#endif

    char *data = attr.data();
    const size_t stride = attr.data_sizeof();
    int ngons = 0;

    switch (attr.element) {
      case ATTR_ELEMENT_VERTEX: {
        for (int f = 0; f < num_faces; f++) {
          SubdFace face = get_subd_face(f);

          if (!face.is_quad()) {
            char *center = data + (verts.size() - num_subd_verts + ngons) * stride;
            attr.zero_data(center);

            const float inv_num_corners = 1.0f / float(face.num_corners);

            for (int corner = 0; corner < face.num_corners; corner++) {
              attr.add_with_weight(center,
                                   data + subd_face_corners[face.start_corner + corner] * stride,
                                   inv_num_corners);
            }

            ngons++;
          }
        }
        break;
      }
      case ATTR_ELEMENT_VERTEX_MOTION: {
        // TODO(mai): implement
        break;
      }
      case ATTR_ELEMENT_CORNER: {
        for (int f = 0; f < num_faces; f++) {
          SubdFace face = get_subd_face(f);

          if (!face.is_quad()) {
            char *center = data + (subd_face_corners.size() + ngons) * stride;
            attr.zero_data(center);

            const float inv_num_corners = 1.0f / float(face.num_corners);

            for (int corner = 0; corner < face.num_corners; corner++) {
              attr.add_with_weight(
                  center, data + (face.start_corner + corner) * stride, inv_num_corners);
            }

            ngons++;
          }
        }
        break;
      }
      case ATTR_ELEMENT_CORNER_BYTE: {
        for (int f = 0; f < num_faces; f++) {
          SubdFace face = get_subd_face(f);

          if (!face.is_quad()) {
            uchar *center = (uchar *)data + (subd_face_corners.size() + ngons) * stride;

            const float inv_num_corners = 1.0f / float(face.num_corners);
            float4 val = zero_float4();

            for (int corner = 0; corner < face.num_corners; corner++) {
              for (int i = 0; i < 4; i++) {
                val[i] += float(*(data + (face.start_corner + corner) * stride + i)) *
                          inv_num_corners;
              }
            }

            for (int i = 0; i < 4; i++) {
              center[i] = uchar(min(max(val[i], 0.0f), 255.0f));
            }

            ngons++;
          }
        }
        break;
      }
      default:
        break;
    }
  }

#ifdef WITH_OPENSUBDIV
  /* pack patch tables */
  if (need_packed_patch_table) {
    patch_table = make_unique<PackedPatchTable>();
    patch_table->pack(osd_data.patch_table.get());
  }
#endif
}

CCL_NAMESPACE_END
