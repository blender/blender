/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_function_ref.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_math_vector_types.hh"

#include "FN_field.hh"
#include "FN_multi_function.hh"

#include "DNA_meshdata_types.h"

#include "BKE_attribute.h"
#include "BKE_geometry_fields.hh"

struct Mesh;
struct BVHTreeFromMesh;

namespace blender {
class RandomNumberGenerator;
}

namespace blender::bke::mesh_surface_sample {

void sample_point_attribute(Span<int> corner_verts,
                            Span<MLoopTri> looptris,
                            Span<int> looptri_indices,
                            Span<float3> bary_coords,
                            const GVArray &src,
                            const IndexMask &mask,
                            GMutableSpan dst);

void sample_point_normals(Span<int> corner_verts,
                          Span<MLoopTri> looptris,
                          Span<int> looptri_indices,
                          Span<float3> bary_coords,
                          Span<float3> src,
                          IndexMask mask,
                          MutableSpan<float3> dst);

void sample_corner_attribute(Span<MLoopTri> looptris,
                             Span<int> looptri_indices,
                             Span<float3> bary_coords,
                             const GVArray &src,
                             const IndexMask &mask,
                             GMutableSpan dst);

void sample_corner_normals(Span<MLoopTri> looptris,
                           Span<int> looptri_indices,
                           Span<float3> bary_coords,
                           Span<float3> src,
                           const IndexMask &mask,
                           MutableSpan<float3> dst);

void sample_face_attribute(Span<int> looptri_polys,
                           Span<int> looptri_indices,
                           const GVArray &src,
                           const IndexMask &mask,
                           GMutableSpan dst);

/**
 * Find randomly distributed points on the surface of a mesh within a 3D sphere. This does not
 * sample an exact number of points because it comes with extra overhead to avoid bias that is only
 * required in some cases. If an exact number of points is required, that has to be implemented at
 * a higher level.
 *
 * \param approximate_density: Roughly the number of points per unit of area.
 * \return The number of added points.
 */
int sample_surface_points_spherical(RandomNumberGenerator &rng,
                                    const Mesh &mesh,
                                    Span<int> looptri_indices_to_sample,
                                    const float3 &sample_pos,
                                    float sample_radius,
                                    float approximate_density,
                                    Vector<float3> &r_bary_coords,
                                    Vector<int> &r_looptri_indices,
                                    Vector<float3> &r_positions);

/**
 * Find randomly distributed points on the surface of a mesh within a circle that is projected on
 * the mesh. This does not result in an exact number of points because that would come with extra
 * overhead and is not always possible. If an exact number of points is required, that has to be
 * implemented at a higher level.
 *
 * \param region_position_to_ray: Function that converts a 2D position into a 3D ray that is used
 *   to find positions on the mesh.
 * \param mesh_bvhtree: BVH tree of the triangles in the mesh. Passed in so that it does not have
 *   to be retrieved again.
 * \param tries_num: Number of 2d positions that are sampled. The maximum
 *   number of new samples.
 * \return The number of added points.
 */
int sample_surface_points_projected(
    RandomNumberGenerator &rng,
    const Mesh &mesh,
    BVHTreeFromMesh &mesh_bvhtree,
    const float2 &sample_pos_re,
    float sample_radius_re,
    FunctionRef<void(const float2 &pos_re, float3 &r_start, float3 &r_end)> region_position_to_ray,
    bool front_face_only,
    int tries_num,
    int max_points,
    Vector<float3> &r_bary_coords,
    Vector<int> &r_looptri_indices,
    Vector<float3> &r_positions);

float3 compute_bary_coord_in_triangle(Span<float3> vert_positions,
                                      Span<int> corner_verts,
                                      const MLoopTri &looptri,
                                      const float3 &position);

template<typename T>
inline T sample_corner_attribute_with_bary_coords(const float3 &bary_weights,
                                                  const MLoopTri &looptri,
                                                  const Span<T> corner_attribute)
{
  return attribute_math::mix3(bary_weights,
                              corner_attribute[looptri.tri[0]],
                              corner_attribute[looptri.tri[1]],
                              corner_attribute[looptri.tri[2]]);
}

template<typename T>
inline T sample_corner_attribute_with_bary_coords(const float3 &bary_weights,
                                                  const MLoopTri &looptri,
                                                  const VArray<T> &corner_attribute)
{
  return attribute_math::mix3(bary_weights,
                              corner_attribute[looptri.tri[0]],
                              corner_attribute[looptri.tri[1]],
                              corner_attribute[looptri.tri[2]]);
}

/**
 * Calculate barycentric weights from triangle indices and positions within the triangles.
 */
class BaryWeightFromPositionFn : public mf::MultiFunction {
  GeometrySet source_;
  Span<float3> vert_positions_;
  Span<int> corner_verts_;
  Span<MLoopTri> looptris_;

 public:
  BaryWeightFromPositionFn(GeometrySet geometry);
  void call(const IndexMask &mask, mf::Params params, mf::Context context) const;
};

/**
 * Calculate face corner weights from triangle indices and positions within the triangles.
 * The weights are 1 for the nearest corner and 0 for the two others.
 */
class CornerBaryWeightFromPositionFn : public mf::MultiFunction {
  GeometrySet source_;
  Span<float3> vert_positions_;
  Span<int> corner_verts_;
  Span<MLoopTri> looptris_;

 public:
  CornerBaryWeightFromPositionFn(GeometrySet geometry);
  void call(const IndexMask &mask, mf::Params params, mf::Context context) const;
};

/**
 * Evaluate an attribute on the input geometry and sample it with input barycentric weights and
 * triangle indices.
 */
class BaryWeightSampleFn : public mf::MultiFunction {
  mf::Signature signature_;

  GeometrySet source_;
  Span<MLoopTri> looptris_;
  std::optional<bke::MeshFieldContext> source_context_;
  std::unique_ptr<fn::FieldEvaluator> source_evaluator_;
  const GVArray *source_data_;
  eAttrDomain domain_;

 public:
  BaryWeightSampleFn(GeometrySet geometry, fn::GField src_field);

  void call(const IndexMask &mask, mf::Params params, mf::Context context) const;

 private:
  void evaluate_source(fn::GField src_field);
};

}  // namespace blender::bke::mesh_surface_sample
