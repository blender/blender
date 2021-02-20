#ifndef VOLUMETRICS

CLOSURE_EVAL_FUNCTION_DECLARE_3(node_eevee_specular, Diffuse, Glossy, Glossy)

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
  CLOSURE_VARS_DECLARE_3(Diffuse, Glossy, Glossy);

  in_common.occlusion = occlusion;

  in_Diffuse_0.N = normal; /* Normalized during eval. */
  in_Diffuse_0.albedo = diffuse.rgb;

  in_Glossy_1.N = normal; /* Normalized during eval. */
  in_Glossy_1.roughness = roughness;

  in_Glossy_2.N = clearcoat_normal; /* Normalized during eval. */
  in_Glossy_2.roughness = clearcoat_roughness;

  CLOSURE_EVAL_FUNCTION_3(node_eevee_specular, Diffuse, Glossy, Glossy);

  result = CLOSURE_DEFAULT;

  vec3 V = cameraVec(worldPosition);

  {
    /* Diffuse. */
    out_Diffuse_0.radiance = render_pass_diffuse_mask(vec3(1), out_Diffuse_0.radiance);
    out_Diffuse_0.radiance *= in_Diffuse_0.albedo;
    result += out_Diffuse_0.radiance;
  }
  {
    /* Glossy. */
    float NV = dot(in_Glossy_1.N, V);
    vec2 split_sum = brdf_lut(NV, in_Glossy_1.roughness);
    vec3 brdf = F_brdf_single_scatter(specular.rgb, vec3(1.0), split_sum);

    out_Glossy_1.radiance = closure_mask_ssr_radiance(out_Glossy_1.radiance, ssr_id);
    out_Glossy_1.radiance *= brdf;
    out_Glossy_1.radiance = render_pass_glossy_mask(spec_color, out_Glossy_1.radiance);
    closure_load_ssr_data(
        out_Glossy_1.radiance, in_Glossy_1.roughness, in_Glossy_1.N, ssr_id, result);
  }
  {
    /* Clearcoat. */
    float NV = dot(in_Glossy_2.N, V);
    vec2 split_sum = brdf_lut(NV, in_Glossy_2.roughness);
    vec3 brdf = F_brdf_single_scatter(vec3(0.04), vec3(1.0), split_sum);

    out_Glossy_2.radiance *= brdf * clearcoat * 0.25;
    out_Glossy_2.radiance = render_pass_glossy_mask(vec3(1), out_Glossy_2.radiance);
    result.radiance += out_Glossy_2.radiance;
  }
  {
    /* Emission. */
    vec3 out_emission_radiance = render_pass_emission_mask(emission.rgb);
    result.radiance += out_emission_radiance;
  }

  float trans = 1.0 - trans;
  result.transmittance = vec3(trans);
  result.radiance *= alpha;
  result.ssr_data.rgb *= alpha;
}

#else
/* Stub specular because it is not compatible with volumetrics. */
#  define node_eevee_specular(a, b, c, d, e, f, g, h, i, j, k, result) (result = CLOSURE_DEFAULT)
#endif
