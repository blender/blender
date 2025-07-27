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

#ifdef WITH_OPENSUBDIV
SubdAttributeInterpolation::SubdAttributeInterpolation(Mesh &mesh,
                                                       OsdMesh &osd_mesh,
                                                       OsdData &osd_data)
#else
SubdAttributeInterpolation::SubdAttributeInterpolation(Mesh &mesh)
#endif
    : mesh(mesh)
#ifdef WITH_OPENSUBDIV
      ,
      osd_mesh(osd_mesh),
      osd_data(osd_data)
#endif
{
}

void SubdAttributeInterpolation::setup()
{
  if (mesh.get_num_subd_faces() == 0) {
    return;
  }

  for (const Attribute &subd_attr : mesh.subd_attributes.attributes) {
    if (!support_interp_attribute(subd_attr)) {
      continue;
    }
    Attribute &mesh_attr = mesh.attributes.copy(subd_attr);
    setup_attribute(subd_attr, mesh_attr);
  }
}

bool SubdAttributeInterpolation::support_interp_attribute(const Attribute &attr) const
{
  switch (attr.std) {
    /* Smooth normals are computed from derivatives, for linear interpolate. */
    case ATTR_STD_VERTEX_NORMAL:
    case ATTR_STD_MOTION_VERTEX_NORMAL:
      if (mesh.get_subdivision_type() == Mesh::SUBDIVISION_CATMULL_CLARK) {
        return false;
      }
      break;
    /* PTex coordinates will be computed by subdivision. */
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

void SubdAttributeInterpolation::setup_attribute(const Attribute &subd_attr, Attribute &mesh_attr)
{
  if (subd_attr.element == ATTR_ELEMENT_CORNER_BYTE) {
    setup_attribute_type<SubdByte>(subd_attr, mesh_attr);
  }
  else if (Attribute::same_storage(subd_attr.type, TypeFloat)) {
    setup_attribute_type<SubdFloat<float>>(subd_attr, mesh_attr);
  }
  else if (Attribute::same_storage(subd_attr.type, TypeFloat2)) {
    setup_attribute_type<SubdFloat<float2>>(subd_attr, mesh_attr);
  }
  else if (Attribute::same_storage(subd_attr.type, TypeVector)) {
    setup_attribute_type<SubdFloat<float3>>(subd_attr, mesh_attr);
  }
  else if (Attribute::same_storage(subd_attr.type, TypeFloat4) ||
           Attribute::same_storage(subd_attr.type, TypeRGBA))
  {
    setup_attribute_type<SubdFloat<float4>>(subd_attr, mesh_attr);
  }
}

template<typename T>
void SubdAttributeInterpolation::setup_attribute_vertex_linear(const Attribute &subd_attr,
                                                               Attribute &mesh_attr,
                                                               const int motion_step)
{
  SubdAttribute attr;

  const typename T::Type *subd_data = reinterpret_cast<const typename T::Type *>(
                                          subd_attr.data()) +
                                      motion_step * mesh.get_num_subd_base_verts();
  typename T::Type *mesh_data = reinterpret_cast<typename T::Type *>(mesh_attr.data()) +
                                motion_step * mesh.get_verts().size();

  assert(mesh_data != nullptr);

  attr.interp = [this, subd_data, mesh_data](const int /*patch_index*/,
                                             const int face_index,
                                             const int corner,
                                             const int *vert_index,
                                             const float2 *vert_uv,
                                             const int vert_num) {
    /* Interpolate values at vertices. */
    const int *subd_face_corners = mesh.get_subd_face_corners().data();

    Mesh::SubdFace face = mesh.get_subd_face(face_index);

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

      for (int i = 0; i < vert_num; i++) {
        const float2 uv = vert_uv[i];
        const typename T::AccumType value = interp(
            interp(value0, value1, uv.x), interp(value3, value2, uv.x), uv.y);
        mesh_data[vert_index[i]] = T::output(value);
      }
    }
    else {
      /* Other n-gons are split into n quads. */

      /* Compute value at center of polygon. */
      typename T::AccumType value_center = T::read(
          subd_data[subd_face_corners[face.start_corner]]);
      for (int j = 1; j < face.num_corners; j++) {
        value_center += T::read(subd_data[subd_face_corners[face.start_corner + j]]);
      }
      value_center /= (float)face.num_corners;

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

      for (int i = 0; i < vert_num; i++) {
        const float2 uv = vert_uv[i];

        /* Interpolate. */
        const typename T::AccumType value = interp(
            interp(value_corner, value_next, uv.x), interp(value_prev, value_center, uv.x), uv.y);

        mesh_data[vert_index[i]] = T::output(value);
      }
    }
  };

  vertex_attributes.push_back(std::move(attr));
}

#ifdef WITH_OPENSUBDIV
template<typename T>
void SubdAttributeInterpolation::setup_attribute_vertex_smooth(const Attribute &subd_attr,
                                                               Attribute &mesh_attr,
                                                               const int motion_step)
{
  SubdAttribute attr;

  // TODO: Avoid computing derivative weights when not needed
  // TODO: overhead of FindPatch and EvaluateBasis with vertex position
  const int num_refiner_verts = osd_data.refiner->GetNumVerticesTotal();
  const int num_local_points = osd_data.patch_table->GetNumLocalPoints();
  const int num_base_verts = mesh.get_num_subd_base_verts();

  /* Refine attribute data to get patch coordinates. */
  attr.refined_data.resize((num_refiner_verts + num_local_points) * sizeof(typename T::AccumType));
  typename T::AccumType *subd_data = reinterpret_cast<typename T::AccumType *>(
      attr.refined_data.data());

  const typename T::Type *base_src = reinterpret_cast<const typename T::Type *>(subd_attr.data()) +
                                     num_base_verts * motion_step;
  typename T::AccumType *base_dst = subd_data;
  for (int i = 0; i < num_base_verts; i++) {
    base_dst[i] = T::read(base_src[i]);
  }

  Far::PrimvarRefiner primvar_refiner(*osd_data.refiner);
  typename T::AccumType *src = subd_data;
  for (int i = 0; i < osd_data.refiner->GetMaxLevel(); i++) {
    typename T::AccumType *dest = src + osd_data.refiner->GetLevel(i).GetNumVertices();
    primvar_refiner.Interpolate(
        i + 1, (OsdValue<typename T::AccumType> *)src, (OsdValue<typename T::AccumType> *&)dest);
    src = dest;
  }

  if (num_local_points) {
    osd_data.patch_table->ComputeLocalPointValues(
        (OsdValue<typename T::AccumType> *)subd_data,
        (OsdValue<typename T::AccumType> *)(subd_data + num_refiner_verts));
  }

  /* Evaluate patches at limit. */
  typename T::Type *mesh_data = reinterpret_cast<typename T::Type *>(mesh_attr.data()) +
                                mesh.get_verts().size() * motion_step;

  assert(mesh_data != nullptr);

  /* Compute motion normals alongside positions. */
  float3 *mesh_normal_data = nullptr;
  if constexpr (std::is_same_v<typename T::Type, float3>) {
    if (mesh_attr.std == ATTR_STD_MOTION_VERTEX_POSITION) {
      Attribute *attr_normal = mesh.attributes.add(ATTR_STD_MOTION_VERTEX_NORMAL);
      mesh_normal_data = attr_normal->data_float3() + mesh.get_verts().size() * motion_step;
    }
  }

  attr.interp = [this, subd_data, mesh_data, mesh_normal_data](const int patch_index,
                                                               const int /*face_index*/,
                                                               const int /*corner*/,
                                                               const int *vert_index,
                                                               const float2 *vert_uv,
                                                               const int vert_num) {
    for (int i = 0; i < vert_num; i++) {
      /* Compute patch weights. */
      const float2 uv = vert_uv[i];

      const Far::PatchTable::PatchHandle &handle = *osd_data.patch_map->FindPatch(
          patch_index, (double)uv.x, (double)uv.y);

      float p_weights[20], du_weights[20], dv_weights[20];
      osd_data.patch_table->EvaluateBasis(handle, uv.x, uv.y, p_weights, du_weights, dv_weights);
      Far::ConstIndexArray cv = osd_data.patch_table->GetPatchVertices(handle);

      /* Compution position. */
      typename T::AccumType value = subd_data[cv[0]] * p_weights[0];
      for (int k = 1; k < cv.size(); k++) {
        value += subd_data[cv[k]] * p_weights[k];
      }
      mesh_data[vert_index[i]] = T::output(value);

      /* Optionally compute normal. */
      if (mesh_normal_data) {
        if constexpr (std::is_same_v<typename T::Type, float3>) {
          float3 du = zero_float3();
          float3 dv = zero_float3();
          for (int k = 0; k < cv.size(); k++) {
            const float3 p = subd_data[cv[k]];
            du += p * du_weights[k];
            dv += p * dv_weights[k];
          }
          mesh_normal_data[vert_index[i]] = safe_normalize_fallback(cross(du, dv),
                                                                    make_float3(0.0f, 0.0f, 1.0f));
        }
      }
    }
  };

  vertex_attributes.push_back(std::move(attr));
}

#endif

template<typename T>
void SubdAttributeInterpolation::setup_attribute_corner_linear(const Attribute &subd_attr,
                                                               Attribute &mesh_attr)
{
  SubdAttribute attr;

  /* Interpolate values at corners. */
  const typename T::Type *subd_data = reinterpret_cast<const typename T::Type *>(subd_attr.data());
  typename T::Type *mesh_data = reinterpret_cast<typename T::Type *>(mesh_attr.data());

  assert(mesh_data != nullptr);

  attr.interp = [this, subd_data, mesh_data](const int /*patch_index*/,
                                             const int face_index,
                                             const int corner,
                                             const int *triangle_index,
                                             const float2 *triangle_uv,
                                             const int triangle_num) {
    Mesh::SubdFace face = mesh.get_subd_face(face_index);

    if (face.is_quad()) {
      /* Simple case for quads. */
      const typename T::AccumType value0 = T::read(subd_data[face.start_corner + 0]);
      const typename T::AccumType value1 = T::read(subd_data[face.start_corner + 1]);
      const typename T::AccumType value2 = T::read(subd_data[face.start_corner + 2]);
      const typename T::AccumType value3 = T::read(subd_data[face.start_corner + 3]);

      for (size_t i = 0; i < triangle_num; i++) {
        for (int j = 0; j < 3; j++) {
          const float2 uv = triangle_uv[(i * 3) + j];
          const typename T::AccumType value = interp(
              interp(value0, value1, uv.x), interp(value3, value2, uv.x), uv.y);
          mesh_data[triangle_index[i] * 3 + j] = T::output(value);
        }
      }
    }
    else {
      /* Other n-gons are split into n quads. */

      /* Compute value at center of polygon. */
      typename T::AccumType value_center = T::read(subd_data[face.start_corner]);
      for (int j = 1; j < face.num_corners; j++) {
        value_center += T::read(subd_data[face.start_corner + j]);
      }
      value_center /= (float)face.num_corners;

      /* Compute value at corner at adjacent vertices. */
      const typename T::AccumType value_corner = T::read(subd_data[face.start_corner + corner]);
      const typename T::AccumType value_prev =
          0.5f * (value_corner +
                  T::read(subd_data[face.start_corner + mod(corner - 1, face.num_corners)]));
      const typename T::AccumType value_next =
          0.5f * (value_corner +
                  T::read(subd_data[face.start_corner + mod(corner + 1, face.num_corners)]));

      for (size_t i = 0; i < triangle_num; i++) {
        for (int j = 0; j < 3; j++) {
          const float2 uv = triangle_uv[(i * 3) + j];

          /* Interpolate. */
          const typename T::AccumType value = interp(interp(value_corner, value_next, uv.x),
                                                     interp(value_prev, value_center, uv.x),
                                                     uv.y);

          mesh_data[triangle_index[i] * 3 + j] = T::output(value);
        }
      }
    }
  };

  triangle_attributes.push_back(std::move(attr));
}

#ifdef WITH_OPENSUBDIV
template<typename T>
void SubdAttributeInterpolation::setup_attribute_corner_smooth(Attribute &mesh_attr,
                                                               const int channel,
                                                               const vector<char> &merged_values)
{
  SubdAttribute attr;

  // TODO: Avoid computing derivative weights when not needed
  const int num_refiner_fvars = osd_data.refiner->GetNumFVarValuesTotal(channel);
  const int num_local_points = osd_data.patch_table->GetNumLocalPointsFaceVarying(channel);
  const int num_base_fvars = osd_data.refiner->GetLevel(0).GetNumFVarValues(channel);

  /* Refine attribute data to get patch coordinates. */
  attr.refined_data.resize((num_refiner_fvars + num_local_points) * sizeof(typename T::AccumType));
  typename T::AccumType *refined_data = reinterpret_cast<typename T::AccumType *>(
      attr.refined_data.data());

  const typename T::Type *base_src = reinterpret_cast<const typename T::Type *>(
      merged_values.data());
  typename T::AccumType *base_dst = refined_data;
  for (int i = 0; i < num_base_fvars; i++) {
    base_dst[i] = T::read(base_src[i]);
  }

  Far::PrimvarRefiner primvar_refiner(*osd_data.refiner);
  typename T::AccumType *src = refined_data;
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
        (OsdValue<typename T::AccumType> *)refined_data,
        (OsdValue<typename T::AccumType> *)(refined_data + num_refiner_fvars),
        channel);
  }

  /* Evaluate patches at limit. */
  const typename T::AccumType *subd_data = refined_data;
  typename T::Type *mesh_data = reinterpret_cast<typename T::Type *>(mesh_attr.data());

  assert(mesh_data != nullptr);

  attr.interp = [this, subd_data, mesh_data, channel](const int patch_index,
                                                      const int /*face_index*/,
                                                      const int /*corner*/,
                                                      const int *triangle_index,
                                                      const float2 *triangle_uv,
                                                      const int triangle_num) {
    for (size_t i = 0; i < triangle_num; i++) {
      for (int j = 0; j < 3; j++) {
        /* Compute patch weights. */
        const float2 uv = triangle_uv[(i * 3) + j];
        const Far::PatchTable::PatchHandle &handle = *osd_data.patch_map->FindPatch(
            patch_index, (double)uv.x, (double)uv.y);

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
        mesh_data[triangle_index[i] * 3 + j] = T::output(value);
      }
    }
  };

  triangle_attributes.push_back(std::move(attr));
}
#endif

template<typename T>
void SubdAttributeInterpolation::setup_attribute_face(const Attribute &subd_attr,
                                                      Attribute &mesh_attr)
{
  /* Copy value from face to triangle .*/
  SubdAttribute attr;
  const typename T::Type *subd_data = reinterpret_cast<const typename T::Type *>(subd_attr.data());
  typename T::Type *mesh_data = reinterpret_cast<typename T::Type *>(mesh_attr.data());

  assert(mesh_data != nullptr);

  attr.interp = [subd_data, mesh_data](const int /*patch_index*/,
                                       const int face_index,
                                       const int /*corner*/,
                                       const int *triangle_index,
                                       const float2 * /*triangle_uv*/,
                                       const int triangle_num) {
    for (int i = 0; i < triangle_num; i++) {
      mesh_data[triangle_index[i]] = subd_data[face_index];
    }
  };

  triangle_attributes.push_back(std::move(attr));
}

template<typename T>
void SubdAttributeInterpolation::setup_attribute_type(const Attribute &subd_attr,
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
            setup_attribute_vertex_smooth<T>(subd_attr, mesh_attr);
            break;
          default:
            setup_attribute_vertex_linear<T>(subd_attr, mesh_attr);
            break;
        }
      }
      else
#endif
      {
        setup_attribute_vertex_linear<T>(subd_attr, mesh_attr);
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
              setup_attribute_corner_smooth<T>(mesh_attr, merged_fvar.channel, merged_fvar.values);
              return;
            }
          }
        }
      }
#endif
      setup_attribute_corner_linear<T>(subd_attr, mesh_attr);
      break;
    }
    case ATTR_ELEMENT_VERTEX_MOTION: {
      /* Interpolate each motion step individually. */
      for (int step = 0; step < mesh.get_motion_steps() - 1; step++) {
#ifdef WITH_OPENSUBDIV
        if (mesh.get_subdivision_type() == Mesh::SUBDIVISION_CATMULL_CLARK) {
          setup_attribute_vertex_smooth<T>(subd_attr, mesh_attr, step);
        }
        else
#endif
        {
          setup_attribute_vertex_linear<T>(subd_attr, mesh_attr, step);
        }
      }
      break;
    }
    case ATTR_ELEMENT_FACE: {
      setup_attribute_face<T>(subd_attr, mesh_attr);
      break;
    }
    default:
      break;
  }
}

CCL_NAMESPACE_END
