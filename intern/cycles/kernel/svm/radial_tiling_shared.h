/* SPDX-FileCopyrightText: 2024-2025 Blender Authors
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* The following files are always to be kept as exact copies of each other:
 * radial_tiling_shared.hh
 * node_radial_tiling_shared.h
 * radial_tiling_shared.h
 * gpu_shader_material_radial_tiling_shared.glsl */

/* The SVM implementation is used as the base shared version because multiple math function
 * identifiers are already used as macros in the SVM code, making a code adaption into an SVM
 * implementation using macros impossible. */

/* Define macros for code adaption. */
#ifdef ADAPT_TO_GEOMETRY_NODES
#  define atanf math::atan
#  define atan2f math::atan2
#  define ceilf math::ceil
#  define cosf math::cos
#  define fabsf math::abs
#  define floorf math::floor
#  define fmaxf math::max
#  define fminf math::min
#  define fractf math::fract
#  define mix math::interpolate
#  define sinf math::sin
#  define sqrtf math::sqrt
#  define sqr math::square
#  define tanf math::tan

#  define make_float2 float2
#  define make_float4 float4
#  define M_PI_F M_PI
#  define M_2PI_F M_TAU
#  define ccl_device
#else
#  ifdef ADAPT_TO_OSL
#    define atanf atan
#    define atan2f atan2
#    define ceilf ceil
#    define cosf cos
#    define fabsf abs
#    define floorf floor
#    define fmaxf max
#    define fminf min
#    define fractf fract
#    define mix mix
#    define sinf sin
#    define sqrtf sqrt
#    define sqr sqr
#    define tanf tan

#    define bool int
#    define float2 vector2
#    define float4 vector4
#    define make_float2 vector2
#    define make_float4 vector4
#    define M_PI_F M_PI
#    define M_2PI_F M_2PI
#    define ccl_device

#    define false 0
#    define true 1
#  else
#    ifdef ADAPT_TO_SVM
/* No code adaption necessary for the SVM implementation as it is the base shared version. */
#    else
/* Adapt code to GLSL by default. */
#      define atanf atan
#      define atan2f atan2
#      define ceilf ceil
#      define cosf cos
#      define fabsf abs
#      define floorf floor
#      define fmaxf max
#      define fminf min
#      define fractf fract
#      define mix mix
#      define sinf sin
#      define sqrtf sqrt
#      define sqr square
#      define tanf tan

#      define make_float2 float2
#      define make_float4 float4
#      define M_PI_F M_PI
#      define M_2PI_F M_TAU
#      define ccl_device
#    endif
#  endif
#endif

/* The geometry nodes has optional specialized functions for certain combinations of input values.
 * These specialized functions output the same values as the general functions, but are faster when
 * they can be used. To reduce the complexity on the GPU kernel while also keeping all rendering
 * implementations the same, they are only enabled in the geometry nodes implementation. */
#ifdef ADAPT_TO_GEOMETRY_NODES
#  define ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(X) (X)
#else
#  define ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(X) true
#endif

/* Naming convention for the Radial Tiling node code:
 * Let x and y be 2D vectors.
 * The length of X is expressed as l_x, which is an abbreviation of length_x.
 * The counterclockwise unsinged angle in [0.0, M_TAU] from X to Y is expressed as x_A_y, which
 * is an abbreviation of x_Angle_y. The singed angle in [-M_PI, M_PI] from x to y is expressed
 * as x_SA_y, which is an abbreviation of x_SingedAngle_y. Counterclockwise angles are positive,
 * clockwise angles are negative. A signed angle from x to y of which the output is mirrored along
 * a certain vector is expressed as x_MSA_y, which is an abbreviation of x_MirroredSingedAngle_y.
 *
 * Let z and w be scalars.
 * The ratio z/w is expressed as z_R_w, which is an abbreviation of z_Ratio_y. */

#ifdef ADAPT_TO_GEOMETRY_NODES
ccl_device float4
calculate_out_variables_full_roundness_irregular_circular(bool calculate_r_gon_parameter_field,
                                                          bool normalize_r_gon_parameter,
                                                          float r_gon_sides,
                                                          float2 coord,
                                                          float l_coord)
{
  float x_axis_A_coord = atan2f(coord.y, coord.x) + float(coord.y < float(0.0)) * M_2PI_F;
  float segment_divider_A_angle_bisector = M_PI_F / r_gon_sides;
  float segment_divider_A_next_segment_divider = float(2.0) * segment_divider_A_angle_bisector;
  float segment_id = floorf(x_axis_A_coord / segment_divider_A_next_segment_divider);
  float segment_divider_A_coord = x_axis_A_coord -
                                  segment_id * segment_divider_A_next_segment_divider;

  float last_angle_bisector_A_x_axis = M_PI_F -
                                       floorf(r_gon_sides) * segment_divider_A_angle_bisector;
  float last_segment_divider_A_x_axis = float(2.0) * last_angle_bisector_A_x_axis;
  float l_last_circle_radius = tanf(last_angle_bisector_A_x_axis) /
                               tanf(float(0.5) * (segment_divider_A_angle_bisector +
                                                  last_angle_bisector_A_x_axis));
  float2 last_circle_center = make_float2(
      cosf(last_angle_bisector_A_x_axis) -
          l_last_circle_radius * cosf(last_angle_bisector_A_x_axis),
      l_last_circle_radius * sinf(last_angle_bisector_A_x_axis) -
          sinf(last_angle_bisector_A_x_axis));
  float2 outer_last_bevel_start = last_circle_center +
                                  l_last_circle_radius *
                                      make_float2(cosf(segment_divider_A_angle_bisector),
                                                  sinf(segment_divider_A_angle_bisector));
  float x_axis_A_outer_last_bevel_start = atanf(outer_last_bevel_start.y /
                                                outer_last_bevel_start.x);
  float outer_last_bevel_start_A_angle_bisector = segment_divider_A_angle_bisector -
                                                  x_axis_A_outer_last_bevel_start;

  if ((x_axis_A_coord >= x_axis_A_outer_last_bevel_start) &&
      (x_axis_A_coord < M_2PI_F - last_segment_divider_A_x_axis - x_axis_A_outer_last_bevel_start))
  {
    if ((x_axis_A_coord >=
         M_2PI_F - last_segment_divider_A_x_axis - segment_divider_A_angle_bisector) ||
        (x_axis_A_coord < segment_divider_A_angle_bisector))
    {
      /* Regular straight part. */

      float l_angle_bisector = float(0.0);
      float r_gon_parameter = float(0.0);

      l_angle_bisector = l_coord *
                         cosf(segment_divider_A_angle_bisector - segment_divider_A_coord);

      float effective_roundness = float(1.0) - tanf(segment_divider_A_angle_bisector -
                                                    x_axis_A_outer_last_bevel_start) /
                                                   tanf(segment_divider_A_angle_bisector);
      float spline_start_outer_last_bevel_start = (float(1.0) - effective_roundness) *
                                                  x_axis_A_outer_last_bevel_start;

      if (calculate_r_gon_parameter_field) {
        r_gon_parameter = l_angle_bisector *
                          tanf(fabsf(segment_divider_A_angle_bisector - segment_divider_A_coord));
        if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
          r_gon_parameter *= -float(1.0);
        }
        if (normalize_r_gon_parameter) {
          float normalize_based_on_l_angle_bisector =
              l_angle_bisector * tanf(outer_last_bevel_start_A_angle_bisector) +
              spline_start_outer_last_bevel_start * (float(0.5) * l_angle_bisector + float(0.5)) +
              effective_roundness * x_axis_A_outer_last_bevel_start;

          r_gon_parameter /= normalize_based_on_l_angle_bisector;
        }
      }
      return make_float4(l_angle_bisector,
                         r_gon_parameter,
                         segment_divider_A_angle_bisector,
                         segment_id * segment_divider_A_next_segment_divider +
                             segment_divider_A_angle_bisector);
    }
    else {
      /* Regular rounded part. */

      float r_gon_parameter = float(0.0);
      if (calculate_r_gon_parameter_field) {
        r_gon_parameter = fabsf(segment_divider_A_angle_bisector - segment_divider_A_coord);
        if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
          r_gon_parameter *= -float(1.0);
        }
        if (normalize_r_gon_parameter) {
          r_gon_parameter /= segment_divider_A_angle_bisector;
        }
      }
      return make_float4(l_coord,
                         r_gon_parameter,
                         segment_divider_A_angle_bisector,
                         segment_id * segment_divider_A_next_segment_divider +
                             segment_divider_A_angle_bisector);
    }
  }
  else {
    /* Irregular rounded part. */

    /* MSA == Mirrored Signed Angle. The values are mirrored around the last angle bisector
     * to avoid a case distinction. */
    float nearest_segment_divider_MSA_coord = atan2f(coord.y, coord.x);
    if ((x_axis_A_coord >=
         M_2PI_F - last_segment_divider_A_x_axis - x_axis_A_outer_last_bevel_start) &&
        (x_axis_A_coord < M_2PI_F - last_angle_bisector_A_x_axis))
    {
      nearest_segment_divider_MSA_coord += last_segment_divider_A_x_axis;
      nearest_segment_divider_MSA_coord *= -float(1.0);
    }
    float l_angle_bisector = float(0.0);
    float r_gon_parameter = float(0.0);
    float max_unit_parameter = float(0.0);
    float x_axis_A_angle_bisector = float(0.0);

    float l_coord_R_l_last_angle_bisector =
        sinf(nearest_segment_divider_MSA_coord) * last_circle_center.y +
        cosf(nearest_segment_divider_MSA_coord) * last_circle_center.x +
        sqrtf(sqr(sinf(nearest_segment_divider_MSA_coord) * last_circle_center.y +
                  cosf(nearest_segment_divider_MSA_coord) * last_circle_center.x) +
              sqr(l_last_circle_radius) - sqr(last_circle_center.x) - sqr(last_circle_center.y));
    float l_angle_bisector_R_l_last_angle_bisector = cosf(segment_divider_A_angle_bisector) /
                                                     cosf(last_angle_bisector_A_x_axis);

    l_angle_bisector = l_angle_bisector_R_l_last_angle_bisector * l_coord /
                       l_coord_R_l_last_angle_bisector;

    if (nearest_segment_divider_MSA_coord < float(0.0)) {
      /* Irregular rounded inner part. */

      if (calculate_r_gon_parameter_field) {
        r_gon_parameter = l_angle_bisector_R_l_last_angle_bisector *
                          (last_angle_bisector_A_x_axis + nearest_segment_divider_MSA_coord);
        if (segment_divider_A_coord < last_angle_bisector_A_x_axis) {
          r_gon_parameter *= -float(1.0);
        }
        if (normalize_r_gon_parameter) {
          r_gon_parameter /= l_angle_bisector_R_l_last_angle_bisector *
                             last_angle_bisector_A_x_axis;
        }
      }
      max_unit_parameter = l_angle_bisector_R_l_last_angle_bisector * last_angle_bisector_A_x_axis;
      x_axis_A_angle_bisector = segment_id * segment_divider_A_next_segment_divider +
                                last_angle_bisector_A_x_axis;
    }
    else {
      /* Irregular rounded outer part. */

      float effective_roundness = float(1.0) - tanf(segment_divider_A_angle_bisector -
                                                    x_axis_A_outer_last_bevel_start) /
                                                   tanf(segment_divider_A_angle_bisector);
      float spline_start_outer_last_bevel_start = (float(1.0) - effective_roundness) *
                                                  x_axis_A_outer_last_bevel_start;

      if (calculate_r_gon_parameter_field) {
        float coord_A_bevel_start = x_axis_A_outer_last_bevel_start -
                                    fabsf(nearest_segment_divider_MSA_coord);
        r_gon_parameter = l_coord * sinf(outer_last_bevel_start_A_angle_bisector);

        if (coord_A_bevel_start < spline_start_outer_last_bevel_start) {
          r_gon_parameter +=
              l_coord * cosf(outer_last_bevel_start_A_angle_bisector) * coord_A_bevel_start +
              float(0.5) * (float(1.0) - l_coord * cosf(outer_last_bevel_start_A_angle_bisector)) *
                  sqr(coord_A_bevel_start) / spline_start_outer_last_bevel_start;
        }
        else {
          r_gon_parameter += spline_start_outer_last_bevel_start *
                                 (float(0.5) * l_coord *
                                      cosf(outer_last_bevel_start_A_angle_bisector) -
                                  float(0.5)) +
                             coord_A_bevel_start;
        }
        if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
          r_gon_parameter *= -float(1.0);
        }
        if (normalize_r_gon_parameter) {
          float normalize_based_on_l_angle_bisector =
              l_angle_bisector * tanf(outer_last_bevel_start_A_angle_bisector) +
              spline_start_outer_last_bevel_start * (float(0.5) * l_angle_bisector + float(0.5)) +
              effective_roundness * x_axis_A_outer_last_bevel_start;
          float normalize_based_on_l_coord =
              l_coord * sinf(outer_last_bevel_start_A_angle_bisector) +
              spline_start_outer_last_bevel_start *
                  (float(0.5) * l_coord * cosf(outer_last_bevel_start_A_angle_bisector) +
                   float(0.5)) +
              effective_roundness * x_axis_A_outer_last_bevel_start;

          /* For effective_roundness -> 1.0 the normalize_based_on_l_angle_bisector field and
           * normalize_based_on_l_coord field converge against the same scalar field. */
          r_gon_parameter /= mix(normalize_based_on_l_angle_bisector,
                                 normalize_based_on_l_coord,
                                 coord_A_bevel_start / x_axis_A_outer_last_bevel_start);
        }
      }
      max_unit_parameter = segment_divider_A_angle_bisector;
      x_axis_A_angle_bisector = segment_id * segment_divider_A_next_segment_divider +
                                segment_divider_A_angle_bisector;
    }
    return make_float4(
        l_angle_bisector, r_gon_parameter, max_unit_parameter, x_axis_A_angle_bisector);
  }
}
#endif

ccl_device float4 calculate_out_variables_irregular_circular(bool calculate_r_gon_parameter_field,
                                                             bool calculate_max_unit_parameter,
                                                             bool normalize_r_gon_parameter,
                                                             float r_gon_sides,
                                                             float r_gon_roundness,
                                                             float2 coord,
                                                             float l_coord)
{
#ifdef ADAPT_TO_SVM
  /* Silence compiler warnings. */
  (void)calculate_r_gon_parameter_field;
  (void)calculate_max_unit_parameter;
#endif

  float x_axis_A_coord = atan2f(coord.y, coord.x) + float(coord.y < float(0.0)) * M_2PI_F;
  float segment_divider_A_angle_bisector = M_PI_F / r_gon_sides;
  float segment_divider_A_next_segment_divider = float(2.0) * segment_divider_A_angle_bisector;
  float segment_id = floorf(x_axis_A_coord / segment_divider_A_next_segment_divider);
  float segment_divider_A_coord = x_axis_A_coord -
                                  segment_id * segment_divider_A_next_segment_divider;
  float segment_divider_A_bevel_start = segment_divider_A_angle_bisector -
                                        atanf((float(1.0) - r_gon_roundness) *
                                              tanf(segment_divider_A_angle_bisector));

  float last_angle_bisector_A_x_axis = M_PI_F -
                                       floorf(r_gon_sides) * segment_divider_A_angle_bisector;
  float last_segment_divider_A_x_axis = float(2.0) * last_angle_bisector_A_x_axis;
  float inner_last_bevel_start_A_x_axis = last_angle_bisector_A_x_axis -
                                          atanf((float(1.0) - r_gon_roundness) *
                                                tanf(last_angle_bisector_A_x_axis));
  float l_last_circle_radius = r_gon_roundness * tanf(last_angle_bisector_A_x_axis) /
                               tanf(float(0.5) * (segment_divider_A_angle_bisector +
                                                  last_angle_bisector_A_x_axis));
  float2 last_circle_center = make_float2(
      (cosf(inner_last_bevel_start_A_x_axis) /
       cosf(last_angle_bisector_A_x_axis - inner_last_bevel_start_A_x_axis)) -
          l_last_circle_radius * cosf(last_angle_bisector_A_x_axis),
      l_last_circle_radius * sinf(last_angle_bisector_A_x_axis) -
          (sinf(inner_last_bevel_start_A_x_axis) /
           cosf(last_angle_bisector_A_x_axis - inner_last_bevel_start_A_x_axis)));
  float2 outer_last_bevel_start = last_circle_center +
                                  l_last_circle_radius *
                                      make_float2(cosf(segment_divider_A_angle_bisector),
                                                  sinf(segment_divider_A_angle_bisector));
  float x_axis_A_outer_last_bevel_start = atanf(outer_last_bevel_start.y /
                                                outer_last_bevel_start.x);
  float outer_last_bevel_start_A_angle_bisector = segment_divider_A_angle_bisector -
                                                  x_axis_A_outer_last_bevel_start;

  if ((x_axis_A_coord >= x_axis_A_outer_last_bevel_start) &&
      (x_axis_A_coord < M_2PI_F - last_segment_divider_A_x_axis - x_axis_A_outer_last_bevel_start))
  {
    float bevel_start_A_angle_bisector = segment_divider_A_angle_bisector -
                                         segment_divider_A_bevel_start;

    if (((segment_divider_A_coord >= segment_divider_A_bevel_start) &&
         (segment_divider_A_coord <
          segment_divider_A_next_segment_divider - segment_divider_A_bevel_start)) ||
        (x_axis_A_coord >=
         M_2PI_F - last_segment_divider_A_x_axis - segment_divider_A_bevel_start) ||
        (x_axis_A_coord < segment_divider_A_bevel_start))
    {
      /* Regular straight part. */

      float l_angle_bisector = float(0.0);
      float r_gon_parameter = float(0.0);
      float max_unit_parameter = float(0.0);

      l_angle_bisector = l_coord *
                         cosf(segment_divider_A_angle_bisector - segment_divider_A_coord);

      float spline_start_A_bevel_start = (float(1.0) - r_gon_roundness) *
                                         segment_divider_A_bevel_start;

      if ((x_axis_A_coord >=
           M_2PI_F - last_segment_divider_A_x_axis - segment_divider_A_angle_bisector) ||
          (x_axis_A_coord < segment_divider_A_angle_bisector))
      {
        /* Irregular rounded outer part. */

        float effective_roundness = float(1.0) - tanf(segment_divider_A_angle_bisector -
                                                      x_axis_A_outer_last_bevel_start) /
                                                     tanf(segment_divider_A_angle_bisector);
        float spline_start_outer_last_bevel_start = (float(1.0) - effective_roundness) *
                                                    x_axis_A_outer_last_bevel_start;

        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
          r_gon_parameter = l_angle_bisector * tanf(fabsf(segment_divider_A_angle_bisector -
                                                          segment_divider_A_coord));
          if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
            r_gon_parameter *= -float(1.0);
          }
          if (normalize_r_gon_parameter) {
            float normalize_based_on_l_angle_bisector =
                l_angle_bisector * tanf(outer_last_bevel_start_A_angle_bisector) +
                spline_start_outer_last_bevel_start *
                    (float(0.5) * l_angle_bisector + float(0.5)) +
                effective_roundness * x_axis_A_outer_last_bevel_start;

            r_gon_parameter /= normalize_based_on_l_angle_bisector;
          }
        }
      }
      else {
        /* Regular straight part. */

        float spline_start_A_bevel_start = (float(1.0) - r_gon_roundness) *
                                           segment_divider_A_bevel_start;

        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
          r_gon_parameter = l_angle_bisector * tanf(fabsf(segment_divider_A_angle_bisector -
                                                          segment_divider_A_coord));
          if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
            r_gon_parameter *= -float(1.0);
          }
          if (normalize_r_gon_parameter) {
            float normalize_based_on_l_angle_bisector =
                l_angle_bisector * tanf(bevel_start_A_angle_bisector) +
                spline_start_A_bevel_start * (float(0.5) * l_angle_bisector + float(0.5)) +
                r_gon_roundness * segment_divider_A_bevel_start;

            r_gon_parameter /= normalize_based_on_l_angle_bisector;
          }
        }
      }

      if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
        r_gon_parameter = l_angle_bisector *
                          tanf(fabsf(segment_divider_A_angle_bisector - segment_divider_A_coord));
        if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
          r_gon_parameter *= -float(1.0);
        }

        if (normalize_r_gon_parameter) {
          if ((x_axis_A_coord >=
               M_2PI_F - last_segment_divider_A_x_axis - segment_divider_A_angle_bisector) ||
              (x_axis_A_coord < segment_divider_A_angle_bisector))
          {
            /* Irregular rounded outer part. */

            float effective_roundness = float(1.0) - tanf(segment_divider_A_angle_bisector -
                                                          x_axis_A_outer_last_bevel_start) /
                                                         tanf(segment_divider_A_angle_bisector);
            float spline_start_outer_last_bevel_start = (float(1.0) - effective_roundness) *
                                                        x_axis_A_outer_last_bevel_start;

            float normalize_based_on_l_angle_bisector =
                l_angle_bisector * tanf(outer_last_bevel_start_A_angle_bisector) +
                spline_start_outer_last_bevel_start *
                    (float(0.5) * l_angle_bisector + float(0.5)) +
                effective_roundness * x_axis_A_outer_last_bevel_start;

            r_gon_parameter /= normalize_based_on_l_angle_bisector;
          }
          else {
            /* Regular straight part. */

            float spline_start_A_bevel_start = (float(1.0) - r_gon_roundness) *
                                               segment_divider_A_bevel_start;

            float normalize_based_on_l_angle_bisector =
                l_angle_bisector * tanf(bevel_start_A_angle_bisector) +
                spline_start_A_bevel_start * (float(0.5) * l_angle_bisector + float(0.5)) +
                r_gon_roundness * segment_divider_A_bevel_start;

            r_gon_parameter /= normalize_based_on_l_angle_bisector;
          }
        }
      }
      if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_max_unit_parameter)) {
        max_unit_parameter = tanf(bevel_start_A_angle_bisector) + spline_start_A_bevel_start +
                             r_gon_roundness * segment_divider_A_bevel_start;
      }
      return make_float4(l_angle_bisector,
                         r_gon_parameter,
                         max_unit_parameter,
                         segment_id * segment_divider_A_next_segment_divider +
                             segment_divider_A_angle_bisector);
    }
    else {
      /* Regular rounded part. */

      /* SA == Signed Angle in [-M_PI, M_PI]. Counterclockwise angles are positive, clockwise
       * angles are negative.*/
      float nearest_segment_divider_SA_coord = segment_divider_A_coord -
                                               float(segment_divider_A_coord >
                                                     segment_divider_A_angle_bisector) *
                                                   segment_divider_A_next_segment_divider;
      float l_angle_bisector = float(0.0);
      float r_gon_parameter = float(0.0);
      float max_unit_parameter = float(0.0);

      float l_circle_center = (float(1.0) - r_gon_roundness) /
                              cosf(segment_divider_A_angle_bisector);
      float l_coord_R_l_bevel_start = cosf(nearest_segment_divider_SA_coord) * l_circle_center +
                                      sqrtf(sqr(cosf(nearest_segment_divider_SA_coord) *
                                                l_circle_center) +
                                            sqr(r_gon_roundness) - sqr(l_circle_center));

      l_angle_bisector = l_coord / l_coord_R_l_bevel_start;

      float spline_start_A_bevel_start = (float(1.0) - r_gon_roundness) *
                                         segment_divider_A_bevel_start;

      if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
        float coord_A_bevel_start = segment_divider_A_bevel_start -
                                    fabsf(nearest_segment_divider_SA_coord);
        r_gon_parameter = l_coord * sinf(bevel_start_A_angle_bisector);

        if (coord_A_bevel_start < spline_start_A_bevel_start) {
          r_gon_parameter += l_coord * cosf(bevel_start_A_angle_bisector) * coord_A_bevel_start +
                             float(0.5) *
                                 (float(1.0) - l_coord * cosf(bevel_start_A_angle_bisector)) *
                                 sqr(coord_A_bevel_start) / spline_start_A_bevel_start;
        }
        else {
          r_gon_parameter += spline_start_A_bevel_start *
                                 (float(0.5) * l_coord * cosf(bevel_start_A_angle_bisector) -
                                  float(0.5)) +
                             coord_A_bevel_start;
        }
        if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
          r_gon_parameter *= -float(1.0);
        }
        if (normalize_r_gon_parameter) {
          float normalize_based_on_l_angle_bisector =
              l_angle_bisector * tanf(bevel_start_A_angle_bisector) +
              spline_start_A_bevel_start * (float(0.5) * l_angle_bisector + float(0.5)) +
              r_gon_roundness * segment_divider_A_bevel_start;
          float normalize_based_on_l_coord =
              l_coord * sinf(bevel_start_A_angle_bisector) +
              spline_start_A_bevel_start *
                  (float(0.5) * l_coord * cosf(bevel_start_A_angle_bisector) + float(0.5)) +
              r_gon_roundness * segment_divider_A_bevel_start;

          /* For r_gon_roundness -> 1.0 the normalize_based_on_l_angle_bisector field and
           * normalize_based_on_l_coord field converge against the same scalar field. */
          r_gon_parameter /= mix(normalize_based_on_l_angle_bisector,
                                 normalize_based_on_l_coord,
                                 coord_A_bevel_start / segment_divider_A_bevel_start);
        }
      }
      if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_max_unit_parameter)) {
        max_unit_parameter = tanf(bevel_start_A_angle_bisector) + spline_start_A_bevel_start +
                             r_gon_roundness * segment_divider_A_bevel_start;
      }
      return make_float4(l_angle_bisector,
                         r_gon_parameter,
                         max_unit_parameter,
                         segment_id * segment_divider_A_next_segment_divider +
                             segment_divider_A_angle_bisector);
    }
  }
  else {
    float inner_last_bevel_start_A_last_angle_bisector = last_angle_bisector_A_x_axis -
                                                         inner_last_bevel_start_A_x_axis;

    if ((x_axis_A_coord >=
         M_2PI_F - last_segment_divider_A_x_axis + inner_last_bevel_start_A_x_axis) &&
        (x_axis_A_coord < M_2PI_F - inner_last_bevel_start_A_x_axis))
    {
      /* Irregular straight part. */

      float l_angle_bisector = float(0.0);
      float r_gon_parameter = float(0.0);
      float max_unit_parameter = float(0.0);

      float l_angle_bisector_R_l_last_angle_bisector = cosf(segment_divider_A_angle_bisector) /
                                                       cosf(last_angle_bisector_A_x_axis);
      float l_last_angle_bisector = l_coord *
                                    cosf(last_angle_bisector_A_x_axis - segment_divider_A_coord);

      l_angle_bisector = l_angle_bisector_R_l_last_angle_bisector * l_last_angle_bisector;

      float spline_start_A_bevel_start = (float(1.0) - r_gon_roundness) *
                                         inner_last_bevel_start_A_x_axis;

      if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
        r_gon_parameter = l_angle_bisector_R_l_last_angle_bisector * l_last_angle_bisector *
                          tanf(fabsf(last_angle_bisector_A_x_axis - segment_divider_A_coord));
        if (segment_divider_A_coord < last_angle_bisector_A_x_axis) {
          r_gon_parameter *= -float(1.0);
        }
        if (normalize_r_gon_parameter) {
          float normalize_based_on_l_l_angle_bisector =
              l_angle_bisector_R_l_last_angle_bisector *
              (l_last_angle_bisector * tanf(inner_last_bevel_start_A_last_angle_bisector) +
               spline_start_A_bevel_start * (float(0.5) * l_last_angle_bisector + float(0.5)) +
               r_gon_roundness * inner_last_bevel_start_A_x_axis);

          r_gon_parameter /= normalize_based_on_l_l_angle_bisector;
        }
      }
      if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_max_unit_parameter)) {
        max_unit_parameter = tanf(inner_last_bevel_start_A_last_angle_bisector) +
                             l_angle_bisector_R_l_last_angle_bisector *
                                 (spline_start_A_bevel_start *
                                      ((float(0.5) / l_angle_bisector_R_l_last_angle_bisector) +
                                       float(0.5)) +
                                  r_gon_roundness * inner_last_bevel_start_A_x_axis);
      }
      return make_float4(l_angle_bisector,
                         r_gon_parameter,
                         max_unit_parameter,
                         segment_id * segment_divider_A_next_segment_divider +
                             last_angle_bisector_A_x_axis);
    }
    else {
      /* Irregular rounded part. */

      /* MSA == Mirrored Signed Angle. The values are mirrored around the last angle bisector
       * to avoid a case distinction. */
      float nearest_segment_divider_MSA_coord = atan2f(coord.y, coord.x);
      if ((x_axis_A_coord >=
           M_2PI_F - last_segment_divider_A_x_axis - x_axis_A_outer_last_bevel_start) &&
          (x_axis_A_coord < M_2PI_F - last_angle_bisector_A_x_axis))
      {
        nearest_segment_divider_MSA_coord += last_segment_divider_A_x_axis;
        nearest_segment_divider_MSA_coord *= -float(1.0);
      }
      float l_angle_bisector = float(0.0);
      float r_gon_parameter = float(0.0);
      float max_unit_parameter = float(0.0);
      float x_axis_A_angle_bisector = float(0.0);

      float l_coord_R_l_last_angle_bisector =
          sinf(nearest_segment_divider_MSA_coord) * last_circle_center.y +
          cosf(nearest_segment_divider_MSA_coord) * last_circle_center.x +
          sqrtf(sqr(sinf(nearest_segment_divider_MSA_coord) * last_circle_center.y +
                    cosf(nearest_segment_divider_MSA_coord) * last_circle_center.x) +
                sqr(l_last_circle_radius) - sqr(last_circle_center.x) - sqr(last_circle_center.y));
      float l_angle_bisector_R_l_last_angle_bisector = cosf(segment_divider_A_angle_bisector) /
                                                       cosf(last_angle_bisector_A_x_axis);
      float l_last_angle_bisector = l_coord / l_coord_R_l_last_angle_bisector;

      l_angle_bisector = l_angle_bisector_R_l_last_angle_bisector * l_last_angle_bisector;

      if (nearest_segment_divider_MSA_coord < float(0.0)) {
        /* Irregular rounded inner part. */

        float spline_start_A_bevel_start = (float(1.0) - r_gon_roundness) *
                                           inner_last_bevel_start_A_x_axis;

        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
          float coord_A_bevel_start = inner_last_bevel_start_A_x_axis -
                                      fabsf(nearest_segment_divider_MSA_coord);
          r_gon_parameter = l_angle_bisector_R_l_last_angle_bisector * l_coord *
                            sinf(inner_last_bevel_start_A_last_angle_bisector);

          if (coord_A_bevel_start < spline_start_A_bevel_start) {
            r_gon_parameter +=
                l_angle_bisector_R_l_last_angle_bisector *
                (l_coord * cosf(inner_last_bevel_start_A_last_angle_bisector) *
                     coord_A_bevel_start +
                 float(0.5) *
                     (float(1.0) - l_coord * cosf(inner_last_bevel_start_A_last_angle_bisector)) *
                     sqr(coord_A_bevel_start) / spline_start_A_bevel_start);
          }
          else {
            r_gon_parameter += l_angle_bisector_R_l_last_angle_bisector *
                               (spline_start_A_bevel_start *
                                    (float(0.5) * l_coord *
                                         cosf(inner_last_bevel_start_A_last_angle_bisector) -
                                     float(0.5)) +
                                coord_A_bevel_start);
          }
          if (segment_divider_A_coord < last_angle_bisector_A_x_axis) {
            r_gon_parameter *= -float(1.0);
          }
          if (normalize_r_gon_parameter) {
            float normalize_based_on_l_l_angle_bisector =
                l_last_angle_bisector * tanf(inner_last_bevel_start_A_last_angle_bisector) +
                spline_start_A_bevel_start * (float(0.5) * l_last_angle_bisector + float(0.5)) +
                r_gon_roundness * inner_last_bevel_start_A_x_axis;
            float normalize_based_on_l_coord =
                l_coord * sinf(inner_last_bevel_start_A_last_angle_bisector) +
                spline_start_A_bevel_start *
                    (float(0.5) * l_coord * cosf(inner_last_bevel_start_A_last_angle_bisector) +
                     float(0.5)) +
                r_gon_roundness * inner_last_bevel_start_A_x_axis;

            /* For r_gon_roundness -> 1.0 the normalize_based_on_l_l_angle_bisector field and
             * normalize_based_on_l_coord field converge against the same scalar field. */
            r_gon_parameter /= l_angle_bisector_R_l_last_angle_bisector *
                               mix(normalize_based_on_l_l_angle_bisector,
                                   normalize_based_on_l_coord,
                                   coord_A_bevel_start / inner_last_bevel_start_A_x_axis);
          }
        }
        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_max_unit_parameter)) {
          max_unit_parameter = tanf(inner_last_bevel_start_A_last_angle_bisector) +
                               l_angle_bisector_R_l_last_angle_bisector *
                                   (spline_start_A_bevel_start *
                                        ((float(0.5) / l_angle_bisector_R_l_last_angle_bisector) +
                                         float(0.5)) +
                                    r_gon_roundness * inner_last_bevel_start_A_x_axis);
        }
        x_axis_A_angle_bisector = segment_id * segment_divider_A_next_segment_divider +
                                  last_angle_bisector_A_x_axis;
      }
      else {
        /* Irregular rounded outer part. */

        float effective_roundness = float(1.0) - tanf(segment_divider_A_angle_bisector -
                                                      x_axis_A_outer_last_bevel_start) /
                                                     tanf(segment_divider_A_angle_bisector);
        float spline_start_outer_last_bevel_start = (float(1.0) - effective_roundness) *
                                                    x_axis_A_outer_last_bevel_start;

        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
          float coord_A_bevel_start = x_axis_A_outer_last_bevel_start -
                                      fabsf(nearest_segment_divider_MSA_coord);
          r_gon_parameter = l_coord * sinf(outer_last_bevel_start_A_angle_bisector);

          if (coord_A_bevel_start < spline_start_outer_last_bevel_start) {
            r_gon_parameter += l_coord * cosf(outer_last_bevel_start_A_angle_bisector) *
                                   coord_A_bevel_start +
                               float(0.5) *
                                   (float(1.0) -
                                    l_coord * cosf(outer_last_bevel_start_A_angle_bisector)) *
                                   sqr(coord_A_bevel_start) / spline_start_outer_last_bevel_start;
          }
          else {
            r_gon_parameter += spline_start_outer_last_bevel_start *
                                   (float(0.5) * l_coord *
                                        cosf(outer_last_bevel_start_A_angle_bisector) -
                                    float(0.5)) +
                               coord_A_bevel_start;
          }
          if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
            r_gon_parameter *= -float(1.0);
          }
          if (normalize_r_gon_parameter) {
            float normalize_based_on_l_angle_bisector =
                l_angle_bisector * tanf(outer_last_bevel_start_A_angle_bisector) +
                spline_start_outer_last_bevel_start *
                    (float(0.5) * l_angle_bisector + float(0.5)) +
                effective_roundness * x_axis_A_outer_last_bevel_start;
            float normalize_based_on_l_coord =
                l_coord * sinf(outer_last_bevel_start_A_angle_bisector) +
                spline_start_outer_last_bevel_start *
                    (float(0.5) * l_coord * cosf(outer_last_bevel_start_A_angle_bisector) +
                     float(0.5)) +
                effective_roundness * x_axis_A_outer_last_bevel_start;

            /* For effective_roundness -> 1.0 the normalize_based_on_l_angle_bisector field and
             * normalize_based_on_l_coord field converge against the same scalar field. */
            r_gon_parameter /= mix(normalize_based_on_l_angle_bisector,
                                   normalize_based_on_l_coord,
                                   coord_A_bevel_start / x_axis_A_outer_last_bevel_start);
          }
        }
        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_max_unit_parameter)) {
          float bevel_start_A_angle_bisector = segment_divider_A_angle_bisector -
                                               segment_divider_A_bevel_start;
          float spline_start_A_bevel_start = (float(1.0) - r_gon_roundness) *
                                             segment_divider_A_bevel_start;

          max_unit_parameter = tanf(bevel_start_A_angle_bisector) + spline_start_A_bevel_start +
                               r_gon_roundness * segment_divider_A_bevel_start;
        }
        x_axis_A_angle_bisector = segment_id * segment_divider_A_next_segment_divider +
                                  segment_divider_A_angle_bisector;
      }
      return make_float4(
          l_angle_bisector, r_gon_parameter, max_unit_parameter, x_axis_A_angle_bisector);
    }
  }
}

ccl_device float4 calculate_out_variables(bool calculate_r_gon_parameter_field,
                                          bool calculate_max_unit_parameter,
                                          bool normalize_r_gon_parameter,
                                          float r_gon_sides,
                                          float r_gon_roundness,
                                          float2 coord)
{
#ifdef ADAPT_TO_SVM
  /* Silence compiler warnings. */
  (void)calculate_r_gon_parameter_field;
  (void)calculate_max_unit_parameter;
#endif

  float l_coord = sqrtf(sqr(coord.x) + sqr(coord.y));
  float4 out_variables;

  if (fractf(r_gon_sides) == float(0.0)) {
    float x_axis_A_coord = atan2f(coord.y, coord.x) + float(coord.y < float(0.0)) * M_2PI_F;
    float segment_divider_A_angle_bisector = M_PI_F / r_gon_sides;
    float segment_divider_A_next_segment_divider = float(2.0) * segment_divider_A_angle_bisector;
    float segment_id = floorf(x_axis_A_coord / segment_divider_A_next_segment_divider);
    float segment_divider_A_coord = x_axis_A_coord -
                                    segment_id * segment_divider_A_next_segment_divider;

    if (r_gon_roundness == float(0.0)) {
      /* Regular straight part. */

      float l_angle_bisector = float(0.0);
      float r_gon_parameter = float(0.0);
      float max_unit_parameter = float(0.0);

      l_angle_bisector = l_coord *
                         cosf(segment_divider_A_angle_bisector - segment_divider_A_coord);

      if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
        r_gon_parameter = l_angle_bisector *
                          tanf(fabsf(segment_divider_A_angle_bisector - segment_divider_A_coord));
        if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
          r_gon_parameter *= -float(1.0);
        }
        if (normalize_r_gon_parameter && (r_gon_sides != float(2.0))) {
          r_gon_parameter /= l_angle_bisector * tanf(segment_divider_A_angle_bisector);
        }
      }
      if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_max_unit_parameter)) {
        max_unit_parameter = (r_gon_sides != float(2.0)) ? tanf(segment_divider_A_angle_bisector) :
                                                           float(0.0);
      }
      out_variables = make_float4(l_angle_bisector,
                                  r_gon_parameter,
                                  max_unit_parameter,
                                  segment_id * segment_divider_A_next_segment_divider +
                                      segment_divider_A_angle_bisector);
    }
    else if (r_gon_roundness == float(1.0)) {
      /* Regular rounded part. */

      float r_gon_parameter = float(0.0);
      if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
        r_gon_parameter = fabsf(segment_divider_A_angle_bisector - segment_divider_A_coord);
        if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
          r_gon_parameter *= -float(1.0);
        }
        if (normalize_r_gon_parameter) {
          r_gon_parameter /= segment_divider_A_angle_bisector;
        }
      }
      out_variables = make_float4(l_coord,
                                  r_gon_parameter,
                                  segment_divider_A_angle_bisector,
                                  segment_id * segment_divider_A_next_segment_divider +
                                      segment_divider_A_angle_bisector);
    }
    else {
      float segment_divider_A_bevel_start = segment_divider_A_angle_bisector -
                                            atanf((float(1.0) - r_gon_roundness) *
                                                  tanf(segment_divider_A_angle_bisector));
      float bevel_start_A_angle_bisector = segment_divider_A_angle_bisector -
                                           segment_divider_A_bevel_start;

      if ((segment_divider_A_coord >=
           segment_divider_A_next_segment_divider - segment_divider_A_bevel_start) ||
          (segment_divider_A_coord < segment_divider_A_bevel_start))
      {
        /* Regular rounded part. */

        /* SA == Signed Angle in [-M_PI, M_PI]. Counterclockwise angles are positive, clockwise
         * angles are negative.*/
        float nearest_segment_divider_SA_coord = segment_divider_A_coord -
                                                 float(segment_divider_A_coord >
                                                       segment_divider_A_angle_bisector) *
                                                     segment_divider_A_next_segment_divider;
        float l_angle_bisector = float(0.0);
        float r_gon_parameter = float(0.0);
        float max_unit_parameter = float(0.0);

        float l_circle_center = (float(1.0) - r_gon_roundness) /
                                cosf(segment_divider_A_angle_bisector);
        float l_coord_R_l_bevel_start = cosf(nearest_segment_divider_SA_coord) * l_circle_center +
                                        sqrtf(sqr(cosf(nearest_segment_divider_SA_coord) *
                                                  l_circle_center) +
                                              sqr(r_gon_roundness) - sqr(l_circle_center));

        l_angle_bisector = l_coord / l_coord_R_l_bevel_start;

        float spline_start_A_bevel_start = (float(1.0) - r_gon_roundness) *
                                           segment_divider_A_bevel_start;

        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
          float coord_A_bevel_start = segment_divider_A_bevel_start -
                                      fabsf(nearest_segment_divider_SA_coord);
          r_gon_parameter = l_coord * sinf(bevel_start_A_angle_bisector);

          if (coord_A_bevel_start < spline_start_A_bevel_start) {
            r_gon_parameter += l_coord * cosf(bevel_start_A_angle_bisector) * coord_A_bevel_start +
                               float(0.5) *
                                   (float(1.0) - l_coord * cosf(bevel_start_A_angle_bisector)) *
                                   sqr(coord_A_bevel_start) / spline_start_A_bevel_start;
          }
          else {
            r_gon_parameter += spline_start_A_bevel_start *
                                   (float(0.5) * l_coord * cosf(bevel_start_A_angle_bisector) -
                                    float(0.5)) +
                               coord_A_bevel_start;
          }
          if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
            r_gon_parameter *= -float(1.0);
          }
          if (normalize_r_gon_parameter) {
            float normalize_based_on_l_angle_bisector =
                l_angle_bisector * tanf(bevel_start_A_angle_bisector) +
                spline_start_A_bevel_start * (float(0.5) * l_angle_bisector + float(0.5)) +
                r_gon_roundness * segment_divider_A_bevel_start;
            float normalize_based_on_l_coord =
                l_coord * sinf(bevel_start_A_angle_bisector) +
                spline_start_A_bevel_start *
                    (float(0.5) * l_coord * cosf(bevel_start_A_angle_bisector) + float(0.5)) +
                r_gon_roundness * segment_divider_A_bevel_start;

            /* For r_gon_roundness -> 1.0 the normalize_based_on_l_angle_bisector field and
             * normalize_based_on_l_coord field converge against the same scalar field. */
            r_gon_parameter /= mix(normalize_based_on_l_angle_bisector,
                                   normalize_based_on_l_coord,
                                   coord_A_bevel_start / segment_divider_A_bevel_start);
          }
        }
        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_max_unit_parameter)) {
          max_unit_parameter = tanf(bevel_start_A_angle_bisector) + spline_start_A_bevel_start +
                               r_gon_roundness * segment_divider_A_bevel_start;
        }
        out_variables = make_float4(l_angle_bisector,
                                    r_gon_parameter,
                                    max_unit_parameter,
                                    segment_id * segment_divider_A_next_segment_divider +
                                        segment_divider_A_angle_bisector);
      }
      else {
        /* Regular straight part. */

        float l_angle_bisector = float(0.0);
        float r_gon_parameter = float(0.0);
        float max_unit_parameter = float(0.0);

        l_angle_bisector = l_coord *
                           cosf(segment_divider_A_angle_bisector - segment_divider_A_coord);

        float spline_start_A_bevel_start = (float(1.0) - r_gon_roundness) *
                                           segment_divider_A_bevel_start;

        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
          r_gon_parameter = l_angle_bisector * tanf(fabsf(segment_divider_A_angle_bisector -
                                                          segment_divider_A_coord));
          if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
            r_gon_parameter *= -float(1.0);
          }
          if (normalize_r_gon_parameter) {
            float normalize_based_on_l_angle_bisector =
                l_angle_bisector * tanf(bevel_start_A_angle_bisector) +
                spline_start_A_bevel_start * (float(0.5) * l_angle_bisector + float(0.5)) +
                r_gon_roundness * segment_divider_A_bevel_start;

            r_gon_parameter /= normalize_based_on_l_angle_bisector;
          }
        }
        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_max_unit_parameter)) {
          max_unit_parameter = tanf(bevel_start_A_angle_bisector) + spline_start_A_bevel_start +
                               r_gon_roundness * segment_divider_A_bevel_start;
        }
        out_variables = make_float4(l_angle_bisector,
                                    r_gon_parameter,
                                    max_unit_parameter,
                                    segment_id * segment_divider_A_next_segment_divider +
                                        segment_divider_A_angle_bisector);
      }
    }
  }
  else {
    if (r_gon_roundness == float(0.0)) {
      float x_axis_A_coord = atan2f(coord.y, coord.x) + float(coord.y < float(0.0)) * M_2PI_F;
      float segment_divider_A_angle_bisector = M_PI_F / r_gon_sides;
      float segment_divider_A_next_segment_divider = float(2.0) * segment_divider_A_angle_bisector;
      float segment_id = floorf(x_axis_A_coord / segment_divider_A_next_segment_divider);
      float segment_divider_A_coord = x_axis_A_coord -
                                      segment_id * segment_divider_A_next_segment_divider;

      float last_angle_bisector_A_x_axis = M_PI_F -
                                           floorf(r_gon_sides) * segment_divider_A_angle_bisector;
      float last_segment_divider_A_x_axis = float(2.0) * last_angle_bisector_A_x_axis;

      if (x_axis_A_coord < M_2PI_F - last_segment_divider_A_x_axis) {
        /* Regular straight part. */

        float l_angle_bisector = float(0.0);
        float r_gon_parameter = float(0.0);
        float max_unit_parameter = float(0.0);

        l_angle_bisector = l_coord *
                           cosf(segment_divider_A_angle_bisector - segment_divider_A_coord);
        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
          r_gon_parameter = l_angle_bisector * tanf(fabsf(segment_divider_A_angle_bisector -
                                                          segment_divider_A_coord));
          if (segment_divider_A_coord < segment_divider_A_angle_bisector) {
            r_gon_parameter *= -float(1.0);
          }
          if (normalize_r_gon_parameter) {
            r_gon_parameter /= l_angle_bisector * tanf(segment_divider_A_angle_bisector);
          }
        }
        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_max_unit_parameter)) {
          max_unit_parameter = tanf(segment_divider_A_angle_bisector);
        }
        out_variables = make_float4(l_angle_bisector,
                                    r_gon_parameter,
                                    max_unit_parameter,
                                    segment_id * segment_divider_A_next_segment_divider +
                                        segment_divider_A_angle_bisector);
      }
      else {
        /* Irregular straight part. */

        float l_angle_bisector = float(0.0);
        float r_gon_parameter = float(0.0);
        float max_unit_parameter = float(0.0);

        float l_angle_bisector_R_l_last_angle_bisector = cosf(segment_divider_A_angle_bisector) /
                                                         cosf(last_angle_bisector_A_x_axis);
        float l_last_angle_bisector = l_coord *
                                      cosf(last_angle_bisector_A_x_axis - segment_divider_A_coord);

        l_angle_bisector = l_angle_bisector_R_l_last_angle_bisector * l_last_angle_bisector;

        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_r_gon_parameter_field)) {
          r_gon_parameter = l_angle_bisector_R_l_last_angle_bisector * l_last_angle_bisector *
                            tanf(fabsf(last_angle_bisector_A_x_axis - segment_divider_A_coord));
          if (segment_divider_A_coord < last_angle_bisector_A_x_axis) {
            r_gon_parameter *= -float(1.0);
          }
          if (normalize_r_gon_parameter) {
            r_gon_parameter /= l_angle_bisector_R_l_last_angle_bisector * l_last_angle_bisector *
                               tanf(last_angle_bisector_A_x_axis);
          }
        }
        if (ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION(calculate_max_unit_parameter)) {
          max_unit_parameter = tanf(last_angle_bisector_A_x_axis);
        }
        out_variables = make_float4(l_angle_bisector,
                                    r_gon_parameter,
                                    max_unit_parameter,
                                    segment_id * segment_divider_A_next_segment_divider +
                                        last_angle_bisector_A_x_axis);
      }
    }
#ifdef ADAPT_TO_GEOMETRY_NODES
    else if (r_gon_roundness == float(1.0)) {
      out_variables = calculate_out_variables_full_roundness_irregular_circular(
          calculate_r_gon_parameter_field, normalize_r_gon_parameter, r_gon_sides, coord, l_coord);
    }
#endif
    else {
      out_variables = calculate_out_variables_irregular_circular(calculate_r_gon_parameter_field,
                                                                 calculate_max_unit_parameter,
                                                                 normalize_r_gon_parameter,
                                                                 r_gon_sides,
                                                                 r_gon_roundness,
                                                                 coord,
                                                                 l_coord);
    }
  }

  if ((coord.x == float(0.0)) && (coord.y == float(0.0))) {
    /* The r_gon_parameter is defined to 0 when it is not normalized and the input coordinate is
     * the zero vector. */
    out_variables.y = float(0.0);
  }

  if (normalize_r_gon_parameter) {
    /* Normalize r_gon_parameter from a [-1, 1] interval to a [0, 1] interval. */
    out_variables.y = float(0.5) * out_variables.y + float(0.5);
  }
  else {
    out_variables.x -= float(1.0);
  }

  return out_variables;
}

ccl_device float calculate_out_segment_id(float r_gon_sides, float2 coord)
{
  return floorf(r_gon_sides *
                ((atan2f(coord.y, coord.x) / M_2PI_F) + float(coord.y < float(0.0))));
}

/* Undefine macros used for code adaption. */
#ifdef ADAPT_TO_GEOMETRY_NODES
#  undef atanf
#  undef atan2f
#  undef ceilf
#  undef cosf
#  undef fabsf
#  undef floorf
#  undef fmaxf
#  undef fminf
#  undef fractf
#  undef mix
#  undef sinf
#  undef sqrtf
#  undef sqr
#  undef tanf

#  undef make_float2
#  undef make_float4
#  undef ccl_device
#else
#  ifdef ADAPT_TO_OSL
#    undef atanf
#    undef atan2f
#    undef ceilf
#    undef cosf
#    undef fabsf
#    undef floorf
#    undef fmaxf
#    undef fminf
#    undef fractf
#    undef mix
#    undef sinf
#    undef sqrtf
#    undef sqr
#    undef tanf

#    undef bool
#    undef float2
#    undef float4
#    undef make_float2
#    undef make_float4
#    undef M_PI_F
#    undef M_2PI_F
#    undef ccl_device

#    undef false
#    undef true
#  else
#    ifdef ADAPT_TO_SVM
/* No code adaption necessary for the SVM implementation as it is the base shared version. */
#    else
/* Adapt code to GLSL by default. */
#      undef atanf
#      undef atan2f
#      undef ceilf
#      undef cosf
#      undef fabsf
#      undef floorf
#      undef fmaxf
#      undef fminf
#      undef fractf
#      undef mix
#      undef sinf
#      undef sqrtf
#      undef sqr
#      undef tanf

#      undef make_float2
#      undef make_float4
#      undef M_PI_F
#      undef M_2PI_F
#      undef ccl_device
#    endif
#  endif
#endif

#undef ONLY_CHECK_IN_GEOMETRY_NODES_IMPLEMENTATION
