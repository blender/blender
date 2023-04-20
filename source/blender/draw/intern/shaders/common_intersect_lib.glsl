
/**
 * Intersection library used for culling.
 * Results are meant to be conservative.
 */

#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(common_shape_lib.glsl)

/* ---------------------------------------------------------------------- */
/** \name Plane extraction functions.
 * \{ */

/** \a v1 and \a v2 are vectors on the plane. \a p is a point on the plane. */
vec4 isect_plane_setup(vec3 p, vec3 v1, vec3 v2)
{
  vec3 normal_to_plane = normalize(cross(v1, v2));
  return vec4(normal_to_plane, -dot(normal_to_plane, p));
}

struct IsectPyramid {
  vec3 corners[5];
  vec4 planes[5];
};

IsectPyramid isect_data_setup(Pyramid shape)
{
  vec3 A1 = shape.corners[1] - shape.corners[0];
  vec3 A2 = shape.corners[2] - shape.corners[0];
  vec3 A3 = shape.corners[3] - shape.corners[0];
  vec3 A4 = shape.corners[4] - shape.corners[0];
  vec3 S4 = shape.corners[4] - shape.corners[1];
  vec3 S2 = shape.corners[2] - shape.corners[1];

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
  vec3 corners[8];
  vec4 planes[6];
};

IsectBox isect_data_setup(Box shape)
{
  vec3 A1 = shape.corners[1] - shape.corners[0];
  vec3 A3 = shape.corners[3] - shape.corners[0];
  vec3 A4 = shape.corners[4] - shape.corners[0];

  IsectBox data;
  data.planes[0] = isect_plane_setup(shape.corners[0], A3, A1);
  data.planes[1] = isect_plane_setup(shape.corners[0], A4, A3);
  data.planes[2] = isect_plane_setup(shape.corners[0], A1, A4);
  /* Assumes that the box is actually a box! */
  data.planes[3] = vec4(-data.planes[0].xyz, -dot(-data.planes[0].xyz, shape.corners[6]));
  data.planes[4] = vec4(-data.planes[1].xyz, -dot(-data.planes[1].xyz, shape.corners[6]));
  data.planes[5] = vec4(-data.planes[2].xyz, -dot(-data.planes[2].xyz, shape.corners[6]));
  for (int i = 0; i < 8; i++) {
    data.corners[i] = shape.corners[i];
  }
  return data;
}

/* Construct box from 1 corner point + 3 side vectors. */
IsectBox isect_data_setup(vec3 origin, vec3 side_x, vec3 side_y, vec3 side_z)
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
  data.planes[3] = vec4(-data.planes[0].xyz, -dot(-data.planes[0].xyz, data.corners[6]));
  data.planes[4] = vec4(-data.planes[1].xyz, -dot(-data.planes[1].xyz, data.corners[6]));
  data.planes[5] = vec4(-data.planes[2].xyz, -dot(-data.planes[2].xyz, data.corners[6]));

  return data;
}

struct IsectFrustum {
  vec3 corners[8];
  vec4 planes[6];
};

IsectFrustum isect_data_setup(Frustum shape)
{
  vec3 A1 = shape.corners[1] - shape.corners[0];
  vec3 A3 = shape.corners[3] - shape.corners[0];
  vec3 A4 = shape.corners[4] - shape.corners[0];
  vec3 B5 = shape.corners[5] - shape.corners[6];
  vec3 B7 = shape.corners[7] - shape.corners[6];
  vec3 B2 = shape.corners[2] - shape.corners[6];

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

#ifdef COMMON_VIEW_LIB_GLSL

bool intersect_view(Pyramid pyramid)
{
  bool intersects = true;

  /* Do Pyramid vertices vs Frustum planes. */
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 5; ++v) {
      float test = dot(drw_view_culling.frustum_planes.planes[p], vec4(pyramid.corners[v], 1.0));
      if (test > 0.0) {
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
  IsectPyramid i_pyramid = isect_data_setup(pyramid);
  for (int p = 0; p < 5; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(i_pyramid.planes[p],
                       vec4(drw_view_culling.frustum_corners.corners[v].xyz, 1.0));
      if (test > 0.0) {
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

bool intersect_view(Box box)
{
  bool intersects = true;

  /* Do Box vertices vs Frustum planes. */
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(drw_view_culling.frustum_planes.planes[p], vec4(box.corners[v], 1.0));
      if (test > 0.0) {
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
  IsectBox i_box = isect_data_setup(box);
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(i_box.planes[p],
                       vec4(drw_view_culling.frustum_corners.corners[v].xyz, 1.0));
      if (test > 0.0) {
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

bool intersect_view(IsectBox i_box)
{
  bool intersects = true;

  /* Do Box vertices vs Frustum planes. */
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(drw_view_culling.frustum_planes.planes[p], vec4(i_box.corners[v], 1.0));
      if (test > 0.0) {
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
      float test = dot(i_box.planes[p],
                       vec4(drw_view_culling.frustum_corners.corners[v].xyz, 1.0));
      if (test > 0.0) {
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

bool intersect_view(Sphere sphere)
{
  bool intersects = true;

  for (int p = 0; p < 6 && intersects; ++p) {
    float dist_to_plane = dot(drw_view_culling.frustum_planes.planes[p], vec4(sphere.center, 1.0));
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
      float test = dot(i_pyramid.planes[p], vec4(box.corners[v], 1.0));
      if (test > 0.0) {
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
  IsectBox i_box = isect_data_setup(box);
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 5; ++v) {
      float test = dot(i_box.planes[p], vec4(i_pyramid.corners[v], 1.0));
      if (test > 0.0) {
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
      float test = dot(i_pyramid.planes[p], vec4(i_box.corners[v], 1.0));
      if (test > 0.0) {
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
      float test = dot(i_box.planes[p], vec4(i_pyramid.corners[v], 1.0));
      if (test > 0.0) {
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
      float test = dot(i_frustum.planes[p], vec4(pyramid.corners[v], 1.0));
      if (test > 0.0) {
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
  IsectPyramid i_pyramid = isect_data_setup(pyramid);
  for (int p = 0; p < 5; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(i_pyramid.planes[p], vec4(i_frustum.corners[v].xyz, 1.0));
      if (test > 0.0) {
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
      float test = dot(i_frustum.planes[p], vec4(box.corners[v], 1.0));
      if (test > 0.0) {
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
  IsectBox i_box = isect_data_setup(box);
  for (int p = 0; p < 6; ++p) {
    bool is_any_vertex_on_positive_side = false;
    for (int v = 0; v < 8; ++v) {
      float test = dot(i_box.planes[p], vec4(i_frustum.corners[v].xyz, 1.0));
      if (test > 0.0) {
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
    float dist_to_plane = dot(i_frustum.planes[p], vec4(sphere.center, 1.0));
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
  float sphere_cos = sqrt(1.0 - sphere_sin * sphere_sin);
  float cone_aperture_sin = sqrt(1.0 - cone.angle_cos * cone.angle_cos);

  float cone_sphere_center_cos = dot(sphere.center * sphere_distance_rcp, cone.direction);
  /* cos(A+B) = cos(A) * cos(B) - sin(A) * sin(B). */
  float cone_sphere_angle_sum_cos = (sphere.radius > sphere_distance) ?
                                        -1.0 :
                                        (cone.angle_cos * sphere_cos -
                                         cone_aperture_sin * sphere_sin);
  /* Comparing cosines instead of angles since we are interested
   * only in the monotonic region [0 .. M_PI / 2]. This saves costly acos() calls. */
  bool intersects = (cone_sphere_center_cos >= cone_sphere_angle_sum_cos);

  return intersects;
}

bool intersect(Circle circle_a, Circle circle_b)
{
  return distance_squared(circle_a.center, circle_b.center) <
         sqr(circle_a.radius + circle_b.radius);
}

/** \} */
