
void node_bsdf_hair(vec4 color,
                    float offset,
                    float roughness_u,
                    float roughness_v,
                    vec3 T,
                    float weight,
                    out Closure result)
{
  ClosureHair hair_data;
  hair_data.weight = weight;
  hair_data.color = color.rgb;
  hair_data.offset = offset;
  hair_data.roughness = vec2(roughness_u, roughness_v);
  hair_data.T = T;

  result = closure_eval(hair_data);
}

void node_bsdf_hair_principled(vec4 color,
                               float melanin,
                               float melanin_redness,
                               vec4 tint,
                               vec3 absorption_coefficient,
                               float roughness,
                               float radial_roughness,
                               float coat,
                               float ior,
                               float offset,
                               float random_color,
                               float random_roughness,
                               float random,
                               float weight,
                               out Closure result)
{
  /* Placeholder closure.
   * Some computation will have to happen here just like the Principled BSDF. */
  ClosureHair hair_data;
  hair_data.weight = weight;
  hair_data.color = color.rgb;
  hair_data.offset = offset;
  hair_data.roughness = vec2(0.0);
  hair_data.T = g_data.curve_B;

  result = closure_eval(hair_data);
}
