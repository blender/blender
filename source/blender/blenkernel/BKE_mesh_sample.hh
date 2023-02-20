/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_function_ref.hh"
#include "BLI_generic_virtual_array.hh"
#include "BLI_math_vector_types.hh"

#include "DNA_meshdata_types.h"

#include "BKE_attribute.h"

struct Mesh;
struct BVHTreeFromMesh;

namespace blender {
class RandomNumberGenerator;
}

namespace blender::bke::mesh_surface_sample {

void sample_point_attribute(const Mesh &mesh,
                            Span<int> looptri_indices,
                            Span<float3> bary_coords,
                            const GVArray &src,
                            IndexMask mask,
                            GMutableSpan dst);

void sample_corner_attribute(const Mesh &mesh,
                             Span<int> looptri_indices,
                             Span<float3> bary_coords,
                             const GVArray &src,
                             IndexMask mask,
                             GMutableSpan dst);

void sample_face_attribute(const Mesh &mesh,
                           Span<int> looptri_indices,
                           const GVArray &src,
                           IndexMask mask,
                           GMutableSpan dst);

enum class eAttributeMapMode {
  INTERPOLATED,
  NEAREST,
};

/**
 * A utility class that performs attribute interpolation from a source mesh.
 *
 * The interpolator is only valid as long as the mesh is valid.
 * Barycentric weights are needed when interpolating point or corner domain attributes,
 * these are computed lazily when needed and re-used.
 */
class MeshAttributeInterpolator {
  const Mesh *mesh_;
  const IndexMask mask_;
  const Span<float3> positions_;
  const Span<int> looptri_indices_;

  Array<float3> bary_coords_;
  Array<float3> nearest_weights_;

 public:
  MeshAttributeInterpolator(const Mesh *mesh,
                            IndexMask mask,
                            Span<float3> positions,
                            Span<int> looptri_indices);

  void sample_data(const GVArray &src,
                   eAttrDomain domain,
                   eAttributeMapMode mode,
                   GMutableSpan dst);

 protected:
  Span<float3> ensure_barycentric_coords();
  Span<float3> ensure_nearest_weights();
};

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
                                      Span<MLoop> loops,
                                      const MLoopTri &looptri,
                                      const float3 &position);

template<typename T>
inline T sample_corner_attrribute_with_bary_coords(const float3 &bary_weights,
                                                   const MLoopTri &looptri,
                                                   const Span<T> corner_attribute)
{
  return attribute_math::mix3(bary_weights,
                              corner_attribute[looptri.tri[0]],
                              corner_attribute[looptri.tri[1]],
                              corner_attribute[looptri.tri[2]]);
}

}  // namespace blender::bke::mesh_surface_sample
