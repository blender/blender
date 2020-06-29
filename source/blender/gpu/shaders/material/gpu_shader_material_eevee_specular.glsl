#ifndef VOLUMETRICS
void node_eevee_specular(vec4 diffuse,
                         vec4 specular,
                         float roughness,
                         vec4 emissive,
                         float transp,
                         vec3 normal,
                         float clearcoat,
                         float clearcoat_roughness,
                         vec3 clearcoat_normal,
                         float occlusion,
                         float ssr_id,
                         out Closure result)
{
  normal = normalize(normal);

  vec3 out_diff, out_spec, ssr_spec;
  eevee_closure_default_clearcoat(normal,
                                  diffuse.rgb,
                                  specular.rgb,
                                  vec3(1.0),
                                  int(ssr_id),
                                  roughness,
                                  clearcoat_normal,
                                  clearcoat * 0.25,
                                  clearcoat_roughness,
                                  occlusion,
                                  true,
                                  out_diff,
                                  out_spec,
                                  ssr_spec);

  float alpha = 1.0 - transp;
  result = CLOSURE_DEFAULT;
  result.radiance = render_pass_diffuse_mask(diffuse.rgb, out_diff * diffuse.rgb);
  result.radiance += render_pass_glossy_mask(vec3(1.0), out_spec);
  result.radiance += render_pass_emission_mask(emissive.rgb);
  result.radiance *= alpha;
  result.transmittance = vec3(transp);

  closure_load_ssr_data(ssr_spec * alpha, roughness, normal, viewCameraVec, int(ssr_id), result);
}
#else
/* Stub specular because it is not compatible with volumetrics. */
#  define node_eevee_specular(a, b, c, d, e, f, g, h, i, j, k, result) (result = CLOSURE_DEFAULT)
#endif
