/* The algorithm is by Werner D. Streidt
 * (http://visca.com/ffactory/archives/5-99/msg00021.html)
 * Extracted of OpenCV demhist.c
 */

#pragma BLENDER_REQUIRE(gpu_shader_common_color_utils.glsl)

#define FLT_EPSILON 1.192092896e-07F

void node_composite_bright_contrast(
    vec4 color, float brightness, float contrast, const float use_premultiply, out vec4 result)
{
  brightness /= 100.0;
  float delta = contrast / 200.0;

  float multiplier, offset;
  if (contrast > 0.0) {
    multiplier = 1.0 - delta * 2.0;
    multiplier = 1.0 / max(multiplier, FLT_EPSILON);
    offset = multiplier * (brightness - delta);
  }
  else {
    delta *= -1.0;
    multiplier = max(1.0 - delta * 2.0, 0.0);
    offset = multiplier * brightness + delta;
  }

  if (use_premultiply != 0.0) {
    color_alpha_unpremultiply(color, color);
  }

  result.rgb = color.rgb * multiplier + offset;
  result.a = color.a;

  if (use_premultiply != 0.0) {
    color_alpha_premultiply(result, result);
  }
}
