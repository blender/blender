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

#include "render/camera.h"
#include "render/colorspace.h"
#include "render/mesh.h"
#include "render/object.h"
#include "render/scene.h"

#include "blender/blender_session.h"
#include "blender/blender_sync.h"
#include "blender/blender_util.h"

#include "subd/subd_patch.h"
#include "subd/subd_split.h"

#include "util/util_algorithm.h"
#include "util/util_disjoint_set.h"
#include "util/util_foreach.h"
#include "util/util_hash.h"
#include "util/util_logging.h"
#include "util/util_math.h"

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
    const AttributeSet &attributes = (mesh->subd_faces.size()) ? mesh->subd_attributes :
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
  if (userdata->mesh->subd_faces.size()) {
    return userdata->mesh->subd_faces.size();
  }
  else {
    return userdata->mesh->num_triangles();
  }
}

static int mikk_get_num_verts_of_face(const SMikkTSpaceContext *context, const int face_num)
{
  const MikkUserData *userdata = (const MikkUserData *)context->m_pUserData;
  if (userdata->mesh->subd_faces.size()) {
    const Mesh *mesh = userdata->mesh;
    return mesh->subd_faces[face_num].num_corners;
  }
  else {
    return 3;
  }
}

static int mikk_vertex_index(const Mesh *mesh, const int face_num, const int vert_num)
{
  if (mesh->subd_faces.size()) {
    const Mesh::SubdFace &face = mesh->subd_faces[face_num];
    return mesh->subd_face_corners[face.start_corner + vert_num];
  }
  else {
    return mesh->triangles[face_num * 3 + vert_num];
  }
}

static int mikk_corner_index(const Mesh *mesh, const int face_num, const int vert_num)
{
  if (mesh->subd_faces.size()) {
    const Mesh::SubdFace &face = mesh->subd_faces[face_num];
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
  const float3 vP = mesh->verts[vertex_index];
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
  if (mesh->subd_faces.size()) {
    const Mesh::SubdFace &face = mesh->subd_faces[face_num];
    if (face.smooth) {
      const int vertex_index = mikk_vertex_index(mesh, face_num, vert_num);
      vN = userdata->vertex_normal[vertex_index];
    }
    else {
      vN = face.normal(mesh);
    }
  }
  else {
    if (mesh->smooth[face_num]) {
      const int vertex_index = mikk_vertex_index(mesh, face_num, vert_num);
      vN = userdata->vertex_normal[vertex_index];
    }
    else {
      const Mesh::Triangle tri = mesh->get_triangle(face_num);
      vN = tri.compute_normal(&mesh->verts[0]);
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
  AttributeSet &attributes = (mesh->subd_faces.size()) ? mesh->subd_attributes : mesh->attributes;
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
  BL::Mesh::sculpt_vertex_colors_iterator l;

  for (b_mesh.sculpt_vertex_colors.begin(l); l != b_mesh.sculpt_vertex_colors.end(); ++l) {
    const bool active_render = l->active_render();
    AttributeStandard vcol_std = (active_render) ? ATTR_STD_VERTEX_COLOR : ATTR_STD_NONE;
    ustring vcol_name = ustring(l->name().c_str());

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
      *(cdata++) = get_float4(l->data[i].color());
    }
  }
}

/* Create vertex color attributes. */
static void attr_create_vertex_color(Scene *scene, Mesh *mesh, BL::Mesh &b_mesh, bool subdivision)
{
  BL::Mesh::vertex_colors_iterator l;

  for (b_mesh.vertex_colors.begin(l); l != b_mesh.vertex_colors.end(); ++l) {
    const bool active_render = l->active_render();
    AttributeStandard vcol_std = (active_render) ? ATTR_STD_VERTEX_COLOR : ATTR_STD_NONE;
    ustring vcol_name = ustring(l->name().c_str());

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

      BL::Mesh::polygons_iterator p;
      uchar4 *cdata = vcol_attr->data_uchar4();

      for (b_mesh.polygons.begin(p); p != b_mesh.polygons.end(); ++p) {
        int n = p->loop_total();
        for (int i = 0; i < n; i++) {
          float4 color = get_float4(l->data[p->loop_start() + i].color());
          /* Compress/encode vertex color using the sRGB curve. */
          *(cdata++) = color_float4_to_uchar4(color_srgb_to_linear_v4(color));
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

      BL::Mesh::loop_triangles_iterator t;
      uchar4 *cdata = vcol_attr->data_uchar4();

      for (b_mesh.loop_triangles.begin(t); t != b_mesh.loop_triangles.end(); ++t) {
        int3 li = get_int3(t->loops());
        float4 c1 = get_float4(l->data[li[0]].color());
        float4 c2 = get_float4(l->data[li[1]].color());
        float4 c3 = get_float4(l->data[li[2]].color());

        /* Compress/encode vertex color using the sRGB curve. */
        cdata[0] = color_float4_to_uchar4(color_srgb_to_linear_v4(c1));
        cdata[1] = color_float4_to_uchar4(color_srgb_to_linear_v4(c2));
        cdata[2] = color_float4_to_uchar4(color_srgb_to_linear_v4(c3));
        cdata += 3;
      }
    }
  }
}

/* Create uv map attributes. */
static void attr_create_uv_map(Scene *scene, Mesh *mesh, BL::Mesh &b_mesh)
{
  if (b_mesh.uv_layers.length() != 0) {
    BL::Mesh::uv_layers_iterator l;

    for (b_mesh.uv_layers.begin(l); l != b_mesh.uv_layers.end(); ++l) {
      const bool active_render = l->active_render();
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

        BL::Mesh::loop_triangles_iterator t;
        float2 *fdata = uv_attr->data_float2();

        for (b_mesh.loop_triangles.begin(t); t != b_mesh.loop_triangles.end(); ++t) {
          int3 li = get_int3(t->loops());
          fdata[0] = get_float2(l->data[li[0]].uv());
          fdata[1] = get_float2(l->data[li[1]].uv());
          fdata[2] = get_float2(l->data[li[2]].uv());
          fdata += 3;
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
  if (b_mesh.uv_layers.length() != 0) {
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

        BL::Mesh::polygons_iterator p;
        float2 *fdata = uv_attr->data_float2();

        for (b_mesh.polygons.begin(p); p != b_mesh.polygons.end(); ++p) {
          int n = p->loop_total();
          for (int j = 0; j < n; j++) {
            *(fdata++) = get_float2(l->data[p->loop_start() + j].uv());
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
  VertexAverageComparator compare(mesh->verts);
  sort(sorted_vert_indeices.begin(), sorted_vert_indeices.end(), compare);
  /* This array stores index of the original vertex for the given vertex
   * index.
   */
  vector<int> vert_orig_index(num_verts);
  for (int sorted_vert_index = 0; sorted_vert_index < num_verts; ++sorted_vert_index) {
    const int vert_index = sorted_vert_indeices[sorted_vert_index];
    const float3 &vert_co = mesh->verts[vert_index];
    bool found = false;
    for (int other_sorted_vert_index = sorted_vert_index + 1; other_sorted_vert_index < num_verts;
         ++other_sorted_vert_index) {
      const int other_vert_index = sorted_vert_indeices[other_sorted_vert_index];
      const float3 &other_vert_co = mesh->verts[other_vert_index];
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
  vector<float3> vert_normal(num_verts, make_float3(0.0f, 0.0f, 0.0f));
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
  vector<float3> edge_accum(num_verts, make_float3(0.0f, 0.0f, 0.0f));
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

  BL::Mesh::edges_iterator e;
  for (b_mesh.edges.begin(e); e != b_mesh.edges.end(); ++e) {
    vertices_sets.join(e->vertices()[0], e->vertices()[1]);
  }

  AttributeSet &attributes = (subdivision) ? mesh->subd_attributes : mesh->attributes;
  Attribute *attribute = attributes.add(ATTR_STD_RANDOM_PER_ISLAND);
  float *data = attribute->data_float();

  if (!subdivision) {
    BL::Mesh::loop_triangles_iterator t;
    for (b_mesh.loop_triangles.begin(t); t != b_mesh.loop_triangles.end(); ++t) {
      data[t->index()] = hash_uint_to_float(vertices_sets.find(t->vertices()[0]));
    }
  }
  else {
    BL::Mesh::polygons_iterator p;
    for (b_mesh.polygons.begin(p); p != b_mesh.polygons.end(); ++p) {
      data[p->index()] = hash_uint_to_float(vertices_sets.find(p->vertices()[0]));
    }
  }
}

/* Create Mesh */

static void create_mesh(Scene *scene,
                        Mesh *mesh,
                        BL::Mesh &b_mesh,
                        const vector<Shader *> &used_shaders,
                        bool subdivision = false,
                        bool subdivide_uvs = true)
{
  /* count vertices and faces */
  int numverts = b_mesh.vertices.length();
  int numfaces = (!subdivision) ? b_mesh.loop_triangles.length() : b_mesh.polygons.length();
  int numtris = 0;
  int numcorners = 0;
  int numngons = 0;
  bool use_loop_normals = b_mesh.use_auto_smooth() &&
                          (mesh->subdivision_type != Mesh::SUBDIVISION_CATMULL_CLARK);

  /* If no faces, create empty mesh. */
  if (numfaces == 0) {
    return;
  }

  if (!subdivision) {
    numtris = numfaces;
  }
  else {
    BL::Mesh::polygons_iterator p;
    for (b_mesh.polygons.begin(p); p != b_mesh.polygons.end(); ++p) {
      numngons += (p->loop_total() == 4) ? 0 : 1;
      numcorners += p->loop_total();
    }
  }

  /* allocate memory */
  mesh->reserve_mesh(numverts, numtris);
  mesh->reserve_subd_faces(numfaces, numngons, numcorners);

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
  const bool need_default_tangent = (subdivision == false) && (b_mesh.uv_layers.length() == 0) &&
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
    BL::Mesh::loop_triangles_iterator t;

    for (b_mesh.loop_triangles.begin(t); t != b_mesh.loop_triangles.end(); ++t) {
      BL::MeshPolygon p = b_mesh.polygons[t->polygon_index()];
      int3 vi = get_int3(t->vertices());

      int shader = clamp(p.material_index(), 0, used_shaders.size() - 1);
      bool smooth = p.use_smooth() || use_loop_normals;

      if (use_loop_normals) {
        BL::Array<float, 9> loop_normals = t->split_normals();
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
    BL::Mesh::polygons_iterator p;
    vector<int> vi;

    for (b_mesh.polygons.begin(p); p != b_mesh.polygons.end(); ++p) {
      int n = p->loop_total();
      int shader = clamp(p->material_index(), 0, used_shaders.size() - 1);
      bool smooth = p->use_smooth() || use_loop_normals;

      vi.resize(n);
      for (int i = 0; i < n; i++) {
        /* NOTE: Autosmooth is already taken care about. */
        vi[i] = b_mesh.loops[p->loop_start() + i].vertex_index();
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
                             BL::Object &b_ob,
                             BL::Mesh &b_mesh,
                             const vector<Shader *> &used_shaders,
                             float dicing_rate,
                             int max_subdivisions)
{
  BL::SubsurfModifier subsurf_mod(b_ob.modifiers[b_ob.modifiers.length() - 1]);
  bool subdivide_uvs = subsurf_mod.uv_smooth() != BL::SubsurfModifier::uv_smooth_NONE;

  create_mesh(scene, mesh, b_mesh, used_shaders, true, subdivide_uvs);

  /* export creases */
  size_t num_creases = 0;
  BL::Mesh::edges_iterator e;

  for (b_mesh.edges.begin(e); e != b_mesh.edges.end(); ++e) {
    if (e->crease() != 0.0f) {
      num_creases++;
    }
  }

  mesh->subd_creases.resize(num_creases);

  Mesh::SubdEdgeCrease *crease = mesh->subd_creases.data();
  for (b_mesh.edges.begin(e); e != b_mesh.edges.end(); ++e) {
    if (e->crease() != 0.0f) {
      crease->v[0] = e->vertices()[0];
      crease->v[1] = e->vertices()[1];
      crease->crease = e->crease();

      crease++;
    }
  }

  /* set subd params */
  if (!mesh->subd_params) {
    mesh->subd_params = new SubdParams(mesh);
  }
  SubdParams &sdparams = *mesh->subd_params;

  PointerRNA cobj = RNA_pointer_get(&b_ob.ptr, "cycles");

  sdparams.dicing_rate = max(0.1f, RNA_float_get(&cobj, "dicing_rate") * dicing_rate);
  sdparams.max_level = max_subdivisions;

  sdparams.objecttoworld = get_transform(b_ob.matrix_world());
}

/* Sync */

static void sync_mesh_fluid_motion(BL::Object &b_ob, Scene *scene, Mesh *mesh)
{
  if (scene->need_motion() == Scene::MOTION_NONE)
    return;

  BL::FluidDomainSettings b_fluid_domain = object_fluid_liquid_domain_find(b_ob);

  if (!b_fluid_domain)
    return;

  /* If the mesh has modifiers following the fluid domain we can't export motion. */
  if (b_fluid_domain.mesh_vertices.length() != mesh->verts.size())
    return;

  /* Find or add attribute */
  float3 *P = &mesh->verts[0];
  Attribute *attr_mP = mesh->attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

  if (!attr_mP) {
    attr_mP = mesh->attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
  }

  /* Only export previous and next frame, we don't have any in between data. */
  float motion_times[2] = {-1.0f, 1.0f};
  for (int step = 0; step < 2; step++) {
    float relative_time = motion_times[step] * scene->motion_shutter_time() * 0.5f;
    float3 *mP = attr_mP->data_float3() + step * mesh->verts.size();

    BL::FluidDomainSettings::mesh_vertices_iterator svi;
    int i = 0;

    for (b_fluid_domain.mesh_vertices.begin(svi); svi != b_fluid_domain.mesh_vertices.end();
         ++svi, ++i) {
      mP[i] = P[i] + get_float3(svi->velocity()) * relative_time;
    }
  }
}

void BlenderSync::sync_mesh(BL::Depsgraph b_depsgraph,
                            BL::Object b_ob,
                            Mesh *mesh,
                            const vector<Shader *> &used_shaders)
{
  array<int> oldtriangles;
  array<Mesh::SubdFace> oldsubd_faces;
  array<int> oldsubd_face_corners;
  oldtriangles.steal_data(mesh->triangles);
  oldsubd_faces.steal_data(mesh->subd_faces);
  oldsubd_face_corners.steal_data(mesh->subd_face_corners);

  mesh->clear();
  mesh->used_shaders = used_shaders;

  mesh->subdivision_type = Mesh::SUBDIVISION_NONE;

  if (view_layer.use_surfaces) {
    /* Adaptive subdivision setup. Not for baking since that requires
     * exact mapping to the Blender mesh. */
    if (!scene->bake_manager->get_baking()) {
      mesh->subdivision_type = object_subdivision_type(b_ob, preview, experimental);
    }

    /* For some reason, meshes do not need this... */
    bool need_undeformed = mesh->need_attribute(scene, ATTR_STD_GENERATED);
    BL::Mesh b_mesh = object_to_mesh(
        b_data, b_ob, b_depsgraph, need_undeformed, mesh->subdivision_type);

    if (b_mesh) {
      /* Sync mesh itself. */
      if (mesh->subdivision_type != Mesh::SUBDIVISION_NONE)
        create_subd_mesh(
            scene, mesh, b_ob, b_mesh, mesh->used_shaders, dicing_rate, max_subdivisions);
      else
        create_mesh(scene, mesh, b_mesh, mesh->used_shaders, false);

      free_object_to_mesh(b_data, b_ob, b_mesh);
    }
  }

  /* mesh fluid motion mantaflow */
  sync_mesh_fluid_motion(b_ob, scene, mesh);

  /* tag update */
  bool rebuild = (oldtriangles != mesh->triangles) || (oldsubd_faces != mesh->subd_faces) ||
                 (oldsubd_face_corners != mesh->subd_face_corners);

  mesh->tag_update(scene, rebuild);
}

void BlenderSync::sync_mesh_motion(BL::Depsgraph b_depsgraph,
                                   BL::Object b_ob,
                                   Mesh *mesh,
                                   int motion_step)
{
  /* Fluid motion blur already exported. */
  BL::FluidDomainSettings b_fluid_domain = object_fluid_liquid_domain_find(b_ob);
  if (b_fluid_domain) {
    return;
  }

  /* Skip if no vertices were exported. */
  size_t numverts = mesh->verts.size();
  if (numverts == 0) {
    return;
  }

  /* Skip objects without deforming modifiers. this is not totally reliable,
   * would need a more extensive check to see which objects are animated. */
  BL::Mesh b_mesh(PointerRNA_NULL);
  if (ccl::BKE_object_is_deform_modified(b_ob, b_scene, preview)) {
    /* get derived mesh */
    b_mesh = object_to_mesh(b_data, b_ob, b_depsgraph, false, Mesh::SUBDIVISION_NONE);
  }

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
          memcmp(mP, &mesh->verts[0], sizeof(float3) * numverts) == 0) {
        /* no motion, remove attributes again */
        if (b_mesh.vertices.length() != numverts) {
          VLOG(1) << "Topology differs, disabling motion blur for object " << b_ob.name();
        }
        else {
          VLOG(1) << "No actual deformation motion for object " << b_ob.name();
        }
        mesh->attributes.remove(ATTR_STD_MOTION_VERTEX_POSITION);
        if (attr_mN)
          mesh->attributes.remove(ATTR_STD_MOTION_VERTEX_NORMAL);
      }
      else if (motion_step > 0) {
        VLOG(1) << "Filling deformation motion for object " << b_ob.name();
        /* motion, fill up previous steps that we might have skipped because
         * they had no motion, but we need them anyway now */
        float3 *P = &mesh->verts[0];
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
        VLOG(1) << "Topology differs, discarding motion blur for object " << b_ob.name()
                << " at time " << motion_step;
        memcpy(mP, &mesh->verts[0], sizeof(float3) * numverts);
        if (mN != NULL) {
          memcpy(mN, attr_N->data_float3(), sizeof(float3) * numverts);
        }
      }
    }

    free_object_to_mesh(b_data, b_ob, b_mesh);
    return;
  }

  /* No deformation on this frame, copy coordinates if other frames did have it. */
  mesh->copy_center_to_motion_step(motion_step);
}

CCL_NAMESPACE_END
