
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(closure_eval_glossy_lib.glsl)
#pragma BLENDER_REQUIRE(closure_eval_lib.glsl)
#pragma BLENDER_REQUIRE(raytrace_lib.glsl)
#pragma BLENDER_REQUIRE(lightprobe_lib.glsl)
#pragma BLENDER_REQUIRE(bsdf_common_lib.glsl)
#pragma BLENDER_REQUIRE(bsdf_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(surface_lib.glsl)

/* Based on:
 * "Stochastic Screen Space Reflections"
 * by Tomasz Stachowiak.
 * https://www.ea.com/frostbite/news/stochastic-screen-space-reflections
 * and
 * "Stochastic all the things: raytracing in hybrid real-time rendering"
 * by Tomasz Stachowiak.
 * https://media.contentapi.ea.com/content/dam/ea/seed/presentations/dd18-seed-raytracing-in-hybrid-real-time-rendering.pdf
 */

uniform ivec2 halfresOffset;

struct HitData {
  /** Hit direction scaled by intersection time. */
  vec3 hit_dir;
  /** Screen space [0..1] depth of the reflection hit position, or -1.0 for planar reflections. */
  float hit_depth;
  /** Inverse probability of ray spawning in this direction. */
  float ray_pdf_inv;
  /** True if ray has hit valid geometry. */
  bool is_hit;
  /** True if ray was generated from a planar reflection probe. */
  bool is_planar;
};

void encode_hit_data(HitData data, vec3 hit_sP, vec3 vP, out vec4 hit_data, out float hit_depth)
{
  vec3 hit_vP = get_view_space_from_depth(hit_sP.xy, hit_sP.z);
  hit_data.xyz = hit_vP - vP;
  hit_depth = data.is_planar ? -1.0 : hit_sP.z;
  /* Record 1.0 / pdf to reduce the computation in the resolve phase. */
  /* Encode hit validity in sign. */
  hit_data.w = data.ray_pdf_inv * ((data.is_hit) ? 1.0 : -1.0);
}

HitData decode_hit_data(vec4 hit_data, float hit_depth)
{
  HitData data;
  data.hit_dir.xyz = hit_data.xyz;
  data.hit_depth = hit_depth;
  data.is_planar = (hit_depth == -1.0);
  data.ray_pdf_inv = abs(hit_data.w);
  data.is_hit = (hit_data.w > 0.0);
  return data;
}

#ifdef STEP_RAYTRACE

uniform sampler2D normalBuffer;
uniform sampler2D specroughBuffer;

layout(location = 0) out vec4 hitData;
layout(location = 1) out float hitDepth;

void do_planar_ssr(int index,
                   vec3 vV,
                   vec3 vN,
                   vec3 vT,
                   vec3 vB,
                   vec3 viewPlaneNormal,
                   vec3 vP,
                   float alpha,
                   vec4 rand)
{
  float pdf;
  /* Microfacet normal */
  vec3 vH = sample_ggx(rand.xzw, alpha, vV, vN, vT, vB, pdf);
  vec3 vR = reflect(-vV, vH);
  vR = reflect(vR, viewPlaneNormal);

  Ray ray;
  ray.origin = vP;
  ray.direction = vR * 1e16;

  RayTraceParameters params;
  params.jitter = rand.y;
  params.trace_quality = ssrQuality;
  params.roughness = alpha * alpha;

  vec3 hit_sP;
  HitData data;
  data.is_planar = true;
  data.ray_pdf_inv = safe_rcp(pdf);
  data.is_hit = raytrace_planar(ray, params, index, hit_sP);

  encode_hit_data(data, hit_sP, ray.origin, hitData, hitDepth);
}

void do_ssr(vec3 vV, vec3 vN, vec3 vT, vec3 vB, vec3 vP, float alpha, vec4 rand)
{
  float pdf;
  /* Microfacet normal */
  vec3 vH = sample_ggx(rand.xzw, alpha, vV, vN, vT, vB, pdf);
  vec3 vR = reflect(-vV, vH);

  Ray ray;
  ray.origin = vP + vN * 1e-4;
  ray.direction = vR * 1e16;

  RayTraceParameters params;
  params.thickness = ssrThickness;
  params.jitter = rand.y;
  params.trace_quality = ssrQuality;
  params.roughness = alpha * alpha;

  vec3 hit_sP;
  HitData data;
  data.is_planar = false;
  data.ray_pdf_inv = safe_rcp(pdf);
  data.is_hit = raytrace(ray, params, true, hit_sP);

  encode_hit_data(data, hit_sP, ray.origin, hitData, hitDepth);
}

in vec4 uvcoordsvar;

void main()
{
  vec2 uvs = uvcoordsvar.xy;
  float depth = textureLod(maxzBuffer, uvs * hizUvScale.xy, 0.0).r;

  HitData data;
  data.is_planar = false;
  data.ray_pdf_inv = 0.0;
  data.is_hit = false;
  data.hit_dir = vec3(0.0, 0.0, 0.0);

  /* Default: not hits. */
  encode_hit_data(data, data.hit_dir, data.hit_dir, hitData, hitDepth);

  /* Early out */
  /* We can't do discard because we don't clear the render target. */
  if (depth == 1.0) {
    return;
  }

  /* Using view space */
  vec3 vP = get_view_space_from_depth(uvs, depth);
  vec3 P = transform_point(ViewMatrixInverse, vP);
  vec3 vV = viewCameraVec(vP);
  vec3 V = cameraVec(P);
  vec3 vN = normal_decode(texture(normalBuffer, uvs, 0).rg, vV);
  vec3 N = transform_direction(ViewMatrixInverse, vN);

  /* Retrieve pixel data */
  vec4 speccol_roughness = texture(specroughBuffer, uvs, 0).rgba;

  /* Early out */
  if (dot(speccol_roughness.rgb, vec3(1.0)) == 0.0) {
    return;
  }

  float roughness = speccol_roughness.a;
  float alpha = max(1e-3, roughness * roughness);

  /* Early out */
  if (roughness > ssrMaxRoughness + 0.2) {
    return;
  }

  vec4 rand = texelfetch_noise_tex(vec2(gl_FragCoord.xy));

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
      vec3 viewPlaneNormal = transform_direction(ViewMatrix, pd.pl_normal);

      do_planar_ssr(i, vV, vN, vT, vB, viewPlaneNormal, tracePosition, alpha, rand);
      return;
    }
  }

  do_ssr(vV, vN, vT, vB, vP, alpha, rand);
}

#else /* STEP_RESOLVE */

uniform sampler2D colorBuffer; /* previous frame */
uniform sampler2D normalBuffer;
uniform sampler2D specroughBuffer;
uniform sampler2D hitBuffer;
uniform sampler2D hitDepth;

in vec4 uvcoordsvar;

out vec4 fragColor;

float brightness(vec3 c)
{
  return max(max(c.r, c.g), c.b);
}

vec4 ssr_get_scene_color_and_mask(vec3 hit_vP, int planar_index, float mip)
{
  vec2 uv;
  if (planar_index != -1) {
    uv = get_uvs_from_view(hit_vP);
    /* Planar X axis is flipped. */
    uv.x = 1.0 - uv.x;
  }
  else {
    /* Find hit position in previous frame. */
    /* TODO Combine matrices. */
    vec3 hit_P = transform_point(ViewMatrixInverse, hit_vP);
    /* TODO real reprojection with motion vectors, etc... */
    uv = project_point(pastViewProjectionMatrix, hit_P).xy * 0.5 + 0.5;
  }

  vec3 color;
  if (planar_index != -1) {
    color = textureLod(probePlanars, vec3(uv, planar_index), mip).rgb;
  }
  else {
    color = textureLod(colorBuffer, uv * hizUvScale.xy, mip).rgb;
  }

  /* Clamped brightness. */
  float luma = brightness(color);
  color *= 1.0 - max(0.0, luma - ssrFireflyFac) * safe_rcp(luma);

  float mask = screen_border_mask(uv);
  return vec4(color, mask);
}

void resolve_reflection_sample(int planar_index,
                               vec2 sample_uv,
                               vec3 vP,
                               vec3 vN,
                               vec3 vV,
                               float roughness_squared,
                               float cone_tan,
                               inout float weight_accum,
                               inout vec4 ssr_accum)
{
  vec4 hit_data = texture(hitBuffer, sample_uv * ssrUvScale);
  float hit_depth = texture(hitDepth, sample_uv * ssrUvScale).r;
  HitData data = decode_hit_data(hit_data, hit_depth);

  float hit_dist = length(data.hit_dir);

  /* Slide 54. */
  float bsdf = bsdf_ggx(vN, data.hit_dir / hit_dist, vV, roughness_squared);

  float weight = bsdf * data.ray_pdf_inv;

  /* Do not add light if ray has failed but still weight it. */
  if (!data.is_hit || (planar_index == -1 && data.is_planar) ||
      (planar_index != -1 && !data.is_planar)) {
    weight_accum += weight;
    return;
  }

  vec3 hit_vP = vP + data.hit_dir;

  /* Compute cone footprint in screen space. */
  float cone_footprint = hit_dist * cone_tan;
  float homcoord = ProjectionMatrix[2][3] * hit_vP.z + ProjectionMatrix[3][3];
  cone_footprint *= max(ProjectionMatrix[0][0], ProjectionMatrix[1][1]) / homcoord;
  cone_footprint *= ssrBrdfBias * 0.5;
  /* Estimate a cone footprint to sample a corresponding mipmap level. */
  float mip = log2(cone_footprint * max_v2(vec2(textureSize(specroughBuffer, 0))));

  vec4 radiance_mask = ssr_get_scene_color_and_mask(hit_vP, planar_index, mip);

  ssr_accum += radiance_mask * weight;
  weight_accum += weight;
}

void raytrace_resolve(ClosureInputGlossy cl_in,
                      inout ClosureEvalGlossy cl_eval,
                      inout ClosureEvalCommon cl_common,
                      inout ClosureOutputGlossy cl_out)
{
  float roughness = cl_in.roughness;

  vec4 ssr_accum = vec4(0.0);
  float weight_acc = 0.0;

  if (roughness < ssrMaxRoughness + 0.2) {
    /* Find Planar Reflections affecting this pixel */
    int planar_index = -1;
    for (int i = 0; i < MAX_PLANAR && i < prbNumPlanar; i++) {
      float fade = probe_attenuation_planar(planars_data[i], cl_common.P);
      fade *= probe_attenuation_planar_normal_roughness(planars_data[i], cl_in.N, 0.0);
      if (fade > 0.5) {
        planar_index = i;
        break;
      }
    }

    vec3 V, P, N;
    if (planar_index != -1) {
      PlanarData pd = planars_data[planar_index];
      /* Evaluate everything in refected space. */
      P = line_plane_intersect(cl_common.P, cl_common.V, pd.pl_plane_eq);
      V = reflect(cl_common.V, pd.pl_normal);
      N = reflect(cl_in.N, pd.pl_normal);
    }
    else {
      V = cl_common.V;
      P = cl_common.P;
      N = cl_in.N;
    }

    /* Using view space */
    vec3 vV = transform_direction(ViewMatrix, cl_common.V);
    vec3 vP = transform_point(ViewMatrix, cl_common.P);
    vec3 vN = transform_direction(ViewMatrix, cl_in.N);

    float roughness_squared = max(1e-3, sqr(roughness));
    float cone_cos = cone_cosine(roughness_squared);
    float cone_tan = sqrt(1.0 - cone_cos * cone_cos) / cone_cos;
    cone_tan *= mix(saturate(dot(vN, -vV) * 2.0), 1.0, roughness); /* Elongation fit */

    vec2 sample_uv = uvcoordsvar.xy;

    resolve_reflection_sample(
        planar_index, sample_uv, vP, vN, vV, roughness_squared, cone_tan, weight_acc, ssr_accum);
  }

  /* Compute SSR contribution */
  ssr_accum *= safe_rcp(weight_acc);
  /* fade between 0.5 and 1.0 roughness */
  ssr_accum.a *= smoothstep(ssrMaxRoughness + 0.2, ssrMaxRoughness, roughness);

  cl_eval.raytrace_radiance = ssr_accum.rgb * ssr_accum.a;
  cl_common.specular_accum -= ssr_accum.a;
}

CLOSURE_EVAL_FUNCTION_DECLARE_1(ssr_resolve, Glossy)

void main()
{
  float depth = textureLod(maxzBuffer, uvcoordsvar.xy * hizUvScale.xy, 0.0).r;

  if (depth == 1.0) {
    discard;
  }

  ivec2 texel = ivec2(gl_FragCoord.xy);
  vec4 speccol_roughness = texelFetch(specroughBuffer, texel, 0).rgba;
  vec3 brdf = speccol_roughness.rgb;
  float roughness = speccol_roughness.a;

  if (max_v3(brdf) <= 0.0) {
    discard;
  }

  FragDepth = depth;

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
