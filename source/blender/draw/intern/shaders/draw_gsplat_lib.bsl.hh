/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 *
 * Draw interfaces, packing/unpacking and data loading code for drawing gsplats. For most
 * compute work, refer to `draw_gsplat.bsl.hh`.
 *
 * References:
 *
 *  [evas2002]  Matthias Zwicker et al.
 *              EWA Splatting
 *              IEEE TVCG 2002
 *              https://www.cs.umd.edu/~zwicker/publications/EWASplatting-TVCG02.pdf
 *
 *  [octa2017]  Rune Stubbe
 *              Octahedral Encoding
 *              https://www.shadertoy.com/view/Mtfyzl (link is not author's source)
 *
 *  [3dgs2023]  Bernhard Kebl, Georgios Kopanas et al.
 *              3D Gaussian Splatting for Real-Time Radiance Field Rendering
 *              ACM Transactions on Graphics (SIGGRAPH 2023)
 *              https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/
 *
 *  [kwok2024]  Kevin Kwok
 *              WebGL 3D Gaussian Splat Viewer (MIT License)
 *              https://github.com/antimatter15/splat
 *
 *  [gspt2026]  Joris Rijsdijk et al.
 *              Gaussian Point Splatting
 *              ACM Transactions on Graphics (SIGGRAPH 2026)
 *              https://jorisar.nl/gaussian_point_splatting/
 */

#pragma once

#include "draw_defines.hh"
#include "draw_shader_shared.hh"

#include "gpu_shader_compat.hh"
#include "gpu_shader_math_base.bsl.hh"
#include "gpu_shader_math_constants.bsl.hh"
#include "gpu_shader_math_matrix_construct.bsl.hh"
#include "gpu_shader_math_matrix_transform.bsl.hh"
#include "gpu_shader_math_quaternion.bsl.hh"
#include "gpu_shader_math_vector.bsl.hh"
#include "gpu_shader_math_vector_reduce.bsl.hh"
#include "gpu_shader_utildefines.bsl.hh"

namespace draw::gsplat {

namespace detail {

/* TODO(not_mark): Move to a `gpu_shader_*` header? */
template<typename T> void swap(T &a, T &b)
{
  T c = a;
  a = b;
  b = c;
}
template void swap<float>(float &, float &);
template void swap<float2>(float2 &, float2 &);

/* TODO(not_mark): use or update `eevee_octahedron_lib` instead? */
/** Octahedral normal encoding [octa2017]. */
float2 pack_normal_octahedral(float3 N)
{
  N /= reduce_add(abs(N));
  N.xy = N.z >= 0.f ?
             N.xy :
             (1.f - abs(N.yx)) * mix(float2(-1.f), float2(1.f), greaterThanEqual(N.xy, float2(0)));
  N.xy = N.xy * .5f + .5f;
  return N.xy;
}

/* TODO(not_mark): use or update `eevee_octahedron_lib` instead? */
/** Octahedral normal decoding [octa2017]. */
float3 unpack_normal_octahedral(float2 p)
{
  p = p * 2.0f - 1.0f;
  float3 N = float3(p.xy, 1.0f - reduce_add(abs(p.xy)));
  float t = clamp(-N.z, 0.0f, 1.0f);
  N.xy += mix(float2(-t), float2(t), greaterThanEqual(N.xy, float2(0.0f)));
  return normalize(N);
}

/**
 * Inverse of expmap packing [quat2017] of quaternion data.
 *
 * \note Keep in sync with `pack_quaternion_4_to_3` in `draw_cache_impl_gsplat.cc`.
 */
float4 unpack_quaternion_3_to_4(float3 v)
{
  float d = dot(v, v);
  float t = inversesqrt(d + 1.0e-8f);
  float a = M_PI_2 * t * d;
  float s = sin(a);
  float c = cos(a);
  float k = s * t;
  return float4(k * v, c);
}

/**
 * Unpack Gaussian mean from quantized representation [gspt2026].
 *
 * \note Keep in sync with `pack_gaussian()` in `draw_cache_impl_gsplat.cc`.
 */
float3 unpack_gaussian_mean(uint4 pack, ScalingRange data_range)
{
  uint3 pu;
  pu.x = /* 22b */ pack.x & 0x3FFFFFu;
  pu.y = /* 10b */ ((pack.x >> 22) & 0x0003FFu) |
         /* 12b */ ((pack.y & 0x000FFFu) << 10);
  pu.z = /* 20b */ ((pack.y >> 12) & 0x0FFFFFu) |
         /* 02b */ ((pack.z & 0x000003u) << 20);
  float3 p = float3(pu) / float((1 << 22) - 1u);
  return data_range.scale_from_unit(p);
}

/**
 * Unpack Gaussian rotational component from quantized representation [gspt2026].
 *
 * \note Keep in sync with `pack_gaussian()` in `draw_cache_impl_gsplat.cc`.
 */
Quaternion unpack_gaussian_rotation(uint4 pack)
{
  uint3 ru;
  ru.x = /* 9b */ (pack.z >> 2) & 0x0001FFu;
  ru.y = /* 9b */ (pack.z >> 11u) & 0x0001FFu;
  ru.z = /* 9b */ (pack.z >> 20u) & 0x0001FFu;
  float3 r = float3(ru) / 511.0f;
  return Quaternion::from_float4(unpack_quaternion_3_to_4(r * 2.0f - 1.0f));
}

/**
 * Unpack Gaussian scaling component from quantized representation [gspt2026].
 *
 * \note Keep in sync with `pack_gaussian()` in `draw_cache_impl_gsplat.cc`.
 */
float3 unpack_gaussian_scale(uint4 pack, ScalingRange data_range)
{
  uint3 su;
  su.x = /* 3b */ ((pack.z >> 29) & 0x000007u) |
         /* 6b */ ((pack.w & 0x00003Fu) << 3);
  su.y = /* 9b */ (pack.w >> 6) & 0x0001FFu;
  su.z = /* 9b */ (pack.w >> 15) & 0x0001FFu;
  float3 s = float3(su) / 511.0f;
  return exp(data_range.scale_from_unit(s));
}

/**
 * Unpack Gaussian opacity from quantized representation.
 *
 * \note Keep in sync with `pack_gaussian()` in `draw_cache_impl_gsplat.cc`.
 */
float unpack_gaussian_opacity(uint4 pack)
{
  uint au = (pack.w >> 24) & 0x0000FFu;
  float a = float(au) / 255.0f;
  return a;
}

/**
 * Helper methods to slice 3-tuples (xyz) of 8b values from uint4
 * packed data ([xyzx][yzxy][zxyz][xyzx][yzxy]...).
 */
uint3 slice_tuple_xyzn(uint t)
{
  return uint3(t, t >> 8, t >> 16) & 0xFFu;
}
uint3 slice_tuple_nxyz(uint t)
{
  return uint3(t >> 8, t >> 16, t >> 24) & 0xFFu;
}
uint3 slice_tuple_xnyz(uint t1, uint t2)
{
  return uint3(t1 >> 24, t2, t2 >> 8) & 0xFFu;
}
uint3 slice_tuple_xynz(uint t1, uint t2)
{
  return uint3(t1 >> 16, t1 >> 24, t2) & 0xFFu;
}

/** Get the index of the current gsplat, given the vertex index. */
uint get_shape_id(uint vert_id)
{
  return int(vert_id / uint(DRW_GSPLAT_STRIP_TILE_SIZE));
}

/** Given a vertex index, determine if the expected winding order of the strip is cw/ccw. */
bool get_shape_winding_ccw(uint vert_id)
{
  return bool((vert_id / DRW_GSPLAT_STRIP_TILE_SIZE) % 2u);
}

/** Get a local vertex offset of the current gsplat, given the vertex index. */
float2 get_shape_offset(uint vert_id)
{
  /* Flip vertices 1,2 for ccw winding order by flipping their sign. */
  const float winding_order_mult = get_shape_winding_ccw(vert_id) ? -1.0f : 1.0f;
  switch (vert_id % DRW_GSPLAT_STRIP_TILE_SIZE) {
    case 0:
      return float2(-1.0, -1.0);
    case 1:
      return float2(1.0, -1.0) * winding_order_mult;
    case 2:
      return float2(-1.0, 1.0) * winding_order_mult;
    case 3:
      return float2(1.0, 1.0);
    default:
      return float2(NAN_FLT);
  };
}

}  // namespace detail

/**
 * Helper tuple storing a pair of perpendicular vectors with non-unit lengths. Packs to a single
 * uint. Vectors are re-ordered to be efficient for packing.
 */
struct AxisTuple {
  float2 axis_0;
  float2 axis_1;

  static AxisTuple from(float2 axis_0, float2 axis_1)
  {
    /* Flip/swap so axis_0 lies in the 1st and axis_1 in the 2nd quadrant of a hemicircle. */
    axis_0 = (axis_0.y < 0) ? -axis_0 : axis_0;
    axis_1 = (axis_1.y < 0) ? -axis_1 : axis_1;
    if (axis_0.x < 0.0f) {
      detail::swap(axis_0, axis_1);
    }
    return AxisTuple{.axis_0 = axis_0, .axis_1 = axis_1};
  }

  uint pack() const
  {

    float axis_0_len;
    float2 axis_0_dir = normalize_and_get_length(axis_0, axis_0_len);
    float axis_1_len = length(axis_1);
    if (axis_0_len == 0.0f && axis_1_len == 0.0f) {
      return 0u;
    }

    constexpr float max_axis_size = 1024.0f;
    axis_0_len = clamp(axis_0_len / max_axis_size, 0.0f, 1.0f);
    axis_1_len = clamp(axis_1_len / max_axis_size, 0.0f, 1.0f);

    /* NOTE(not_mark): do not reduce packing size further; already causes jittering on zoom. */
    uint data = /* 10 bits */ uint(clamp(axis_0_dir.x, 0.0f, 1.0f) * 1023.0f) |
                /* 11 bits */ (uint(clamp(axis_0_len, 0.0f, 1.0f) * 2047.0f) << 10u) |
                /* 11 bits */ (uint(clamp(axis_1_len, 0.0f, 1.0f) * 2047.0f) << 21u);
    return data;
  }

  static AxisTuple unpack(uint data)
  {
    /* NOTE(not_mark): do not reduce packing size further; already causes jittering on zoom. */
    float axis_0_dir = /* 10 bits */ float(data & 0x3FFu) / 1023.0f;
    float axis_0_len = /* 11 bits */ float((data >> 10u) & 0x7FFu) / 2047.0f;
    float axis_1_len = /* 11 bits */ float((data >> 21u) & 0x7FFu) / 2047.0f;

    constexpr float max_axis_size = 1024.0f;
    const float2 axis = normalize(float2(axis_0_dir, cos_from_sin(axis_0_dir)));

    AxisTuple tuple;
    tuple.axis_0 = axis * axis_0_len * max_axis_size;
    tuple.axis_1 = float2(-axis.y, axis.x) * axis_1_len * max_axis_size;
    return tuple;
  }
};

/**
 * Construct a 3x3 matrix from a rotation+scaling representation.
 */
float3x3 get_rot_scale_mat(Quaternion rotation, float3 scale)
{
  float3x3 R = quaternion_to_float3x3(rotation);
  float3x3 S = from_scale(scale);
  return R * S;
}

/**
 * Construct a 3x3 covariance matrix from a rotation+scaling representation.
 */
float3x3 get_covmat(Quaternion rotation, float3 scale)
{
  float3x3 M = get_rot_scale_mat(rotation, scale);
  return M * transpose(M);
}

/**
 * Compute perspective-projected 2D covariance matrix for a Gaussian ellipse, from the
 * covariance matrix of the 3D ellipsoid.
 *
 * \note Reproduces eq. 29 and eq. 31 in [evas2002], with aspect/scaling tweaks from the
 *       code of [3dgs2023], and a slightly modified Jacobian for orthographic cameras.
 * \note The 2d covmatrix is of form [[x, y], [y, z]], and returned as [x, y, z].
 */
float3 project_covmat(float3 lP,
                      float4x4 viewmat,
                      float4x4 winmat,
                      float3x3 covmat_3d,
                      float2 viewport_size,
                      bool is_perspective)
{
  /* Compute viewport parameters. */
  float aspect = winmat[0][0] / winmat[1][1];
  float focal = viewport_size.x * winmat[0][0] * 0.5f;

  float tan_fov_x = 1.0f / winmat[0][0];
  float tan_fov_y = 1.0f / (winmat[1][1] * aspect);
  float lim_x = 1.3f * tan_fov_x;
  float lim_y = 1.3f * tan_fov_y;

  /* Parameter `t` in [ewas2023], effectively projected point. */
  float3 vP = transform_point(viewmat, lP);
  vP.x = clamp(vP.x / vP.z, -lim_x, lim_x) * vP.z;
  vP.y = clamp(vP.y / vP.z, -lim_y, lim_y) * vP.z;

  /* Eq. 29: construct Jacobian matrix. */
  float3x3 J;
  if (is_perspective) {
    J = float3x3(float3(focal / vP.z, 0.0f, -(focal * vP.x) / (vP.z * vP.z)),
                 float3(0.0f, focal / vP.z, -(focal * vP.y) / (vP.z * vP.z)),
                 float3(0.0f, 0.0f, 0.0f));
  }
  else {
    /* For orthographic matrices, the Jacobian is slightly different. */
    J = float3x3(float3(focal, 0.0f, 0.0f), float3(0.0f, focal, 0.0f), float3(0.0f));
  }

  /* Eq. 31: construct covariance matrix in screen space. */
  float3x3 V = float3x3(float3(covmat_3d[0][0], covmat_3d[0][1], covmat_3d[0][2]),
                        float3(covmat_3d[0][1], covmat_3d[1][1], covmat_3d[1][2]),
                        float3(covmat_3d[0][2], covmat_3d[1][2], covmat_3d[2][2]));
  float3x3 W = to_float3x3(viewmat); /* Rotational part of matrix. */
  float3x3 T = transpose(W) * J;
  float3x3 covmat_2d = transpose(T) * V * T;

  /* Applied in [3dgs2023], either as a low-pass regularization, or to enforce some minimum pixel
   * size. Reason unclear, but I guess we should match the reference here. */
  /* NOTE(not_mark): disabled for now, better matches Cycles implementation. */
  // covmat_2d[0][0] += 0.3f;
  // covmat_2d[1][1] += 0.3f;

  return float3(covmat_2d[0][0], covmat_2d[0][1], covmat_2d[1][1]);
}

/**
 * Given the packed 2D covariance matrix, decompose into scaled eigenvectors, which we can use
 * to fit a billboard rectangle shape.
 *
 * \note Variant of a decomposition in [3dgs2023], though more like the version in [kwok2024].
 */
AxisTuple covmat_to_axes(float3 covmat_2d)
{
  float mid = 0.5f * (covmat_2d.x + covmat_2d.z);
  float radius = length(float2((covmat_2d.x - covmat_2d.z) * 0.5f, covmat_2d.y));
  float lambda_1 = mid + radius;
  float lambda_2 = max(mid - radius, 0.001f);

  float2 diag = normalize(float2(covmat_2d.y, lambda_1 - covmat_2d.x));

  constexpr float max_axis_size = 1024.0f;
  float2 axis_0 = min(sqrt(2.0f * lambda_1), max_axis_size) * diag;
  float2 axis_1 = min(sqrt(2.0f * lambda_2), max_axis_size) * float2(diag.y, -diag.x);

  return AxisTuple::from(axis_0, axis_1);
}

/**
 * Construct a planar normal based on an ellipsoid's shortest axis.
 *
 * \note Follows the "Flattening the 3D Gaussian" section in [pgsr2025].
 */
float3 get_planar_normal(Quaternion rotation, float3 scale)
{
#if 0
  /* Reference implementation. */
  float3x3 R = quaternion_to_float3x3(rotation);
  if ((scale.x <= scale.y) && (scale.x <= scale.z)) {
    return R * float3(1.0f, 0.0f, 0.0f);
  }
  if ((scale.y < scale.x) && (scale.y <= scale.z)) {
    return R * float3(0.0f, 1.0f, 0.0f);
  }
  return R * float3(0.0f, 0.0f, 1.0f);
#else
  const float norm = 1.0f / (rotation.x * rotation.x + rotation.y * rotation.y +
                             rotation.z * rotation.z + rotation.w * rotation.w);

  const float a = rotation.w;
  const float b = rotation.x;
  const float c = rotation.y;
  const float d = rotation.z;

  if ((scale.x <= scale.y) && (scale.x <= scale.z)) {
    return float3(a * a + b * b - c * c - d * d, 2.0f * (a * d + b * c), 2.0f * (b * d - a * c)) *
           norm;
  }
  if ((scale.y < scale.x) && (scale.y <= scale.z)) {
    return float3(2.0f * (b * c - a * d), a * a - b * b + c * c - d * d, 2.0f * (a * b + c * d)) *
           norm;
  }
  return float3(2.0f * (a * c + b * d), 2.0f * (c * d - a * b), a * a - b * b - c * c + d * d) *
         norm;
#endif
}

/**
 * Helper struct for gsplat unprojected shape data.
 */
struct Gaussian {
  /* Index stored for further attribute loading. */
  uint id;
  /* Gaussian local centroid and covariance data. */
  float3 mean;
  float3 scale;
  Quaternion rotation;
  /* Gaussian opacity, applied in alpha resolve. */
  float opacity;

  /** Unpack Gaussian from quantized data representation. */
  static Gaussian unpack(uint gs_id, uint4 data, ScalingRange mean_range, ScalingRange scale_range)
  {
    Gaussian gs;
    gs.id = gs_id;
    gs.mean = detail::unpack_gaussian_mean(data, mean_range);
    gs.rotation = detail::unpack_gaussian_rotation(data);
    gs.scale = detail::unpack_gaussian_scale(data, scale_range);
    gs.opacity = detail::unpack_gaussian_opacity(data);
    return gs;
  }
};

/**
 * Helper struct for billboarded geometry output.
 */
struct SplatShape {
  /* Index stored for further attribute loading. */
  uint id;
  /* Gaussian local centroid. */
  float3 mean;
  /* Billboard vertex.  */
  float2 shape_offset;
  /* Vertex output data. */
  float3 wN;
  float3 wP;
  float4 hP;

  static SplatShape culled()
  {
    return SplatShape{.id = ~0u};
  }

  bool is_culled() const
  {
    return id == (~0u);
  }
};

/**
 * Helper struct for point geometry output.
 */
struct PointShape {
  /* Index stored for further attribute loading. */
  uint id;
  /* Gaussian local centroid. */
  float3 mean;
  /* Vertex output data. */
  float3 wP;
  float4 hP;

  static PointShape culled()
  {
    return PointShape{.id = ~0u};
  }

  bool is_culled() const
  {
    return id == (~0u);
  }
};

/**
 * Shader resource table for gsplat data. Used by an entry point for
 * geometry output from precomputed ellipse data.
 */
struct ShapeResource {
  [[uniform(DRW_OBJ_DATA_INFO_UBO_SLOT)]] const GSplatInfos &infos;

  [[sampler(DRW_GSPLAT_SHAPE_DATA_TEX_SLOT)]] const usamplerBuffer shape_data_tx;
  [[sampler(DRW_GSPLAT_ELLIPSE_COMP_TEX_SLOT)]] const usamplerBuffer ellipses_comp_tx;
  [[sampler(DRW_GSPLAT_RADIANCE_COMP_TEX_SLOT)]] const samplerBuffer radiance_comp_tx;

  [[push_constant]] const float2 viewport_size;

 private:
  Gaussian get_gaussian(uint gs_id) const
  {
    const uint4 data = texelFetch(shape_data_tx, int(gs_id));
    return Gaussian::unpack(gs_id, data, infos.splat_mean_range, infos.splat_scale_range);
  }

  SplatShape get_splat_implementation(uint vert_id,
                                      float3 lP,
                                      float3 wN,
                                      ObjectMatrices object,
                                      ViewMatrices view,
                                      AxisTuple axes) const
  {
    float2 shape_offset = detail::get_shape_offset(vert_id);
    if (any(isnan(shape_offset))) {
      return SplatShape::culled();
    }

    SplatShape shape;
    shape.id = detail::get_shape_id(vert_id);
    shape.wN = wN;
    shape.mean = lP;
    shape.shape_offset = 2.0f * shape_offset;

    /* Screen-space offset determined by ellipse axis tuple. */
    float2 ss_delta = (shape.shape_offset.x * axes.axis_0 + shape.shape_offset.y * axes.axis_1) *
                      2.0f / viewport_size;

    /* Add screen-space offset to homogeneous position. */
    float3 wP = object.point_object_to_world(lP);
    shape.hP = view.point_world_to_homogenous(wP);
    shape.hP.xy += ss_delta * shape.hP.w;
    shape.wP = view.point_homogeneous_to_world(shape.hP);

    return shape;
  }

 public:
  /**
   * Recover view-dependent radiance and alpha.
   *
   * \note Available when a compute prepass has run.
   */
  float3 get_radiance(uint gs_id) const
  {
    uint2 data = floatBitsToUint(texelFetch(radiance_comp_tx, int(gs_id)).xy);
    return float4(unpackHalf2x16(data.x), unpackHalf2x16(data.y)).rgb;
  }

  /**
   * Recover view-independent splat opacity.
   *
   * \note Available when a compute prepass is not done (shadows). Further used as
   * gaussians should appear correct even with shader graphs that omit computed radiance.
   */
  float get_opacity(uint gs_id) const
  {
    const uint4 shape_data = texelFetch(shape_data_tx, int(gs_id));
    return detail::unpack_gaussian_opacity(shape_data);
  }

  /**
   * Recover point draw data for a specific vertex.
   */
  PointShape get_point(uint vert_id, ObjectMatrices object, ViewMatrices view) const
  {
    PointShape gs;
    gs.id = vert_id;

    const uint4 shape_data = texelFetch(shape_data_tx, int(vert_id));
    gs.mean = detail::unpack_gaussian_mean(shape_data, infos.splat_mean_range);
    gs.wP = object.point_object_to_world(gs.mean);
    gs.hP = view.point_world_to_homogenous(gs.wP);
    return gs;
  }

  /**
   * Recover billboard splat draw data for a specific vertex.
   *
   * \note Available when a compute prepass has run.
   */
  SplatShape get_splat(uint vert_id, ObjectMatrices object, ViewMatrices view) const
  {
    /* TODO(not_mark): Load of precompute data is disabled until we can address incompatibility
     * of a compute prepass with instancing and other factors. For now, compute splat data on
     * the fly using `get_splat_fallback`. */
    return get_splat_fallback(vert_id, object, view);

    // const uint gs_id = detail::get_shape_id(vert_id);

    // const uint2 prepass_data = texelFetch(ellipses_comp_tx, int(gs_id)).xy;
    // if (prepass_data.x == 0u) {
    //   return SplatShape::culled();
    // }

    // /* Unpack ellipse axes, normal from compute prepass data. */
    // AxisTuple axes = AxisTuple::unpack(prepass_data.x);
    // float3 wN = detail::unpack_normal_octahedral(unpackSnorm2x16(prepass_data.y));

    // /* Unpack ellipse centroid as Gaussian mean. */
    // const uint4 shape_data = texelFetch(shape_data_tx, int(gs_id));
    // float3 lP = detail::unpack_gaussian_mean(shape_data, infos.splat_mean_range);

    // return get_splat_implementation(vert_id, lP, wN, object, view, axes);
  }

  /**
   * Recover billboard splat draw data for a specific vertex.
   *
   * \note Available when a compute prepass has not run. More expensive. Used for e.g. shadows.
   */
  SplatShape get_splat_fallback(uint vert_id, ObjectMatrices object, ViewMatrices view) const
  {
    const uint gs_id = detail::get_shape_id(vert_id);
    const Gaussian gs = get_gaussian(gs_id);

    /* Project Gaussian to screen-space, recovering 2D axes for a splatted ellipse. */
    float4x4 viewmat = view.viewmat * object.model;
    float3x3 covmat = get_covmat(gs.rotation, gs.scale);
    float3 covmat_proj = project_covmat(
        gs.mean, viewmat, view.winmat, covmat, viewport_size, view.is_perspective());
    AxisTuple axes = covmat_to_axes(covmat_proj);

    float3 lP = gs.mean;
    float3 wP = object.point_object_to_world(gs.mean);

    /* Compute world normal. Normal is oriented towards view, never back-facing. */
    float3 lN = normalize(get_planar_normal(gs.rotation, gs.scale));
    float3 wN = object.normal_object_to_world(lN);
    float3 wV = view.world_incident_vector(wP);
    if (dot(wN, wV) < 0.0f) {
      wN = -wN;
    }

    return get_splat_implementation(vert_id, lP, wN, object, view, axes);
  }
};

/**
 * Gaussian alpha, following eq. 2 in [3dgs2023] it is multiplied by data opacity.
 */
float evaluate_gaussian(float2 x_min_mu, float opacity)
{
  return saturate(exp(-dot(x_min_mu, x_min_mu)) * opacity);
}

}  // namespace draw::gsplat
