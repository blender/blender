
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(raytrace_lib.glsl)

/* Based on Practical Realtime Strategies for Accurate Indirect Occlusion
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pptx
 */

#if defined(MESH_SHADER)
#  if !defined(USE_ALPHA_HASH)
#    if !defined(DEPTH_SHADER)
#      if !defined(USE_ALPHA_BLEND)
#        define ENABLE_DEFERED_AO
#      endif
#    endif
#  endif
#endif

#ifndef ENABLE_DEFERED_AO
#  if defined(STEP_RESOLVE)
#    define ENABLE_DEFERED_AO
#  endif
#endif

uniform sampler2D horizonBuffer;

/* aoSettings flags */
#define USE_AO 1
#define USE_BENT_NORMAL 2
#define USE_DENOISE 4

#define NO_OCCLUSION_DATA OcclusionData(vec4(M_PI, -M_PI, M_PI, -M_PI), 1.0)

struct OcclusionData {
  /* 4 horizons angles, one in each direction around the view vector to form a cross pattern. */
  vec4 horizons;
  /* Custom large scale occlusion. */
  float custom_occlusion;
};

vec4 pack_occlusion_data(OcclusionData data)
{
  return vec4(1.0 - data.horizons * vec4(1, -1, 1, -1) * M_1_PI);
}

OcclusionData unpack_occlusion_data(vec4 v)
{
  return OcclusionData((1.0 - v) * vec4(1, -1, 1, -1) * M_PI, 0.0);
}

vec2 get_ao_noise(void)
{
  vec2 noise = texelfetch_noise_tex(gl_FragCoord.xy).xy;
  /* Decorrelate noise from AA. */
  /* TODO(fclem) we should use a more general approach for more random number dimentions. */
  noise = fract(noise * 6.1803402007);
  return noise;
}

vec2 get_ao_dir(float jitter)
{
  /* Only a quarter of a turn because we integrate using 2 slices.
   * We use this instead of using utiltex circle noise to improve cache hits
   * since all tracing direction will be in the same quadrant. */
  jitter *= M_PI_2;
  return vec2(cos(jitter), sin(jitter));
}

/* Return horizon angle cosine. */
float search_horizon(vec3 vI,
                     vec3 vP,
                     float noise,
                     ScreenSpaceRay ssray,
                     sampler2D depth_tx,
                     const float inverted,
                     float radius,
                     const float sample_count)
{
  /* Init at cos(M_PI). */
  float h = (inverted != 0.0) ? 1.0 : -1.0;

  ssray.max_time -= 1.0;

  if (ssray.max_time <= 2.0) {
    /* Produces self shadowing under this threshold. */
    return fast_acos(h);
  }

  float prev_time, time = 0.0;
  for (float iter = 0.0; time < ssray.max_time && iter < sample_count; iter++) {
    prev_time = time;
    /* Gives us good precision at center and ensure we cross at least one pixel per iteration. */
    time = 1.0 + iter + sqr((iter + noise) / sample_count) * ssray.max_time;
    float stride = time - prev_time;
    float lod = (log2(stride) - noise) / (1.0 + aoQuality);

    vec2 uv = ssray.origin.xy + ssray.direction.xy * time;
    float depth = textureLod(depth_tx, uv * hizUvScale.xy, floor(lod)).r;

    if (depth == 1.0 && inverted == 0.0) {
      /* Skip background. Avoids making shadow on the geometry near the far plane. */
      continue;
    }

    /* Bias depth a bit to avoid self shadowing issues. */
    const float bias = 2.0 * 2.4e-7;
    depth += (inverted != 0.0) ? -bias : bias;

    vec3 s = get_view_space_from_depth(uv, depth);
    vec3 omega_s = s - vP;
    float len = length(omega_s);
    /* Sample's horizon angle cosine. */
    float s_h = dot(vI, omega_s / len);
    /* Blend weight to fade artifacts. */
    float dist_ratio = abs(len) / radius;
    /* Sphere falloff. */
    float dist_fac = sqr(saturate(dist_ratio));
    /* Unbiased, gives too much hard cut behind objects */
    // float dist_fac = step(0.999, dist_ratio);

    if (inverted != 0.0) {
      h = min(h, s_h);
    }
    else {
      h = mix(max(h, s_h), h, dist_fac);
    }
  }
  return fast_acos(h);
}

OcclusionData occlusion_search(
    vec3 vP, sampler2D depth_tx, float radius, const float inverted, const float dir_sample_count)
{
  if ((int(aoSettings) & USE_AO) == 0) {
    return NO_OCCLUSION_DATA;
  }

  vec2 noise = get_ao_noise();
  vec2 dir = get_ao_dir(noise.x);
  vec2 uv = get_uvs_from_view(vP);
  vec3 vI = ((ProjectionMatrix[3][3] == 0.0) ? normalize(-vP) : vec3(0.0, 0.0, 1.0));
  vec3 avg_dir = vec3(0.0);
  float avg_apperture = 0.0;

  OcclusionData data = (inverted != 0.0) ? OcclusionData(vec4(0, 0, 0, 0), 1.0) :
                                           NO_OCCLUSION_DATA;

  for (int i = 0; i < 2; i++) {
    Ray ray;
    ray.origin = vP;
    ray.direction = vec3(dir * radius, 0.0);

    ScreenSpaceRay ssray;

    ssray = raytrace_screenspace_ray_create(ray);
    data.horizons[0 + i * 2] = search_horizon(
        vI, vP, noise.y, ssray, depth_tx, inverted, radius, dir_sample_count);

    ray.direction = -ray.direction;

    ssray = raytrace_screenspace_ray_create(ray);
    data.horizons[1 + i * 2] = -search_horizon(
        vI, vP, noise.y, ssray, depth_tx, inverted, radius, dir_sample_count);

    /* Rotate 90 degrees. */
    dir = vec2(-dir.y, dir.x);
  }

  return data;
}

vec2 clamp_horizons_to_hemisphere(vec2 horizons, float angle_N, const float inverted)
{
  /* Add a little bias to fight self shadowing. */
  const float max_angle = M_PI_2 - 0.05;

  if (inverted != 0.0) {
    horizons.x = max(horizons.x, angle_N + max_angle);
    horizons.y = min(horizons.y, angle_N - max_angle);
  }
  else {
    horizons.x = min(horizons.x, angle_N + max_angle);
    horizons.y = max(horizons.y, angle_N - max_angle);
  }
  return horizons;
}

void occlusion_eval(OcclusionData data,
                    vec3 V,
                    vec3 N,
                    vec3 Ng,
                    const float inverted,
                    out float visibility,
                    out vec3 bent_normal)
{
  if ((int(aoSettings) & USE_AO) == 0) {
    visibility = data.custom_occlusion;
    bent_normal = N;
    return;
  }

  bool early_out = (inverted != 0.0) ? (max_v4(abs(data.horizons)) == 0.0) :
                                       (min_v4(abs(data.horizons)) == M_PI);
  if (early_out) {
    visibility = dot(N, Ng) * 0.5 + 0.5;
    visibility = min(visibility, data.custom_occlusion);

    if ((int(aoSettings) & USE_BENT_NORMAL) == 0) {
      bent_normal = N;
    }
    else {
      bent_normal = normalize(N + Ng);
    }
    return;
  }

  vec2 noise = get_ao_noise();
  vec2 dir = get_ao_dir(noise.x);

  visibility = 0.0;
  bent_normal = N * 0.001;

  for (int i = 0; i < 2; i++) {
    vec3 T = transform_direction(ViewMatrixInverse, vec3(dir, 0.0));
    /* Setup integration domain around V. */
    vec3 B = normalize(cross(V, T));
    T = normalize(cross(B, V));

    float proj_N_len;
    vec3 proj_N = normalize_len(N - B * dot(N, B), proj_N_len);
    vec3 proj_Ng = normalize(Ng - B * dot(Ng, B));

    vec2 h = (i == 0) ? data.horizons.xy : data.horizons.zw;

    float N_sin = dot(proj_N, T);
    float Ng_sin = dot(proj_Ng, T);
    float N_cos = saturate(dot(proj_N, V));
    float Ng_cos = saturate(dot(proj_Ng, V));
    /* Gamma, angle between normalized projected normal and view vector. */
    float angle_Ng = sign(Ng_sin) * fast_acos(Ng_cos);
    float angle_N = sign(N_sin) * fast_acos(N_cos);
    /* Clamp horizons to hemisphere around shading normal. */
    h = clamp_horizons_to_hemisphere(h, angle_N, inverted);

    float bent_angle = (h.x + h.y) * 0.5;
    /* NOTE: here we multiply z by 0.5 as it shows less difference with the geometric normal.
     * Also modulate by projected normal length to reduce issues with slanted surfaces.
     * All of this is ad-hoc and not really grounded. */
    bent_normal += proj_N_len * (T * sin(bent_angle) + V * 0.5 * cos(bent_angle));

    /* Clamp to geometric normal only for integral to keep smooth bent normal. */
    /* This is done to match Cycles ground truth but adds some computation. */
    h = clamp_horizons_to_hemisphere(h, angle_Ng, inverted);

    /* Inner integral (Eq. 7). */
    float a = dot(-cos(2.0 * h - angle_N) + N_cos + 2.0 * h * N_sin, vec2(0.25));
    /* Correct normal not on plane (Eq. 8). */
    visibility += proj_N_len * a;

    /* Rotate 90 degrees. */
    dir = vec2(-dir.y, dir.x);
  }
  /* We integrated 2 directions. */
  visibility *= 0.5;

  visibility = min(visibility, data.custom_occlusion);

  if ((int(aoSettings) & USE_BENT_NORMAL) == 0) {
    bent_normal = N;
  }
  else {
    bent_normal = normalize(mix(bent_normal, N, sqr(sqr(sqr(visibility)))));
  }
}

/* Multibounce approximation base on surface albedo.
 * Page 78 in the .pdf version. */
float gtao_multibounce(float visibility, vec3 albedo)
{
  if (aoBounceFac == 0.0) {
    return visibility;
  }

  /* Median luminance. Because Colored multibounce looks bad. */
  float lum = dot(albedo, vec3(0.3333));

  float a = 2.0404 * lum - 0.3324;
  float b = -4.7951 * lum + 0.6417;
  float c = 2.7552 * lum + 0.6903;

  float x = visibility;
  return max(x, ((x * a + b) * x + c) * x);
}

float diffuse_occlusion(OcclusionData data, vec3 V, vec3 N, vec3 Ng)
{
  vec3 unused;
  float visibility;
  occlusion_eval(data, V, N, Ng, 0.0, visibility, unused);
  /* Scale by user factor */
  visibility = pow(saturate(visibility), aoFactor);
  return visibility;
}

float diffuse_occlusion(
    OcclusionData data, vec3 V, vec3 N, vec3 Ng, vec3 albedo, out vec3 bent_normal)
{
  float visibility;
  occlusion_eval(data, V, N, Ng, 0.0, visibility, bent_normal);

  visibility = gtao_multibounce(visibility, albedo);
  /* Scale by user factor */
  visibility = pow(saturate(visibility), aoFactor);
  return visibility;
}

/**
 * Approximate the area of intersection of two spherical caps
 * radius1 : First cap’s radius (arc length in radians)
 * radius2 : Second caps’ radius (in radians)
 * dist : Distance between caps (radians between centers of caps)
 * Note: Result is divided by pi to save one multiply.
 **/
float spherical_cap_intersection(float radius1, float radius2, float dist)
{
  /* From "Ambient Aperture Lighting" by Chris Oat
   * Slide 15. */
  float max_radius = max(radius1, radius2);
  float min_radius = min(radius1, radius2);
  float sum_radius = radius1 + radius2;
  float area;
  if (dist <= max_radius - min_radius) {
    /* One cap in completely inside the other */
    area = 1.0 - cos(min_radius);
  }
  else if (dist >= sum_radius) {
    /* No intersection exists */
    area = 0;
  }
  else {
    float diff = max_radius - min_radius;
    area = smoothstep(0.0, 1.0, 1.0 - saturate((dist - diff) / (sum_radius - diff)));
    area *= 1.0 - cos(min_radius);
  }
  return area;
}

float specular_occlusion(
    OcclusionData data, vec3 V, vec3 N, float roughness, inout vec3 specular_dir)
{
  vec3 visibility_dir;
  float visibility;
  occlusion_eval(data, V, N, N, 0.0, visibility, visibility_dir);

  specular_dir = normalize(mix(specular_dir, visibility_dir, roughness * (1.0 - visibility)));

  /* Visibility to cone angle (eq. 18). */
  float vis_angle = fast_acos(sqrt(1 - visibility));
  /* Roughness to cone angle (eq. 26). */
  float spec_angle = max(0.001, fast_acos(cone_cosine(roughness)));
  /* Angle between cone axes. */
  float cone_cone_dist = fast_acos(saturate(dot(visibility_dir, specular_dir)));
  float cone_nor_dist = fast_acos(saturate(dot(N, specular_dir)));

  float isect_solid_angle = spherical_cap_intersection(vis_angle, spec_angle, cone_cone_dist);
  float specular_solid_angle = spherical_cap_intersection(M_PI_2, spec_angle, cone_nor_dist);
  float specular_occlusion = isect_solid_angle / specular_solid_angle;
  /* Mix because it is unstable in unoccluded areas. */
  visibility = mix(isect_solid_angle / specular_solid_angle, 1.0, pow(visibility, 8.0));

  /* Scale by user factor */
  visibility = pow(saturate(visibility), aoFactor);
  return visibility;
}

/* Use the right occlusion. */
OcclusionData occlusion_load(vec3 vP, float custom_occlusion)
{
  /* Default to fully openned cone. */
  OcclusionData data = NO_OCCLUSION_DATA;

#ifdef ENABLE_DEFERED_AO
  if ((int(aoSettings) & USE_AO) != 0) {
    data = unpack_occlusion_data(texelFetch(horizonBuffer, ivec2(gl_FragCoord.xy), 0));
  }
#else
  /* For blended surfaces.  */
  data = occlusion_search(vP, maxzBuffer, aoDistance, 0.0, 8.0);
#endif

  data.custom_occlusion = custom_occlusion;

  return data;
}
