/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpencil_geometry)

#include "draw_colormanagement_lib.glsl"
#include "draw_grease_pencil_lib.glsl"
#include "gpu_shader_common_color_utils.glsl"
#include "gpu_shader_common_hash.glsl"
#include "gpu_shader_math_vector_lib.glsl"

float3 gpencil_lighting()
{
  float3 light_accum = float3(0.0f);
  for (int i = 0; i < GPENCIL_LIGHT_BUFFER_LEN; i++) {
    if (float3(gp_lights[i].light_color).x == -1.0f) {
      break;
    }
    float3 L = gp_lights[i].position - gp_interp.pos;
    float vis = 1.0f;
    gpLightType type = gp_lights[i].type;
    /* Spot Attenuation. */
    if (type == GP_LIGHT_TYPE_SPOT) {
      float3x3 rot_scale = float3x3(gp_lights[i].right, gp_lights[i].up, gp_lights[i].forward);
      float3 local_L = rot_scale * L;
      local_L /= abs(local_L.z);
      float ellipse = inversesqrt(length_squared(local_L));
      vis *= smoothstep(0.0f, 1.0f, (ellipse - gp_lights[i].spot_size) / gp_lights[i].spot_blend);
      /* Also mask +Z cone. */
      vis *= step(0.0f, local_L.z);
    }
    /* Inverse square decay. Skip for suns. */
    float L_len_sqr = length_squared(L);
    if (type < GP_LIGHT_TYPE_SUN) {
      vis /= L_len_sqr;
    }
    else {
      L = gp_lights[i].forward;
      L_len_sqr = 1.0f;
    }
    /* Lambertian falloff */
    if (type != GP_LIGHT_TYPE_AMBIENT) {
      L /= sqrt(L_len_sqr);
      vis *= clamp(dot(gp_normal, L), 0.0f, 1.0f);
    }
    light_accum += vis * gp_lights[i].light_color;
  }
  /* Clamp to avoid NaNs. */
  return clamp(light_accum, 0.0f, 1e10f);
}

/* dx and dy are only needed for dots and squares. */
float4 get_color(float2 uv, float2 dx, float2 dy)
{
  float4 col;
  if (flag_test(gp_interp_flat.mat_flag, GP_STROKE_TEXTURE_USE)) {
    bool premul = flag_test(gp_interp_flat.mat_flag, GP_STROKE_TEXTURE_PREMUL);
    col = textureGrad(gp_stroke_tx, uv, dx, dy);
    if (premul && !(col.a == 0.0f || col.a == 1.0f)) {
      col.rgb = col.rgb / col.a;
    }
  }
  else if (flag_test(gp_interp_flat.mat_flag, GP_FILL_TEXTURE_USE)) {
    bool use_clip = flag_test(gp_interp_flat.mat_flag, GP_FILL_TEXTURE_CLIP);
    float2 uvs = (use_clip) ? clamp(uv, 0.0f, 1.0f) : uv;
    bool premul = flag_test(gp_interp_flat.mat_flag, GP_FILL_TEXTURE_PREMUL);
    col = textureGrad(gp_fill_tx, uvs, dx, dy);
    if (premul && !(col.a == 0.0f || col.a == 1.0f)) {
      col.rgb = col.rgb / col.a;
    }
  }
  else if (flag_test(gp_interp_flat.mat_flag, GP_FILL_GRADIENT_USE)) {
    bool radial = flag_test(gp_interp_flat.mat_flag, GP_FILL_GRADIENT_RADIAL);
    float fac = clamp(radial ? length(uv * 2.0f - 1.0f) : uv.x, 0.0f, 1.0f);
    uint matid = gp_interp_flat.mat_flag >> GPENCIL_MATID_SHIFT;
    col = mix(gp_materials[matid].fill_color, gp_materials[matid].fill_mix_color, fac);
  }
  else /* SOLID */ {
    col = float4(1.0f);
  }
  col.rgb *= col.a;

  /* Composite all other colors on top of texture color.
   * Everything is pre-multiply by `col.a` to have the stencil effect. */
  col = col * gp_interp.color_mul + col.a * gp_interp.color_add;

  col.rgb *= gpencil_lighting();

  if (flag_test(gp_interp_flat.mat_flag, GP_STROKE_ALIGNMENT))  // dot and squares
  {
    uv = uv * 2.0f - 1.0f;
    if (flag_test(gp_interp_flat.mat_flag, GP_STROKE_DOTS)) {
      col *= gpencil_stroke_hardess_mask(length(uv), gp_interp_noperspective.hardness);
    }
    else {
      uv = abs(uv);
      col *= gpencil_stroke_hardess_mask(max(uv.x, uv.y), gp_interp_noperspective.hardness);
    }
  }

  return col;
}

float2x2 calculate_rotation_matrix(float2 x_axis)
{
  float2 y_axis = orthogonal(x_axis);
  return transpose(float2x2(x_axis, y_axis));
}

struct RandomParameters {
  float random_size;
  float random_strength;
  float random_rotation;

  float random_hue;
  float random_saturation;
  float random_value;

  float random_noise_scale;
};

RandomParameters unpack_random(uint4 random_packed)
{
  float2 unpacked_x = unpackUnorm2x16(random_packed.x);
  float2 unpacked_y = unpackUnorm2x16(random_packed.y);
  float2 unpacked_z = unpackUnorm2x16(random_packed.z);
  return {unpacked_x.x,
          unpacked_x.y,
          unpacked_y.x,
          unpacked_y.y,
          unpacked_z.x,
          unpacked_z.y,
          uintBitsToFloat(random_packed.w)};
}

float simple_noise(float x)
{
  int int_x = int(x);
  float factor = smoothstep(0.0f, 1.0f, fract(x));
  return mix(hash_uint_to_float(int_x), hash_uint_to_float(int_x + 1), factor);
}

float noise_level_2(float x)
{
  return (simple_noise(x) + simple_noise(x * 0.353953f)) * 0.5f;
}

float4 get_dot_color(float2 uv, int i, float2 dx, float2 dy)
{
  uint matid = gp_interp_flat.mat_flag >> GPENCIL_MATID_SHIFT;
  RandomParameters Parameters = unpack_random(gp_materials[matid].random_packed);

  float noise_x = float(i) * Parameters.random_noise_scale;

  if (Parameters.random_rotation > 0.0f || Parameters.random_size > 0.0f) {
    float rand_rot = noise_level_2(noise_x + 69637.532f);
    rand_rot -= 0.5f;
    rand_rot *= 2.0f;
    rand_rot *= M_PI;
    rand_rot *= Parameters.random_rotation;

    float2x2 mat = calculate_rotation_matrix(float2(cos(rand_rot), sin(rand_rot)));

    if (Parameters.random_size > 0.0f) {
      float rand_siz = noise_level_2(noise_x + 18559.853f);
      rand_siz *= Parameters.random_size;
      rand_siz = 1.0f - rand_siz;

      rand_siz = 1.0f / rand_siz;
      mat[0] = mat[0] * rand_siz;
      mat[1] = mat[1] * rand_siz;
    }

    uv -= 0.5f;
    uv = mat * uv;
    dx = mat * dx;
    dy = mat * dy;
    uv += 0.5f;
  }

  float4 col = get_color(uv, dx, dy);
  if (Parameters.random_hue > 0.0f || Parameters.random_saturation > 0.0f ||
      Parameters.random_value > 0.0f)
  {
    float4 col_hsva;
    rgb_to_hsv(col, col_hsva);

    float rand_hue = noise_level_2(noise_x + 97715.184f);
    float rand_sat = noise_level_2(noise_x + 16430.953f);
    float rand_val = noise_level_2(noise_x + 86191.195f);

    col_hsva.x += (rand_hue - 0.5f) * Parameters.random_hue;
    col_hsva.y *= 1.0f + (rand_sat * 2.0f - 1.0f) * Parameters.random_saturation;
    col_hsva.z *= 1.0f - Parameters.random_value + rand_val * 2.0f * Parameters.random_value;

    col_hsva.x = fract(col_hsva.x);
    col_hsva.y = clamp(col_hsva.y, 0.0f, 1.0f);
    col_hsva.z = clamp(col_hsva.z, 0.0f, 1.0f);

    hsv_to_rgb(col_hsva, col);
  }

  if (Parameters.random_strength > 0.0f) {
    float rand = noise_level_2(noise_x + 68916.135f);

    rand -= 1.0f;
    rand *= Parameters.random_strength;
    rand += 1.0f;

    col *= rand;
  }

  return col;
}

float4 alpha_over(float4 base, float4 over)
{
  return (1.0 - over.w) * base + over;
}

float4 to_cam(float4 a)
{
  if (drw_view_is_perspective()) {
    return float4(a.x / a.z, a.y / a.z, a.z, a.w / a.z);
  }
  return a;
}

float4 from_cam(float4 a)
{
  if (drw_view_is_perspective()) {
    return float4(a.x * a.z, a.y * a.z, a.z, a.w * a.z);
  }
  return a;
}

float point_i_to_local_t(float i, float4 p1, float4 p2)
{
  float i_start = gp_interp_flat.point_length.x;
  float i_end = gp_interp_flat.point_length.y;
  float point_density = gp_interp_flat.point_length.z;
  float i_delta = i_end - i_start;

  uint placement_mode = gp_interp_flat.mat_flag & GP_DOTS_PLACEMENT_MODE;

  if (placement_mode == GP_DOTS_PLACEMENT_MODE_RADIUS) {
    float4 P1 = from_cam(p1);
    float4 P2 = from_cam(p2);
    float r1 = P1.w;
    float r2 = P2.w;
    float a = r2 - r1;
    float l = length(P1.xyz - P2.xyz);

    if (abs(a) < 0.001f * l) {
      return (i / point_density - i_start) / i_delta;
    }

    if (!drw_view_is_perspective()) {
      float b = 2.0f * log(a / r1 + 1.0f) / i_delta;
      float exp_b = exp(b);
      l = a * (exp_b + 1.0f) / (exp_b - 1.0f);
    }

    /* Avoid division by zero. */
    if (r1 <= 0.0f || l <= 0.0f || l == a) {
      return 0.0f;
    }

    float E = (l + a) / (l - a);
    float E_i = pow(E, (i / point_density - i_start) / 2.0f);

    return r1 * (E_i - 1.0f) / a;
  }

  return (i / point_density - i_start) / i_delta;
}

float local_t_to_point_i(float t, float4 p1, float4 p2)
{
  float i_start = gp_interp_flat.point_length.x;
  float i_end = gp_interp_flat.point_length.y;
  float point_density = gp_interp_flat.point_length.z;
  float i_delta = i_end - i_start;

  uint placement_mode = gp_interp_flat.mat_flag & GP_DOTS_PLACEMENT_MODE;

  if (placement_mode == GP_DOTS_PLACEMENT_MODE_RADIUS) {
    float4 P1 = from_cam(p1);
    float4 P2 = from_cam(p2);
    float r1 = P1.w;
    float r2 = P2.w;
    float a = r2 - r1;
    float l = length(P1.xyz - P2.xyz);

    if (abs(a) < 0.001f * l) {
      return (t * i_delta + i_start) * point_density;
    }

    if (!drw_view_is_perspective()) {
      float b = 2.0f * log(a / r1 + 1.0f) / i_delta;
      float exp_b = exp(b);
      l = a * (exp_b + 1.0f) / (exp_b - 1.0f);
    }

    /* Avoid division by zero. */
    if (r1 <= 0.0f || l <= 0.0f || l == a) {
      return 0.0f;
    }

    float E = (l + a) / (l - a);
    float E_i = t * a / r1 + 1.0f;

    return (2.0f * log(E_i) / log(E) + i_start) * point_density;
  }

  return (t * i_delta + i_start) * point_density;
}

float screen_t_to_local_t(float screen_t, float z1, float z2)
{
  if (!drw_view_is_perspective()) {
    return screen_t;
  }

  float f = (1.0f - screen_t);
  float k = z2 / z1 - 1.0f;
  return screen_t / (k * f + 1.0f);
}

/**
 * Calculate the `t` values for the two circles on a uneven capsule that intersection the point
 * `p0`.
 */
float2 uneven_capsule_intersection(float2 p0, float2 p1, float2 p2, float r1, float r2)
{
  float l = distance(p1, p2);

  float local_dis_sq = dot(p2 - p1, p2 - p1);
  float X = (dot(p0 - p1, p2 - p1) / local_dis_sq) * l;
  float2 p_t = p1 + (p2 - p1) * (X / l);
  float Y = distance(p_t, p0);

  float a = l * l - (r2 - r1) * (r2 - r1);
  float b = -2.0f * (r1 * (r2 - r1) + l * X);
  float c = Y * Y + X * X - r1 * r1;

  float discriminant = b * b - 4.0f * a * c;
  if (discriminant < 0.0f) {
    return float2(-1.0f, -1.0f);
  }

  /* The quadratic equation. */
  float2 t = (float2(-1.0f, 1.0f) * sqrt(discriminant) - b) / (2.0f * a);

  if (r1 < r2) {
    if (l - r2 < -r1) {
      return float2(t.x, 1.0f);
    }
  }
  else {
    if (l + r2 < r1) {
      return float2(0.0f, t.y);
    }
  }

  return t;
}

int2 get_bounds(float2 p0, float4 p1, float4 p2)
{
  uint placement_mode = gp_interp_flat.mat_flag & GP_DOTS_PLACEMENT_MODE;
  if (placement_mode == GP_DOTS_PLACEMENT_MODE_COUNT && gp_interp_flat.point_length.z == 1.0f) {
    return int2(0, 1);
  }

  int min_lower = int(ceil(local_t_to_point_i(0.0f, p1, p2)));
  int max_upper = int(ceil(local_t_to_point_i(1.0f, p1, p2)));

  if (!(p1.z > 0.0f && p2.z > 0.0f)) {
    return int2(min_lower, max_upper);
  }

  float r1 = p1.w;
  float r2 = p2.w;

  /* Scale each circle up by the diagonal of the square. */
  bool is_squares = !flag_test(gp_interp_flat.mat_flag, GP_STROKE_DOTS);
  if (is_squares) {
    r1 *= M_SQRT2;
    r2 *= M_SQRT2;
  }

  float2 ts = uneven_capsule_intersection(p0, p1.xy, p2.xy, r1, r2);

  if (ts.x == -1.0f && ts.y == -1.0f) {
    return int2(0, 0);
  }

  if (ts.y < 0.0f || ts.x > 1.0f) {
    return int2(0, 0);
  }

  float t_min = screen_t_to_local_t(saturate(ts.x), p1.z, p2.z);
  float t_max = screen_t_to_local_t(saturate(ts.y), p1.z, p2.z);

  int lower = int(floor(local_t_to_point_i(t_min, p1, p2)));
  int upper = int(ceil(local_t_to_point_i(t_max, p1, p2))) + 1;

  lower = max(min_lower, lower);
  upper = min(max_upper, upper);

  return int2(lower, upper);
}

float3 ndc_to_view(float4 ndc)
{
  if (drw_view_is_perspective()) {
    float3 view = (drw_view().wininv * ndc).xyz;
    view.z *= -1.0f;
    return view;
  }
  float aspect = viewport_size.x / viewport_size.y;
  return float3(ndc.xy / float2(1.0f, aspect), 1.0f);
}

void main()
{
  uint placement_mode = gp_interp_flat.mat_flag & GP_DOTS_PLACEMENT_MODE;
  bool is_single_dot = placement_mode == GP_DOTS_PLACEMENT_MODE_COUNT &&
                       gp_interp_flat.point_length.z == 1.0f;

  if (flag_test(gp_interp_flat.mat_flag, GP_FILL))  // fill
  {
    float2 dx = gpu_dfdx(gp_interp.uv);
    float2 dy = gpu_dfdy(gp_interp.uv);

    frag_color = get_color(gp_interp.uv, dx, dy);
  }
  else {
    if (flag_test(gp_interp_flat.mat_flag, GP_STROKE_ALIGNMENT))  // dot and squares
    {
      if (!is_single_dot) {
        float radius1 = screen_space_to_radius(gp_interp_flat.sspos_1);
        float radius2 = screen_space_to_radius(gp_interp_flat.sspos_2);

        float4 ndc1 = screen_space_to_ndc(gp_interp_flat.sspos_1, viewport_size);
        float4 ndc2 = screen_space_to_ndc(gp_interp_flat.sspos_2, viewport_size);

        float3 v1 = ndc_to_view(ndc1);
        float3 v2 = ndc_to_view(ndc2);

        float3 view_dir = ndc_to_view(
            float4(gl_FragCoord.xy / viewport_size.xy, 0.0f, 1.0f) * 2.0f - 1.0f);
        float2 view_coord = view_dir.xy / view_dir.z;

        float scale_fac = 2.0f / viewport_size.x;
        if (drw_view_is_perspective()) {
          scale_fac *= drw_view().wininv[0][0] / view_dir.z;
        }

        float4 P1 = float4(v1, radius1 * scale_fac);
        float4 P2 = float4(v2, radius2 * scale_fac);

        float4 p1 = to_cam(P1);
        float4 p2 = to_cam(P2);

        int2 bounds = get_bounds(view_coord, p1, p2);
        int lower = bounds.x;
        int upper = bounds.y;

        float2 pre_dx = gpu_dfdx(view_coord);
        float2 pre_dy = gpu_dfdy(view_coord);

        frag_color = float4(0.0f);
        /* Loop through backwards so we can break early. */
        for (int i = upper - 1; i >= lower; i--) {
          float t = point_i_to_local_t(i, p1, p2);

          float4 pos = to_cam(P1 + (P2 - P1) * t);

          float2 uv = (view_coord - pos.xy) / pos.w;
          float2 dx = pre_dx / pos.w;
          float2 dy = pre_dy / pos.w;

          float2x2 mat = calculate_rotation_matrix(gp_interp_flat.aspect.zw);
          uv = mat * uv;
          dx = mat * dx;
          dy = mat * dy;

          uv = uv * 0.5f + 0.5f;
          dx = dx * 0.5f;
          dy = dy * 0.5f;

          frag_color = alpha_over(get_dot_color(uv, i, dx, dy), frag_color);

          /* Break early if full opacity. */
          if (frag_color.w > 0.999f) {
            break;
          }
        }
      }
      else {
        int i = int(gp_interp_flat.point_length.x);
        float2 dx = gpu_dfdx(gp_interp.uv);
        float2 dy = gpu_dfdy(gp_interp.uv);

        frag_color = get_dot_color(gp_interp.uv, i, dx, dy);
      }
    }
    else {  // line
      float2 dx = gpu_dfdx(gp_interp.uv);
      float2 dy = gpu_dfdy(gp_interp.uv);

      frag_color = get_color(gp_interp.uv, dx, dy);
      frag_color *= gpencil_stroke_mask(gp_interp_flat.sspos_1.xy,
                                        gp_interp_flat.sspos_2.xy,
                                        gp_interp_flat.sspos_0,
                                        gp_interp_flat.sspos_3,
                                        gp_interp.uv,
                                        gp_interp_flat.mat_flag,
                                        gp_interp_noperspective.thickness.x,
                                        gp_interp_noperspective.hardness,
                                        gp_interp_noperspective.thickness.zw);
    }
  }

  /* To avoid aliasing artifacts, we reduce the opacity of small strokes. */
  frag_color *= smoothstep(0.0f, 1.0f, gp_interp_noperspective.thickness.y);

  /* Holdout materials. */
  if (flag_test(gp_interp_flat.mat_flag, GP_STROKE_HOLDOUT | GP_FILL_HOLDOUT)) {
    revealColor = frag_color.aaaa;
  }
  else {
    /* NOT holdout materials.
     * For compatibility with colored alpha buffer.
     * Note that we are limited to mono-chromatic alpha blending here
     * because of the blend equation and the limit of 1 color target
     * when using custom color blending. */
    revealColor = float4(0.0f, 0.0f, 0.0f, frag_color.a);

    if (frag_color.a < 0.001f) {
      gpu_discard_fragment();
      return;
    }
  }

  float2 fb_size = max(float2(textureSize(gp_scene_depth_tx, 0).xy),
                       float2(textureSize(gp_mask_tx, 0).xy));
  float2 uvs = gl_FragCoord.xy / fb_size;
  /* Manual depth test */
  float scene_depth = texture(gp_scene_depth_tx, uvs).r;
  if (gl_FragCoord.z > scene_depth) {
    gpu_discard_fragment();
    return;
  }

  /* FIXME(fclem): Grrr. This is bad for performance but it's the easiest way to not get
   * depth written where the mask obliterate the layer. */
  float mask = texture(gp_mask_tx, uvs).r;
  if (mask < 0.001f) {
    gpu_discard_fragment();
    return;
  }

  /* We override the fragment depth using the fragment shader to ensure a constant value.
   * This has a cost as the depth test cannot happen early.
   * We could do this in the vertex shader but then perspective interpolation of uvs and
   * fragment clipping gets really complicated. */
  if (gp_interp_flat.depth >= 0.0f) {
    gl_FragDepth = gp_interp_flat.depth;
  }
  else {
    gl_FragDepth = gl_FragCoord.z;
  }
}
