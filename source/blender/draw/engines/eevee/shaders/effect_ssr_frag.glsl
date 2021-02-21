
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(closure_eval_glossy_lib.glsl)
#pragma BLENDER_REQUIRE(closure_eval_lib.glsl)
#pragma BLENDER_REQUIRE(raytrace_lib.glsl)
#pragma BLENDER_REQUIRE(lightprobe_lib.glsl)
#pragma BLENDER_REQUIRE(ssr_lib.glsl)

/* Based on Stochastic Screen Space Reflections
 * https://www.ea.com/frostbite/news/stochastic-screen-space-reflections */

#define MAX_MIP 9.0

uniform ivec2 halfresOffset;

ivec2 encode_hit_data(vec2 hit_pos, bool has_hit, bool is_planar)
{
  ivec2 hit_data = ivec2(saturate(hit_pos) * 32767.0); /* 16bit signed int limit */
  hit_data.x *= (is_planar) ? -1 : 1;
  hit_data.y *= (has_hit) ? 1 : -1;
  return hit_data;
}

vec2 decode_hit_data(vec2 hit_data, out bool has_hit, out bool is_planar)
{
  is_planar = (hit_data.x < 0);
  has_hit = (hit_data.y > 0);
  vec2 hit_co = vec2(abs(hit_data)) / 32767.0; /* 16bit signed int limit */
  if (is_planar) {
    hit_co.x = 1.0 - hit_co.x;
  }
  return hit_co;
}

#ifdef STEP_RAYTRACE

uniform sampler2D normalBuffer;
uniform sampler2D specroughBuffer;

layout(location = 0) out ivec2 hitData;
layout(location = 1) out float pdfData;

void do_planar_ssr(
    int index, vec3 V, vec3 N, vec3 T, vec3 B, vec3 planeNormal, vec3 vP, float a2, vec4 rand)
{
  float NH;
  vec3 H = sample_ggx(rand.xzw, a2, N, T, B, NH); /* Microfacet normal */
  float pdf = pdf_ggx_reflect(NH, a2);

  vec3 R = reflect(-V, H);
  R = reflect(R, planeNormal);

  /* If ray is bad (i.e. going below the plane) regenerate. */
  if (dot(R, planeNormal) > 0.0) {
    vec3 H = sample_ggx(rand.xzw * vec3(1.0, -1.0, -1.0), a2, N, T, B, NH); /* Microfacet normal */
    pdf = pdf_ggx_reflect(NH, a2);

    R = reflect(-V, H);
    R = reflect(R, planeNormal);
  }

  pdfData = min(1024e32, pdf); /* Theoretical limit of 16bit float */

  /* Since viewspace hit position can land behind the camera in this case,
   * we save the reflected view position (visualize it as the hit position
   * below the reflection plane). This way it's garanted that the hit will
   * be in front of the camera. That let us tag the bad rays with a negative
   * sign in the Z component. */
  vec3 hit_pos = raycast(index, vP, R * 1e16, 1e16, rand.y, ssrQuality, a2, false);

  hitData = encode_hit_data(hit_pos.xy, (hit_pos.z > 0.0), true);
}

void do_ssr(vec3 V, vec3 N, vec3 T, vec3 B, vec3 vP, float a2, vec4 rand)
{
  float NH;
  /* Microfacet normal */
  vec3 H = sample_ggx(rand.xzw, a2, N, T, B, NH);
  vec3 R = reflect(-V, H);

  /* If ray is bad (i.e. going below the surface) regenerate. */
  /* This threshold is a bit higher than 0 to improve self intersection cases. */
  const float bad_ray_threshold = 0.085;
  if (dot(R, N) <= bad_ray_threshold) {
    H = sample_ggx(rand.xzw * vec3(1.0, -1.0, -1.0), a2, N, T, B, NH);
    R = reflect(-V, H);
  }

  if (dot(R, N) <= bad_ray_threshold) {
    H = sample_ggx(rand.xzw * vec3(1.0, 1.0, -1.0), a2, N, T, B, NH);
    R = reflect(-V, H);
  }

  if (dot(R, N) <= bad_ray_threshold) {
    H = sample_ggx(rand.xzw * vec3(1.0, -1.0, 1.0), a2, N, T, B, NH);
    R = reflect(-V, H);
  }

  if (dot(R, N) <= bad_ray_threshold) {
    /* Not worth tracing. */
    return;
  }

  pdfData = min(1024e32, pdf_ggx_reflect(NH, a2)); /* Theoretical limit of 16bit float */

  vec3 hit_pos = raycast(-1, vP, R * 1e16, ssrThickness, rand.y, ssrQuality, a2, true);

  hitData = encode_hit_data(hit_pos.xy, (hit_pos.z > 0.0), false);
}

void main()
{
#  ifdef FULLRES
  ivec2 fullres_texel = ivec2(gl_FragCoord.xy);
  ivec2 halfres_texel = fullres_texel;
#  else
  ivec2 fullres_texel = ivec2(gl_FragCoord.xy) * 2 + halfresOffset;
  ivec2 halfres_texel = ivec2(gl_FragCoord.xy);
#  endif

  float depth = texelFetch(depthBuffer, fullres_texel, 0).r;

  /* Default: not hits. */
  hitData = encode_hit_data(vec2(0.5), false, false);
  pdfData = 0.0;

  /* Early out */
  /* We can't do discard because we don't clear the render target. */
  if (depth == 1.0) {
    return;
  }

  vec2 uvs = vec2(fullres_texel) / vec2(textureSize(depthBuffer, 0));

  /* Using view space */
  vec3 vP = get_view_space_from_depth(uvs, depth);
  vec3 P = transform_point(ViewMatrixInverse, vP);
  vec3 vV = viewCameraVec(vP);
  vec3 V = cameraVec(P);
  vec3 vN = normal_decode(texelFetch(normalBuffer, fullres_texel, 0).rg, vV);
  vec3 N = transform_direction(ViewMatrixInverse, vN);

  /* Retrieve pixel data */
  vec4 speccol_roughness = texelFetch(specroughBuffer, fullres_texel, 0).rgba;

  /* Early out */
  if (dot(speccol_roughness.rgb, vec3(1.0)) == 0.0) {
    return;
  }

  float roughness = speccol_roughness.a;
  float roughnessSquared = max(1e-3, roughness * roughness);
  float a2 = roughnessSquared * roughnessSquared;

  /* Early out */
  if (roughness > ssrMaxRoughness + 0.2) {
    return;
  }

  vec4 rand = texelfetch_noise_tex(halfres_texel);

  /* Gives *perfect* reflection for very small roughness */
  if (roughness < 0.04) {
    rand.xzw *= 0.0;
  }
  /* Importance sampling bias */
  rand.x = mix(rand.x, 0.0, ssrBrdfBias);

  vec3 vT, vB;
  make_orthonormal_basis(vN, vT, vB); /* Generate tangent space */

  /* Planar Reflections */
  for (int i = 0; i < MAX_PLANAR && i < prbNumPlanar; i++) {
    PlanarData pd = planars_data[i];

    float fade = probe_attenuation_planar(pd, P);
    fade *= probe_attenuation_planar_normal_roughness(pd, N, 0.0);

    if (fade > 0.5) {
      /* Find view vector / reflection plane intersection. */
      /* TODO optimize, use view space for all. */
      vec3 tracePosition = line_plane_intersect(P, V, pd.pl_plane_eq);
      tracePosition = transform_point(ViewMatrix, tracePosition);
      vec3 planeNormal = transform_direction(ViewMatrix, pd.pl_normal);

      do_planar_ssr(i, vV, vN, vT, vB, planeNormal, tracePosition, a2, rand);
      return;
    }
  }

  /* Constant bias (due to depth buffer precision). Helps with self intersection. */
  /* Magic numbers for 24bits of precision.
   * From http://terathon.com/gdc07_lengyel.pdf (slide 26) */
  vP.z = get_view_z_from_depth(depth - mix(2.4e-7, 4.8e-7, depth));

  do_ssr(vV, vN, vT, vB, vP, a2, rand);
}

#else /* STEP_RESOLVE */

uniform sampler2D prevColorBuffer; /* previous frame */
uniform sampler2D normalBuffer;
uniform sampler2D specroughBuffer;

uniform isampler2D hitBuffer;
uniform sampler2D pdfBuffer;

uniform int neighborOffset;

in vec4 uvcoordsvar;

const ivec2 neighbors[32] = ivec2[32](ivec2(0, 0),
                                      ivec2(1, 1),
                                      ivec2(-2, 0),
                                      ivec2(0, -2),
                                      ivec2(0, 0),
                                      ivec2(1, -1),
                                      ivec2(-2, 0),
                                      ivec2(0, 2),
                                      ivec2(0, 0),
                                      ivec2(-1, -1),
                                      ivec2(2, 0),
                                      ivec2(0, 2),
                                      ivec2(0, 0),
                                      ivec2(-1, 1),
                                      ivec2(2, 0),
                                      ivec2(0, -2),

                                      ivec2(0, 0),
                                      ivec2(2, 2),
                                      ivec2(-2, 2),
                                      ivec2(0, -1),
                                      ivec2(0, 0),
                                      ivec2(2, -2),
                                      ivec2(-2, -2),
                                      ivec2(0, 1),
                                      ivec2(0, 0),
                                      ivec2(-2, -2),
                                      ivec2(-2, 2),
                                      ivec2(1, 0),
                                      ivec2(0, 0),
                                      ivec2(2, 2),
                                      ivec2(2, -2),
                                      ivec2(-1, 0));

out vec4 fragColor;

#  if 0 /* Finish reprojection with motion vectors */
vec3 get_motion_vector(vec3 pos)
{
}

/* http://bitsquid.blogspot.fr/2017/06/reprojecting-reflections_22.html */
vec3 find_reflection_incident_point(vec3 cam, vec3 hit, vec3 pos, vec3 N)
{
  float d_cam = point_plane_projection_dist(cam, pos, N);
  float d_hit = point_plane_projection_dist(hit, pos, N);

  if (d_hit < d_cam) {
    /* Swap */
    float tmp = d_cam;
    d_cam = d_hit;
    d_hit = tmp;
  }

  vec3 proj_cam = cam - (N * d_cam);
  vec3 proj_hit = hit - (N * d_hit);

  return (proj_hit - proj_cam) * d_cam / (d_cam + d_hit) + proj_cam;
}
#  endif

float brightness(vec3 c)
{
  return max(max(c.r, c.g), c.b);
}

vec2 get_reprojected_reflection(vec3 hit, vec3 pos, vec3 N)
{
  /* TODO real reprojection with motion vectors, etc... */
  return project_point(pastViewProjectionMatrix, hit).xy * 0.5 + 0.5;
}

float get_sample_depth(vec2 hit_co, bool is_planar, float planar_index)
{
  if (is_planar) {
    hit_co.x = 1.0 - hit_co.x;
    return textureLod(planarDepth, vec3(hit_co, planar_index), 0.0).r;
  }
  else {
    return textureLod(depthBuffer, hit_co, 0.0).r;
  }
}

vec3 get_hit_vector(vec3 hit_pos,
                    PlanarData pd,
                    vec3 P,
                    vec3 N,
                    vec3 V,
                    bool is_planar,
                    inout vec2 hit_co,
                    inout float mask)
{
  vec3 hit_vec;

  if (is_planar) {
    /* Reflect back the hit position to have it in non-reflected world space */
    vec3 trace_pos = line_plane_intersect(P, V, pd.pl_plane_eq);
    hit_vec = hit_pos - trace_pos;
    hit_vec = reflect(hit_vec, pd.pl_normal);
    /* Modify here so mip texel alignment is correct. */
    hit_co.x = 1.0 - hit_co.x;
  }
  else {
    /* Find hit position in previous frame. */
    hit_co = get_reprojected_reflection(hit_pos, P, N);
    hit_vec = hit_pos - P;
  }

  mask = screen_border_mask(hit_co);
  return hit_vec;
}

vec3 get_scene_color(vec2 ref_uvs, float mip, float planar_index, bool is_planar)
{
  if (is_planar) {
    return textureLod(probePlanars, vec3(ref_uvs, planar_index), min(mip, prbLodPlanarMax)).rgb;
  }
  else {
    return textureLod(prevColorBuffer, ref_uvs, mip).rgb;
  }
}

vec4 get_ssr_samples(vec4 hit_pdf,
                     ivec4 hit_data[2],
                     PlanarData pd,
                     float planar_index,
                     vec3 P,
                     vec3 N,
                     vec3 V,
                     float roughnessSquared,
                     float cone_tan,
                     vec2 source_uvs,
                     inout float weight_acc)
{
  bvec4 is_planar, has_hit;
  vec4 hit_co[2];
  hit_co[0].xy = decode_hit_data(hit_data[0].xy, has_hit.x, is_planar.x);
  hit_co[0].zw = decode_hit_data(hit_data[0].zw, has_hit.y, is_planar.y);
  hit_co[1].xy = decode_hit_data(hit_data[1].xy, has_hit.z, is_planar.z);
  hit_co[1].zw = decode_hit_data(hit_data[1].zw, has_hit.w, is_planar.w);

  vec4 hit_depth;
  hit_depth.x = get_sample_depth(hit_co[0].xy, is_planar.x, planar_index);
  hit_depth.y = get_sample_depth(hit_co[0].zw, is_planar.y, planar_index);
  hit_depth.z = get_sample_depth(hit_co[1].xy, is_planar.z, planar_index);
  hit_depth.w = get_sample_depth(hit_co[1].zw, is_planar.w, planar_index);

  /* Hit position in view space. */
  vec3 hit_view[4];
  hit_view[0] = get_view_space_from_depth(hit_co[0].xy, hit_depth.x);
  hit_view[1] = get_view_space_from_depth(hit_co[0].zw, hit_depth.y);
  hit_view[2] = get_view_space_from_depth(hit_co[1].xy, hit_depth.z);
  hit_view[3] = get_view_space_from_depth(hit_co[1].zw, hit_depth.w);

  vec4 homcoord = vec4(hit_view[0].z, hit_view[1].z, hit_view[2].z, hit_view[3].z);
  homcoord = ProjectionMatrix[2][3] * homcoord + ProjectionMatrix[3][3];

  /* Hit position in world space. */
  vec3 hit_pos[4];
  hit_pos[0] = transform_point(ViewMatrixInverse, hit_view[0]);
  hit_pos[1] = transform_point(ViewMatrixInverse, hit_view[1]);
  hit_pos[2] = transform_point(ViewMatrixInverse, hit_view[2]);
  hit_pos[3] = transform_point(ViewMatrixInverse, hit_view[3]);

  /* Get actual hit vector and hit coordinate (from last frame). */
  vec4 mask = vec4(1.0);
  hit_pos[0] = get_hit_vector(hit_pos[0], pd, P, N, V, is_planar.x, hit_co[0].xy, mask.x);
  hit_pos[1] = get_hit_vector(hit_pos[1], pd, P, N, V, is_planar.y, hit_co[0].zw, mask.y);
  hit_pos[2] = get_hit_vector(hit_pos[2], pd, P, N, V, is_planar.z, hit_co[1].xy, mask.z);
  hit_pos[3] = get_hit_vector(hit_pos[3], pd, P, N, V, is_planar.w, hit_co[1].zw, mask.w);

  vec4 hit_dist;
  hit_dist.x = length(hit_pos[0]);
  hit_dist.y = length(hit_pos[1]);
  hit_dist.z = length(hit_pos[2]);
  hit_dist.w = length(hit_pos[3]);
  hit_dist = max(vec4(1e-8), hit_dist);

  /* Normalize */
  hit_pos[0] /= hit_dist.x;
  hit_pos[1] /= hit_dist.y;
  hit_pos[2] /= hit_dist.z;
  hit_pos[3] /= hit_dist.w;

  /* Compute cone footprint in screen space. */
  vec4 cone_footprint = hit_dist * cone_tan;
  cone_footprint = ssrBrdfBias * 0.5 * cone_footprint *
                   max(ProjectionMatrix[0][0], ProjectionMatrix[1][1]) / homcoord;

  /* Estimate a cone footprint to sample a corresponding mipmap level. */
  vec4 mip = log2(cone_footprint * max_v2(vec2(textureSize(depthBuffer, 0))));
  mip = clamp(mip, 0.0, MAX_MIP);

  /* Correct UVs for mipmaping mis-alignment */
  hit_co[0].xy *= mip_ratio_interp(mip.x);
  hit_co[0].zw *= mip_ratio_interp(mip.y);
  hit_co[1].xy *= mip_ratio_interp(mip.z);
  hit_co[1].zw *= mip_ratio_interp(mip.w);

  /* Slide 54 */
  vec4 bsdf;
  bsdf.x = bsdf_ggx(N, hit_pos[0], V, roughnessSquared);
  bsdf.y = bsdf_ggx(N, hit_pos[1], V, roughnessSquared);
  bsdf.z = bsdf_ggx(N, hit_pos[2], V, roughnessSquared);
  bsdf.w = bsdf_ggx(N, hit_pos[3], V, roughnessSquared);

  vec4 weight = step(1e-8, hit_pdf) * bsdf / max(vec4(1e-8), hit_pdf);

  vec3 sample[4];
  sample[0] = get_scene_color(hit_co[0].xy, mip.x, planar_index, is_planar.x);
  sample[1] = get_scene_color(hit_co[0].zw, mip.y, planar_index, is_planar.y);
  sample[2] = get_scene_color(hit_co[1].xy, mip.z, planar_index, is_planar.z);
  sample[3] = get_scene_color(hit_co[1].zw, mip.w, planar_index, is_planar.w);

  /* Clamped brightness. */
  vec4 luma;
  luma.x = brightness(sample[0]);
  luma.y = brightness(sample[1]);
  luma.z = brightness(sample[2]);
  luma.w = brightness(sample[3]);
  luma = max(vec4(1e-8), luma);
  luma = 1.0 - max(vec4(0.0), luma - ssrFireflyFac) / luma;

  sample[0] *= luma.x;
  sample[1] *= luma.y;
  sample[2] *= luma.z;
  sample[3] *= luma.w;

  /* Protection against NaNs in the history buffer.
   * This could be removed if some previous pass has already
   * sanitized the input. */
  if (any(isnan(sample[0]))) {
    sample[0] = vec3(0.0);
    weight.x = 0.0;
  }
  if (any(isnan(sample[1]))) {
    sample[1] = vec3(0.0);
    weight.y = 0.0;
  }
  if (any(isnan(sample[2]))) {
    sample[2] = vec3(0.0);
    weight.z = 0.0;
  }
  if (any(isnan(sample[3]))) {
    sample[3] = vec3(0.0);
    weight.w = 0.0;
  }

  weight_acc += sum(weight);

  /* Do not add light if ray has failed. */
  vec4 accum;
  accum = vec4(sample[0], mask.x) * weight.x * float(has_hit.x);
  accum += vec4(sample[1], mask.y) * weight.y * float(has_hit.y);
  accum += vec4(sample[2], mask.z) * weight.z * float(has_hit.z);
  accum += vec4(sample[3], mask.w) * weight.w * float(has_hit.w);
  return accum;
}

void raytrace_resolve(ClosureInputGlossy cl_in,
                      inout ClosureEvalGlossy cl_eval,
                      inout ClosureEvalCommon cl_common,
                      inout ClosureOutputGlossy cl_out)
{
#  ifdef FULLRES
  ivec2 texel = ivec2(gl_FragCoord.xy);
#  else
  ivec2 texel = ivec2(gl_FragCoord.xy / 2.0);
#  endif
  /* Using world space */
  vec3 V = cl_common.V;
  vec3 N = cl_in.N;
  vec3 P = cl_common.P;

  float roughness = cl_in.roughness;
  float roughnessSquared = max(1e-3, sqr(roughness));

  /* Resolve SSR */
  float cone_cos = cone_cosine(roughnessSquared);
  float cone_tan = sqrt(1 - cone_cos * cone_cos) / cone_cos;
  cone_tan *= mix(saturate(dot(N, -V) * 2.0), 1.0, roughness); /* Elongation fit */

  vec2 source_uvs = project_point(pastViewProjectionMatrix, P).xy * 0.5 + 0.5;

  vec4 ssr_accum = vec4(0.0);
  float weight_acc = 0.0;

  if (roughness < ssrMaxRoughness + 0.2) {
    /* TODO optimize with textureGather */
    /* Doing these fetches early to hide latency. */
    vec4 hit_pdf;
    hit_pdf.x = texelFetch(pdfBuffer, texel + neighbors[0 + neighborOffset], 0).r;
    hit_pdf.y = texelFetch(pdfBuffer, texel + neighbors[1 + neighborOffset], 0).r;
    hit_pdf.z = texelFetch(pdfBuffer, texel + neighbors[2 + neighborOffset], 0).r;
    hit_pdf.w = texelFetch(pdfBuffer, texel + neighbors[3 + neighborOffset], 0).r;

    ivec4 hit_data[2];
    hit_data[0].xy = texelFetch(hitBuffer, texel + neighbors[0 + neighborOffset], 0).rg;
    hit_data[0].zw = texelFetch(hitBuffer, texel + neighbors[1 + neighborOffset], 0).rg;
    hit_data[1].xy = texelFetch(hitBuffer, texel + neighbors[2 + neighborOffset], 0).rg;
    hit_data[1].zw = texelFetch(hitBuffer, texel + neighbors[3 + neighborOffset], 0).rg;

    /* Find Planar Reflections affecting this pixel */
    PlanarData pd;
    float planar_index;
    for (int i = 0; i < MAX_PLANAR && i < prbNumPlanar; i++) {
      pd = planars_data[i];

      float fade = probe_attenuation_planar(pd, P);
      fade *= probe_attenuation_planar_normal_roughness(pd, N, 0.0);

      if (fade > 0.5) {
        planar_index = float(i);
        break;
      }
    }

    ssr_accum += get_ssr_samples(hit_pdf,
                                 hit_data,
                                 pd,
                                 planar_index,
                                 P,
                                 N,
                                 V,
                                 roughnessSquared,
                                 cone_tan,
                                 source_uvs,
                                 weight_acc);
  }

  /* Compute SSR contribution */
  ssr_accum *= (weight_acc == 0.0) ? 0.0 : (1.0 / weight_acc);
  /* fade between 0.5 and 1.0 roughness */
  ssr_accum.a *= smoothstep(ssrMaxRoughness + 0.2, ssrMaxRoughness, roughness);

  cl_eval.raytrace_radiance = ssr_accum.rgb * ssr_accum.a;
  cl_common.specular_accum -= ssr_accum.a;
}

CLOSURE_EVAL_FUNCTION_DECLARE_1(ssr_resolve, Glossy)

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);
  float depth = texelFetch(depthBuffer, texel, 0).r;

  if (depth == 1.0) {
    discard;
  }

  vec4 speccol_roughness = texelFetch(specroughBuffer, texel, 0).rgba;
  vec3 brdf = speccol_roughness.rgb;
  float roughness = speccol_roughness.a;

  if (max_v3(brdf) <= 0.0) {
    discard;
  }

  viewPosition = get_view_space_from_depth(uvcoordsvar.xy, depth);
  worldPosition = transform_point(ViewMatrixInverse, viewPosition);

  vec2 normal_encoded = texelFetch(normalBuffer, texel, 0).rg;
  viewNormal = normal_decode(normal_encoded, viewCameraVec(viewPosition));
  worldNormal = transform_direction(ViewMatrixInverse, viewNormal);

  CLOSURE_VARS_DECLARE_1(Glossy);

  in_Glossy_0.N = worldNormal;
  in_Glossy_0.roughness = roughness;

  /* Do a full deferred evaluation of the glossy BSDF. The only difference is that we inject the
   * SSR resolve before the cubemap iter. BRDF term is already computed during main pass and is
   * passed as specular color. */
  CLOSURE_EVAL_FUNCTION_1(ssr_resolve, Glossy);

  fragColor = vec4(out_Glossy_0.radiance * brdf, 1.0);
}

#endif
