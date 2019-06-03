
/* Based on Practical Realtime Strategies for Accurate Indirect Occlusion
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pdf
 * http://blog.selfshadow.com/publications/s2016-shading-course/activision/s2016_pbs_activision_occlusion.pptx
 */

#if defined(MESH_SHADER)
#  if !defined(USE_ALPHA_HASH)
#    if !defined(USE_ALPHA_CLIP)
#      if !defined(SHADOW_SHADER)
#        if !defined(USE_MULTIPLY)
#          if !defined(USE_ALPHA_BLEND)
#            define ENABLE_DEFERED_AO
#          endif
#        endif
#      endif
#    endif
#  endif
#endif

#ifndef ENABLE_DEFERED_AO
#  if defined(STEP_RESOLVE)
#    define ENABLE_DEFERED_AO
#  endif
#endif

#define MAX_PHI_STEP 32
#define MAX_SEARCH_ITER 32
#define MAX_LOD 6.0

#ifndef UTIL_TEX
#  define UTIL_TEX
uniform sampler2DArray utilTex;
#  define texelfetch_noise_tex(coord) texelFetch(utilTex, ivec3(ivec2(coord) % LUT_SIZE, 2.0), 0)
#endif /* UTIL_TEX */

uniform sampler2D horizonBuffer;

/* aoSettings flags */
#define USE_AO 1
#define USE_BENT_NORMAL 2
#define USE_DENOISE 4

vec4 pack_horizons(vec4 v)
{
  return v * 0.5 + 0.5;
}
vec4 unpack_horizons(vec4 v)
{
  return v * 2.0 - 1.0;
}

/* Returns maximum screen distance an AO ray can travel for a given view depth */
vec2 get_max_dir(float view_depth)
{
  float homcco = ProjectionMatrix[2][3] * view_depth + ProjectionMatrix[3][3];
  float max_dist = aoDistance / homcco;
  return vec2(ProjectionMatrix[0][0], ProjectionMatrix[1][1]) * max_dist;
}

vec2 get_ao_dir(float jitter)
{
  /* Only half a turn because we integrate in slices. */
  jitter *= M_PI;
  return vec2(cos(jitter), sin(jitter));
}

void get_max_horizon_grouped(vec4 co1, vec4 co2, vec3 x, float lod, inout float h)
{
  int mip = int(lod) + hizMipOffset;
  co1 *= mipRatio[mip].xyxy;
  co2 *= mipRatio[mip].xyxy;

  float depth1 = textureLod(maxzBuffer, co1.xy, floor(lod)).r;
  float depth2 = textureLod(maxzBuffer, co1.zw, floor(lod)).r;
  float depth3 = textureLod(maxzBuffer, co2.xy, floor(lod)).r;
  float depth4 = textureLod(maxzBuffer, co2.zw, floor(lod)).r;

  vec4 len, s_h;

  vec3 s1 = get_view_space_from_depth(co1.xy, depth1); /* s View coordinate */
  vec3 omega_s1 = s1 - x;
  len.x = length(omega_s1);
  s_h.x = omega_s1.z / len.x;

  vec3 s2 = get_view_space_from_depth(co1.zw, depth2); /* s View coordinate */
  vec3 omega_s2 = s2 - x;
  len.y = length(omega_s2);
  s_h.y = omega_s2.z / len.y;

  vec3 s3 = get_view_space_from_depth(co2.xy, depth3); /* s View coordinate */
  vec3 omega_s3 = s3 - x;
  len.z = length(omega_s3);
  s_h.z = omega_s3.z / len.z;

  vec3 s4 = get_view_space_from_depth(co2.zw, depth4); /* s View coordinate */
  vec3 omega_s4 = s4 - x;
  len.w = length(omega_s4);
  s_h.w = omega_s4.z / len.w;

  /* Blend weight after half the aoDistance to fade artifacts */
  vec4 blend = saturate((1.0 - len / aoDistance) * 2.0);

  h = mix(h, max(h, s_h.x), blend.x);
  h = mix(h, max(h, s_h.y), blend.y);
  h = mix(h, max(h, s_h.z), blend.z);
  h = mix(h, max(h, s_h.w), blend.w);
}

vec2 search_horizon_sweep(vec2 t_phi, vec3 pos, vec2 uvs, float jitter, vec2 max_dir)
{
  max_dir *= max_v2(abs(t_phi));

  /* Convert to pixel space. */
  t_phi /= vec2(textureSize(maxzBuffer, 0));

  /* Avoid division by 0 */
  t_phi += vec2(1e-5);

  jitter *= 0.25;

  /* Compute end points */
  vec2 corner1 = min(vec2(1.0) - uvs, max_dir);  /* Top right */
  vec2 corner2 = max(vec2(0.0) - uvs, -max_dir); /* Bottom left */
  vec2 iter1 = corner1 / t_phi;
  vec2 iter2 = corner2 / t_phi;

  vec2 min_iter = max(-iter1, -iter2);
  vec2 max_iter = max(iter1, iter2);

  vec2 times = vec2(-min_v2(min_iter), min_v2(max_iter));

  vec2 h = vec2(-1.0); /* init at cos(pi) */

  /* This is freaking sexy optimized. */
  for (float i = 0.0, ofs = 4.0, time = -1.0; i < MAX_SEARCH_ITER && time > times.x;
       i++, time -= ofs, ofs = min(exp2(MAX_LOD) * 4.0, ofs + ofs * aoQuality)) {
    vec4 t = max(times.xxxx, vec4(time) - (vec4(0.25, 0.5, 0.75, 1.0) - jitter) * ofs);
    vec4 cos1 = uvs.xyxy + t_phi.xyxy * t.xxyy;
    vec4 cos2 = uvs.xyxy + t_phi.xyxy * t.zzww;
    float lod = min(MAX_LOD, max(i - jitter * 4.0, 0.0) * aoQuality);
    get_max_horizon_grouped(cos1, cos2, pos, lod, h.y);
  }

  for (float i = 0.0, ofs = 4.0, time = 1.0; i < MAX_SEARCH_ITER && time < times.y;
       i++, time += ofs, ofs = min(exp2(MAX_LOD) * 4.0, ofs + ofs * aoQuality)) {
    vec4 t = min(times.yyyy, vec4(time) + (vec4(0.25, 0.5, 0.75, 1.0) - jitter) * ofs);
    vec4 cos1 = uvs.xyxy + t_phi.xyxy * t.xxyy;
    vec4 cos2 = uvs.xyxy + t_phi.xyxy * t.zzww;
    float lod = min(MAX_LOD, max(i - jitter * 4.0, 0.0) * aoQuality);
    get_max_horizon_grouped(cos1, cos2, pos, lod, h.x);
  }

  return h;
}

void integrate_slice(
    vec3 normal, vec2 t_phi, vec2 horizons, inout float visibility, inout vec3 bent_normal)
{
  /* Projecting Normal to Plane P defined by t_phi and omega_o */
  vec3 np = vec3(t_phi.y, -t_phi.x, 0.0); /* Normal vector to Integration plane */
  vec3 t = vec3(-t_phi, 0.0);
  vec3 n_proj = normal - np * dot(np, normal);
  float n_proj_len = max(1e-16, length(n_proj));

  float cos_n = clamp(n_proj.z / n_proj_len, -1.0, 1.0);
  float n = sign(dot(n_proj, t)) * fast_acos(cos_n); /* Angle between view vec and normal */

  /* (Slide 54) */
  vec2 h = fast_acos(horizons);
  h.x = -h.x;

  /* Clamping thetas (slide 58) */
  h.x = n + max(h.x - n, -M_PI_2);
  h.y = n + min(h.y - n, M_PI_2);

  /* Solving inner integral */
  vec2 h_2 = 2.0 * h;
  vec2 vd = -cos(h_2 - n) + cos_n + h_2 * sin(n);
  float vis = (vd.x + vd.y) * 0.25 * n_proj_len;

  visibility += vis;

  /* O. Klehm, T. Ritschel, E. Eisemann, H.-P. Seidel
   * Bent Normals and Cones in Screen-space
   * Sec. 3.1 : Bent normals */
  float b_angle = (h.x + h.y) * 0.5;
  bent_normal += vec3(sin(b_angle) * -t_phi, cos(b_angle)) * vis;
}

void gtao_deferred(
    vec3 normal, vec4 noise, float frag_depth, out float visibility, out vec3 bent_normal)
{
  /* Fetch early, hide latency! */
  vec4 horizons = texelFetch(horizonBuffer, ivec2(gl_FragCoord.xy), 0);

  vec4 dirs;
  dirs.xy = get_ao_dir(noise.x * 0.5);
  dirs.zw = get_ao_dir(noise.x * 0.5 + 0.5);

  bent_normal = vec3(0.0);
  visibility = 0.0;

  horizons = unpack_horizons(horizons);

  integrate_slice(normal, dirs.xy, horizons.xy, visibility, bent_normal);
  integrate_slice(normal, dirs.zw, horizons.zw, visibility, bent_normal);

  bent_normal = normalize(bent_normal / visibility);

  visibility *= 0.5; /* We integrated 2 slices. */
}

void gtao(vec3 normal, vec3 position, vec4 noise, out float visibility, out vec3 bent_normal)
{
  vec2 uvs = get_uvs_from_view(position);
  vec2 max_dir = get_max_dir(position.z);
  vec2 dir = get_ao_dir(noise.x);

  bent_normal = vec3(0.0);
  visibility = 0.0;

  /* Only trace in 2 directions. May lead to a darker result but since it's mostly for
   * alpha blended objects that will have overdraw, we limit the performance impact. */
  vec2 horizons = search_horizon_sweep(dir, position, uvs, noise.y, max_dir);
  integrate_slice(normal, dir, horizons, visibility, bent_normal);

  bent_normal = normalize(bent_normal / visibility);
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

/* Use the right occlusion  */
float occlusion_compute(vec3 N, vec3 vpos, float user_occlusion, vec4 rand, out vec3 bent_normal)
{
#ifndef USE_REFRACTION
  if ((int(aoSettings) & USE_AO) != 0) {
    float visibility;
    vec3 vnor = mat3(ViewMatrix) * N;

#  ifdef ENABLE_DEFERED_AO
    gtao_deferred(vnor, rand, gl_FragCoord.z, visibility, bent_normal);
#  else
    gtao(vnor, vpos, rand, visibility, bent_normal);
#  endif

    /* Prevent some problems down the road. */
    visibility = max(1e-3, visibility);

    if ((int(aoSettings) & USE_BENT_NORMAL) != 0) {
      /* The bent normal will show the facet look of the mesh. Try to minimize this. */
      float mix_fac = visibility * visibility * visibility;
      bent_normal = normalize(mix(bent_normal, vnor, mix_fac));

      bent_normal = transform_direction(ViewMatrixInverse, bent_normal);
    }
    else {
      bent_normal = N;
    }

    /* Scale by user factor */
    visibility = pow(visibility, aoFactor);

    return min(visibility, user_occlusion);
  }
#endif

  bent_normal = N;
  return user_occlusion;
}
