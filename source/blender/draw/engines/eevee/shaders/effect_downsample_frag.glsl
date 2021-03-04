
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

/**
 * Simple down-sample shader.
 * Do a gaussian filter using 4 bilinear texture samples.
 */

uniform sampler2D source;
uniform float fireflyFactor;

out vec4 FragColor;

float brightness(vec3 c)
{
  return max(max(c.r, c.g), c.b);
}

void main()
{
  vec2 texel_size = 1.0 / vec2(textureSize(source, 0));
  vec2 uvs = gl_FragCoord.xy * texel_size;

#ifdef COPY_SRC
  FragColor = textureLod(source, uvs, 0.0);
  FragColor = safe_color(FragColor);

  /* Clamped brightness. */
  float luma = max(1e-8, brightness(FragColor.rgb));
  FragColor *= 1.0 - max(0.0, luma - fireflyFactor) / luma;

#else
  vec4 ofs = texel_size.xyxy * vec4(0.75, 0.75, -0.75, -0.75);
  uvs *= 2.0;

  FragColor = textureLod(source, uvs + ofs.xy, 0.0);
  FragColor += textureLod(source, uvs + ofs.xw, 0.0);
  FragColor += textureLod(source, uvs + ofs.zy, 0.0);
  FragColor += textureLod(source, uvs + ofs.zw, 0.0);
  FragColor *= 0.25;
#endif
}
