#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

void color_to_luminance(vec4 color, const vec3 luminance_coefficients, out float result)
{
  result = get_luminance(color.rgb, luminance_coefficients);
}
