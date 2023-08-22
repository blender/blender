/* Tone mapping based on equation (1) and the trilinear interpolation between equations (6) and (7)
 * from Reinhard, Erik, and Kate Devlin. "Dynamic range reduction inspired by photoreceptor
 * physiology." IEEE transactions on visualization and computer graphics 11.1 (2005): 13-24. */

#pragma BLENDER_REQUIRE(gpu_shader_compositor_texture_utilities.glsl)

void main()
{
  ivec2 texel = ivec2(gl_GlobalInvocationID.xy);

  vec4 input_color = texture_load(input_tx, texel);
  float input_luminance = dot(input_color.rgb, luminance_coefficients);

  /* Trilinear interpolation between equations (6) and (7) from Reinhard's 2005 paper. */
  vec4 local_adaptation_level = mix(vec4(input_luminance), input_color, chromatic_adaptation);
  vec4 adaptation_level = mix(global_adaptation_level, local_adaptation_level, light_adaptation);

  /* Equation (1) from Reinhard's 2005 paper, assuming Vmax is 1. */
  vec4 semi_saturation = pow(intensity * adaptation_level, vec4(contrast));
  vec4 tone_mapped_color = input_color / (input_color + semi_saturation);

  imageStore(output_img, texel, vec4(tone_mapped_color.rgb, input_color.a));
}
