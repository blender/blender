#pragma BLENDER_REQUIRE(common_colormanagement_lib.glsl)

uniform sampler2D imgTexture;
uniform vec4 color;

in vec2 uvs;
out vec4 fragColor;

void main()
{
  vec2 uvs_clamped = clamp(uvs, 0.0, 1.0);
  float mask_value = texture_read_as_linearrgb(imgTexture, true, uvs_clamped).r;
  fragColor = vec4(color.rgb * mask_value, color.a);
}
