#pragma BLENDER_REQUIRE(common_hair_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_common_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_material_lib.glsl)
#pragma BLENDER_REQUIRE(workbench_image_lib.glsl)

/* From http://libnoise.sourceforge.net/noisegen/index.html */
float integer_noise(int n)
{
  /* Integer bit-shifts cause precision issues due to overflow
   * in a number of workbench tests. Use uint instead. */
  uint nn = (uint(n) >> 13u) ^ uint(n);
  nn = (nn * (nn * nn * 60493u + 19990303u) + 1376312589u) & 0x7fffffffu;
  return (float(nn) / 1073741824.0);
}

vec3 workbench_hair_random_normal(vec3 tan, vec3 binor, float rand)
{
  /* To "simulate" anisotropic shading, randomize hair normal per strand. */
  vec3 nor = cross(tan, binor);
  nor = normalize(mix(nor, -tan, rand * 0.1));
  float cos_theta = (rand * 2.0 - 1.0) * 0.2;
  float sin_theta = sqrt(max(0.0, 1.0 - cos_theta * cos_theta));
  nor = nor * sin_theta + binor * cos_theta;
  return nor;
}

void workbench_hair_random_material(float rand,
                                    inout vec3 color,
                                    inout float roughness,
                                    inout float metallic)
{
  /* Center noise around 0. */
  rand -= 0.5;
  rand *= 0.1;
  /* Add some variation to the hairs to avoid uniform look. */
  metallic = clamp(metallic + rand, 0.0, 1.0);
  roughness = clamp(roughness + rand, 0.0, 1.0);
  /* Modulate by color intensity to reduce very high contrast when color is dark. */
  color = clamp(color + rand * (color + 0.05), 0.0, 1.0);
}

void main()
{
  bool is_persp = (drw_view.winmat[3][3] == 0.0);
  float time, thick_time, thickness;
  vec3 world_pos, tan, binor;
  hair_get_pos_tan_binor_time(is_persp,
                              ModelMatrixInverse,
                              drw_view.viewinv[3].xyz,
                              drw_view.viewinv[2].xyz,
                              world_pos,
                              tan,
                              binor,
                              time,
                              thickness,
                              thick_time);

  gl_Position = point_world_to_ndc(world_pos);

  float hair_rand = integer_noise(hair_get_strand_id());
  vec3 nor = workbench_hair_random_normal(tan, binor, hair_rand);

  view_clipping_distances(world_pos);

  uv_interp = hair_get_customdata_vec2(au);

  normal_interp = normalize(normal_world_to_view(nor));

#ifdef WORKBENCH_NEXT
  workbench_material_data_get(int(drw_CustomID),
                              hair_get_customdata_vec3(ac),
                              color_interp,
                              alpha_interp,
                              _roughness,
                              metallic);
#else
  workbench_material_data_get(resource_handle,
                              hair_get_customdata_vec3(ac),
                              color_interp,
                              alpha_interp,
                              _roughness,
                              metallic);
#endif

  /* Hairs have lots of layer and can rapidly become the most prominent surface.
   * So we lower their alpha artificially. */
  alpha_interp *= 0.3;

  workbench_hair_random_material(hair_rand, color_interp, _roughness, metallic);

  object_id = int(uint(resource_handle) & 0xFFFFu) + 1;

#ifdef WORKBENCH_NEXT
  if (emitter_object_id != 0) {
    object_id = int(uint(emitter_object_id) & 0xFFFFu) + 1;
  }
#endif
}
