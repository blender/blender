/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

#include "mikktspace.h"

CCL_NAMESPACE_BEGIN

/* Tangent Space */

struct MikkUserData {
  MikkUserData(const BL::Mesh &b_mesh,
               const char *layer_name,
               const Mesh *mesh,
               float3 *tangent,
               float *tangent_sign)
      : mesh(mesh), texface(NULL), orco(NULL), tangent(tangent), tangent_sign(tangent_sign)
  {
    const AttributeSet &attributes = (mesh->get_num_subd_faces()) ? mesh->subd_attributes :
                                                                    mesh->attributes;

    Attribute *attr_vN = attributes.find(ATTR_STD_VERTEX_NORMAL);
    vertex_normal = attr_vN->data_float3();

    if (layer_name == NULL) {
      Attribute *attr_orco = attributes.find(ATTR_STD_GENERATED);

      if (attr_orco) {
        orco = attr_orco->data_float3();
        mesh_texture_space(*(BL::Mesh *)&b_mesh, orco_loc, orco_size);
      }
    }
    else {
      Attribute *attr_uv = attributes.find(ustring(layer_name));
      if (attr_uv != NULL) {
        texface = attr_uv->data_float2();
      }
    }
  }

  const Mesh *mesh;
  int num_faces;

  float3 *vertex_normal;
  float2 *texface;
  float3 *orco;
  float3 orco_loc, orco_size;

  float3 *tangent;
  float *tangent_sign;
};

static int mikk_get_num_faces(const SMikkTSpaceContext *context)
{
  const MikkUserData *userdata = (const MikkUserData *)context->m_pUserData;
  if (userdata->mesh->get_num_subd_faces()) {
    return userdata->mesh->get_num_subd_faces();
  }
  else {
    return userdata->mesh->num_triangles();
  }
}

static int mikk_get_num_verts_of_face(const SMikkTSpaceContext *context, const int face_num)
{
  const MikkUserData *userdata = (const MikkUserData *)context->m_pUserData;
  if (userdata->mesh->get_num_subd_faces()) {
    const Mesh *mesh = userdata->mesh;
    return mesh->get_subd_num_corners()[face_num];
  }
  else {
    return 3;
  }
}

static int mikk_vertex_index(const Mesh *mesh, const int face_num, const int vert_num)
{
  if (mesh->get_num_subd_faces()) {
    const Mesh::SubdFace &face = mesh->get_subd_face(face_num);
    return mesh->get_subd_face_corners()[face.start_corner + vert_num];
  }
  else {
    return mesh->get_triangles()[face_num * 3 + vert_num];
  }
}

static int mikk_corner_index(const Mesh *mesh, const int face_num, const int vert_num)
{
  if (mesh->get_num_subd_faces()) {
    const Mesh::SubdFace &face = mesh->get_subd_face(face_num);
    return face.start_corner + vert_num;
  }
  else {
    return face_num * 3 + vert_num;
  }
}

static void mikk_get_position(const SMikkTSpaceContext *context,
                              float P[3],
                              const int face_num,
                              const int vert_num)
{
  const MikkUserData *userdata = (const MikkUserData *)context->m_pUserData;
  const Mesh *mesh = userdata->mesh;
  const int vertex_index = mikk_vertex_index(mesh, face_num, vert_num);
  const float3 vP = mesh->get_verts()[vertex_index];
  P[0] = vP.x;
  P[1] = vP.y;
  P[2] = vP.z;
}

static void mikk_get_texture_coordinate(const SMikkTSpaceContext *context,
                                        float uv[2],
                                        const int face_num,
                                        const int vert_num)
{
  const MikkUserData *userdata = (const MikkUserData *)context->m_pUserData;
  const Mesh *mesh = userdata->mesh;
  if (userdata->texface != NULL) {
    const int corner_index = mikk_corner_index(mesh, face_num, vert_num);
    float2 tfuv = userdata->texface[corner_index];
    uv[0] = tfuv.x;
    uv[1] = tfuv.y;
  }
  else if (userdata->orco != NULL) {
    const int vertex_index = mikk_vertex_index(mesh, face_num, vert_num);
    const float3 orco_loc = userdata->orco_loc;
    const float3 orco_size = userdata->orco_size;
    const float3 orco = (userdata->orco[vertex_index] + orco_loc) / orco_size;

    const float2 tmp = map_to_sphere(orco);
    uv[0] = tmp.x;
    uv[1] = tmp.y;
  }
  else {
    uv[0] = 0.0f;
    uv[1] = 0.0f;
  }
}

static void mikk_get_normal(const SMikkTSpaceContext *context,
                            float N[3],
                            const int face_num,
                            const int vert_num)
{
  const MikkUserData *userdata = (const MikkUserData *)context->m_pUserData;
  const Mesh *mesh = userdata->mesh;
  float3 vN;
  if (mesh->get_num_subd_faces()) {
    const Mesh::SubdFace &face = mesh->get_subd_face(face_num);
    if (face.smooth) {
      const int vertex_index = mikk_vertex_index(mesh, face_num, vert_num);
      vN = userdata->vertex_normal[vertex_index];
    }
    else {
      vN = face.normal(mesh);
    }
  }
  else {
    if (mesh->get_smooth()[face_num]) {
      const int vertex_index = mikk_vertex_index(mesh, face_num, vert_num);
      vN = userdata->vertex_normal[vertex_index];
    }
    else {
      const Mesh::Triangle tri = mesh->get_triangle(face_num);
      vN = tri.compute_normal(&mesh->get_verts()[0]);
    }
  }
  N[0] = vN.x;
  N[1] = vN.y;
  N[2] = vN.z;
}

static void mikk_set_tangent_space(const SMikkTSpaceContext *context,
                                   const float T[],
                                   const float sign,
                                   const int face_num,
                                   const int vert_num)
{
  MikkUserData *userdata = (MikkUserData *)context->m_pUserData;
  const Mesh *mesh = userdata->mesh;
  const int corner_index = mikk_corner_index(mesh, face_num, vert_num);
  userdata->tangent[corner_index] = make_float3(T[0], T[1], T[2]);
  if (userdata->tangent_sign != NULL) {
    userdata->tangent_sign[corner_index] = sign;
  }
}

static void mikk_compute_tangents(
    const BL::Mesh &b_mesh, const char *layer_name, Mesh *mesh, bool need_sign, bool active_render)
{
  /* Create tangent attributes. */
  AttributeSet &attributes = (mesh->get_num_subd_faces()) ? mesh->subd_attributes :
                                                            mesh->attributes;
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
  MikkUserData userdata(b_mesh, layer_name, mesh, tangent, tangent_sign);
  /* Setup interface. */
  SMikkTSpaceInterface sm_interface;
  memset(&sm_interface, 0, sizeof(sm_interface));
  sm_interface.m_getNumFaces = mikk_get_num_faces;
  sm_interface.m_getNumVerticesOfFace = mikk_get_num_verts_of_face;
  sm_interface.m_getPosition = mikk_get_position;
  sm_interface.m_getTexCoord = mikk_get_texture_coordinate;
  sm_interface.m_getNormal = mikk_get_normal;
  sm_interface.m_setTSpaceBasic = mikk_set_tangent_space;
  /* Setup context. */
  SMikkTSpaceContext context;
  memset(&context, 0, sizeof(context));
  context.m_pUserData = &userdata;
  context.m_pInterface = &sm_interface;
  /* Compute tangents. */
  genTangSpaceDefault(&context);
}

/* Create sculpt vertex color attributes. */
static void attr_create_sculpt_vertex_color(Scene *scene,
                                            Mesh *mesh,
                                            BL::Mesh &b_mesh,
                                            bool subdivision)
{
  for (BL::MeshVertColorLayer &l : b_mesh.sculpt_vertex_colors) {
    const bool active_render = l.active_render();
    AttributeStandard vcol_std = (active_render) ? ATTR_STD_VERTEX_COLOR : ATTR_STD_NONE;
    ustring vcol_name = ustring(l.name().c_str());

    const bool need_vcol = mesh->need_attribute(scene, vcol_name) ||
                           mesh->need_attribute(scene, vcol_std);

    if (!need_vcol) {
      continue;
    }

    AttributeSet &attributes = (subdivision) ? mesh->subd_attributes : mesh->attributes;
    Attribute *vcol_attr = attributes.add(vcol_name, TypeRGBA, ATTR_ELEMENT_VERTEX);
    vcol_attr->std = vcol_std;

    float4 *cdata = vcol_attr->data_float4();
    int numverts = b_mesh.vertices.length();

    for (int i = 0; i < numverts; i++) {
      *(cdata++) = get_float4(l.data[i].color());
    }
  }
}

template<typename TypeInCycles, typename GetValueAtIndex>
static void fill_generic_attribute(BL::Mesh &b_mesh,
                                   TypeInCycles *data,
                                   const AttributeElement element,
                                   const GetValueAtIndex &get_value_at_index)
{
  switch (element) {
    case ATTR_ELEMENT_CORNER: {
      for (BL::MeshLoopTriangle &t : b_mesh.loop_triangles) {
        const int index = t.index() * 3;
        BL::Array<int, 3> loops = t.loops();
        data[index] = get_value_at_index(loops[0]);
        data[index + 1] = get_value_at_index(loops[1]);
        data[index + 2] = get_value_at_index(loops[2]);
      }
      break;
    }
    case ATTR_ELEMENT_VERTEX: {
      const int num_verts = b_mesh.vertices.length();
      for (int i = 0; i < num_verts; i++) {
        data[i] = get_value_at_index(i);
      }
      break;
    }
    case ATTR_ELEMENT_FACE: {
      for (BL::MeshLoopTriangle &t : b_mesh.loop_triangles) {
        data[t.index()] = get_value_at_index(t.polygon_index());
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
      (b_attribute.data_type() == BL::Attribute::data_type_FLOAT_VECTOR)) {
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
  if (subdivision) {
    /* TODO: Handle subdivision correctly. */
    return;
  }
  AttributeSet &attributes = mesh->attributes;
  static const ustring u_velocity("velocity");

  for (BL::Attribute &b_attribute : b_mesh.attributes) {
    const ustring name{b_attribute.name().c_str()};

    if (need_motion && name == u_velocity) {
      attr_create_motion(mesh, b_attribute, motion_scale);
    }

    if (!mesh->need_attribute(scene, name)) {
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
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(
            b_mesh, data, element, [&](int i) { return b_float_attribute.data[i].value(); });
        break;
      }
      case BL::Attribute::data_type_BOOLEAN: {
        BL::BoolAttribute b_bool_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(
            b_mesh, data, element, [&](int i) { return (float)b_bool_attribute.data[i].value(); });
        break;
      }
      case BL::Attribute::data_type_INT: {
        BL::IntAttribute b_int_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeFloat, element);
        float *data = attr->data_float();
        fill_generic_attribute(
            b_mesh, data, element, [&](int i) { return (float)b_int_attribute.data[i].value(); });
        break;
      }
      case BL::Attribute::data_type_FLOAT_VECTOR: {
        BL::FloatVectorAttribute b_vector_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeVector, element);
        float3 *data = attr->data_float3();
        fill_generic_attribute(b_mesh, data, element, [&](int i) {
          BL::Array<float, 3> v = b_vector_attribute.data[i].vector();
          return make_float3(v[0], v[1], v[2]);
        });
        break;
      }
      case BL::Attribute::data_type_FLOAT_COLOR: {
        BL::FloatColorAttribute b_color_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeRGBA, element);
        float4 *data = attr->data_float4();
        fill_generic_attribute(b_mesh, data, element, [&](int i) {
          BL::Array<float, 4> v = b_color_attribute.data[i].color();
          return make_float4(v[0], v[1], v[2], v[3]);
        });
        break;
      }
      case BL::Attribute::data_type_FLOAT2: {
        BL::Float2Attribute b_float2_attribute{b_attribute};
        Attribute *attr = attributes.add(name, TypeFloat2, element);
        float2 *data = attr->data_float2();
        fill_generic_attribute(b_mesh, data, element, [&](int i) {
          BL::Array<float, 2> v = b_float2_attribute.data[i].vector();
          return make_float2(v[0], v[1]);
        });
        break;
      }
      default:
        /* Not supported. */
        break;
    }
  }
}

/* Create vertex color attributes. */
static void attr_create_vertex_color(Scene *scene, Mesh *mesh, BL::Mesh &b_mesh, bool subdivision)
{
  for (BL::MeshLoopColorLayer &l : b_mesh.vertex_colors) {
    const bool active_render = l.active_render();
    AttributeStandard vcol_std = (active_render) ? ATTR_STD_VERTEX_COLOR : ATTR_STD_NONE;
    ustring vcol_name = ustring(l.name().c_str());

    const bool need_vcol = mesh->need_attribute(scene, vcol_name) ||
                           mesh->need_attribute(scene, vcol_std);

    if (!need_vcol) {
      continue;
    }

    Attribute *vcol_attr = NULL;

    if (subdivision) {
      if (active_render) {
        vcol_attr = mesh->subd_attributes.add(vcol_std, vcol_name);
      }
      else {
        vcol_attr = mesh->subd_attributes.add(vcol_name, TypeRGBA, ATTR_ELEMENT_CORNER_BYTE);
      }

      uchar4 *cdata = vcol_attr->data_uchar4();

      for (BL::MeshPolygon &p : b_mesh.polygons) {
        int n = p.loop_total();
        for (int i = 0; i < n; i++) {
          float4 color = get_float4(l.data[p.loop_start() + i].color());
          /* Compress/encode vertex color using the sRGB curve. */
          *(cdata++) = color_float4_to_uchar4(color);
        }
      }
    }
    else {
      if (active_render) {
        vcol_attr = mesh->attributes.add(vcol_std, vcol_name);
      }
      else {
        vcol_attr = mesh->attributes.add(vcol_name, TypeRGBA, ATTR_ELEMENT_CORNER_BYTE);
      }

      uchar4 *cdata = vcol_attr->data_uchar4();

      for (BL::MeshLoopTriangle &t : b_mesh.loop_triangles) {
        int3 li = get_int3(t.loops());
        float4 c1 = get_float4(l.data[li[0]].color());
        float4 c2 = get_float4(l.data[li[1]].color());
        float4 c3 = get_float4(l.data[li[2]].color());

        /* Compress/encode vertex color using the sRGB curve. */
        cdata[0] = color_float4_to_uchar4(c1);
        cdata[1] = color_float4_to_uchar4(c2);
        cdata[2] = color_float4_to_uchar4(c3);

        cdata += 3;
      }
    }
  }
}

/* Create uv map attributes. */
static void attr_create_uv_map(Scene *scene, Mesh *mesh, BL::Mesh &b_mesh)
{
  if (!b_mesh.uv_layers.empty()) {
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

        float2 *fdata = uv_attr->data_float2();

        for (BL::MeshLoopTriangle &t : b_mesh.loop_triangles) {
          int3 li = get_int3(t.loops());
          fdata[0] = get_float2(l.data[li[0]].uv());
          fdata[1] = get_float2(l.data[li[1]].uv());
          fdata[2] = get_float2(l.data[li[2]].uv());
          fdata += 3;
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

        for (BL::MeshPolygon &p : b_mesh.polygons) {
          int n = p.loop_total();
          for (int j = 0; j < n; j++) {
            *(fdata++) = get_float2(l->data[p.loop_start() + j].uv());
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
  VertexAverageComparator(const array<float3> &verts) : verts_(verts)
  {
  }

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
         ++other_sorted_vert_index) {
      const int other_vert_index = sorted_vert_indeices[other_sorted_vert_index];
      const float3 &other_vert_co = mesh->get_verts()[other_vert_index];
      /* We are too far away now, we wouldn't have duplicate. */
      if ((other_vert_co.x + other_vert_co.y + other_vert_co.z) -
              (vert_co.x + vert_co.y + vert_co.z) >
          3 * FLT_EPSILON) {
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
    const float3 normal = get_float3(b_mesh.vertices[vert_index].normal());
    const int orig_index = vert_orig_index[vert_index];
    vert_normal[orig_index] += normal;
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
  BL::Mesh::edges_iterator e;
  EdgeMap visited_edges;
  int edge_index = 0;
  memset(&counter[0], 0, sizeof(int) * counter.size());
  for (b_mesh.edges.begin(e); e != b_mesh.edges.end(); ++e, ++edge_index) {
    const int v0 = vert_orig_index[b_mesh.edges[edge_index].vertices()[0]],
              v1 = vert_orig_index[b_mesh.edges[edge_index].vertices()[1]];
    if (visited_edges.exists(v0, v1)) {
      continue;
    }
    visited_edges.insert(v0, v1);
    float3 co0 = get_float3(b_mesh.vertices[v0].co()), co1 = get_float3(b_mesh.vertices[v1].co());
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
  edge_index = 0;
  visited_edges.clear();
  for (b_mesh.edges.begin(e); e != b_mesh.edges.end(); ++e, ++edge_index) {
    const int v0 = vert_orig_index[b_mesh.edges[edge_index].vertices()[0]],
              v1 = vert_orig_index[b_mesh.edges[edge_index].vertices()[1]];
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

  for (BL::MeshEdge &e : b_mesh.edges) {
    vertices_sets.join(e.vertices()[0], e.vertices()[1]);
  }

  AttributeSet &attributes = (subdivision) ? mesh->subd_attributes : mesh->attributes;
  Attribute *attribute = attributes.add(ATTR_STD_RANDOM_PER_ISLAND);
  float *data = attribute->data_float();

  if (!subdivision) {
    for (BL::MeshLoopTriangle &t : b_mesh.loop_triangles) {
      data[t.index()] = hash_uint_to_float(vertices_sets.find(t.vertices()[0]));
    }
  }
  else {
    for (BL::MeshPolygon &p : b_mesh.polygons) {
      data[p.index()] = hash_uint_to_float(vertices_sets.find(p.vertices()[0]));
    }
  }
}

/* Create Mesh */

static void create_mesh(Scene *scene,
                        Mesh *mesh,
                        BL::Mesh &b_mesh,
                        const array<Node *> &used_shaders,
                        const bool need_motion,
                        const float motion_scale,
                        const bool subdivision = false,
                        const bool subdivide_uvs = true)
{
  /* count vertices and faces */
  int numverts = b_mesh.vertices.length();
  int numfaces = (!subdivision) ? b_mesh.loop_triangles.length() : b_mesh.polygons.length();
  int numtris = 0;
  int numcorners = 0;
  int numngons = 0;
  bool use_loop_normals = b_mesh.use_auto_smooth() &&
                          (mesh->get_subdivision_type() != Mesh::SUBDIVISION_CATMULL_CLARK);

  /* If no faces, create empty mesh. */
  if (numfaces == 0) {
    return;
  }

  if (!subdivision) {
    numtris = numfaces;
  }
  else {
    for (BL::MeshPolygon &p : b_mesh.polygons) {
      numngons += (p.loop_total() == 4) ? 0 : 1;
      numcorners += p.loop_total();
    }
  }

  /* allocate memory */
  if (subdivision) {
    mesh->reserve_subd_faces(numfaces, numngons, numcorners);
  }

  mesh->reserve_mesh(numverts, numtris);

  /* create vertex coordinates and normals */
  BL::Mesh::vertices_iterator v;
  for (b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v)
    mesh->add_vertex(get_float3(v->co()));

  AttributeSet &attributes = (subdivision) ? mesh->subd_attributes : mesh->attributes;
  Attribute *attr_N = attributes.add(ATTR_STD_VERTEX_NORMAL);
  float3 *N = attr_N->data_float3();

  for (b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v, ++N)
    *N = get_float3(v->normal());
  N = attr_N->data_float3();

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

    for (b_mesh.vertices.begin(v); v != b_mesh.vertices.end(); ++v) {
      generated[i++] = get_float3(v->undeformed_co()) * size - loc;
    }
  }

  /* create faces */
  if (!subdivision) {
    for (BL::MeshLoopTriangle &t : b_mesh.loop_triangles) {
      BL::MeshPolygon p = b_mesh.polygons[t.polygon_index()];
      int3 vi = get_int3(t.vertices());

      int shader = clamp(p.material_index(), 0, used_shaders.size() - 1);
      bool smooth = p.use_smooth() || use_loop_normals;

      if (use_loop_normals) {
        BL::Array<float, 9> loop_normals = t.split_normals();
        for (int i = 0; i < 3; i++) {
          N[vi[i]] = make_float3(
              loop_normals[i * 3], loop_normals[i * 3 + 1], loop_normals[i * 3 + 2]);
        }
      }

      /* Create triangles.
       *
       * NOTE: Autosmooth is already taken care about.
       */
      mesh->add_triangle(vi[0], vi[1], vi[2], shader, smooth);
    }
  }
  else {
    vector<int> vi;

    for (BL::MeshPolygon &p : b_mesh.polygons) {
      int n = p.loop_total();
      int shader = clamp(p.material_index(), 0, used_shaders.size() - 1);
      bool smooth = p.use_smooth() || use_loop_normals;

      vi.resize(n);
      for (int i = 0; i < n; i++) {
        /* NOTE: Autosmooth is already taken care about. */
        vi[i] = b_mesh.loops[p.loop_start() + i].vertex_index();
      }

      /* create subd faces */
      mesh->add_subd_face(&vi[0], n, shader, smooth);
    }
  }

  /* Create all needed attributes.
   * The calculate functions will check whether they're needed or not.
   */
  attr_create_pointiness(scene, mesh, b_mesh, subdivision);
  attr_create_vertex_color(scene, mesh, b_mesh, subdivision);
  attr_create_sculpt_vertex_color(scene, mesh, b_mesh, subdivision);
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

  /* export creases */
  size_t num_creases = 0;

  for (BL::MeshEdge &e : b_mesh.edges) {
    if (e.crease() != 0.0f) {
      num_creases++;
    }
  }

  mesh->reserve_subd_creases(num_creases);

  for (BL::MeshEdge &e : b_mesh.edges) {
    if (e.crease() != 0.0f) {
      mesh->add_edge_crease(e.vertices()[0], e.vertices()[1], e.crease());
    }
  }

  for (BL::MeshVertexCreaseLayer &c : b_mesh.vertex_creases) {
    for (int i = 0; i < c.data.length(); ++i) {
      if (c.data[i].value() != 0.0f) {
        mesh->add_vertex_crease(i, c.data[i].value());
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
        socket.name == "used_shaders") {
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
    /* NOTE: We don't copy more that existing amount of vertices to prevent
     * possible memory corruption.
     */
    BL::Mesh::vertices_iterator v;
    int i = 0;
    for (b_mesh.vertices.begin(v); v != b_mesh.vertices.end() && i < numverts; ++v, ++i) {
      mP[i] = get_float3(v->co());
      if (mN)
        mN[i] = get_float3(v->normal());
    }
    if (new_attribute) {
      /* In case of new attribute, we verify if there really was any motion. */
      if (b_mesh.vertices.length() != numverts ||
          memcmp(mP, &mesh->get_verts()[0], sizeof(float3) * numverts) == 0) {
        /* no motion, remove attributes again */
        if (b_mesh.vertices.length() != numverts) {
          VLOG(1) << "Topology differs, disabling motion blur for object " << ob_name;
        }
        else {
          VLOG(1) << "No actual deformation motion for object " << ob_name;
        }
        mesh->attributes.remove(ATTR_STD_MOTION_VERTEX_POSITION);
        if (attr_mN)
          mesh->attributes.remove(ATTR_STD_MOTION_VERTEX_NORMAL);
      }
      else if (motion_step > 0) {
        VLOG(1) << "Filling deformation motion for object " << ob_name;
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
      if (b_mesh.vertices.length() != numverts) {
        VLOG(1) << "Topology differs, discarding motion blur for object " << ob_name << " at time "
                << motion_step;
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
