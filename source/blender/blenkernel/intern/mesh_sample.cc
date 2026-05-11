/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_sample.hh"

#include "BLI_array_utils.hh"
#include "BLI_math_geom.h"
#include "BLI_rand.hh"

namespace blender::bke::mesh_surface_sample {

template<typename T>
BLI_NOINLINE static void sample_point_attribute(const Span<int> corner_verts,
                                                const Span<int3> corner_tris,
                                                const Span<int> tri_indices,
                                                const Span<float3> bary_coords,
                                                const VArray<T> &src,
                                                const IndexMask &mask,
                                                const MutableSpan<T> dst)
{
  mask.foreach_index(
      [&](const int i) {
        const int3 &tri = corner_tris[tri_indices[i]];
        dst[i] = attribute_math::mix3(bary_coords[i],
                                      src[corner_verts[tri[0]]],
                                      src[corner_verts[tri[1]]],
                                      src[corner_verts[tri[2]]]);
      },
      exec_mode::grain_size(4096));
}

void sample_point_normals(const Span<int> corner_verts,
                          const Span<int3> corner_tris,
                          const Span<int> tri_indices,
                          const Span<float3> bary_coords,
                          const Span<float3> src,
                          const IndexMask mask,
                          const MutableSpan<float3> dst)
{
  mask.foreach_index(
      [&](const int i) {
        const int3 &tri = corner_tris[tri_indices[i]];
        const float3 value = attribute_math::mix3(bary_coords[i],
                                                  src[corner_verts[tri[0]]],
                                                  src[corner_verts[tri[1]]],
                                                  src[corner_verts[tri[2]]]);
        dst[i] = math::normalize(value);
      },
      exec_mode::grain_size(4096));
}

void sample_point_attribute(const Span<int> corner_verts,
                            const Span<int3> corner_tris,
                            const Span<int> tri_indices,
                            const Span<float3> bary_coords,
                            const GVArray &src,
                            const IndexMask &mask,
                            const GMutableSpan dst)
{
  BLI_assert(src.type() == dst.type());

  const CPPType &type = src.type();
  attribute_math::to_static_type(type, [&]<typename T>() {
    if constexpr (!std::is_same_v<T, std::string>) {
      sample_point_attribute<T>(corner_verts,
                                corner_tris,
                                tri_indices,
                                bary_coords,
                                src.typed<T>(),
                                mask,
                                dst.typed<T>());
    }
  });
}

template<typename T>
BLI_NOINLINE static void sample_corner_attribute(const Span<int3> corner_tris,
                                                 const Span<int> tri_indices,
                                                 const Span<float3> bary_coords,
                                                 const VArray<T> &src,
                                                 const IndexMask &mask,
                                                 const MutableSpan<T> dst)
{
  mask.foreach_index(
      [&](const int i) {
        const int3 &tri = corner_tris[tri_indices[i]];
        dst[i] = sample_corner_attribute_with_bary_coords(bary_coords[i], tri, src);
      },
      exec_mode::grain_size(4096));
}

void sample_corner_normals(const Span<int3> corner_tris,
                           const Span<int> tri_indices,
                           const Span<float3> bary_coords,
                           const Span<float3> src,
                           const IndexMask &mask,
                           const MutableSpan<float3> dst)
{
  mask.foreach_index(
      [&](const int i) {
        const int3 &tri = corner_tris[tri_indices[i]];
        const float3 value = sample_corner_attribute_with_bary_coords(bary_coords[i], tri, src);
        dst[i] = math::normalize(value);
      },
      exec_mode::grain_size(4096));
}

void sample_corner_attribute(const Span<int3> corner_tris,
                             const Span<int> tri_indices,
                             const Span<float3> bary_coords,
                             const GVArray &src,
                             const IndexMask &mask,
                             const GMutableSpan dst)
{
  BLI_assert(src.type() == dst.type());

  const CPPType &type = src.type();
  attribute_math::to_static_type(type, [&]<typename T>() {
    sample_corner_attribute<T>(
        corner_tris, tri_indices, bary_coords, src.typed<T>(), mask, dst.typed<T>());
  });
}

template<typename T>
void sample_face_attribute(const Span<int> tri_faces,
                           const Span<int> tri_indices,
                           const VArray<T> &src,
                           const IndexMask &mask,
                           const MutableSpan<T> dst)
{
  mask.foreach_index(
      [&](const int i) {
        const int tri_index = tri_indices[i];
        const int face_index = tri_faces[tri_index];
        dst[i] = src[face_index];
      },
      exec_mode::grain_size(4096));
}

void sample_face_attribute(const Span<int> corner_tri_faces,
                           const Span<int> tri_indices,
                           const GVArray &src,
                           const IndexMask &mask,
                           const GMutableSpan dst)
{
  BLI_assert(src.type() == dst.type());

  const CPPType &type = src.type();
  attribute_math::to_static_type(type, [&]<typename T>() {
    sample_face_attribute<T>(corner_tri_faces, tri_indices, src.typed<T>(), mask, dst.typed<T>());
  });
}

template<bool check_indices = false>
static void sample_barycentric_weights(const Span<float3> vert_positions,
                                       const Span<int> corner_verts,
                                       const Span<int3> corner_tris,
                                       const Span<int> tri_indices,
                                       const Span<float3> sample_positions,
                                       const IndexMask &mask,
                                       MutableSpan<float3> bary_coords)
{
  mask.foreach_index([&](const int i) {
    if constexpr (check_indices) {
      if (tri_indices[i] == -1) {
        bary_coords[i] = {};
        return;
      }
    }
    const int3 &tri = corner_tris[tri_indices[i]];
    bary_coords[i] = compute_bary_coord_in_triangle(
        vert_positions, corner_verts, tri, sample_positions[i]);
  });
}

void sample_barycentric_weights(const Span<float3> vert_positions,
                                const Span<int> corner_verts,
                                const Span<int3> corner_tris,
                                const Span<int> tri_indices,
                                const Span<float3> sample_positions,
                                const IndexMask &mask,
                                MutableSpan<float3> bary_coords)
{
  sample_barycentric_weights<false>(
      vert_positions, corner_verts, corner_tris, tri_indices, sample_positions, mask, bary_coords);
}

template<bool check_indices = false>
static void sample_nearest_corner(const Span<float3> vert_positions,
                                  const Span<int> corner_verts,
                                  const Span<int3> corner_tris,
                                  const Span<int> tri_indices,
                                  const Span<float3> sample_positions,
                                  const IndexMask &mask,
                                  MutableSpan<int> nearest_corner)
{
  mask.foreach_index([&](const int i) {
    if constexpr (check_indices) {
      if (tri_indices[i] == -1) {
        nearest_corner[i] = -1;
        return;
      }
    }
    const int3 &tri = corner_tris[tri_indices[i]];
    const std::array<float, 3> distances{
        math::distance_squared(sample_positions[i], vert_positions[corner_verts[tri[0]]]),
        math::distance_squared(sample_positions[i], vert_positions[corner_verts[tri[1]]]),
        math::distance_squared(sample_positions[i], vert_positions[corner_verts[tri[2]]]),
    };
    const int index = std::min_element(distances.begin(), distances.end()) - distances.begin();
    nearest_corner[i] = tri[index];
  });
}

int sample_surface_points_spherical(RandomNumberGenerator &rng,
                                    const Mesh &mesh,
                                    const Span<int> tris_to_sample,
                                    const float3 &sample_pos,
                                    const float sample_radius,
                                    const float approximate_density,
                                    Vector<float3> &r_bary_coords,
                                    Vector<int> &r_tri_indices,
                                    Vector<float3> &r_positions)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int3> corner_tris = mesh.corner_tris();

  const float sample_radius_sq = pow2f(sample_radius);
  const float sample_plane_area = M_PI * sample_radius_sq;
  /* Used for switching between two triangle sampling strategies. */
  const float area_threshold = sample_plane_area;

  const int old_num = r_bary_coords.size();

  for (const int tri_index : tris_to_sample) {
    const int3 &tri = corner_tris[tri_index];

    const float3 &v0 = positions[corner_verts[tri[0]]];
    const float3 &v1 = positions[corner_verts[tri[1]]];
    const float3 &v2 = positions[corner_verts[tri[2]]];

    const float corner_tri_area = area_tri_v3(v0, v1, v2);

    if (corner_tri_area < area_threshold) {
      /* The triangle is small compared to the sample radius. Sample by generating random
       * barycentric coordinates. */
      const int amount = rng.round_probabilistic(approximate_density * corner_tri_area);
      for ([[maybe_unused]] const int i : IndexRange(amount)) {
        const float3 bary_coord = rng.get_barycentric_coordinates();
        const float3 point_pos = attribute_math::mix3(bary_coord, v0, v1, v2);
        const float dist_to_sample_sq = math::distance_squared(point_pos, sample_pos);
        if (dist_to_sample_sq > sample_radius_sq) {
          continue;
        }

        r_bary_coords.append(bary_coord);
        r_tri_indices.append(tri_index);
        r_positions.append(point_pos);
      }
    }
    else {
      /* The triangle is large compared to the sample radius. Sample by generating random points
       * on the triangle plane within the sample radius. */
      float3 normal;
      normal_tri_v3(normal, v0, v1, v2);

      float3 sample_pos_proj = sample_pos;
      project_v3_plane(sample_pos_proj, normal, v0);

      const float proj_distance_sq = math::distance_squared(sample_pos_proj, sample_pos);
      const float sample_radius_factor_sq = 1.0f -
                                            std::min(1.0f, proj_distance_sq / sample_radius_sq);
      const float radius_proj_sq = sample_radius_sq * sample_radius_factor_sq;
      const float radius_proj = std::sqrt(radius_proj_sq);
      const float circle_area = M_PI * radius_proj_sq;

      const int amount = rng.round_probabilistic(approximate_density * circle_area);

      const float3 axis_1 = math::normalize(v1 - v0) * radius_proj;
      const float3 axis_2 = math::normalize(math::cross(axis_1, math::cross(axis_1, v2 - v0))) *
                            radius_proj;

      for ([[maybe_unused]] const int i : IndexRange(amount)) {
        const float r = std::sqrt(rng.get_float());
        const float angle = rng.get_float() * 2.0f * M_PI;
        const float x = r * std::cos(angle);
        const float y = r * std::sin(angle);
        const float3 point_pos = sample_pos_proj + axis_1 * x + axis_2 * y;
        if (!isect_point_tri_prism_v3(point_pos, v0, v1, v2)) {
          /* Sampled point is not in the triangle. */
          continue;
        }

        float3 bary_coord;
        interp_weights_tri_v3(bary_coord, v0, v1, v2, point_pos);

        r_bary_coords.append(bary_coord);
        r_tri_indices.append(tri_index);
        r_positions.append(point_pos);
      }
    }
  }
  return r_bary_coords.size() - old_num;
}

int sample_surface_points_projected(
    RandomNumberGenerator &rng,
    const Mesh &mesh,
    BVHTreeFromMesh &mesh_bvhtree,
    const float2 &sample_pos_re,
    const float sample_radius_re,
    const FunctionRef<void(const float2 &pos_re, float3 &r_start, float3 &r_end)>
        region_position_to_ray,
    const bool front_face_only,
    const int tries_num,
    const int max_points,
    Vector<float3> &r_bary_coords,
    Vector<int> &r_tri_indices,
    Vector<float3> &r_positions)
{
  const Span<float3> positions = mesh.vert_positions();
  const Span<int> corner_verts = mesh.corner_verts();
  const Span<int3> corner_tris = mesh.corner_tris();

  int point_count = 0;
  for ([[maybe_unused]] const int _ : IndexRange(tries_num)) {
    if (point_count == max_points) {
      break;
    }

    const float r = sample_radius_re * std::sqrt(rng.get_float());
    const float angle = rng.get_float() * 2.0f * M_PI;
    float3 ray_start, ray_end;
    const float2 pos_re = sample_pos_re + r * float2(std::cos(angle), std::sin(angle));
    region_position_to_ray(pos_re, ray_start, ray_end);
    const float3 ray_direction = math::normalize(ray_end - ray_start);

    BVHTreeRayHit ray_hit;
    ray_hit.dist = FLT_MAX;
    ray_hit.index = -1;
    BLI_bvhtree_ray_cast(mesh_bvhtree.tree,
                         ray_start,
                         ray_direction,
                         0.0f,
                         &ray_hit,
                         mesh_bvhtree.raycast_callback,
                         &mesh_bvhtree);

    if (ray_hit.index == -1) {
      continue;
    }

    if (front_face_only) {
      const float3 normal = ray_hit.no;
      if (math::dot(ray_direction, normal) >= 0.0f) {
        continue;
      }
    }

    const int tri_index = ray_hit.index;
    const float3 pos = ray_hit.co;

    const float3 bary_coords = compute_bary_coord_in_triangle(
        positions, corner_verts, corner_tris[tri_index], pos);

    r_positions.append(pos);
    r_bary_coords.append(bary_coords);
    r_tri_indices.append(tri_index);
    point_count++;
  }
  return point_count;
}

float3 compute_bary_coord_in_triangle(const Span<float3> vert_positions,
                                      const Span<int> corner_verts,
                                      const int3 &tri,
                                      const float3 &position)
{
  const float3 &v0 = vert_positions[corner_verts[tri[0]]];
  const float3 &v1 = vert_positions[corner_verts[tri[1]]];
  const float3 &v2 = vert_positions[corner_verts[tri[2]]];
  float3 bary_coords;
  interp_weights_tri_v3(bary_coords, v0, v1, v2, position);
  return bary_coords;
}

BaryWeightFromPositionFn::BaryWeightFromPositionFn(GeometrySet geometry)
    : source_(std::move(geometry))
{
  source_.ensure_owns_direct_data();
  static const mf::Signature signature = []() {
    mf::Signature signature;
    mf::SignatureBuilder builder{"Bary Weight from Position", signature};
    builder.single_input<float3>("Position");
    builder.single_input<int>("Triangle Index");
    builder.single_output<float3>("Barycentric Weight");
    return signature;
  }();
  this->set_signature(&signature);
  const Mesh &mesh = *source_.get_mesh();
  vert_positions_ = mesh.vert_positions();
  corner_verts_ = mesh.corner_verts();
  corner_tris_ = mesh.corner_tris();
}

void BaryWeightFromPositionFn::call(const IndexMask &mask,
                                    mf::Params params,
                                    mf::Context /*context*/) const
{
  const VArraySpan<float3> sample_positions = params.readonly_single_input<float3>(0, "Position");
  const VArraySpan<int> triangle_indices = params.readonly_single_input<int>(1, "Triangle Index");
  MutableSpan<float3> bary_weights = params.uninitialized_single_output<float3>(
      2, "Barycentric Weight");
  sample_barycentric_weights<true>(vert_positions_,
                                   corner_verts_,
                                   corner_tris_,
                                   triangle_indices,
                                   sample_positions,
                                   mask,
                                   bary_weights);
}

void BaryWeightFromPositionFn::hash_unique(UniqueHashBytes &hash) const
{
  static constexpr int8_t id = 0;
  hash.add(&id);
  hash.add(source_.get_mesh());
}

NearestCornerFromPositionFn::NearestCornerFromPositionFn(GeometrySet geometry)
    : source_(std::move(geometry))
{
  source_.ensure_owns_direct_data();
  static const mf::Signature signature = []() {
    mf::Signature signature;
    mf::SignatureBuilder builder{"Nearest Weight from Position", signature};
    builder.single_input<float3>("Position");
    builder.single_input<int>("Triangle Index");
    builder.single_output<int>("Nearest Corner");
    return signature;
  }();
  this->set_signature(&signature);
  const Mesh &mesh = *source_.get_mesh();
  vert_positions_ = mesh.vert_positions();
  corner_verts_ = mesh.corner_verts();
  corner_tris_ = mesh.corner_tris();
}

void NearestCornerFromPositionFn::call(const IndexMask &mask,
                                       mf::Params params,
                                       mf::Context /*context*/) const
{
  const VArraySpan<float3> sample_positions = params.readonly_single_input<float3>(0, "Position");
  const VArraySpan<int> triangle_indices = params.readonly_single_input<int>(1, "Triangle Index");
  MutableSpan<int> nearest_corner = params.uninitialized_single_output<int>(2, "Nearest Corner");
  sample_nearest_corner<true>(vert_positions_,
                              corner_verts_,
                              corner_tris_,
                              triangle_indices,
                              sample_positions,
                              mask,
                              nearest_corner);
}

void NearestCornerFromPositionFn::hash_unique(UniqueHashBytes &hash) const
{
  static constexpr int8_t id = 0;
  hash.add(&id);
  hash.add(source_.get_mesh());
}

BaryWeightSampleFn::BaryWeightSampleFn(GeometrySet geometry, fn::GField src_field)
    : source_(std::move(geometry)), src_field_(std::move(src_field))
{
  source_.ensure_owns_direct_data();
  mf::SignatureBuilder builder{"Sample Barycentric Triangles", signature_};
  builder.single_input<int>("Triangle Index");
  builder.single_input<float3>("Barycentric Weight");
  builder.single_output("Value", src_field_.cpp_type());
  this->set_signature(&signature_);
}

void BaryWeightSampleFn::call(const IndexMask &mask,
                              mf::Params params,
                              mf::Context /*context*/) const
{
  const VArraySpan<int> triangle_indices = params.readonly_single_input<int>(0, "Triangle Index");
  const VArraySpan<float3> bary_weights = params.readonly_single_input<float3>(
      1, "Barycentric Weight");
  GMutableSpan dst = params.uninitialized_single_output(2, "Value");
  IndexMaskMemory memory;
  const IndexMask valid_mask = array_utils::indices_non_negative(mask, triangle_indices, memory);
  switch (src_domain_) {
    case AttrDomain::Point:
      attribute_math::to_static_type(dst.type(), [&]<typename T>() {
        if constexpr (!std::is_same_v<T, std::string>) {
          sample_point_attribute<T>(corner_verts_,
                                    corner_tris_,
                                    triangle_indices,
                                    bary_weights,
                                    source_data_->typed<T>(),
                                    valid_mask,
                                    dst.typed<T>());
        }
      });
      break;
    case AttrDomain::Face:
      attribute_math::to_static_type(dst.type(), [&]<typename T>() {
        sample_face_attribute<T>(
            tri_faces_, triangle_indices, source_data_->typed<T>(), valid_mask, dst.typed<T>());
      });
      break;
    case AttrDomain::Corner:
      attribute_math::to_static_type(dst.type(), [&]<typename T>() {
        if constexpr (!std::is_same_v<T, std::string>) {
          sample_corner_attribute<T>(corner_tris_,
                                     triangle_indices,
                                     bary_weights,
                                     source_data_->typed<T>(),
                                     valid_mask,
                                     dst.typed<T>());
        }
      });
      break;
    default:
      BLI_assert_unreachable();
      break;
  }
  dst.type().value_initialize_indices(dst.data(), valid_mask.complement(mask, memory));
}

void BaryWeightSampleFn::hash_unique(UniqueHashBytes &hash) const
{
  static constexpr int8_t id = 0;
  hash.add(&id);
  hash.add(source_.get_mesh());
  fn::FieldHashDeep field_hash;
  hash.add(field_hash.ensure(src_field_));
}

void BaryWeightSampleFn::prepare_for_execution() const
{
  mutex_.ensure([&]() {
    const auto &component = *source_.get_component<bke::MeshComponent>();
    const Mesh &mesh = *component.get();
    corner_verts_ = mesh.corner_verts();
    corner_tris_ = mesh.corner_tris();
    src_domain_ =
        bke::try_detect_native_field_domain(component, src_field_).value_or(AttrDomain::Corner);
    if (src_domain_ == AttrDomain::Edge) {
      /* To maintain legacy behavior and avoid making decisions here about barycentric mixing of
       * edge attributes, just use the domain interpolation to read the attribute on the face
       * corner domain. */
      src_domain_ = AttrDomain::Corner;
    }
    else if (src_domain_ == AttrDomain::Face) {
      tri_faces_ = mesh.corner_tri_faces();
    }
    source_context_.emplace(MeshFieldContext(mesh, src_domain_));
    const int domain_size = mesh.attributes().domain_size(src_domain_);
    source_evaluator_ = std::make_unique<fn::FieldEvaluator>(*source_context_, domain_size);
    source_evaluator_->add(src_field_);
    source_evaluator_->evaluate();
    source_data_ = &source_evaluator_->get_evaluated(0);
  });
}

}  // namespace blender::bke::mesh_surface_sample
