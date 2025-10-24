/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include <algorithm>
#include <optional>

#include "blender/attribute_convert.h"
#include "blender/session.h"
#include "blender/sync.h"
#include "blender/util.h"

#include "scene/camera.h"
#include "scene/mesh.h"
#include "scene/object.h"
#include "scene/scene.h"

#include "subd/split.h"

#include "util/algorithm.h"
#include "util/disjoint_set.h"

#include "util/hash.h"
#include "util/log.h"
#include "util/math.h"

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_customdata.hh"
#include "BKE_mesh.hh"

CCL_NAMESPACE_BEGIN

static void attr_create_motion_from_velocity(Mesh *mesh,
                                             const blender::Span<blender::float3> b_attr,
                                             const float motion_scale)
{
  const int numverts = mesh->get_verts().size();

  /* Override motion steps to fixed number. */
  mesh->set_motion_steps(3);

  AttributeSet &attributes = mesh->get_subdivision_type() == Mesh::SUBDIVISION_NONE ?
                                 mesh->attributes :
                                 mesh->subd_attributes;

  /* Find or add attribute */
  float3 *P = mesh->get_verts().data();
  Attribute *attr_mP = attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);

  if (!attr_mP) {
    attr_mP = attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
  }

  /* Only export previous and next frame, we don't have any in between data. */
  const float motion_times[2] = {-1.0f, 1.0f};
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
  const ustring default_color_name{
      std::string_view(BKE_id_attributes_default_color_name(&b_mesh.id).value_or(""))};

  b_attributes.foreach_attribute([&](const blender::bke::AttributeIter &iter) {
    const ustring name{std::string_view(iter.name)};
    const bool is_render_color = name == default_color_name;

    if (need_motion && name == u_velocity) {
      const blender::VArraySpan b_attribute = *iter.get<blender::float3>(
          blender::bke::AttrDomain::Point);
      attr_create_motion_from_velocity(mesh, b_attribute, motion_scale);
    }

    if (!(mesh->need_attribute(scene, name) ||
          (is_render_color && mesh->need_attribute(scene, ATTR_STD_VERTEX_COLOR))))
    {
      return;
    }
    if (attributes.find(name)) {
      return;
    }

    blender::bke::AttrDomain b_domain = iter.domain;
    if (b_domain == blender::bke::AttrDomain::Edge) {
      /* Blender's attribute API handles edge to vertex attribute domain interpolation. */
      b_domain = blender::bke::AttrDomain::Point;
    }

    const blender::bke::GAttributeReader b_attr = iter.get(b_domain);
    if (b_attr.varray.is_empty()) {
      return;
    }

    if (b_attr.domain == blender::bke::AttrDomain::Corner &&
        iter.data_type == blender::bke::AttrType::ColorByte)
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
      return;
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
        return;
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
  });
}

static set<ustring> get_blender_uv_names(const ::Mesh &b_mesh)
{
  set<ustring> uv_names;
  b_mesh.attributes().foreach_attribute([&](const blender::bke::AttributeIter &iter) {
    if (iter.domain == blender::bke::AttrDomain::Corner &&
        iter.data_type == blender::bke::AttrType::Float2)
    {
      if (!blender::bke::attribute_name_is_anonymous(iter.name)) {
        uv_names.emplace(std::string_view(iter.name));
      }
    }
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
  const ustring render_name(std::string_view(b_mesh.default_uv_map_name()));
  if (blender_uv_names.empty()) {
    return;
  }

  for (const ustring &uv_name : blender_uv_names) {
    const bool active_render = uv_name == render_name;
    const AttributeStandard uv_std = (active_render) ? ATTR_STD_UV : ATTR_STD_NONE;

    /* Denotes whether UV map was requested directly. */
    const bool need_uv = mesh->need_attribute(scene, uv_name) ||
                         (active_render && mesh->need_attribute(scene, uv_std));

    /* UV map */
    Attribute *uv_attr = nullptr;
    if (need_uv) {
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
  }
}

static void attr_create_subd_uv_map(Scene *scene,
                                    Mesh *mesh,
                                    const ::Mesh &b_mesh,
                                    const set<ustring> &blender_uv_names)
{
  const blender::OffsetIndices faces = b_mesh.faces();
  if (faces.is_empty()) {
    return;
  }

  if (blender_uv_names.empty()) {
    return;
  }

  const blender::bke::AttributeAccessor b_attributes = b_mesh.attributes();
  const ustring render_name(std::string_view(b_mesh.default_uv_map_name()));
  for (const ustring &uv_name : blender_uv_names) {
    const bool active_render = uv_name == render_name;
    const AttributeStandard uv_std = (active_render) ? ATTR_STD_UV : ATTR_STD_NONE;

    /* Denotes whether UV map was requested directly. */
    const bool need_uv = mesh->need_attribute(scene, uv_name) ||
                         (active_render && mesh->need_attribute(scene, uv_std));

    Attribute *uv_attr = nullptr;

    /* UV map */
    if (need_uv) {
      if (active_render) {
        uv_attr = mesh->subd_attributes.add(uv_std, uv_name);
      }
      else {
        uv_attr = mesh->subd_attributes.add(uv_name, TypeFloat2, ATTR_ELEMENT_CORNER);
      }

      uv_attr->flags |= ATTR_SUBDIVIDE_SMOOTH_FVAR;

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
  const VertexAverageComparator compare(mesh->get_verts());
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
  memset(counter.data(), 0, sizeof(int) * counter.size());

  for (const int i : edges.index_range()) {
    const blender::int2 b_edge = edges[i];
    const int v0 = vert_orig_index[b_edge[0]];
    const int v1 = vert_orig_index[b_edge[1]];
    if (visited_edges.exists(v0, v1)) {
      continue;
    }
    visited_edges.insert(v0, v1);
    const float3 co0 = make_float3(positions[v0][0], positions[v0][1], positions[v0][2]);
    const float3 co1 = make_float3(positions[v1][0], positions[v1][1], positions[v1][2]);
    const float3 edge = safe_normalize(co1 - co0);
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
  memcpy(data, raw_data.data(), sizeof(float) * raw_data.size());
  memset(counter.data(), 0, sizeof(int) * counter.size());
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
                        const bool subdivision = false)
{
  const blender::Span<blender::float3> positions = b_mesh.vert_positions();
  const blender::OffsetIndices faces = b_mesh.faces();
  const blender::Span<int> corner_verts = b_mesh.corner_verts();
  const blender::bke::AttributeAccessor b_attributes = b_mesh.attributes();
  const blender::bke::MeshNormalDomain normals_domain = b_mesh.normals_domain(true);
  const int numfaces = (!subdivision) ? b_mesh.corner_tris().size() : faces.size();

  const bool use_corner_normals = normals_domain == blender::bke::MeshNormalDomain::Corner &&
                                  (mesh->get_subdivision_type() !=
                                   Mesh::SUBDIVISION_CATMULL_CLARK);

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

  int numtris = 0;
  if (!subdivision) {
    numtris = numfaces;
  }
  /* allocate memory */
  if (subdivision) {
    mesh->resize_subd_faces(numfaces, corner_verts.size());
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
    const float (*orco)[3] = static_cast<const float (*)[3]>(
        CustomData_get_layer(&b_mesh.vert_data, CD_ORCO));
    Attribute *attr = attributes.add(ATTR_STD_GENERATED);

    float3 loc;
    float3 size;
    mesh_texture_space(b_mesh, loc, size);

    float texspace_location[3];
    float texspace_size[3];
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
    attr_create_subd_uv_map(scene, mesh, b_mesh, blender_uv_names);
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

    float3 loc;
    float3 size;
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
                             const float dicing_rate,
                             const int max_subdivisions)
{
  BL::Object b_ob = b_ob_info.real_object;

  BL::SubsurfModifier subsurf_mod(b_ob.modifiers[b_ob.modifiers.length() - 1]);
  const bool use_creases = subsurf_mod.use_creases();

  create_mesh(scene, mesh, b_mesh, used_shaders, need_motion, motion_scale, true);

  if (use_creases) {
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
  }

  /* Set subd parameters. */
  Mesh::SubdivisionAdaptiveSpace space = Mesh::SUBDIVISION_ADAPTIVE_SPACE_PIXEL;
  switch (subsurf_mod.adaptive_space()) {
    case BL::SubsurfModifier::adaptive_space_OBJECT:
      space = Mesh::SUBDIVISION_ADAPTIVE_SPACE_OBJECT;
      break;
    case BL::SubsurfModifier::adaptive_space_PIXEL:
      space = Mesh::SUBDIVISION_ADAPTIVE_SPACE_PIXEL;
      break;
  }
  const float subd_dicing_rate = (space == Mesh::SUBDIVISION_ADAPTIVE_SPACE_PIXEL) ?
                                     max(0.1f, subsurf_mod.adaptive_pixel_size() * dicing_rate) :
                                     subsurf_mod.adaptive_object_edge_length() * dicing_rate;

  mesh->set_subd_adaptive_space(space);
  mesh->set_subd_dicing_rate(subd_dicing_rate);
  mesh->set_subd_max_level(max_subdivisions);
  mesh->set_subd_objecttoworld(get_transform(b_ob.matrix_world()));
}

/* Sync */

void BlenderSync::sync_mesh(BObjectInfo &b_ob_info, Mesh *mesh)
{
  /* make a copy of the shaders as the caller in the main thread still need them for syncing the
   * attributes */
  array<Node *> used_shaders = mesh->get_used_shaders();

  Mesh new_mesh;
  new_mesh.set_used_shaders(used_shaders);

  if (view_layer.use_surfaces) {
    object_subdivision_to_mesh(b_ob_info.real_object, new_mesh, preview, use_adaptive_subdivision);
    BL::Mesh b_mesh = object_to_mesh(b_ob_info);

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

      free_object_to_mesh(b_ob_info, b_mesh);
    }
  }

  /* update original sockets */

  mesh->clear_non_sockets();

  for (const SocketType &socket : new_mesh.type->inputs) {
    /* Those sockets are updated in sync_object, so do not modify them. */
    if (socket.name == "use_motion_blur" || socket.name == "used_shaders") {
      continue;
    }
    mesh->set_value(socket, new_mesh, socket);
  }

  mesh->attributes.update(std::move(new_mesh.attributes));
  mesh->subd_attributes.update(std::move(new_mesh.subd_attributes));

  mesh->set_num_subd_faces(new_mesh.get_num_subd_faces());

  /* tag update */
  const bool rebuild = (mesh->triangles_is_modified()) || (mesh->subd_num_corners_is_modified()) ||
                       (mesh->subd_shader_is_modified()) || (mesh->subd_smooth_is_modified()) ||
                       (mesh->subd_ptex_offset_is_modified()) ||
                       (mesh->subd_start_corner_is_modified()) ||
                       (mesh->subd_face_corners_is_modified());

  mesh->tag_update(scene, rebuild);
}

void BlenderSync::sync_mesh_motion(BObjectInfo &b_ob_info, Mesh *mesh, const int motion_step)
{
  /* Skip if no vertices were exported. */
  const size_t numverts = mesh->get_verts().size();
  if (numverts == 0) {
    return;
  }

  /* Skip objects without deforming modifiers. this is not totally reliable,
   * would need a more extensive check to see which objects are animated. */
  BL::Mesh b_mesh_rna(PointerRNA_NULL);
  if (ccl::BKE_object_is_deform_modified(b_ob_info, b_scene, preview)) {
    /* get derived mesh */
    b_mesh_rna = object_to_mesh(b_ob_info);
  }

  const std::string ob_name = b_ob_info.real_object.name();

  /* TODO(sergey): Perform preliminary check for number of vertices. */
  if (b_mesh_rna) {
    const ::Mesh &b_mesh = *static_cast<const ::Mesh *>(b_mesh_rna.ptr.data);
    const int b_verts_num = b_mesh.verts_num;
    const blender::Span<blender::float3> positions = b_mesh.vert_positions();
    if (positions.is_empty()) {
      free_object_to_mesh(b_ob_info, b_mesh_rna);
      return;
    }

    AttributeSet &attributes = mesh->get_subdivision_type() == Mesh::SUBDIVISION_NONE ?
                                   mesh->attributes :
                                   mesh->subd_attributes;

    /* Export deformed coordinates. */
    /* Find attributes. */
    Attribute *attr_mP = attributes.find(ATTR_STD_MOTION_VERTEX_POSITION);
    Attribute *attr_mN = attributes.find(ATTR_STD_MOTION_VERTEX_NORMAL);
    Attribute *attr_N = attributes.find(ATTR_STD_VERTEX_NORMAL);
    bool new_attribute = false;
    /* Add new attributes if they don't exist already. */
    if (!attr_mP) {
      attr_mP = attributes.add(ATTR_STD_MOTION_VERTEX_POSITION);
      if (attr_N) {
        attr_mN = attributes.add(ATTR_STD_MOTION_VERTEX_NORMAL);
      }

      new_attribute = true;
    }
    /* Load vertex data from mesh. */
    float3 *mP = attr_mP->data_float3() + motion_step * numverts;
    float3 *mN = (attr_mN) ? attr_mN->data_float3() + motion_step * numverts : nullptr;

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
          memcmp(mP, mesh->get_verts().data(), sizeof(float3) * numverts) == 0)
      {
        /* no motion, remove attributes again */
        if (b_verts_num != numverts) {
          LOG_WARNING << "Topology differs, disabling motion blur for object " << ob_name;
        }
        else {
          LOG_TRACE << "No actual deformation motion for object " << ob_name;
        }
        attributes.remove(ATTR_STD_MOTION_VERTEX_POSITION);
        if (attr_mN) {
          attributes.remove(ATTR_STD_MOTION_VERTEX_NORMAL);
        }
      }
      else if (motion_step > 0) {
        LOG_TRACE << "Filling deformation motion for object " << ob_name;
        /* motion, fill up previous steps that we might have skipped because
         * they had no motion, but we need them anyway now */
        const float3 *P = mesh->get_verts().data();
        const float3 *N = (attr_N) ? attr_N->data_float3() : nullptr;
        for (int step = 0; step < motion_step; step++) {
          std::copy_n(P, numverts, attr_mP->data_float3() + step * numverts);
          if (attr_mN) {
            std::copy_n(N, numverts, attr_mN->data_float3() + step * numverts);
          }
        }
      }
    }
    else {
      if (b_verts_num != numverts) {
        LOG_WARNING << "Topology differs, discarding motion blur for object " << ob_name
                    << " at time " << motion_step;
        const float3 *P = mesh->get_verts().data();
        const float3 *N = (attr_N) ? attr_N->data_float3() : nullptr;
        std::copy_n(P, numverts, mP);
        if (mN != nullptr) {
          std::copy_n(N, numverts, mN);
        }
      }
    }

    free_object_to_mesh(b_ob_info, b_mesh_rna);
    return;
  }

  /* No deformation on this frame, copy coordinates if other frames did have it. */
  mesh->copy_center_to_motion_step(motion_step);
}

CCL_NAMESPACE_END
