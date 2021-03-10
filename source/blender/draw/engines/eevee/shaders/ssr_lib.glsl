
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(bsdf_common_lib.glsl)
#pragma BLENDER_REQUIRE(bsdf_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(raytrace_lib.glsl)
#pragma BLENDER_REQUIRE(surface_lib.glsl)

/* ------------ Refraction ------------ */

#define BTDF_BIAS 0.85

uniform sampler2D refractColorBuffer;

uniform float refractionDepth;

vec4 screen_space_refraction(vec3 vP, vec3 N, vec3 V, float ior, float roughnessSquared, vec4 rand)
{
  float alpha = max(0.002, roughnessSquared);

  /* Importance sampling bias */
  rand.x = mix(rand.x, 0.0, BTDF_BIAS);

  vec3 T, B;
  make_orthonormal_basis(N, T, B);
  float pdf;
  /* Microfacet normal */
  vec3 H = sample_ggx(rand.xzw, alpha, V, N, T, B, pdf);

  /* If ray is bad (i.e. going below the plane) regenerate. */
  if (F_eta(ior, dot(H, V)) < 1.0) {
    H = sample_ggx(rand.xzw * vec3(1.0, -1.0, -1.0), alpha, V, N, T, B, pdf);
  }

  vec3 vV = viewCameraVec(vP);
  float eta = 1.0 / ior;
  if (dot(H, V) < 0.0) {
    H = -H;
    eta = ior;
  }

  vec3 R = refract(-V, H, 1.0 / ior);

  R = transform_direction(ViewMatrix, R);

  Ray ray;
  ray.origin = vP;
  ray.direction = R * 1e16;

  RayTraceParameters params;
  params.thickness = ssrThickness;
  params.jitter = rand.y;
  params.trace_quality = ssrQuality;
  params.roughness = roughnessSquared;

  vec3 hit_pos;
  bool hit = raytrace(ray, params, false, hit_pos);

  if (hit && (F_eta(ior, dot(H, V)) < 1.0)) {
    hit_pos = get_view_space_from_depth(hit_pos.xy, hit_pos.z);
    float hit_dist = distance(hit_pos, vP);

    float cone_cos = cone_cosine(roughnessSquared);
    float cone_tan = sqrt(1 - cone_cos * cone_cos) / cone_cos;

    /* Empirical fit for refraction. */
    /* TODO find a better fit or precompute inside the LUT. */
    cone_tan *= 0.5 * fast_sqrt(f0_from_ior((ior < 1.0) ? 1.0 / ior : ior));

    float cone_footprint = hit_dist * cone_tan;

    /* find the offset in screen space by multiplying a point
     * in camera space at the depth of the point by the projection matrix. */
    float homcoord = ProjectionMatrix[2][3] * hit_pos.z + ProjectionMatrix[3][3];
    /* UV space footprint */
    cone_footprint = BTDF_BIAS * 0.5 * max(ProjectionMatrix[0][0], ProjectionMatrix[1][1]) *
                     cone_footprint / homcoord;

    vec2 hit_uvs = project_point(ProjectionMatrix, hit_pos).xy * 0.5 + 0.5;

    /* Texel footprint */
    vec2 texture_size = vec2(textureSize(refractColorBuffer, 0).xy) / hizUvScale.xy;
    float mip = clamp(log2(cone_footprint * max(texture_size.x, texture_size.y)), 0.0, 9.0);

    vec3 spec = textureLod(refractColorBuffer, hit_uvs * hizUvScale.xy, mip).xyz;
    float mask = screen_border_mask(hit_uvs);

    return vec4(spec, mask);
  }

  return vec4(0.0);
}
