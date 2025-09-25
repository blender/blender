/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Intersection library used for culling.
 * Results are meant to be conservative.
 */

#include "draw_view_infos.hh"

#include "draw_shape_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/* ---------------------------------------------------------------------- */
/** \name Plane extraction functions.
 * \{ */

/** \a v1 and \a v2 are vectors on the plane. \a p is a point on the plane. */
float4 isect_plane_setup(float3 p, float3 v1, float3 v2)
{
  float3 normal_to_plane = normalize(cross(v1, v2));
  return float4(normal_to_plane, -dot(normal_to_plane, p));
}

struct IsectPyramid {
  float3 corners[5];
  float4 planes[5];
};

IsectPyramid isect_pyramid_setup(Pyramid shape)
{
  float3 A1 = shape.corners[1] - shape.corners[0];
  float3 A2 = shape.corners[2] - shape.corners[0];
  float3 A3 = shape.corners[3] - shape.corners[0];
  float3 A4 = shape.corners[4] - shape.corners[0];
  float3 S4 = shape.corners[4] - shape.corners[1];
  float3 S2 = shape.corners[2] - shape.corners[1];

  IsectPyramid data;
  data.planes[0] = isect_plane_setup(shape.corners[0], A2, A1);
  data.planes[1] = isect_plane_setup(shape.corners[0], A3, A2);
  data.planes[2] = isect_plane_setup(shape.corners[0], A4, A3);
  data.planes[3] = isect_plane_setup(shape.corners[0], A1, A4);
  data.planes[4] = isect_plane_setup(shape.corners[1], S2, S4);
  for (int i = 0; i < 5; i++) {
    data.corners[i] = shape.corners[i];
  }
  return data;
}

struct IsectBox {
  float3 corners[8];
  float4 planes[6];
};

IsectBox isect_box_setup(Box shape)
{
  float3 A1 = shape.corners[1] - shape.corners[0];
  float3 A3 = shape.corners[3] - shape.corners[0];
  float3 A4 = shape.corners[4] - shape.corners[0];

  IsectBox data;
  data.planes[0] = isect_plane_setup(shape.corners[0], A3, A1);
  data.planes[1] = isect_plane_setup(shape.corners[0], A4, A3);
  data.planes[2] = isect_plane_setup(shape.corners[0], A1, A4);
  /* Assumes that the box is actually a box! */
  data.planes[3] = float4(-data.planes[0].xyz, -dot(-data.planes[0].xyz, shape.corners[6]));
  data.planes[4] = float4(-data.planes[1].xyz, -dot(-data.planes[1].xyz, shape.corners[6]));
  data.planes[5] = float4(-data.planes[2].xyz, -dot(-data.planes[2].xyz, shape.corners[6]));
  for (int i = 0; i < 8; i++) {
    data.corners[i] = shape.corners[i];
  }
  return data;
}

/* Construct box from 1 corner point + 3 side vectors. */
IsectBox isect_box_setup(float3 origin, float3 side_x, float3 side_y, float3 side_z)
{
  IsectBox data;
  data.corners[0] = origin;
  data.corners[1] = origin + side_x;
  data.corners[2] = origin + side_y + side_x;
  data.corners[3] = origin + side_y;
  data.corners[4] = data.corners[0] + side_z;
  data.corners[5] = data.corners[1] + side_z;
  data.corners[6] = data.corners[2] + side_z;
  data.corners[7] = data.corners[3] + side_z;

  data.planes[0] = isect_plane_setup(data.corners[0], side_y, side_z);
  data.planes[1] = isect_plane_setup(data.corners[0], side_x, side_y);
  data.planes[2] = isect_plane_setup(data.corners[0], side_z, side_x);
  /* Assumes that the box is actually a box! */
  data.planes[3] = float4(-data.planes[0].xyz, -dot(-data.planes[0].xyz, data.corners[6]));
  data.planes[4] = float4(-data.planes[1].xyz, -dot(-data.planes[1].xyz, data.corners[6]));
  data.planes[5] = float4(-data.planes[2].xyz, -dot(-data.planes[2].xyz, data.corners[6]));

  return data;
}

struct IsectFrustum {
  float3 corners[8];
  float4 planes[6];
};

IsectFrustum isect_frustum_setup(Frustum shape)
{
  float3 A1 = shape.corners[1] - shape.corners[0];
  float3 A3 = shape.corners[3] - shape.corners[0];
  float3 A4 = shape.corners[4] - shape.corners[0];
  float3 B5 = shape.corners[5] - shape.corners[6];
  float3 B7 = shape.corners[7] - shape.corners[6];
  float3 B2 = shape.corners[2] - shape.corners[6];

  IsectFrustum data;
  data.planes[0] = isect_plane_setup(shape.corners[0], A3, A1);
  data.planes[1] = isect_plane_setup(shape.corners[0], A4, A3);
  data.planes[2] = isect_plane_setup(shape.corners[0], A1, A4);
  data.planes[3] = isect_plane_setup(shape.corners[6], B7, B5);
  data.planes[4] = isect_plane_setup(shape.corners[6], B5, B2);
  data.planes[5] = isect_plane_setup(shape.corners[6], B2, B7);
  for (int i = 0; i < 8; i++) {
    data.corners[i] = shape.corners[i];
  }
  return data;
}

/** \} */

/* ---------------------------------------------------------------------- */
/** \name View Intersection functions.
 * \{ */

#ifdef DRW_VIEW_CULLING_INFO
SHADER_LIBRARY_CREATE_INFO(draw_view_culling)

ViewCullingData drw_view_culling(uint view_id = drw_view_id)
{
  return drw_view_culling_buf[view_id];
}

bool intersect_view(Pyramid pyramid, uint view_id = drw_view_id)
{
  bool intersects = true;

  /* WORKAROUND: There is a GLSL compiler bug on legacy AMD GPU drivers that returns an incorrect
   * computation if `drw_view_culling()` is called in both of the loops below (see #143336). */
  ViewCullingData culling = drw_view_culling(view_id);

  /* Do Pyramid vertices vs Frustum planes. */
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 5; ++v) {
      float test = dot(culling.frustum_planes.planes[p], float4(pyramid.corners[v], 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }

  if (!intersects) {
    return intersects;
  }

  /* Now do Frustum vertices vs Pyramid planes. */
  IsectPyramid i_pyramid = isect_pyramid_setup(pyramid);
  for (int p = 0; p < 5; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(i_pyramid.planes[p], float4(culling.frustum_corners.corners[v].xyz, 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }
  return intersects;
}

bool intersect_view(Box box, uint view_id = drw_view_id)
{
  bool intersects = true;

  /* WORKAROUND: There is a GLSL compiler bug on legacy AMD GPU drivers that returns an incorrect
   * computation if `drw_view_culling()` is called in both of the loops below (see #143336). */
  ViewCullingData culling = drw_view_culling(view_id);

  /* Do Box vertices vs Frustum planes. */
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(culling.frustum_planes.planes[p], float4(box.corners[v], 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }

  if (!intersects) {
    return intersects;
  }

  /* Now do Frustum vertices vs Box planes. */
  IsectBox i_box = isect_box_setup(box);
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(i_box.planes[p], float4(culling.frustum_corners.corners[v].xyz, 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }

  return intersects;
}

bool intersect_view(IsectBox i_box, uint view_id = drw_view_id)
{
  bool intersects = true;

  /* WORKAROUND: There is a GLSL compiler bug on legacy AMD GPU drivers that returns an incorrect
   * computation if `drw_view_culling()` is called in both of the loops below (see #143336). */
  ViewCullingData culling = drw_view_culling(view_id);

  /* Do Box vertices vs Frustum planes. */
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(culling.frustum_planes.planes[p], float4(i_box.corners[v], 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }

  if (!intersects) {
    return intersects;
  }

  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(i_box.planes[p], float4(culling.frustum_corners.corners[v].xyz, 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }

  return intersects;
}

bool intersect_view(Sphere sphere, uint view_id = drw_view_id)
{
  bool intersects = true;

  /* WORKAROUND: There is a GLSL compiler bug on legacy AMD GPU drivers that returns an incorrect
   * computation if `drw_view_culling()` is called in both of the loops below (see #143336). */
  ViewCullingData culling = drw_view_culling(view_id);

  for (int p = 0; p < 6 && intersects; ++p) {
    float dist_to_plane = dot(culling.frustum_planes.planes[p], float4(sphere.center, 1.0f));
    if (dist_to_plane < -sphere.radius) {
      intersects = false;
    }
  }
  /* TODO reject false positive. */
  return intersects;
}

#endif

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Shape vs. Shape Intersection functions.
 * \{ */

bool intersect(IsectPyramid i_pyramid, Box box)
{
  bool intersects = true;

  /* Do Box vertices vs Pyramid planes. */
  for (int p = 0; p < 5; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(i_pyramid.planes[p], float4(box.corners[v], 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }

  if (!intersects) {
    return intersects;
  }

  /* Now do Pyramid vertices vs Box planes. */
  IsectBox i_box = isect_box_setup(box);
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 5; ++v) {
      float test = dot(i_box.planes[p], float4(i_pyramid.corners[v], 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }
  return intersects;
}

bool intersect(IsectPyramid i_pyramid, IsectBox i_box)
{
  bool intersects = true;

  /* Do Box vertices vs Pyramid planes. */
  for (int p = 0; p < 5; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(i_pyramid.planes[p], float4(i_box.corners[v], 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }

  if (!intersects) {
    return intersects;
  }

  /* Now do Pyramid vertices vs Box planes. */
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 5; ++v) {
      float test = dot(i_box.planes[p], float4(i_pyramid.corners[v], 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }
  return intersects;
}

bool intersect(IsectFrustum i_frustum, Pyramid pyramid)
{
  bool intersects = true;

  /* Do Pyramid vertices vs Frustum planes. */
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 5; ++v) {
      float test = dot(i_frustum.planes[p], float4(pyramid.corners[v], 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }

  if (!intersects) {
    return intersects;
  }

  /* Now do Frustum vertices vs Pyramid planes. */
  IsectPyramid i_pyramid = isect_pyramid_setup(pyramid);
  for (int p = 0; p < 5; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(i_pyramid.planes[p], float4(i_frustum.corners[v].xyz, 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }
  return intersects;
}

bool intersect(IsectFrustum i_frustum, Box box)
{
  bool intersects = true;

  /* Do Box vertices vs Frustum planes. */
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(i_frustum.planes[p], float4(box.corners[v], 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }

  if (!intersects) {
    return intersects;
  }

  /* Now do Frustum vertices vs Box planes. */
  IsectBox i_box = isect_box_setup(box);
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(i_box.planes[p], float4(i_frustum.corners[v].xyz, 1.0f));
      if (test > 0.0f) {
        is_any_vertex_on_positive_side = true;
        break;
      }
    }
    bool all_vertex_on_negative_side = !is_any_vertex_on_positive_side;
    if (all_vertex_on_negative_side) {
      intersects = false;
      break;
    }
  }

  return intersects;
}

bool intersect(IsectFrustum i_frustum, Sphere sphere)
{
  bool intersects = true;
  for (int p = 0; p < 6; ++p) {
    float dist_to_plane = dot(i_frustum.planes[p], float4(sphere.center, 1.0f));
    if (dist_to_plane < -sphere.radius) {
      intersects = false;
      break;
    }
  }
  return intersects;
}

bool intersect(Cone cone, Sphere sphere)
{
  /**
   * Following "Improve Tile-based Light Culling with Spherical-sliced Cone"
   * by Eric Zhang
   * https://lxjk.github.io/2018/03/25/Improve-Tile-based-Light-Culling-with-Spherical-sliced-Cone.html
   */
  float sphere_distance = length(sphere.center);
  float sphere_distance_rcp = safe_rcp(sphere_distance);
  float sphere_sin = saturate(sphere.radius * sphere_distance_rcp);
  float sphere_cos = sqrt(1.0f - sphere_sin * sphere_sin);
  float cone_aperture_sin = sqrt(1.0f - cone.angle_cos * cone.angle_cos);

  float cone_sphere_center_cos = dot(sphere.center * sphere_distance_rcp, cone.direction);
  /* cos(A+B) = cos(A) * cos(B) - sin(A) * sin(B). */
  float cone_sphere_angle_sum_cos = (sphere.radius > sphere_distance) ?
                                        -1.0f :
                                        (cone.angle_cos * sphere_cos -
                                         cone_aperture_sin * sphere_sin);
  /* Comparing cosines instead of angles since we are interested
   * only in the monotonic region [0 .. M_PI / 2]. This saves costly `acos()` calls. */
  bool intersects = (cone_sphere_center_cos >= cone_sphere_angle_sum_cos);

  return intersects;
}

bool intersect(Circle circle_a, Circle circle_b)
{
  return distance_squared(circle_a.center, circle_b.center) <
         square(circle_a.radius + circle_b.radius);
}

/** \} */
