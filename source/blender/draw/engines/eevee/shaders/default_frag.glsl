
uniform vec3 basecol;
uniform float metallic;
uniform float specular;
uniform float roughness;

Closure nodetree_exec(void)
{
#ifdef HAIR_SHADER
  vec3 B = normalize(cross(worldNormal, hairTangent));
  float cos_theta;
  if (hairThicknessRes == 1) {
    vec4 rand = texelFetch(utilTex, ivec3(ivec2(gl_FragCoord.xy) % LUT_SIZE, 2.0), 0);
    /* Random cosine normal distribution on the hair surface. */
    cos_theta = rand.x * 2.0 - 1.0;
  }
  else {
    /* Shade as a cylinder. */
    cos_theta = hairThickTime / hairThickness;
  }
  float sin_theta = sqrt(max(0.0, 1.0f - cos_theta * cos_theta));
  vec3 N = normalize(worldNormal * sin_theta + B * cos_theta);
  vec3 vN = mat3(ViewMatrix) * N;
#else
  vec3 N = normalize(gl_FrontFacing ? worldNormal : -worldNormal);
  vec3 vN = normalize(gl_FrontFacing ? viewNormal : -viewNormal);
#endif

  vec3 dielectric = vec3(0.034) * specular * 2.0;
  vec3 albedo = mix(basecol, vec3(0.0), metallic);
  vec3 f0 = mix(dielectric, basecol, metallic);
  vec3 f90 = mix(vec3(1.0), f0, (1.0 - specular) * metallic);
  vec3 out_diff, out_spec, ssr_spec;
  eevee_closure_default(N, albedo, f0, f90, 1, roughness, 1.0, true, out_diff, out_spec, ssr_spec);

  Closure cl = CLOSURE_DEFAULT;
  cl.radiance = render_pass_glossy_mask(vec3(1.0), out_spec) +
                render_pass_diffuse_mask(albedo, out_diff * albedo);
  closure_load_ssr_data(ssr_spec, roughness, N, viewCameraVec, 1, cl);

#ifdef LOOKDEV
  gl_FragDepth = 0.0;
#endif

#ifdef HOLDOUT
  cl = CLOSURE_DEFAULT;
  cl.holdout = 1.0;
#endif

  return cl;
}
