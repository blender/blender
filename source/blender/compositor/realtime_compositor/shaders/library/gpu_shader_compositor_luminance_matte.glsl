#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

void node_composite_luminance_matte(vec4 color,
                                    float high,
                                    float low,
                                    const vec3 luminance_coefficients,
                                    out vec4 result,
                                    out float matte)
{
  float luminance = get_luminance(color.rgb, luminance_coefficients);
  float alpha = clamp((luminance - low) / (high - low), 0.0, 1.0);
  matte = min(alpha, color.a);
  result = color * matte;
}
