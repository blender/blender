/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <optional>

#include "blender/attribute_convert.h"
#include "blender/session.h"
#include "blender/sync.h"
#include "blender/util.h"

#include "scene/camera.h"
#include "scene/colorspace.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/scene.h"

#include "subd/patch.h"
#include "subd/split.h"

#include "util/algorithm.h"
#include "util/color.h"
#include "util/disjoint_set.h"
#include "util/foreach.h"
#include "util/hash.h"
#include "util/log.h"
#include "util/math.h"

#include "mikktspace.hh"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"

CCL_NAMESPACE_BEGIN

/* Tangent Space */

template<bool is_subd> struct MikkMeshWrapper {
  MikkMeshWrapper(const ::Mesh &b_mesh,
                  const char *layer_name,
                  const Mesh *mesh,
                  float3 *tangent,
                  float *tangent_sign)
      : mesh(mesh), uv(NULL), orco(NULL), tangent(tangent), tangent_sign(tangent_sign)
  {
    const AttributeSet &attributes = is_subd ? mesh->subd_attributes : mesh->attributes;

    Attribute *attr_vN = attributes.find(ATTR_STD_VERTEX_NORMAL);
    vertex_normal = attr_vN->data_float3();

    if (layer_name == NULL) {
      Attribute *attr_orco = attributes.find(ATTR_STD_GENERATED);

      if (attr_orco) {
        orco = attr_orco->data_float3();
        float3 orco_size;
        mesh_texture_space(b_mesh, orco_loc, orco_size);
        inv_orco_size = 1.0f / orco_size;
      }
    }
    else {
      Attribute *attr_uv = attributes.find(ustring(layer_name));
      if (attr_uv != NULL) {
        uv = attr_uv->data_float2();
      }
    }
  }

  int GetNumFaces()
  {
    if constexpr (is_subd) {
      return mesh->get_num_subd_faces();
    }
    else {
      return mesh->num_triangles();
    }
  }

  int GetNumVerticesOfFace(const int face_num)
  {
    if constexpr (is_subd) {
      return mesh->get_subd_num_corners()[face_num];
    }
    else {
      return 3;
    }
  }

  int CornerIndex(const int face_num, const int vert_num)
  {
    if constexpr (is_subd) {
      const Mesh::SubdFace &face = mesh->get_subd_face(face_num);
      return face.start_corner + vert_num;
    }
    else {
      return face_num * 3 + vert_num;
    }
  }

  int VertexIndex(const int face_num, const int vert_num)
  {
    int corner = CornerIndex(face_num, vert_num);
    if constexpr (is_subd) {
      return mesh->get_subd_face_corners()[corner];
    }
    else {
      return mesh->get_triangles()[corner];
    }
  }

  mikk::float3 GetPosition(const int face_num, const int vert_num)
  {
    const float3 vP = mesh->get_verts()[VertexIndex(face_num, vert_num)];
    return mikk::float3(vP.x, vP.y, vP.z);
  }

  mikk::float3 GetTexCoord(const int face_num, const int vert_num)
  {
    /* TODO: Check whether introducing a template boolean in order to
     * turn this into a constexpr is worth it. */
    if (uv != NULL) {
      const int corner_index = CornerIndex(face_num, vert_num);
      float2 tfuv = uv[corner_index];
      return mikk::float3(tfuv.x, tfuv.y, 1.0f);
    }
    else if (orco != NULL) {
      const int vertex_index = VertexIndex(face_num, vert_num);
      const float2 uv = map_to_sphere((orco[vertex_index] + orco_loc) * inv_orco_size);
      return mikk::float3(uv.x, uv.y, 1.0f);
    }
    else {
      return mikk::float3(0.0f, 0.0f, 1.0f);
    }
  }

  mikk::float3 GetNormal(const int face_num, const int vert_num)
  {
    float3 vN;
    if (is_subd) {
      const Mesh::SubdFace &face = mesh->get_subd_face(face_num);
      if (face.smooth) {
        const int vertex_index = VertexIndex(face_num, vert_num);
        vN = vertex_normal[vertex_index];
      }
      else {
        vN = face.normal(mesh);
      }
    }
    else {
      if (mesh->get_smooth()[face_num]) {
        const int vertex_index = VertexIndex(face_num, vert_num);
        vN = vertex_normal[vertex_index];
      }
      else {
        const Mesh::Triangle tri = mesh->get_triangle(face_num);
        vN = tri.compute_normal(&mesh->get_verts()[0]);
      }
    }
    return mikk::float3(vN.x, vN.y, vN.z);
  }

  void SetTangentSpace(const int face_num, const int vert_num, mikk::float3 T, bool orientation)
  {
    const int corner_index = CornerIndex(face_num, vert_num);
    tangent[corner_index] = make_float3(T.x, T.y, T.z);
    if (tangent_sign != NULL) {
      tangent_sign[corner_index] = orientation ? 1.0f : -1.0f;
    }
  }

  const Mesh *mesh;
  int num_faces;

  float3 *vertex_normal;
  float2 *texface;
  float2 *uv;
  float3 *orco;
  float3 orco_loc, inv_orco_size;

  float3 *tangent;
  float *tangent_sign;
};

static void mikk_compute_tangents(
    const ::Mesh &b_mesh, const char *layer_name, Mesh *mesh, bool need_sign, bool active_render)
{
  /* Create tangent attributes. */
  const bool is_subd = mesh->get_num_subd_faces();
  AttributeSet &attributes = is_subd ? mesh->subd_attributes : mesh->attributes;
  Attribute *attr;
  ustring name;
  if (layer_name != NULL) {
    name = ustring((string(layer_name) + ".tangent").c_str());
  }
  else {
    name = ustring("orco.tangent");
  }
  if (active_render) {
    attr = attributes.add(ATTR_STD_UV_TANGENT, name);
  }
  else {
    attr = attributes.add(name, TypeDesc::TypeVector, ATTR_ELEMENT_CORNER);
  }
  float3 *tangent = attr->data_float3();
  /* Create bitangent sign attribute. */
  float *tangent_sign = NULL;
  if (need_sign) {
    Attribute *attr_sign;
    ustring name_sign;
    if (layer_name != NULL) {
      name_sign = ustring((string(layer_name) + ".tangent_sign").c_str());
    }
    else {
      name_sign = ustring("orco.tangent_sign");
    }

    if (active_render) {
      attr_sign = attributes.add(ATTR_STD_UV_TANGENT_SIGN, name_sign);
    }
    else {
      attr_sign = attributes.add(name_sign, TypeDesc::TypeFloat, ATTR_ELEMENT_CORNER);
    }
    tangent_sign = attr_sign->data_float();
  }

  /* Setup userdata. */
  if (is_subd) {
    MikkMeshWrapper<true> userdata(b_mesh, layer_name, mesh, tangent, tangent_sign);
    /* Compute tangents. */
    mikk::Mikktspace(userdata).genTangSpace();
  }
  else {
    MikkMeshWrapper<false> userdata(b_mesh, layer_name, mesh, tangent, tangent_sign);
    /* Compute tangents. */
    mikk::Mikktspace(userdata).genTangSpace();
  }
}

static void attr_create_motion_from_velocity(Mesh *mesh,
                                             const blender::Span<blender::float3> b_attr,
                                             const float motion_scale)
{
  const int numverts = mesh->get_verts().size();

  /* Override motion steps to fixed number. */
  mesh->set_motion_steps(3);

  /* Find or add attribute */
  float3 *P = &mesh->get_verts()[0];
  Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

  if (!attr_mP) {
    attr_mP = mesh->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
  }

  /* Only export previous and next frame, we don't have any in between data. */
  float motion_times[2] = {-1.0f, 1.0f};
  for (int step = 0; step < 2; step++) {
    const float relative_time = motion_times[step] * 0.5f * motion_scale;
    float3 *mP = attr_mP->data_float3() + step * numverts;

    for (int i = 0; i < numverts; i++) {
      mP[i] = P[i] + make_float3(b_attr[i][0], b_attr[i][1], b_attr[i][2]) * relative_time;
    }
  }
}

static void attr_create_generic(Scene *scene,
                                Mesh *mesh,
                                const ::Mesh &b_mesh,
                                const bool subdivision,
                                const bool need_motion,
                                const float motion_scale)
{
  blender::Span<blender::int3> corner_tris;
  blender::Span<int> tri_faces;
  if (!subdivision) {
    corner_tris = b_mesh.corner_tris();
    tri_faces = b_mesh.corner_tri_faces();
  }
  const blender::bke::AttributeAccessor b_attributes = b_mesh.attributes();
  AttributeSet &attributes = (subdivision) ? mesh->subd_attributes : mesh->attributes;
  static const ustring u_velocity("velocity");
  const ustring default_color_name{BKE_id_attributes_default_color_name(&b_mesh.id)};

  b_attributes.for_all([&](const blender::bke::AttributeIDRef &id,
                           const blender::bke::AttributeMetaData meta_data) {
    const ustring name{std::string_view(id.name())};
    const bool is_render_color = name == default_color_name;

    if (need_motion && name == u_velocity) {
      const blender::VArraySpan b_attribute = *b_attributes.lookup<blender::float3>(
          id, blender::bke::AttrDomain::Point);
      attr_create_motion_from_velocity(mesh, b_attribute, motion_scale);
    }

    if (!(mesh->need_attribute(scene, name) ||
          (is_render_color && mesh->need_attribute(scene, ATTR_STD_VERTEX_COLOR))))
    {
      return true;
    }
    if (attributes.find(name)) {
      return true;
    }

    blender::bke::AttrDomain b_domain = meta_data.domain;
    if (b_domain == blender::bke::AttrDomain::Edge) {
      /* Blender's attribute API handles edge to vertex attribute domain interpolation. */
      b_domain = blender::bke::AttrDomain::Point;
    }

    const blender::bke::GAttributeReader b_attr = b_attributes.lookup(id, b_domain);
    if (b_attr.varray.is_empty()) {
      return true;
    }

    if (b_attr.domain == blender::bke::AttrDomain::Corner &&
        meta_data.data_type == CD_PROP_BYTE_COLOR)
    {
      Attribute *attr = attributes.add(name, TypeRGBA, ATTR_ELEMENT_CORNER_BYTE);
      if (is_render_color) {
        attr->std = ATTR_STD_VERTEX_COLOR;
      }

      uchar4 *data = attr->data_uchar4();
      const blender::VArraySpan src = b_attr.varray.typed<blender::ColorGeometry4b>();
      if (subdivision) {
        for (const int i : src.index_range()) {
          data[i] = make_uchar4(src[i][0], src[i][1], src[i][2], src[i][3]);
        }
      }
      else {
        for (const int i : corner_tris.index_range()) {
          const blender::int3 &tri = corner_tris[i];
          data[i * 3 + 0] = make_uchar4(
              src[tri[0]][0], src[tri[0]][1], src[tri[0]][2], src[tri[0]][3]);
          data[i * 3 + 1] = make_uchar4(
              src[tri[1]][0], src[tri[1]][1], src[tri[1]][2], src[tri[1]][3]);
          data[i * 3 + 2] = make_uchar4(
              src[tri[2]][0], src[tri[2]][1], src[tri[2]][2], src[tri[2]][3]);
        }
      }
      return true;
    }

    AttributeElement element = ATTR_ELEMENT_NONE;
    switch (b_domain) {
      case blender::bke::AttrDomain::Corner:
        element = ATTR_ELEMENT_CORNER;
        break;
      case blender::bke::AttrDomain::Point:
        element = ATTR_ELEMENT_VERTEX;
        break;
      case blender::bke::AttrDomain::Face:
        element = ATTR_ELEMENT_FACE;
        break;
      default:
        assert(false);
        return true;
    }

    blender::bke::attribute_math::convert_to_static_type(b_attr.varray.type(), [&](auto dummy) {
      using BlenderT = decltype(dummy);
      using Converter = typename ccl::AttributeConverter<BlenderT>;
      using CyclesT = typename Converter::CyclesT;
      if constexpr (!std::is_void_v<CyclesT>) {
        Attribute *attr = attributes.add(name, Converter::type_desc, element);
        if (is_render_color) {
          attr->std = ATTR_STD_VERTEX_COLOR;
        }

        CyclesT *data = reinterpret_cast<CyclesT *>(attr->data());

        const blender::VArraySpan src = b_attr.varray.typed<BlenderT>();
        switch (b_attr.domain) {
          case blender::bke::AttrDomain::Corner: {
            if (subdivision) {
              for (const int i : src.index_range()) {
                data[i] = Converter::convert(src[i]);
              }
            }
            else {
              for (const int i : corner_tris.index_range()) {
                const blender::int3 &tri = corner_tris[i];
                data[i * 3 + 0] = Converter::convert(src[tri[0]]);
                data[i * 3 + 1] = Converter::convert(src[tri[1]]);
                data[i * 3 + 2] = Converter::convert(src[tri[2]]);
              }
            }
            break;
          }
          case blender::bke::AttrDomain::Point: {
            for (const int i : src.index_range()) {
              data[i] = Converter::convert(src[i]);
            }
            break;
          }
          case blender::bke::AttrDomain::Face: {
            if (subdivision) {
              for (const int i : src.index_range()) {
                data[i] = Converter::convert(src[i]);
              }
            }
            else {
              for (const int i : corner_tris.index_range()) {
                data[i] = Converter::convert(src[tri_faces[i]]);
              }
            }
            break;
          }
          default: {
            assert(false);
            break;
          }
        }
      }
    });
    return true;
  });
}

static set<ustring> get_blender_uv_names(const ::Mesh &b_mesh)
{
  set<ustring> uv_names;
  b_mesh.attributes().for_all([&](const blender::bke::AttributeIDRef &id,
                                  const blender::bke::AttributeMetaData meta_data) {
    if (meta_data.domain == blender::bke::AttrDomain::Corner &&
        meta_data.data_type == CD_PROP_FLOAT2)
    {
      if (!id.is_anonymous()) {
        uv_names.emplace(std::string_view(id.name()));
      }
    }
    return true;
  });
  return uv_names;
}

/* Create uv map attributes. */
static void attr_create_uv_map(Scene *scene,
                               Mesh *mesh,
                               const ::Mesh &b_mesh,
                               const set<ustring> &blender_uv_names)
{
  const blender::Span<blender::int3> corner_tris = b_mesh.corner_tris();
  const blender::bke::AttributeAccessor b_attributes = b_mesh.attributes();
  const ustring render_name(CustomData_get_render_layer_name(&b_mesh.corner_data, CD_PROP_FLOAT2));
  if (!blender_uv_names.empty()) {
    for (const ustring &uv_name : blender_uv_names) {
      const bool active_render = uv_name == render_name;
      AttributeStandard uv_std = (active_render) ? ATTR_STD_UV : ATTR_STD_NONE;
      AttributeStandard tangent_std = (active_render) ? ATTR_STD_UV_TANGENT : ATTR_STD_NONE;
      ustring tangent_name = ustring((string(uv_name) + ".tangent").c_str());

      /* Denotes whether UV map was requested directly. */
      const bool need_uv = mesh->need_attribute(scene, uv_name) ||
                           mesh->need_attribute(scene, uv_std);
      /* Denotes whether tangent was requested directly. */
      const bool need_tangent = mesh->need_attribute(scene, tangent_name) ||
                                (active_render && mesh->need_attribute(scene, tangent_std));

      /* UV map */
      /* NOTE: We create temporary UV layer if its needed for tangent but
       * wasn't requested by other nodes in shaders.
       */
      Attribute *uv_attr = NULL;
      if (need_uv || need_tangent) {
        if (active_render) {
          uv_attr = mesh->attributes.add(uv_std, uv_name);
        }
        else {
          uv_attr = mesh->attributes.add(uv_name, TypeFloat2, ATTR_ELEMENT_CORNER);
        }

        const blender::VArraySpan b_uv_map = *b_attributes.lookup<blender::float2>(
            uv_name.c_str(), blender::bke::AttrDomain::Corner);
        float2 *fdata = uv_attr->data_float2();
        for (const int i : corner_tris.index_range()) {
          const blender::int3 &tri = corner_tris[i];
          fdata[i * 3 + 0] = make_float2(b_uv_map[tri[0]][0], b_uv_map[tri[0]][1]);
          fdata[i * 3 + 1] = make_float2(b_uv_map[tri[1]][0], b_uv_map[tri[1]][1]);
          fdata[i * 3 + 2] = make_float2(b_uv_map[tri[2]][0], b_uv_map[tri[2]][1]);
        }
      }

      /* UV tangent */
      if (need_tangent) {
        AttributeStandard sign_std = (active_render) ? ATTR_STD_UV_TANGENT_SIGN : ATTR_STD_NONE;
        ustring sign_name = ustring((string(uv_name) + ".tangent_sign").c_str());
        bool need_sign = (mesh->need_attribute(scene, sign_name) ||
                          mesh->need_attribute(scene, sign_std));
        mikk_compute_tangents(b_mesh, uv_name.c_str(), mesh, need_sign, active_render);
      }
      /* Remove temporarily created UV attribute. */
      if (!need_uv && uv_attr != NULL) {
        mesh->attributes.remove(uv_attr);
      }
    }
  }
  else if (mesh->need_attribute(scene, ATTR_STD_UV_TANGENT)) {
    bool need_sign = mesh->need_attribute(scene, ATTR_STD_UV_TANGENT_SIGN);
    mikk_compute_tangents(b_mesh, NULL, mesh, need_sign, true);
    if (!mesh->need_attribute(scene, ATTR_STD_GENERATED)) {
      mesh->attributes.remove(ATTR_STD_GENERATED);
    }
  }
}

static void attr_create_subd_uv_map(Scene *scene,
                                    Mesh *mesh,
                                    const ::Mesh &b_mesh,
                                    bool subdivide_uvs,
                                    const set<ustring> &blender_uv_names)
{
  const blender::OffsetIndices faces = b_mesh.faces();
  if (faces.is_empty()) {
    return;
  }

  if (!blender_uv_names.empty()) {
    const blender::bke::AttributeAccessor b_attributes = b_mesh.attributes();
    const ustring render_name(
        CustomData_get_render_layer_name(&b_mesh.corner_data, CD_PROP_FLOAT2));
    for (const ustring &uv_name : blender_uv_names) {
      const bool active_render = uv_name == render_name;
      AttributeStandard uv_std = (active_render) ? ATTR_STD_UV : ATTR_STD_NONE;
      AttributeStandard tangent_std = (active_render) ? ATTR_STD_UV_TANGENT : ATTR_STD_NONE;
      ustring tangent_name = ustring((string(uv_name) + ".tangent").c_str());

      /* Denotes whether UV map was requested directly. */
      const bool need_uv = mesh->need_attribute(scene, uv_name) ||
                           mesh->need_attribute(scene, uv_std);
      /* Denotes whether tangent was requested directly. */
      const bool need_tangent = mesh->need_attribute(scene, tangent_name) ||
                                (active_render && mesh->need_attribute(scene, tangent_std));

      Attribute *uv_attr = NULL;

      /* UV map */
      if (need_uv || need_tangent) {
        if (active_render) {
          uv_attr = mesh->subd_attributes.add(uv_std, uv_name);
        }
        else {
          uv_attr = mesh->subd_attributes.add(uv_name, TypeFloat2, ATTR_ELEMENT_CORNER);
        }

        if (subdivide_uvs) {
          uv_attr->flags |= ATTR_SUBDIVIDED;
        }

        const blender::VArraySpan b_uv_map = *b_attributes.lookup<blender::float2>(
            uv_name.c_str(), blender::bke::AttrDomain::Corner);
        float2 *fdata = uv_attr->data_float2();

        for (const int i : faces.index_range()) {
          const blender::IndexRange face = faces[i];
          for (const int corner : face) {
            *(fdata++) = make_float2(b_uv_map[corner][0], b_uv_map[corner][1]);
          }
        }
      }

      /* UV tangent */
      if (need_tangent) {
        AttributeStandard sign_std = (active_render) ? ATTR_STD_UV_TANGENT_SIGN : ATTR_STD_NONE;
        ustring sign_name = ustring((string(uv_name) + ".tangent_sign").c_str());
        bool need_sign = (mesh->need_attribute(scene, sign_name) ||
                          mesh->need_attribute(scene, sign_std));
        mikk_compute_tangents(b_mesh, uv_name.c_str(), mesh, need_sign, active_render);
      }
      /* Remove temporarily created UV attribute. */
      if (!need_uv && uv_attr != NULL) {
        mesh->subd_attributes.remove(uv_attr);
      }
    }
  }
  else if (mesh->need_attribute(scene, ATTR_STD_UV_TANGENT)) {
    bool need_sign = mesh->need_attribute(scene, ATTR_STD_UV_TANGENT_SIGN);
    mikk_compute_tangents(b_mesh, NULL, mesh, need_sign, true);
    if (!mesh->need_attribute(scene, ATTR_STD_GENERATED)) {
      mesh->subd_attributes.remove(ATTR_STD_GENERATED);
    }
  }
}

/* Create vertex pointiness attributes. */

/* Compare vertices by sum of their coordinates. */
class VertexAverageComparator {
 public:
  VertexAverageComparator(const array<float3> &verts) : verts_(verts) {}

  bool operator()(const int &vert_idx_a, const int &vert_idx_b)
  {
    const float3 &vert_a = verts_[vert_idx_a];
    const float3 &vert_b = verts_[vert_idx_b];
    if (vert_a == vert_b) {
      /* Special case for doubles, so we ensure ordering. */
      return vert_idx_a > vert_idx_b;
    }
    const float x1 = vert_a.x + vert_a.y + vert_a.z;
    const float x2 = vert_b.x + vert_b.y + vert_b.z;
    return x1 < x2;
  }

 protected:
  const array<float3> &verts_;
};

static void attr_create_pointiness(Mesh *mesh,
                                   const blender::Span<blender::float3> positions,
                                   const blender::Span<blender::float3> b_vert_normals,
                                   const blender::Span<blender::int2> edges,
                                   bool subdivision)
{
  const int num_verts = positions.size();
  if (positions.is_empty()) {
    return;
  }

  /* STEP 1: Find out duplicated vertices and point duplicates to a single
   *         original vertex.
   */
  vector<int> sorted_vert_indeices(num_verts);
  for (int vert_index = 0; vert_index < num_verts; ++vert_index) {
    sorted_vert_indeices[vert_index] = vert_index;
  }
  VertexAverageComparator compare(mesh->get_verts());
  sort(sorted_vert_indeices.begin(), sorted_vert_indeices.end(), compare);
  /* This array stores index of the original vertex for the given vertex
   * index.
   */
  vector<int> vert_orig_index(num_verts);
  for (int sorted_vert_index = 0; sorted_vert_index < num_verts; ++sorted_vert_index) {
    const int vert_index = sorted_vert_indeices[sorted_vert_index];
    const float3 &vert_co = mesh->get_verts()[vert_index];
    bool found = false;
    for (int other_sorted_vert_index = sorted_vert_index + 1; other_sorted_vert_index < num_verts;
         ++other_sorted_vert_index)
    {
      const int other_vert_index = sorted_vert_indeices[other_sorted_vert_index];
      const float3 &other_vert_co = mesh->get_verts()[other_vert_index];
      /* We are too far away now, we wouldn't have duplicate. */
      if ((other_vert_co.x + other_vert_co.y + other_vert_co.z) -
              (vert_co.x + vert_co.y + vert_co.z) >
          3 * FLT_EPSILON)
      {
        break;
      }
      /* Found duplicate. */
      if (len_squared(other_vert_co - vert_co) < FLT_EPSILON) {
        found = true;
        vert_orig_index[vert_index] = other_vert_index;
        break;
      }
    }
    if (!found) {
      vert_orig_index[vert_index] = vert_index;
    }
  }
  /* Make sure we always points to the very first orig vertex. */
  for (int vert_index = 0; vert_index < num_verts; ++vert_index) {
    int orig_index = vert_orig_index[vert_index];
    while (orig_index != vert_orig_index[orig_index]) {
      orig_index = vert_orig_index[orig_index];
    }
    vert_orig_index[vert_index] = orig_index;
  }
  sorted_vert_indeices.free_memory();
  /* STEP 2: Calculate vertex normals taking into account their possible
   *         duplicates which gets "welded" together.
   */
  vector<float3> vert_normal(num_verts, zero_float3());
  /* First we accumulate all vertex normals in the original index. */
  for (int vert_index = 0; vert_index < num_verts; ++vert_index) {
    const float *b_vert_normal = b_vert_normals[vert_index];
    const int orig_index = vert_orig_index[vert_index];
    vert_normal[orig_index] += make_float3(b_vert_normal[0], b_vert_normal[1], b_vert_normal[2]);
  }
  /* Then we normalize the accumulated result and flush it to all duplicates
   * as well.
   */
  for (int vert_index = 0; vert_index < num_verts; ++vert_index) {
    const int orig_index = vert_orig_index[vert_index];
    vert_normal[vert_index] = normalize(vert_normal[orig_index]);
  }
  /* STEP 3: Calculate pointiness using single ring neighborhood. */
  vector<int> counter(num_verts, 0);
  vector<float> raw_data(num_verts, 0.0f);
  vector<float3> edge_accum(num_verts, zero_float3());
  EdgeMap visited_edges;
  memset(&counter[0], 0, sizeof(int) * counter.size());

  for (const int i : edges.index_range()) {
    const blender::int2 b_edge = edges[i];
    const int v0 = vert_orig_index[b_edge[0]];
    const int v1 = vert_orig_index[b_edge[1]];
    if (visited_edges.exists(v0, v1)) {
      continue;
    }
    visited_edges.insert(v0, v1);
    float3 co0 = make_float3(positions[v0][0], positions[v0][1], positions[v0][2]);
    float3 co1 = make_float3(positions[v1][0], positions[v1][1], positions[v1][2]);
    float3 edge = normalize(co1 - co0);
    edge_accum[v0] += edge;
    edge_accum[v1] += -edge;
    ++counter[v0];
    ++counter[v1];
  }
  for (int vert_index = 0; vert_index < num_verts; ++vert_index) {
    const int orig_index = vert_orig_index[vert_index];
    if (orig_index != vert_index) {
      /* Skip duplicates, they'll be overwritten later on. */
      continue;
    }
    if (counter[vert_index] > 0) {
      const float3 normal = vert_normal[vert_index];
      const float angle = safe_acosf(dot(normal, edge_accum[vert_index] / counter[vert_index]));
      raw_data[vert_index] = angle * M_1_PI_F;
    }
    else {
      raw_data[vert_index] = 0.0f;
    }
  }
  /* STEP 3: Blur vertices to approximate 2 ring neighborhood. */
  AttributeSet &attributes = (subdivision) ? mesh->subd_attributes : mesh->attributes;
  Attribute *attr = attributes.add(ATTR_STD_POINTINESS);
  float *data = attr->data_float();
  memcpy(data, &raw_data[0], sizeof(float) * raw_data.size());
  memset(&counter[0], 0, sizeof(int) * counter.size());
  visited_edges.clear();
  for (const int i : edges.index_range()) {
    const blender::int2 b_edge = edges[i];
    const int v0 = vert_orig_index[b_edge[0]];
    const int v1 = vert_orig_index[b_edge[1]];
    if (visited_edges.exists(v0, v1)) {
      continue;
    }
    visited_edges.insert(v0, v1);
    data[v0] += raw_data[v1];
    data[v1] += raw_data[v0];
    ++counter[v0];
    ++counter[v1];
  }
  for (int vert_index = 0; vert_index < num_verts; ++vert_index) {
    data[vert_index] /= counter[vert_index] + 1;
  }
  /* STEP 4: Copy attribute to the duplicated vertices. */
  for (int vert_index = 0; vert_index < num_verts; ++vert_index) {
    const int orig_index = vert_orig_index[vert_index];
    data[vert_index] = data[orig_index];
  }
}

/* The Random Per Island attribute is a random float associated with each
 * connected component (island) of the mesh. The attribute is computed by
 * first classifying the vertices into different sets using a Disjoint Set
 * data structure. Then the index of the root of each vertex (Which is the
 * representative of the set the vertex belongs to) is hashed and stored.
 *
 * We are using a face attribute to avoid interpolation during rendering,
 * allowing the user to safely hash the output further. Had we used vertex
 * attribute, the interpolation will introduce very slight variations,
 * making the output unsafe to hash. */
static void attr_create_random_per_island(Scene *scene,
                                          Mesh *mesh,
                                          const ::Mesh &b_mesh,
                                          bool subdivision)
{
  if (!mesh->need_attribute(scene, ATTR_STD_RANDOM_PER_ISLAND)) {
    return;
  }

  if (b_mesh.verts_num == 0) {
    return;
  }

  DisjointSet vertices_sets(b_mesh.verts_num);

  const blender::Span<blender::int2> edges = b_mesh.edges();
  const blender::Span<int> corner_verts = b_mesh.corner_verts();

  for (const int i : edges.index_range()) {
    vertices_sets.join(edges[i][0], edges[i][1]);
  }

  AttributeSet &attributes = (subdivision) ? mesh->subd_attributes : mesh->attributes;
  Attribute *attribute = attributes.add(ATTR_STD_RANDOM_PER_ISLAND);
  float *data = attribute->data_float();

  if (!subdivision) {
    const blender::Span<blender::int3> corner_tris = b_mesh.corner_tris();
    if (!corner_tris.is_empty()) {
      for (const int i : corner_tris.index_range()) {
        const int vert = corner_verts[corner_tris[i][0]];
        data[i] = hash_uint_to_float(vertices_sets.find(vert));
      }
    }
  }
  else {
    const blender::OffsetIndices<int> faces = b_mesh.faces();
    if (!faces.is_empty()) {
      for (const int i : faces.index_range()) {
        const int vert = corner_verts[faces[i].start()];
        data[i] = hash_uint_to_float(vertices_sets.find(vert));
      }
    }
  }
}

/* Create Mesh */

static void create_mesh(Scene *scene,
                        Mesh *mesh,
                        const ::Mesh &b_mesh,
                        const array<Node *> &used_shaders,
                        const bool need_motion,
                        const float motion_scale,
                        const bool subdivision = false,
                        const bool subdivide_uvs = true)
{
  const blender::Span<blender::float3> positions = b_mesh.vert_positions();
  const blender::OffsetIndices faces = b_mesh.faces();
  const blender::Span<int> corner_verts = b_mesh.corner_verts();
  const blender::bke::AttributeAccessor b_attributes = b_mesh.attributes();
  const blender::bke::MeshNormalDomain normals_domain = b_mesh.normals_domain(true);
  int numfaces = (!subdivision) ? b_mesh.corner_tris().size() : faces.size();

  bool use_corner_normals = normals_domain == blender::bke::MeshNormalDomain::Corner &&
                            (mesh->get_subdivision_type() != Mesh::SUBDIVISION_CATMULL_CLARK);

  /* If no faces, create empty mesh. */
  if (faces.is_empty()) {
    return;
  }

  const blender::VArraySpan material_indices = *b_attributes.lookup<int>(
      "material_index", blender::bke::AttrDomain::Face);
  const blender::VArraySpan sharp_faces = *b_attributes.lookup<bool>(
      "sharp_face", blender::bke::AttrDomain::Face);
  blender::Span<blender::float3> corner_normals;
  if (use_corner_normals) {
    corner_normals = b_mesh.corner_normals();
  }

  int numngons = 0;
  int numtris = 0;
  if (!subdivision) {
    numtris = numfaces;
  }
  else {
    const blender::OffsetIndices faces = b_mesh.faces();
    for (const int i : faces.index_range()) {
      numngons += (faces[i].size() == 4) ? 0 : 1;
    }
  }

  /* allocate memory */
  if (subdivision) {
    mesh->resize_subd_faces(numfaces, numngons, corner_verts.size());
  }
  mesh->resize_mesh(positions.size(), numtris);

  float3 *verts = mesh->get_verts().data();
  for (const int i : positions.index_range()) {
    verts[i] = make_float3(positions[i][0], positions[i][1], positions[i][2]);
  }

  AttributeSet &attributes = (subdivision) ? mesh->subd_attributes : mesh->attributes;
  Attribute *attr_N = attributes.add(ATTR_STD_VERTEX_NORMAL);
  float3 *N = attr_N->data_float3();

  if (subdivision || !(use_corner_normals && !corner_normals.is_empty())) {
    const blender::Span<blender::float3> vert_normals = b_mesh.vert_normals();
    for (const int i : vert_normals.index_range()) {
      N[i] = make_float3(vert_normals[i][0], vert_normals[i][1], vert_normals[i][2]);
    }
  }

  const set<ustring> blender_uv_names = get_blender_uv_names(b_mesh);

  /* create generated coordinates from undeformed coordinates */
  const bool need_default_tangent = (subdivision == false) && (blender_uv_names.empty()) &&
                                    (mesh->need_attribute(scene, ATTR_STD_UV_TANGENT));
  if (mesh->need_attribute(scene, ATTR_STD_GENERATED) || need_default_tangent) {
    const float(*orco)[3] = static_cast<const float(*)[3]>(
        CustomData_get_layer(&b_mesh.vert_data, CD_ORCO));
    Attribute *attr = attributes.add(ATTR_STD_GENERATED);
    attr->flags |= ATTR_SUBDIVIDED;

    float3 loc, size;
    mesh_texture_space(b_mesh, loc, size);

    float texspace_location[3], texspace_size[3];
    BKE_mesh_texspace_get(const_cast<::Mesh *>(b_mesh.texcomesh ? b_mesh.texcomesh : &b_mesh),
                          texspace_location,
                          texspace_size);

    float3 *generated = attr->data_float3();

    for (const int i : positions.index_range()) {
      blender::float3 value;
      if (orco) {
        madd_v3_v3v3v3(value, texspace_location, orco[i], texspace_size);
      }
      else {
        value = positions[i];
      }
      generated[i] = make_float3(value[0], value[1], value[2]) * size - loc;
    }
  }

  auto clamp_material_index = [&](const int material_index) -> int {
    return clamp(material_index, 0, used_shaders.size() - 1);
  };

  /* create faces */
  if (!subdivision) {
    int *triangles = mesh->get_triangles().data();
    bool *smooth = mesh->get_smooth().data();
    int *shader = mesh->get_shader().data();

    const blender::Span<blender::int3> corner_tris = b_mesh.corner_tris();
    for (const int i : corner_tris.index_range()) {
      const blender::int3 &tri = corner_tris[i];
      triangles[i * 3 + 0] = corner_verts[tri[0]];
      triangles[i * 3 + 1] = corner_verts[tri[1]];
      triangles[i * 3 + 2] = corner_verts[tri[2]];
    }

    if (!material_indices.is_empty()) {
      const blender::Span<int> tri_faces = b_mesh.corner_tri_faces();
      for (const int i : corner_tris.index_range()) {
        shader[i] = clamp_material_index(material_indices[tri_faces[i]]);
      }
    }
    else {
      std::fill(shader, shader + numtris, 0);
    }

    if (!sharp_faces.is_empty() && !(use_corner_normals && !corner_normals.is_empty())) {
      const blender::Span<int> tri_faces = b_mesh.corner_tri_faces();
      for (const int i : corner_tris.index_range()) {
        smooth[i] = !sharp_faces[tri_faces[i]];
      }
    }
    else {
      /* If only face normals are needed, all faces are sharp. */
      std::fill(smooth, smooth + numtris, normals_domain != blender::bke::MeshNormalDomain::Face);
    }

    if (use_corner_normals && !corner_normals.is_empty()) {
      for (const int i : corner_tris.index_range()) {
        const blender::int3 &tri = corner_tris[i];
        for (int i = 0; i < 3; i++) {
          const int corner = tri[i];
          const int vert = corner_verts[corner];
          const float *normal = corner_normals[corner];
          N[vert] = make_float3(normal[0], normal[1], normal[2]);
        }
      }
    }

    mesh->tag_triangles_modified();
    mesh->tag_shader_modified();
    mesh->tag_smooth_modified();
  }
  else {
    int *subd_start_corner = mesh->get_subd_start_corner().data();
    int *subd_num_corners = mesh->get_subd_num_corners().data();
    int *subd_shader = mesh->get_subd_shader().data();
    bool *subd_smooth = mesh->get_subd_smooth().data();
    int *subd_ptex_offset = mesh->get_subd_ptex_offset().data();
    int *subd_face_corners = mesh->get_subd_face_corners().data();

    if (!sharp_faces.is_empty() && !use_corner_normals) {
      for (int i = 0; i < numfaces; i++) {
        subd_smooth[i] = !sharp_faces[i];
      }
    }
    else {
      std::fill(subd_smooth, subd_smooth + numfaces, true);
    }

    if (!material_indices.is_empty()) {
      for (int i = 0; i < numfaces; i++) {
        subd_shader[i] = clamp_material_index(material_indices[i]);
      }
    }
    else {
      std::fill(subd_shader, subd_shader + numfaces, 0);
    }

    std::copy(corner_verts.data(), corner_verts.data() + corner_verts.size(), subd_face_corners);

    const blender::OffsetIndices faces = b_mesh.faces();
    int ptex_offset = 0;
    for (const int i : faces.index_range()) {
      const blender::IndexRange face = faces[i];

      subd_start_corner[i] = face.start();
      subd_num_corners[i] = face.size();
      subd_ptex_offset[i] = ptex_offset;
      const int num_ptex = (face.size() == 4) ? 1 : face.size();
      ptex_offset += num_ptex;
    }

    mesh->tag_subd_face_corners_modified();
    mesh->tag_subd_start_corner_modified();
    mesh->tag_subd_num_corners_modified();
    mesh->tag_subd_shader_modified();
    mesh->tag_subd_smooth_modified();
    mesh->tag_subd_ptex_offset_modified();
  }

  /* Create all needed attributes.
   * The calculate functions will check whether they're needed or not.
   */
  if (mesh->need_attribute(scene, ATTR_STD_POINTINESS)) {
    attr_create_pointiness(mesh, positions, b_mesh.vert_normals(), b_mesh.edges(), subdivision);
  }
  attr_create_random_per_island(scene, mesh, b_mesh, subdivision);
  attr_create_generic(scene, mesh, b_mesh, subdivision, need_motion, motion_scale);

  if (subdivision) {
    attr_create_subd_uv_map(scene, mesh, b_mesh, subdivide_uvs, blender_uv_names);
  }
  else {
    attr_create_uv_map(scene, mesh, b_mesh, blender_uv_names);
  }

  /* For volume objects, create a matrix to transform from object space to
   * mesh texture space. this does not work with deformations but that can
   * probably only be done well with a volume grid mapping of coordinates. */
  if (mesh->need_attribute(scene, ATTR_STD_GENERATED_TRANSFORM)) {
    Attribute *attr = mesh->attributes.add(ATTR_STD_GENERATED_TRANSFORM);
    Transform *tfm = attr->data_transform();

    float3 loc, size;
    mesh_texture_space(b_mesh, loc, size);

    *tfm = transform_translate(-loc) * transform_scale(size);
  }
}

static void create_subd_mesh(Scene *scene,
                             Mesh *mesh,
                             BObjectInfo &b_ob_info,
                             const ::Mesh &b_mesh,
                             const array<Node *> &used_shaders,
                             const bool need_motion,
                             const float motion_scale,
                             float dicing_rate,
                             int max_subdivisions)
{
  BL::Object b_ob = b_ob_info.real_object;

  BL::SubsurfModifier subsurf_mod(b_ob.modifiers[b_ob.modifiers.length() - 1]);
  bool subdivide_uvs = subsurf_mod.uv_smooth() != BL::SubsurfModifier::uv_smooth_NONE;

  create_mesh(scene, mesh, b_mesh, used_shaders, need_motion, motion_scale, true, subdivide_uvs);

  const blender::VArraySpan creases = *b_mesh.attributes().lookup<float>(
      "crease_edge", blender::bke::AttrDomain::Edge);
  if (!creases.is_empty()) {
    size_t num_creases = 0;
    for (const int i : creases.index_range()) {
      if (creases[i] != 0.0f) {
        num_creases++;
      }
    }

    mesh->reserve_subd_creases(num_creases);

    const blender::Span<blender::int2> edges = b_mesh.edges();
    for (const int i : edges.index_range()) {
      const float crease = creases[i];
      if (crease != 0.0f) {
        const blender::int2 &b_edge = edges[i];
        mesh->add_edge_crease(b_edge[0], b_edge[1], crease);
      }
    }
  }

  const blender::VArraySpan vert_creases = *b_mesh.attributes().lookup<float>(
      "crease_vert", blender::bke::AttrDomain::Point);
  if (!vert_creases.is_empty()) {
    for (const int i : vert_creases.index_range()) {
      if (vert_creases[i] != 0.0f) {
        mesh->add_vertex_crease(i, vert_creases[i]);
      }
    }
  }

  /* set subd params */
  PointerRNA cobj = RNA_pointer_get(&b_ob.ptr, "cycles");
  float subd_dicing_rate = max(0.1f, RNA_float_get(&cobj, "dicing_rate") * dicing_rate);

  mesh->set_subd_dicing_rate(subd_dicing_rate);
  mesh->set_subd_max_level(max_subdivisions);
  mesh->set_subd_objecttoworld(get_transform(b_ob.matrix_world()));
}

/* Sync */

void BlenderSync::sync_mesh(BL::Depsgraph b_depsgraph, BObjectInfo &b_ob_info, Mesh *mesh)
{
  /* make a copy of the shaders as the caller in the main thread still need them for syncing the
   * attributes */
  array<Node *> used_shaders = mesh->get_used_shaders();

  Mesh new_mesh;
  new_mesh.set_used_shaders(used_shaders);

  if (view_layer.use_surfaces) {
    /* Adaptive subdivision setup. Not for baking since that requires
     * exact mapping to the Blender mesh. */
    if (!scene->bake_manager->get_baking()) {
      new_mesh.set_subdivision_type(
          object_subdivision_type(b_ob_info.real_object, preview, experimental));
    }

    /* For some reason, meshes do not need this... */
    bool need_undeformed = new_mesh.need_attribute(scene, ATTR_STD_GENERATED);
    BL::Mesh b_mesh = object_to_mesh(
        b_data, b_ob_info, b_depsgraph, need_undeformed, new_mesh.get_subdivision_type());

    if (b_mesh) {
      /* Motion blur attribute is relative to seconds, we need it relative to frames. */
      const bool need_motion = object_need_motion_attribute(b_ob_info, scene);
      const float motion_scale = (need_motion) ?
                                     scene->motion_shutter_time() /
                                         (b_scene.render().fps() / b_scene.render().fps_base()) :
                                     0.0f;

      /* Sync mesh itself. */
      if (new_mesh.get_subdivision_type() != Mesh::SUBDIVISION_NONE) {
        create_subd_mesh(scene,
                         &new_mesh,
                         b_ob_info,
                         *static_cast<const ::Mesh *>(b_mesh.ptr.data),
                         new_mesh.get_used_shaders(),
                         need_motion,
                         motion_scale,
                         dicing_rate,
                         max_subdivisions);
      }
      else {
        create_mesh(scene,
                    &new_mesh,
                    *static_cast<const ::Mesh *>(b_mesh.ptr.data),
                    new_mesh.get_used_shaders(),
                    need_motion,
                    motion_scale,
                    false);
      }

      free_object_to_mesh(b_data, b_ob_info, b_mesh);
    }
  }

  /* update original sockets */

  mesh->clear_non_sockets();

  for (const SocketType &socket : new_mesh.type->inputs) {
    /* Those sockets are updated in sync_object, so do not modify them. */
    if (socket.name == "use_motion_blur" || socket.name == "motion_steps" ||
        socket.name == "used_shaders")
    {
      continue;
    }
    mesh->set_value(socket, new_mesh, socket);
  }

  mesh->attributes.update(std::move(new_mesh.attributes));
  mesh->subd_attributes.update(std::move(new_mesh.subd_attributes));

  mesh->set_num_subd_faces(new_mesh.get_num_subd_faces());

  /* tag update */
  bool rebuild = (mesh->triangles_is_modified()) || (mesh->subd_num_corners_is_modified()) ||
                 (mesh->subd_shader_is_modified()) || (mesh->subd_smooth_is_modified()) ||
                 (mesh->subd_ptex_offset_is_modified()) ||
                 (mesh->subd_start_corner_is_modified()) ||
                 (mesh->subd_face_corners_is_modified());

  mesh->tag_update(scene, rebuild);
}

void BlenderSync::sync_mesh_motion(BL::Depsgraph b_depsgraph,
                                   BObjectInfo &b_ob_info,
                                   Mesh *mesh,
                                   int motion_step)
{
  /* Skip if no vertices were exported. */
  size_t numverts = mesh->get_verts().size();
  if (numverts == 0) {
    return;
  }

  /* Skip objects without deforming modifiers. this is not totally reliable,
   * would need a more extensive check to see which objects are animated. */
  BL::Mesh b_mesh_rna(PointerRNA_NULL);
  if (ccl::BKE_object_is_deform_modified(b_ob_info, b_scene, preview)) {
    /* get derived mesh */
    b_mesh_rna = object_to_mesh(b_data, b_ob_info, b_depsgraph, false, Mesh::SUBDIVISION_NONE);
  }

  const std::string ob_name = b_ob_info.real_object.name();

  /* TODO(sergey): Perform preliminary check for number of vertices. */
  if (b_mesh_rna) {
    const ::Mesh &b_mesh = *static_cast<const ::Mesh *>(b_mesh_rna.ptr.data);
    const int b_verts_num = b_mesh.verts_num;
    const blender::Span<blender::float3> positions = b_mesh.vert_positions();
    if (positions.is_empty()) {
      free_object_to_mesh(b_data, b_ob_info, b_mesh_rna);
      return;
    }

    /* Export deformed coordinates. */
    /* Find attributes. */
    Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    Attribute *attr_mN = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);
    Attribute *attr_N = mesh->attributes.find(ATTR_STD_VERTEX_NORMAL);
    bool new_attribute = false;
    /* Add new attributes if they don't exist already. */
    if (!attr_mP) {
      attr_mP = mesh->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
      if (attr_N) {
        attr_mN = mesh->attributes.add(ATTR_STD_MOTION_VERTEX_NORMAL);
      }

      new_attribute = true;
    }
    /* Load vertex data from mesh. */
    float3 *mP = attr_mP->data_float3() + motion_step * numverts;
    float3 *mN = (attr_mN) ? attr_mN->data_float3() + motion_step * numverts : NULL;

    /* NOTE: We don't copy more that existing amount of vertices to prevent
     * possible memory corruption.
     */
    for (int i = 0; i < std::min<size_t>(b_verts_num, numverts); i++) {
      mP[i] = make_float3(positions[i][0], positions[i][1], positions[i][2]);
    }
    if (mN) {
      const blender::Span<blender::float3> b_vert_normals = b_mesh.vert_normals();
      for (int i = 0; i < std::min<size_t>(b_verts_num, numverts); i++) {
        mN[i] = make_float3(b_vert_normals[i][0], b_vert_normals[i][1], b_vert_normals[i][2]);
      }
    }
    if (new_attribute) {
      /* In case of new attribute, we verify if there really was any motion. */
      if (b_verts_num != numverts ||
          memcmp(mP, &mesh->get_verts()[0], sizeof(float3) * numverts) == 0)
      {
        /* no motion, remove attributes again */
        if (b_verts_num != numverts) {
          VLOG_WARNING << "Topology differs, disabling motion blur for object " << ob_name;
        }
        else {
          VLOG_DEBUG << "No actual deformation motion for object " << ob_name;
        }
        mesh->attributes.remove(ATTR_STD_MOTION_VERTEX_POSITION);
        if (attr_mN) {
          mesh->attributes.remove(ATTR_STD_MOTION_VERTEX_NORMAL);
        }
      }
      else if (motion_step > 0) {
        VLOG_DEBUG << "Filling deformation motion for object " << ob_name;
        /* motion, fill up previous steps that we might have skipped because
         * they had no motion, but we need them anyway now */
        float3 *P = &mesh->get_verts()[0];
        float3 *N = (attr_N) ? attr_N->data_float3() : NULL;
        for (int step = 0; step < motion_step; step++) {
          memcpy(attr_mP->data_float3() + step * numverts, P, sizeof(float3) * numverts);
          if (attr_mN) {
            memcpy(attr_mN->data_float3() + step * numverts, N, sizeof(float3) * numverts);
          }
        }
      }
    }
    else {
      if (b_verts_num != numverts) {
        VLOG_WARNING << "Topology differs, discarding motion blur for object " << ob_name
                     << " at time " << motion_step;
        memcpy(mP, &mesh->get_verts()[0], sizeof(float3) * numverts);
        if (mN != NULL) {
          memcpy(mN, attr_N->data_float3(), sizeof(float3) * numverts);
        }
      }
    }

    free_object_to_mesh(b_data, b_ob_info, b_mesh_rna);
    return;
  }

  /* No deformation on this frame, copy coordinates if other frames did have it. */
  mesh->copy_center_to_motion_step(motion_step);
}

CCL_NAMESPACE_END
