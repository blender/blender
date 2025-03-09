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
      interp_attribute_vertex_linear<T>(subd_attr, mesh_attr);
      break;
    }
    case ATTR_ELEMENT_CORNER:
    case ATTR_ELEMENT_CORNER_BYTE: {
      interp_attribute_corner_linear<T>(subd_attr, mesh_attr);
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
