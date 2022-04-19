#pragma BLENDER_REQUIRE(gpu_shader_material_hash.glsl)

void node_hair_info(float hair_length,
                    out float is_strand,
                    out float intercept,
                    out float out_length,
                    out float thickness,
                    out vec3 tangent,
                    out float random)
{
  is_strand = float(g_data.is_strand);
  intercept = g_data.hair_time;
  thickness = g_data.hair_thickness;
  out_length = hair_length;
  tangent = g_data.T;
  /* TODO: could be precomputed per strand instead. */
  random = wang_hash_noise(uint(g_data.hair_strand_id));
}
