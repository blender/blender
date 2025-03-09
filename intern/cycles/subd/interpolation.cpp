/* SPDX-FileCopyrightText: 2011-2024 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#include "subd/interpolation.h"

#include "scene/attribute.h"
#include "scene/mesh.h"

#include "util/color.h"

CCL_NAMESPACE_BEGIN

/* Classes for interpolation to use float math for byte value, for precision. */
template<typename T> struct SubdFloat {
  using Type = T;
  using AccumType = T;

  static Type read(const Type &value)
  {
    return value;
  }

  static Type output(const Type &value)
  {
    return value;
  }
};

struct SubdByte {
  using Type = uchar4;
  using AccumType = float4;

  static AccumType read(const Type &value)
  {
    return color_uchar4_to_float4(value);
  }

  static Type output(const AccumType &value)
  {
    return color_float4_to_uchar4(value);
  }
};

bool SubdAttributeInterpolation::support_interp_attribute(const Attribute &attr) const
{
  // TODO: Recompute UV tangent
  switch (attr.std) {
    /* Smooth normals are computed from derivatives, for linear interpolate. */
    case ATTR_STD_VERTEX_NORMAL:
    case ATTR_STD_MOTION_VERTEX_NORMAL:
      if (mesh.get_subdivision_type() == Mesh::SUBDIVISION_CATMULL_CLARK) {
        return false;
      }
      break;
    /* Ptex coordinates will be computed by subdivision. */
    case ATTR_STD_PTEX_FACE_ID:
    case ATTR_STD_PTEX_UV:
      return false;
    default:
      break;
  }

  /* Skip element types that should not exist for subd attributes anyway. */
  switch (attr.element) {
    case ATTR_ELEMENT_VERTEX:
    case ATTR_ELEMENT_CORNER:
    case ATTR_ELEMENT_CORNER_BYTE:
    case ATTR_ELEMENT_VERTEX_MOTION:
    case ATTR_ELEMENT_FACE:
      break;
    default:
      return false;
  }

  return true;
}

void SubdAttributeInterpolation::interp_attribute(const Attribute &subd_attr, Attribute &mesh_attr)
{
  if (subd_attr.element == ATTR_ELEMENT_CORNER_BYTE) {
    interp_attribute_type<SubdByte>(subd_attr, mesh_attr);
  }
  else if (Attribute::same_storage(subd_attr.type, TypeFloat)) {
    interp_attribute_type<SubdFloat<float>>(subd_attr, mesh_attr);
  }
  else if (Attribute::same_storage(subd_attr.type, TypeFloat2)) {
    interp_attribute_type<SubdFloat<float2>>(subd_attr, mesh_attr);
  }
  else if (Attribute::same_storage(subd_attr.type, TypeVector)) {
    interp_attribute_type<SubdFloat<float3>>(subd_attr, mesh_attr);
  }
  else if (Attribute::same_storage(subd_attr.type, TypeFloat4)) {
    interp_attribute_type<SubdFloat<float4>>(subd_attr, mesh_attr);
  }
}
const int *SubdAttributeInterpolation::get_ptex_face_mapping()
{
  if (ptex_face_to_base_face.empty()) {
    ptex_face_to_base_face.resize(num_patches);

    int *ptex_face_to_base_face_data = ptex_face_to_base_face.data();
    const size_t num_faces = mesh.get_num_subd_faces();
    int i = 0;

    for (size_t f = 0; f < num_faces; f++) {
      Mesh::SubdFace face = mesh.get_subd_face(f);
      const int num_ptex_faces = (face.is_quad()) ? 1 : face.num_corners;
      for (int j = 0; j < num_ptex_faces; j++) {
        ptex_face_to_base_face_data[i++] = f;
      }
    }
  }

  return ptex_face_to_base_face.data();
}

template<typename T>
void SubdAttributeInterpolation::interp_attribute_vertex_linear(const Attribute &subd_attr,
                                                                Attribute &mesh_attr,
                                                                const int motion_step)
{
  const int *ptex_face_to_base_face_data = get_ptex_face_mapping();
  const int num_base_verts = mesh.get_num_subd_base_verts();

  /* Interpolate values at vertices. */
  const size_t triangles_size = mesh.num_triangles();
  const int *patch_index = mesh.subd_triangle_patch_index.data();
  const float2 *patch_uv = mesh.subd_corner_patch_uv.data();
  const typename T::Type *subd_data = reinterpret_cast<const typename T::Type *>(
                                          subd_attr.data()) +
                                      motion_step * num_base_verts;
  typename T::Type *mesh_data = reinterpret_cast<typename T::Type *>(mesh_attr.data()) +
                                motion_step * mesh.get_verts().size();
  const int *subd_face_corners = mesh.get_subd_face_corners().data();

  for (size_t i = 0; i < triangles_size; i++) {
    const int p = patch_index[i];
    const int f = ptex_face_to_base_face_data[p];
    Mesh::SubdFace face = mesh.get_subd_face(f);
    Mesh::Triangle triangle = mesh.get_triangle(i);

    if (face.is_quad()) {
      /* Simple case for quads. */
      const typename T::AccumType value0 = T::read(
          subd_data[subd_face_corners[face.start_corner + 0]]);
      const typename T::AccumType value1 = T::read(
          subd_data[subd_face_corners[face.start_corner + 1]]);
      const typename T::AccumType value2 = T::read(
          subd_data[subd_face_corners[face.start_corner + 2]]);
      const typename T::AccumType value3 = T::read(
          subd_data[subd_face_corners[face.start_corner + 3]]);

      for (int j = 0; j < 3; j++) {
        const float2 uv = patch_uv[(i * 3) + j];
        const typename T::AccumType value = interp(
            interp(value0, value1, uv.x), interp(value3, value2, uv.x), uv.y);
        mesh_data[triangle.v[j]] = T::output(value);
      }
    }
    else {
      /* Other n-gons are split into n quads. */
      const int corner = p - face.ptex_offset;

      /* Compute value at center of polygon. */
      typename T::AccumType value_center = T::read(
          subd_data[subd_face_corners[face.start_corner]]);
      for (int j = 1; j < face.num_corners; j++) {
        value_center += T::read(subd_data[subd_face_corners[face.start_corner + j]]);
      }
      value_center /= (float)face.num_corners;

      for (int j = 0; j < 3; j++) {
        const float2 uv = patch_uv[(i * 3) + j];

        /* Compute value at corner at adjacent vertices. */
        const typename T::AccumType value_corner = T::read(
            subd_data[subd_face_corners[face.start_corner + corner]]);
        const typename T::AccumType value_prev =
            0.5f * (value_corner +
                    T::read(subd_data[subd_face_corners[face.start_corner +
                                                        mod(corner - 1, face.num_corners)]]));
        const typename T::AccumType value_next =
            0.5f * (value_corner +
                    T::read(subd_data[subd_face_corners[face.start_corner +
                                                        mod(corner + 1, face.num_corners)]]));

        /* Interpolate. */
        const typename T::AccumType value = interp(
            interp(value_corner, value_next, uv.x), interp(value_prev, value_center, uv.x), uv.y);

        mesh_data[triangle.v[j]] = T::output(value);
      }
    }
  }
}

#ifdef WITH_OPENSUBDIV
template<typename T>
void SubdAttributeInterpolation::interp_attribute_vertex_smooth(const Attribute &subd_attr,
                                                                Attribute &mesh_attr,
                                                                const int motion_step)
{
  // TODO: Avoid computing derivative weights when not needed
  // TODO: overhead of FindPatch and EvaluateBasis with vertex position
  const int num_refiner_verts = osd_data.refiner->GetNumVerticesTotal();
  const int num_local_points = osd_data.patch_table->GetNumLocalPoints();
  const int num_base_verts = mesh.get_num_subd_base_verts();

  /* Refine attribute data to get patch coordinates. */
  array<typename T::AccumType> refined_array(num_refiner_verts + num_local_points);

  const typename T::Type *base_src = reinterpret_cast<const typename T::Type *>(subd_attr.data()) +
                                     num_base_verts * motion_step;
  typename T::AccumType *base_dst = refined_array.data();
  for (int i = 0; i < num_base_verts; i++) {
    base_dst[i] = T::read(base_src[i]);
  }

  Far::PrimvarRefiner primvar_refiner(*osd_data.refiner);
  typename T::AccumType *src = refined_array.data();
  for (int i = 0; i < osd_data.refiner->GetMaxLevel(); i++) {
    typename T::AccumType *dest = src + osd_data.refiner->GetLevel(i).GetNumVertices();
    primvar_refiner.Interpolate(
        i + 1, (OsdValue<typename T::AccumType> *)src, (OsdValue<typename T::AccumType> *&)dest);
    src = dest;
  }

  if (num_local_points) {
    osd_data.patch_table->ComputeLocalPointValues(
        (OsdValue<typename T::AccumType> *)refined_array.data(),
        (OsdValue<typename T::AccumType> *)(refined_array.data() + num_refiner_verts));
  }

  /* Evaluate patches at limit. */
  const size_t triangles_size = mesh.num_triangles();
  const int *patch_index = mesh.subd_triangle_patch_index.data();
  const float2 *patch_uv = mesh.subd_corner_patch_uv.data();
  const typename T::AccumType *subd_data = refined_array.data();
  typename T::Type *mesh_data = reinterpret_cast<typename T::Type *>(mesh_attr.data()) +
                                mesh.get_verts().size() * motion_step;

  /* Compute motion normals alongside positions. */
  float3 *mesh_normal_data = nullptr;
  if constexpr (std::is_same_v<typename T::Type, float3>) {
    if (mesh_attr.std == ATTR_STD_MOTION_VERTEX_POSITION) {
      Attribute *attr_normal = mesh.attributes.add(ATTR_STD_MOTION_VERTEX_NORMAL);
      mesh_normal_data = attr_normal->data_float3() + mesh.get_verts().size() * motion_step;
    }
  }

  for (size_t i = 0; i < triangles_size; i++) {
    const int p = patch_index[i];
    Mesh::Triangle triangle = mesh.get_triangle(i);

    for (int j = 0; j < 3; j++) {
      /* Compute patch weights. */
      const float2 uv = patch_uv[(i * 3) + j];
      const Far::PatchTable::PatchHandle &handle = *osd_data.patch_map->FindPatch(
          p, (double)uv.x, (double)uv.y);

      float p_weights[20], du_weights[20], dv_weights[20];
      osd_data.patch_table->EvaluateBasis(handle, uv.x, uv.y, p_weights, du_weights, dv_weights);
      Far::ConstIndexArray cv = osd_data.patch_table->GetPatchVertices(handle);

      /* Compution position. */
      typename T::AccumType value = subd_data[cv[0]] * p_weights[0];
      for (int k = 1; k < cv.size(); k++) {
        value += subd_data[cv[k]] * p_weights[k];
      }
      mesh_data[triangle.v[j]] = T::output(value);

      /* Optionally compute normal. */
      if constexpr (std::is_same_v<typename T::Type, float3>) {
        if (mesh_normal_data) {
          float3 du = zero_float3();
          float3 dv = zero_float3();
          for (int k = 0; k < cv.size(); k++) {
            const float3 p = subd_data[cv[k]];
            du += p * du_weights[k];
            dv += p * dv_weights[k];
          }
          mesh_normal_data[triangle.v[j]] = safe_normalize_fallback(cross(du, dv),
                                                                    make_float3(0.0f, 0.0f, 1.0f));
        }
      }
    }
  }
}
#endif

template<typename T>
void SubdAttributeInterpolation::interp_attribute_corner_linear(const Attribute &subd_attr,
                                                                Attribute &mesh_attr)
{
  const int *ptex_face_to_base_face_data = get_ptex_face_mapping();

  /* Interpolate values at corners. */
  const size_t triangles_size = mesh.num_triangles();
  const int *patch_index = mesh.subd_triangle_patch_index.data();
  const float2 *patch_uv = mesh.subd_corner_patch_uv.data();
  const typename T::Type *subd_data = reinterpret_cast<const typename T::Type *>(subd_attr.data());
  typename T::Type *mesh_data = reinterpret_cast<typename T::Type *>(mesh_attr.data());

  for (size_t i = 0; i < triangles_size; i++) {
    const int p = patch_index[i];
    const int f = ptex_face_to_base_face_data[p];
    Mesh::SubdFace face = mesh.get_subd_face(f);

    if (face.is_quad()) {
      /* Simple case for quads. */
      const typename T::AccumType value0 = T::read(subd_data[face.start_corner + 0]);
      const typename T::AccumType value1 = T::read(subd_data[face.start_corner + 1]);
      const typename T::AccumType value2 = T::read(subd_data[face.start_corner + 2]);
      const typename T::AccumType value3 = T::read(subd_data[face.start_corner + 3]);

      for (int j = 0; j < 3; j++) {
        const float2 uv = patch_uv[(i * 3) + j];
        const typename T::AccumType value = interp(
            interp(value0, value1, uv.x), interp(value3, value2, uv.x), uv.y);
        mesh_data[(i * 3) + j] = T::output(value);
      }
    }
    else {
      /* Other n-gons are split into n quads. */
      const int corner = p - face.ptex_offset;

      /* Compute value at center of polygon. */
      typename T::AccumType value_center = T::read(subd_data[face.start_corner]);
      for (int j = 1; j < face.num_corners; j++) {
        value_center += T::read(subd_data[face.start_corner + j]);
      }

      for (int j = 0; j < 3; j++) {
        const float2 uv = patch_uv[(i * 3) + j];

        /* Compute value at corner at adjacent vertices. */
        const typename T::AccumType value_corner = T::read(subd_data[face.start_corner + corner]);
        const typename T::AccumType value_prev =
            0.5f * (value_corner +
                    T::read(subd_data[face.start_corner + mod(corner - 1, face.num_corners)]));
        const typename T::AccumType value_next =
            0.5f * (value_corner +
                    T::read(subd_data[face.start_corner + mod(corner + 1, face.num_corners)]));

        /* Interpolate. */
        const typename T::AccumType value = interp(
            interp(value_corner, value_next, uv.x), interp(value_prev, value_center, uv.x), uv.y);

        mesh_data[(i * 3) + j] = T::output(value);
      }
    }
  }
}

#ifdef WITH_OPENSUBDIV
template<typename T>
void SubdAttributeInterpolation::interp_attribute_corner_smooth(Attribute &mesh_attr,
                                                                const int channel,
                                                                const vector<char> &merged_values)
{
  // TODO: Avoid computing derivative weights when not needed
  const int num_refiner_fvars = osd_data.refiner->GetNumFVarValuesTotal(channel);
  const int num_local_points = osd_data.patch_table->GetNumLocalPointsFaceVarying(channel);
  const int num_base_fvars = osd_data.refiner->GetLevel(0).GetNumFVarValues(channel);

  /* Refine attribute data to get patch coordinates. */
  array<typename T::AccumType> refined_array(num_refiner_fvars + num_local_points);

  const typename T::Type *base_src = reinterpret_cast<const typename T::Type *>(
      merged_values.data());
  typename T::AccumType *base_dst = refined_array.data();
  for (int i = 0; i < num_base_fvars; i++) {
    base_dst[i] = T::read(base_src[i]);
  }

  Far::PrimvarRefiner primvar_refiner(*osd_data.refiner);
  typename T::AccumType *src = refined_array.data();
  for (int i = 0; i < osd_data.refiner->GetMaxLevel(); i++) {
    typename T::AccumType *dest = src + osd_data.refiner->GetLevel(i).GetNumFVarValues(channel);
    primvar_refiner.InterpolateFaceVarying(i + 1,
                                           (OsdValue<typename T::AccumType> *)src,
                                           (OsdValue<typename T::AccumType> *&)dest,
                                           channel);
    src = dest;
  }

  if (num_local_points) {
    osd_data.patch_table->ComputeLocalPointValuesFaceVarying(
        (OsdValue<typename T::AccumType> *)refined_array.data(),
        (OsdValue<typename T::AccumType> *)(refined_array.data() + num_refiner_fvars),
        channel);
  }

  /* Evaluate patches at limit. */
  const size_t triangles_size = mesh.num_triangles();
  const int *patch_index = mesh.subd_triangle_patch_index.data();
  const float2 *patch_uv = mesh.subd_corner_patch_uv.data();
  const typename T::AccumType *subd_data = refined_array.data();
  typename T::Type *mesh_data = reinterpret_cast<typename T::Type *>(mesh_attr.data());

  for (size_t i = 0; i < triangles_size; i++) {
    const int p = patch_index[i];

    for (int j = 0; j < 3; j++) {
      /* Compute patch weights. */
      const float2 uv = patch_uv[(i * 3) + j];
      const Far::PatchTable::PatchHandle &handle = *osd_data.patch_map->FindPatch(
          p, (double)uv.x, (double)uv.y);

      float p_weights[20], du_weights[20], dv_weights[20];
      osd_data.patch_table->EvaluateBasisFaceVarying(handle,
                                                     uv.x,
                                                     uv.y,
                                                     p_weights,
                                                     du_weights,
                                                     dv_weights,
                                                     nullptr,
                                                     nullptr,
                                                     nullptr,
                                                     channel);
      Far::ConstIndexArray cv = osd_data.patch_table->GetPatchFVarValues(handle, channel);

      /* Compution position. */
      typename T::AccumType value = subd_data[cv[0]] * p_weights[0];
      for (int k = 1; k < cv.size(); k++) {
        value += subd_data[cv[k]] * p_weights[k];
      }
      mesh_data[(i * 3) + j] = T::output(value);
    }
  }
}
#endif

template<typename T>
void SubdAttributeInterpolation::interp_attribute_face(const Attribute &subd_attr,
                                                       Attribute &mesh_attr)
{
  const int *ptex_face_to_base_face_data = get_ptex_face_mapping();

  /* Interpolate values at corners. */
  const size_t triangles_size = mesh.num_triangles();
  const int *patch_index = mesh.subd_triangle_patch_index.data();
  const typename T::Type *subd_data = reinterpret_cast<const typename T::Type *>(subd_attr.data());
  typename T::Type *mesh_data = reinterpret_cast<typename T::Type *>(mesh_attr.data());

  for (size_t i = 0; i < triangles_size; i++) {
    const int p = patch_index[i];
    const int f = ptex_face_to_base_face_data[p];
    mesh_data[i] = subd_data[f];
  }
}

template<typename T>
void SubdAttributeInterpolation::interp_attribute_type(const Attribute &subd_attr,
                                                       Attribute &mesh_attr)
{
  switch (subd_attr.element) {
    case ATTR_ELEMENT_VERTEX: {
#ifdef WITH_OPENSUBDIV
      if (mesh.get_subdivision_type() == Mesh::SUBDIVISION_CATMULL_CLARK) {
        /* Only smoothly interpolation known position-like attributes. */
        switch (subd_attr.std) {
          case ATTR_STD_GENERATED:
          case ATTR_STD_POSITION_UNDEFORMED:
          case ATTR_STD_POSITION_UNDISPLACED:
            interp_attribute_vertex_smooth<T>(subd_attr, mesh_attr);
            break;
          default:
            interp_attribute_vertex_linear<T>(subd_attr, mesh_attr);
            break;
        }
      }
      else
#endif
      {
        interp_attribute_vertex_linear<T>(subd_attr, mesh_attr);
      }
      break;
    }
    case ATTR_ELEMENT_CORNER:
    case ATTR_ELEMENT_CORNER_BYTE: {
#ifdef WITH_OPENSUBDIV
      if (osd_mesh.use_smooth_fvar(subd_attr)) {
        for (const auto &merged_fvar : osd_mesh.merged_fvars) {
          if (&merged_fvar.attr == &subd_attr) {
            if constexpr (std::is_same_v<typename T::Type, float2>) {
              interp_attribute_corner_smooth<T>(
                  mesh_attr, merged_fvar.channel, merged_fvar.values);
              return;
            }
          }
        }
      }
#endif
      interp_attribute_corner_linear<T>(subd_attr, mesh_attr);
      break;
    }
    case ATTR_ELEMENT_VERTEX_MOTION: {
      /* Interpolate each motion step individually. */
      for (int step = 0; step < mesh.get_motion_steps() - 1; step++) {
#ifdef WITH_OPENSUBDIV
        if (mesh.get_subdivision_type() == Mesh::SUBDIVISION_CATMULL_CLARK) {
          interp_attribute_vertex_smooth<T>(subd_attr, mesh_attr, step);
        }
        else
#endif
        {
          interp_attribute_vertex_linear<T>(subd_attr, mesh_attr, step);
        }
      }
      break;
    }
    case ATTR_ELEMENT_FACE: {
      interp_attribute_face<T>(subd_attr, mesh_attr);
      break;
    }
    default:
      break;
  }
}

CCL_NAMESPACE_END
