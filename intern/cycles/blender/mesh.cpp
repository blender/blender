/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include <optional>

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

#include "DNA_meshdata_types.h"

CCL_NAMESPACE_BEGIN

/* Tangent Space */

template<bool is_subd> struct MikkMeshWrapper {
  MikkMeshWrapper(const BL::Mesh &b_mesh,
                  const char *layer_name,
                  const Mesh *mesh,
                  float3 *tangent,
                  float *tangent_sign)
      : mesh(mesh), texface(NULL), orco(NULL), tangent(tangent), tangent_sign(tangent_sign)
  {
    const AttributeSet &attributes = is_subd ? mesh->subd_attributes : mesh->attributes;

    Attribute *attr_vN = attributes.find(ATTR_STD_VERTEX_NORMAL);
    vertex_normal = attr_vN->data_float3();

    if (layer_name == NULL) {
      Attribute *attr_orco = attributes.find(ATTR_STD_GENERATED);

      if (attr_orco) {
        orco = attr_orco->data_float3();
        float3 orco_size;
        mesh_texture_space(*(BL::Mesh *)&b_mesh, orco_loc, orco_size);
        inv_orco_size = 1.0f / orco_size;
      }
    }
    else {
      Attribute *attr_uv = attributes.find(ustring(layer_name));
      if (attr_uv != NULL) {
        texface = attr_uv->data_float2();
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
    if (texface != NULL) {
      const int corner_index = CornerIndex(face_num, vert_num);
      float2 tfuv = texface[corner_index];
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
  float3 *orco;
  float3 orco_loc, inv_orco_size;

  float3 *tangent;
  float *tangent_sign;
};

static void mikk_compute_tangents(
    const BL::Mesh &b_mesh, const char *layer_name, Mesh *mesh, bool need_sign, bool active_render)
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

template<typename TypeInCycles, typename GetValueAtIndex>
static void fill_generic_attribute(BL::Mesh &b_mesh,
                                   TypeInCycles *data,
                                   const BL::Attribute::domain_enum b_domain,
                                   const bool subdivision,
                                   const GetValueAtIndex &get_value_at_index)
{
  switch (b_domain) {
    case BL::Attribute::domain_CORNER: {
      if (subdivision) {
        const int polys_num = b_mesh.polygons.length();
        if (polys_num == 0) {
          return;
        }
        const int *poly_offsets = static_cast<const int *>(b_mesh.polygons[0].ptr.data);
        for (int i = 0; i < polys_num; i++) {
          const int poly_start = poly_offsets[i];
          const int poly_size = poly_offsets[i + 1] - poly_start;
          for (int j = 0; j < poly_size; j++) {
            *data = get_value_at_index(poly_start + j);
            data++;
          }
        }
      }
      else {
        const int tris_num = b_mesh.loop_triangles.length();
        const MLoopTri *looptris = static_cast<const MLoopTri *>(
            b_mesh.loop_triangles[0].ptr.data);
        for (int i = 0; i < tris_num; i++) {
          const MLoopTri &tri = looptris[i];
          data[i * 3 + 0] = get_value_at_index(tri.tri[0]);
          data[i * 3 + 1] = get_value_at_index(tri.tri[1]);
          data[i * 3 + 2] = get_value_at_index(tri.tri[2]);
        }
      }
      break;
    }
    case BL::Attribute::domain_EDGE: {
      const size_t edges_num = b_mesh.edges.length();
      if (edges_num == 0) {
        return;
      }
      if constexpr (std::is_same_v<TypeInCycles, uchar4>) {
        /* uchar4 edge attributes do not exist, and averaging in place
         * would not work. */
        assert(0);
      }
      else {
        const int2 *edges = static_cast<const int2 *>(b_mesh.edges[0].ptr.data);
        const size_t verts_num = b_mesh.vertices.length();
        vector<int> count(verts_num, 0);

        /* Average edge attributes at vertices. */
        for (int i = 0; i < edges_num; i++) {
          TypeInCycles value = get_value_at_index(i);

          const int2 &b_edge = edges[i];
          data[b_edge[0]] += value;
          data[b_edge[1]] += value;
          count[b_edge[0]]++;
          count[b_edge[1]]++;
        }

        for (size_t i = 0; i < verts_num; i++) {
          if (count[i] > 1) {
            data[i] /= (float)count[i];
          }
        }
      }
      break;
    }
    case BL::Attribute::domain_POINT: {
      const int num_verts = b_mesh.vertices.length();
      for (int i = 0; i < num_verts; i++) {
        data[i] = get_value_at_index(i);
      }
      break;
    }
    case BL::Attribute::domain_FACE: {
      if (subdivision) {
        const int num_polygons = b_mesh.polygons.length();
        for (int i = 0; i < num_polygons; i++) {
          data[i] = get_value_at_index(i);
        }
      }
      else {
        const int tris_num = b_mesh.loop_triangles.length();
        const int *looptri_polys = static_cast<const int *>(
            b_mesh.loop_triangle_polygons[0].ptr.data);
        for (int i = 0; i < tris_num; i++) {
          data[i] = get_value_at_index(looptri_polys[i]);
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

static void attr_create_motion(Mesh *mesh, BL::Attribute &b_attribute, const float motion_scale)
{
  if (!(b_attribute.domain() == BL::Attribute::domain_POINT) &&
      (b_attribute.data_type() == BL::Attribute::data_type_FLOAT_VECTOR))
  {
    return;
  }

  BL::FloatVectorAttribute b_vector_attribute(b_attribute);
  const int numverts = mesh->get_verts().size();

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
      mP[i] = P[i] + get_float3(b_vector_attribute.data[i].vector()) * relative_time;
    }
  }
}

static void attr_create_generic(Scene *scene,
                                Mesh *mesh,
                                BL::Mesh &b_mesh,
                                const bool subdivision,
                                const bool need_motion,
                                const float motion_scale)
{
  AttributeSet &attributes = (subdivision) ? mesh->subd_attributes : mesh->attributes;
  static const ustring u_velocity("velocity");
  const ustring default_color_name{b_mesh.attributes.default_color_name().c_str()};

  for (BL::Attribute &b_attribute : b_mesh.attributes) {
    const ustring name{b_attribute.name().c_str()};
    const bool is_render_color = name == default_color_name;

    if (need_motion && name == u_velocity) {
      attr_create_motion(mesh, b_attribute, motion_scale);
    }

    if (!(mesh->need_attribute(scene, name) ||
          (is_render_color && mesh->need_attribute(scene, ATTR_STD_VERTEX_COLOR))))
    {
      continue;
    }
    if (attributes.find(name)) {
      continue;
    }

    const BL::Attribute::domain_enum b_domain = b_attribute.domain();
    const BL::Attribute::data_type_enum b_data_type = b_attribute.data_type();

    AttributeElement element = ATTR_ELEMENT_NONE;
    switch (b_domain) {
      case BL::Attribute::domain_CORNER:
        element = ATTR_ELEMENT_CORNER;
        break;
      case BL::Attribute::domain_POINT:
        element = ATTR_ELEMENT_VERTEX;
        break;
      case BL::Attribute::domain_EDGE:
        element = ATTR_ELEMENT_VERTEX;
        break;
      case BL::Attribute::domain_FACE:
        element = ATTR_ELEMENT_FACE;
        break;
      default:
        break;
    }
    if (element == ATTR_ELEMENT_NONE) {
      /* Not supported. */
      continue;
    }
    switch (b_data_type) {
      case BL::Attribute::data_type_FLOAT: {
        BL::FloatAttribute b_float_attribute{b_attribute};
        if (b_float_attribute.data.length() == 0) {
          continue;
        }
        const float *src = static_cast<const float *>(b_float_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(b_mesh, data, b_domain, subdivision, [&](int i) { return src[i]; });
        break;
      }
      case BL::Attribute::data_type_BOOLEAN: {
        BL::BoolAttribute b_bool_attribute{b_attribute};
        if (b_bool_attribute.data.length() == 0) {
          continue;
        }
        const bool *src = static_cast<const bool *>(b_bool_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(
            b_mesh, data, b_domain, subdivision, [&](int i) { return (float)src[i]; });
        break;
      }
      case BL::Attribute::data_type_INT: {
        BL::IntAttribute b_int_attribute{b_attribute};
        if (b_int_attribute.data.length() == 0) {
          continue;
        }
        const int *src = static_cast<const int *>(b_int_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(
            b_mesh, data, b_domain, subdivision, [&](int i) { return (float)src[i]; });
        break;
      }
      case BL::Attribute::data_type_FLOAT_VECTOR: {
        BL::FloatVectorAttribute b_vector_attribute{b_attribute};
        if (b_vector_attribute.data.length() == 0) {
          continue;
        }
        const float(*src)[3] = static_cast<const float(*)[3]>(b_vector_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeVector, element);
        float3 *data = attr->data_float3();
        fill_generic_attribute(b_mesh, data, b_domain, subdivision, [&](int i) {
          return make_float3(src[i][0], src[i][1], src[i][2]);
        });
        break;
      }
      case BL::Attribute::data_type_BYTE_COLOR: {
        BL::ByteColorAttribute b_color_attribute{b_attribute};
        if (b_color_attribute.data.length() == 0) {
          continue;
        }
        const uchar(*src)[4] = static_cast<const uchar(*)[4]>(b_color_attribute.data[0].ptr.data);

        if (element == ATTR_ELEMENT_CORNER) {
          element = ATTR_ELEMENT_CORNER_BYTE;
        }
        Attribute *attr = attributes.add(name, TypeRGBA, element);
        if (is_render_color) {
          attr->std = ATTR_STD_VERTEX_COLOR;
        }

        if (element == ATTR_ELEMENT_CORNER_BYTE) {
          uchar4 *data = attr->data_uchar4();
          fill_generic_attribute(b_mesh, data, b_domain, subdivision, [&](int i) {
            /* Compress/encode vertex color using the sRGB curve. */
            return make_uchar4(src[i][0], src[i][1], src[i][2], src[i][3]);
          });
        }
        else {
          float4 *data = attr->data_float4();
          fill_generic_attribute(b_mesh, data, b_domain, subdivision, [&](int i) {
            return make_float4(color_srgb_to_linear(byte_to_float(src[i][0])),
                               color_srgb_to_linear(byte_to_float(src[i][1])),
                               color_srgb_to_linear(byte_to_float(src[i][2])),
                               color_srgb_to_linear(byte_to_float(src[i][3])));
          });
        }
        break;
      }
      case BL::Attribute::data_type_FLOAT_COLOR: {
        BL::FloatColorAttribute b_color_attribute{b_attribute};
        if (b_color_attribute.data.length() == 0) {
          continue;
        }
        const float(*src)[4] = static_cast<const float(*)[4]>(b_color_attribute.data[0].ptr.data);

        Attribute *attr = attributes.add(name, TypeRGBA, element);
        if (is_render_color) {
          attr->std = ATTR_STD_VERTEX_COLOR;
        }

        float4 *data = attr->data_float4();
        fill_generic_attribute(b_mesh, data, b_domain, subdivision, [&](int i) {
          return make_float4(src[i][0], src[i][1], src[i][2], src[i][3]);
        });
        break;
      }
      case BL::Attribute::data_type_FLOAT2: {
        BL::Float2Attribute b_float2_attribute{b_attribute};
        if (b_float2_attribute.data.length() == 0) {
          continue;
        }
        const float(*src)[2] = static_cast<const float(*)[2]>(b_float2_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat2, element);
        float2 *data = attr->data_float2();
        fill_generic_attribute(b_mesh, data, b_domain, subdivision, [&](int i) {
          return make_float2(src[i][0], src[i][1]);
        });
        break;
      }
      case BL::Attribute::data_type_INT32_2D: {
        BL::Int2Attribute b_int2_attribute{b_attribute};
        if (b_int2_attribute.data.length() == 0) {
          continue;
        }
        const int2 *src = static_cast<const int2 *>(b_int2_attribute.data[0].ptr.data);
        Attribute *attr = attributes.add(name, TypeFloat2, element);
        float2 *data = attr->data_float2();
        fill_generic_attribute(b_mesh, data, b_domain, subdivision, [&](int i) {
          return make_float2(float(src[i][0]), float(src[i][1]));
        });
        break;
      }
      default:
        /* Not supported. */
        break;
    }
  }
}

/* Create uv map attributes. */
static void attr_create_uv_map(Scene *scene, Mesh *mesh, BL::Mesh &b_mesh)
{
  if (!b_mesh.uv_layers.empty()) {
    const int tris_num = b_mesh.loop_triangles.length();
    const MLoopTri *looptris = static_cast<const MLoopTri *>(b_mesh.loop_triangles[0].ptr.data);

    for (BL::MeshUVLoopLayer &l : b_mesh.uv_layers) {
      const bool active_render = l.active_render();
      AttributeStandard uv_std = (active_render) ? ATTR_STD_UV : ATTR_STD_NONE;
      ustring uv_name = ustring(l.name().c_str());
      AttributeStandard tangent_std = (active_render) ? ATTR_STD_UV_TANGENT : ATTR_STD_NONE;
      ustring tangent_name = ustring((string(l.name().c_str()) + ".tangent").c_str());

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

        const float(*b_uv_map)[2] = static_cast<const float(*)[2]>(l.uv[0].ptr.data);
        float2 *fdata = uv_attr->data_float2();
        for (int i = 0; i < tris_num; i++) {
          const MLoopTri &tri = looptris[i];
          fdata[i * 3 + 0] = make_float2(b_uv_map[tri.tri[0]][0], b_uv_map[tri.tri[0]][1]);
          fdata[i * 3 + 1] = make_float2(b_uv_map[tri.tri[1]][0], b_uv_map[tri.tri[1]][1]);
          fdata[i * 3 + 2] = make_float2(b_uv_map[tri.tri[2]][0], b_uv_map[tri.tri[2]][1]);
        }
      }

      /* UV tangent */
      if (need_tangent) {
        AttributeStandard sign_std = (active_render) ? ATTR_STD_UV_TANGENT_SIGN : ATTR_STD_NONE;
        ustring sign_name = ustring((string(l.name().c_str()) + ".tangent_sign").c_str());
        bool need_sign = (mesh->need_attribute(scene, sign_name) ||
                          mesh->need_attribute(scene, sign_std));
        mikk_compute_tangents(b_mesh, l.name().c_str(), mesh, need_sign, active_render);
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

static void attr_create_subd_uv_map(Scene *scene, Mesh *mesh, BL::Mesh &b_mesh, bool subdivide_uvs)
{
  const int polys_num = b_mesh.polygons.length();
  if (polys_num == 0) {
    return;
  }
  const int *poly_offsets = static_cast<const int *>(b_mesh.polygons[0].ptr.data);

  if (!b_mesh.uv_layers.empty()) {
    BL::Mesh::uv_layers_iterator l;
    int i = 0;

    for (b_mesh.uv_layers.begin(l); l != b_mesh.uv_layers.end(); ++l, ++i) {
      bool active_render = l->active_render();
      AttributeStandard uv_std = (active_render) ? ATTR_STD_UV : ATTR_STD_NONE;
      ustring uv_name = ustring(l->name().c_str());
      AttributeStandard tangent_std = (active_render) ? ATTR_STD_UV_TANGENT : ATTR_STD_NONE;
      ustring tangent_name = ustring((string(l->name().c_str()) + ".tangent").c_str());

      /* Denotes whether UV map was requested directly. */
      const bool need_uv = mesh->need_attribute(scene, uv_name) ||
                           mesh->need_attribute(scene, uv_std);
      /* Denotes whether tangent was requested directly. */
      const bool need_tangent = mesh->need_attribute(scene, tangent_name) ||
                                (active_render && mesh->need_attribute(scene, tangent_std));

      Attribute *uv_attr = NULL;

      /* UV map */
      if (need_uv || need_tangent) {
        if (active_render)
          uv_attr = mesh->subd_attributes.add(uv_std, uv_name);
        else
          uv_attr = mesh->subd_attributes.add(uv_name, TypeFloat2, ATTR_ELEMENT_CORNER);

        if (subdivide_uvs) {
          uv_attr->flags |= ATTR_SUBDIVIDED;
        }

        float2 *fdata = uv_attr->data_float2();

        for (int i = 0; i < polys_num; i++) {
          const int poly_start = poly_offsets[i];
          const int poly_size = poly_offsets[i + 1] - poly_start;
          for (int j = 0; j < poly_size; j++) {
            *(fdata++) = get_float2(l->data[poly_start + j].uv());
          }
        }
      }

      /* UV tangent */
      if (need_tangent) {
        AttributeStandard sign_std = (active_render) ? ATTR_STD_UV_TANGENT_SIGN : ATTR_STD_NONE;
        ustring sign_name = ustring((string(l->name().c_str()) + ".tangent_sign").c_str());
        bool need_sign = (mesh->need_attribute(scene, sign_name) ||
                          mesh->need_attribute(scene, sign_std));
        mikk_compute_tangents(b_mesh, l->name().c_str(), mesh, need_sign, active_render);
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

static void attr_create_pointiness(Scene *scene, Mesh *mesh, BL::Mesh &b_mesh, bool subdivision)
{
  if (!mesh->need_attribute(scene, ATTR_STD_POINTINESS)) {
    return;
  }
  const int num_verts = b_mesh.vertices.length();
  if (num_verts == 0) {
    return;
  }
  const float(*positions)[3] = static_cast<const float(*)[3]>(b_mesh.vertices[0].ptr.data);

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
  const float(*b_vert_normals)[3] = static_cast<const float(*)[3]>(
      b_mesh.vertex_normals[0].ptr.data);
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

  const int2 *edges = static_cast<int2 *>(b_mesh.edges[0].ptr.data);
  const int edges_num = b_mesh.edges.length();

  for (int i = 0; i < edges_num; i++) {
    const int2 &b_edge = edges[i];
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
  for (int i = 0; i < edges_num; i++) {
    const int2 &b_edge = edges[i];
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

static const int *find_corner_vert_attribute(BL::Mesh b_mesh)
{
  for (BL::Attribute &b_attribute : b_mesh.attributes) {
    if (b_attribute.domain() != BL::Attribute::domain_CORNER) {
      continue;
    }
    if (b_attribute.data_type() != BL::Attribute::data_type_INT) {
      continue;
    }
    if (b_attribute.name() != ".corner_vert") {
      continue;
    }
    BL::IntAttribute b_int_attribute{b_attribute};
    if (b_int_attribute.data.length() == 0) {
      return nullptr;
    }
    return static_cast<const int *>(b_int_attribute.data[0].ptr.data);
  }
  return nullptr;
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
                                          BL::Mesh &b_mesh,
                                          bool subdivision)
{
  if (!mesh->need_attribute(scene, ATTR_STD_RANDOM_PER_ISLAND)) {
    return;
  }

  int number_of_vertices = b_mesh.vertices.length();
  if (number_of_vertices == 0) {
    return;
  }

  DisjointSet vertices_sets(number_of_vertices);

  const int2 *edges = static_cast<int2 *>(b_mesh.edges[0].ptr.data);
  const int edges_num = b_mesh.edges.length();
  const int *corner_verts = find_corner_vert_attribute(b_mesh);

  for (int i = 0; i < edges_num; i++) {
    vertices_sets.join(edges[i][0], edges[i][1]);
  }

  AttributeSet &attributes = (subdivision) ? mesh->subd_attributes : mesh->attributes;
  Attribute *attribute = attributes.add(ATTR_STD_RANDOM_PER_ISLAND);
  float *data = attribute->data_float();

  if (!subdivision) {
    const int tris_num = b_mesh.loop_triangles.length();
    if (tris_num != 0) {
      const MLoopTri *looptris = static_cast<const MLoopTri *>(b_mesh.loop_triangles[0].ptr.data);
      for (int i = 0; i < tris_num; i++) {
        const int vert = corner_verts[looptris[i].tri[0]];
        data[i] = hash_uint_to_float(vertices_sets.find(vert));
      }
    }
  }
  else {
    const int polys_num = b_mesh.polygons.length();
    if (polys_num != 0) {
      const int *poly_offsets = static_cast<const int *>(b_mesh.polygons[0].ptr.data);
      for (int i = 0; i < polys_num; i++) {
        const int vert = corner_verts[poly_offsets[i]];
        data[i] = hash_uint_to_float(vertices_sets.find(vert));
      }
    }
  }
}

/* Create Mesh */

static const int *find_material_index_attribute(BL::Mesh b_mesh)
{
  for (BL::Attribute &b_attribute : b_mesh.attributes) {
    if (b_attribute.domain() != BL::Attribute::domain_FACE) {
      continue;
    }
    if (b_attribute.data_type() != BL::Attribute::data_type_INT) {
      continue;
    }
    if (b_attribute.name() != "material_index") {
      continue;
    }
    BL::IntAttribute b_int_attribute{b_attribute};
    if (b_int_attribute.data.length() == 0) {
      return nullptr;
    }
    return static_cast<const int *>(b_int_attribute.data[0].ptr.data);
  }
  return nullptr;
}

static const bool *find_sharp_face_attribute(BL::Mesh b_mesh)
{
  for (BL::Attribute &b_attribute : b_mesh.attributes) {
    if (b_attribute.domain() != BL::Attribute::domain_FACE) {
      continue;
    }
    if (b_attribute.data_type() != BL::Attribute::data_type_BOOLEAN) {
      continue;
    }
    if (b_attribute.name() != "sharp_face") {
      continue;
    }
    BL::IntAttribute b_int_attribute{b_attribute};
    if (b_int_attribute.data.length() == 0) {
      return nullptr;
    }
    return static_cast<const bool *>(b_int_attribute.data[0].ptr.data);
  }
  return nullptr;
}

static void create_mesh(Scene *scene,
                        Mesh *mesh,
                        BL::Mesh &b_mesh,
                        const array<Node *> &used_shaders,
                        const bool need_motion,
                        const float motion_scale,
                        const bool subdivision = false,
                        const bool subdivide_uvs = true)
{
  const int numverts = b_mesh.vertices.length();
  const int polys_num = b_mesh.polygons.length();
  int numfaces = (!subdivision) ? b_mesh.loop_triangles.length() : b_mesh.polygons.length();
  const int numcorners = b_mesh.loops.length();
  bool use_loop_normals = b_mesh.use_auto_smooth() &&
                          (mesh->get_subdivision_type() != Mesh::SUBDIVISION_CATMULL_CLARK);

  /* If no faces, create empty mesh. */
  if (numfaces == 0) {
    return;
  }

  const float(*positions)[3] = static_cast<const float(*)[3]>(b_mesh.vertices[0].ptr.data);
  const int *corner_verts = find_corner_vert_attribute(b_mesh);
  const int *material_indices = find_material_index_attribute(b_mesh);
  const bool *sharp_faces = find_sharp_face_attribute(b_mesh);
  const float(*corner_normals)[3] = nullptr;
  if (use_loop_normals) {
    corner_normals = static_cast<const float(*)[3]>(b_mesh.corner_normals[0].ptr.data);
  }

  int numngons = 0;
  int numtris = 0;
  if (!subdivision) {
    numtris = numfaces;
  }
  else {
    const int *poly_offsets = static_cast<const int *>(b_mesh.polygons[0].ptr.data);
    for (int i = 0; i < polys_num; i++) {
      const int poly_start = poly_offsets[i];
      const int poly_size = poly_offsets[i + 1] - poly_start;
      numngons += (poly_size == 4) ? 0 : 1;
    }
  }

  /* allocate memory */
  if (subdivision) {
    mesh->resize_subd_faces(numfaces, numngons, numcorners);
  }
  mesh->resize_mesh(numverts, numtris);

  float3 *verts = mesh->get_verts().data();
  for (int i = 0; i < numverts; i++) {
    verts[i] = make_float3(positions[i][0], positions[i][1], positions[i][2]);
  }

  AttributeSet &attributes = (subdivision) ? mesh->subd_attributes : mesh->attributes;
  Attribute *attr_N = attributes.add(ATTR_STD_VERTEX_NORMAL);
  float3 *N = attr_N->data_float3();

  if (subdivision || !(use_loop_normals && corner_normals)) {
    const float(*b_vert_normals)[3] = static_cast<const float(*)[3]>(
        b_mesh.vertex_normals[0].ptr.data);
    for (int i = 0; i < numverts; i++) {
      const float *b_vert_normal = b_vert_normals[i];
      N[i] = make_float3(b_vert_normal[0], b_vert_normal[1], b_vert_normal[2]);
    }
  }

  /* create generated coordinates from undeformed coordinates */
  const bool need_default_tangent = (subdivision == false) && (b_mesh.uv_layers.empty()) &&
                                    (mesh->need_attribute(scene, ATTR_STD_UV_TANGENT));
  if (mesh->need_attribute(scene, ATTR_STD_GENERATED) || need_default_tangent) {
    Attribute *attr = attributes.add(ATTR_STD_GENERATED);
    attr->flags |= ATTR_SUBDIVIDED;

    float3 loc, size;
    mesh_texture_space(b_mesh, loc, size);

    float3 *generated = attr->data_float3();
    size_t i = 0;

    BL::Mesh::vertices_iterator v;
    for (b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v) {
      generated[i++] = get_float3(v->undeformed_co()) * size - loc;
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

    const MLoopTri *looptris = static_cast<const MLoopTri *>(b_mesh.loop_triangles[0].ptr.data);
    for (int i = 0; i < numtris; i++) {
      const MLoopTri &tri = looptris[i];
      triangles[i * 3 + 0] = corner_verts[tri.tri[0]];
      triangles[i * 3 + 1] = corner_verts[tri.tri[1]];
      triangles[i * 3 + 2] = corner_verts[tri.tri[2]];
    }

    if (material_indices) {
      const int *looptri_polys = static_cast<const int *>(
          b_mesh.loop_triangle_polygons[0].ptr.data);
      for (int i = 0; i < numtris; i++) {
        shader[i] = clamp_material_index(material_indices[looptri_polys[i]]);
      }
    }
    else {
      std::fill(shader, shader + numtris, 0);
    }

    if (sharp_faces && !(use_loop_normals && corner_normals)) {
      const int *looptri_polys = static_cast<const int *>(
          b_mesh.loop_triangle_polygons[0].ptr.data);
      for (int i = 0; i < numtris; i++) {
        smooth[i] = !sharp_faces[looptri_polys[i]];
      }
    }
    else {
      std::fill(smooth, smooth + numtris, true);
    }

    if (use_loop_normals && corner_normals) {
      for (int i = 0; i < numtris; i++) {
        const MLoopTri &tri = looptris[i];
        for (int i = 0; i < 3; i++) {
          const int corner = tri.tri[i];
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

    if (sharp_faces && !use_loop_normals) {
      for (int i = 0; i < numfaces; i++) {
        subd_smooth[i] = !sharp_faces[i];
      }
    }
    else {
      std::fill(subd_smooth, subd_smooth + numfaces, true);
    }

    if (material_indices) {
      for (int i = 0; i < numfaces; i++) {
        subd_shader[i] = clamp_material_index(material_indices[i]);
      }
    }
    else {
      std::fill(subd_shader, subd_shader + numfaces, 0);
    }

    std::copy(corner_verts, corner_verts + numcorners, subd_face_corners);

    const int *poly_offsets = static_cast<const int *>(b_mesh.polygons[0].ptr.data);
    int ptex_offset = 0;
    for (int i = 0; i < numfaces; i++) {
      const int poly_start = poly_offsets[i];
      const int poly_size = poly_offsets[i + 1] - poly_start;

      subd_start_corner[i] = poly_start;
      subd_num_corners[i] = poly_size;
      subd_ptex_offset[i] = ptex_offset;
      const int num_ptex = (poly_size == 4) ? 1 : poly_size;
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
  attr_create_pointiness(scene, mesh, b_mesh, subdivision);
  attr_create_random_per_island(scene, mesh, b_mesh, subdivision);
  attr_create_generic(scene, mesh, b_mesh, subdivision, need_motion, motion_scale);

  if (subdivision) {
    attr_create_subd_uv_map(scene, mesh, b_mesh, subdivide_uvs);
  }
  else {
    attr_create_uv_map(scene, mesh, b_mesh);
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
                             BL::Mesh &b_mesh,
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

  const int edges_num = b_mesh.edges.length();

  if (edges_num != 0 && b_mesh.edge_creases.length() > 0) {
    const float *creases = static_cast<const float *>(b_mesh.edge_creases[0].data[0].ptr.data);

    size_t num_creases = 0;
    for (int i = 0; i < edges_num; i++) {
      if (creases[i] != 0.0f) {
        num_creases++;
      }
    }

    mesh->reserve_subd_creases(num_creases);

    const int2 *edges = static_cast<int2 *>(b_mesh.edges[0].ptr.data);
    for (int i = 0; i < edges_num; i++) {
      const float crease = creases[i];
      if (crease != 0.0f) {
        const int2 &b_edge = edges[i];
        mesh->add_edge_crease(b_edge[0], b_edge[1], crease);
      }
    }
  }

  for (BL::MeshVertexCreaseLayer &layer : b_mesh.vertex_creases) {
    const float *creases = static_cast<const float *>(layer.data[0].ptr.data);
    for (int i = 0; i < layer.data.length(); ++i) {
      if (creases[i] != 0.0f) {
        mesh->add_vertex_crease(i, creases[i]);
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
      if (new_mesh.get_subdivision_type() != Mesh::SUBDIVISION_NONE)
        create_subd_mesh(scene,
                         &new_mesh,
                         b_ob_info,
                         b_mesh,
                         new_mesh.get_used_shaders(),
                         need_motion,
                         motion_scale,
                         dicing_rate,
                         max_subdivisions);
      else
        create_mesh(scene,
                    &new_mesh,
                    b_mesh,
                    new_mesh.get_used_shaders(),
                    need_motion,
                    motion_scale,
                    false);

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
  BL::Mesh b_mesh(PointerRNA_NULL);
  if (ccl::BKE_object_is_deform_modified(b_ob_info, b_scene, preview)) {
    /* get derived mesh */
    b_mesh = object_to_mesh(b_data, b_ob_info, b_depsgraph, false, Mesh::SUBDIVISION_NONE);
  }

  const std::string ob_name = b_ob_info.real_object.name();

  /* TODO(sergey): Perform preliminary check for number of vertices. */
  if (b_mesh) {
    const int b_verts_num = b_mesh.vertices.length();
    if (b_verts_num == 0) {
      free_object_to_mesh(b_data, b_ob_info, b_mesh);
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
      if (attr_N)
        attr_mN = mesh->attributes.add(ATTR_STD_MOTION_VERTEX_NORMAL);

      new_attribute = true;
    }
    /* Load vertex data from mesh. */
    float3 *mP = attr_mP->data_float3() + motion_step * numverts;
    float3 *mN = (attr_mN) ? attr_mN->data_float3() + motion_step * numverts : NULL;

    const float(*positions)[3] = static_cast<const float(*)[3]>(b_mesh.vertices[0].ptr.data);

    /* NOTE: We don't copy more that existing amount of vertices to prevent
     * possible memory corruption.
     */
    for (int i = 0; i < std::min<size_t>(b_verts_num, numverts); i++) {
      mP[i] = make_float3(positions[i][0], positions[i][1], positions[i][2]);
    }
    if (mN) {
      const float(*b_vert_normals)[3] = static_cast<const float(*)[3]>(
          b_mesh.vertex_normals[0].ptr.data);
      for (int i = 0; i < std::min<size_t>(b_verts_num, numverts); i++) {
        const float *b_vert_normal = b_vert_normals[i];
        mN[i] = make_float3(b_vert_normal[0], b_vert_normal[1], b_vert_normal[2]);
      }
    }
    if (new_attribute) {
      /* In case of new attribute, we verify if there really was any motion. */
      if (b_verts_num != numverts ||
          memcmp(mP, &mesh->get_verts()[0], sizeof(float3) * numverts) == 0) {
        /* no motion, remove attributes again */
        if (b_verts_num != numverts) {
          VLOG_WARNING << "Topology differs, disabling motion blur for object " << ob_name;
        }
        else {
          VLOG_DEBUG << "No actual deformation motion for object " << ob_name;
        }
        mesh->attributes.remove(ATTR_STD_MOTION_VERTEX_POSITION);
        if (attr_mN)
          mesh->attributes.remove(ATTR_STD_MOTION_VERTEX_NORMAL);
      }
      else if (motion_step > 0) {
        VLOG_DEBUG << "Filling deformation motion for object " << ob_name;
        /* motion, fill up previous steps that we might have skipped because
         * they had no motion, but we need them anyway now */
        float3 *P = &mesh->get_verts()[0];
        float3 *N = (attr_N) ? attr_N->data_float3() : NULL;
        for (int step = 0; step < motion_step; step++) {
          memcpy(attr_mP->data_float3() + step * numverts, P, sizeof(float3) * numverts);
          if (attr_mN)
            memcpy(attr_mN->data_float3() + step * numverts, N, sizeof(float3) * numverts);
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

    free_object_to_mesh(b_data, b_ob_info, b_mesh);
    return;
  }

  /* No deformation on this frame, copy coordinates if other frames did have it. */
  mesh->copy_center_to_motion_step(motion_step);
}

CCL_NAMESPACE_END
