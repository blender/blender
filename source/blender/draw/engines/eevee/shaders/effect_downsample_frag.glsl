
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

/**
 * Simple down-sample shader.
 * Do a gaussian filter using 4 bilinear texture samples.
 */

uniform sampler2D source;
uniform float fireflyFactor;

#ifndef COPY_SRC
uniform vec2 texelSize;
#endif

out vec4 FragColor;

void main()
{
#ifdef COPY_SRC
  vec2 uvs = gl_FragCoord.xy / vec2(textureSize(source, 0));
  FragColor = textureLod(source, uvs, 0.0);
  FragColor = safe_color(FragColor);

  /* Clamped brightness. */
  float luma = max(1e-8, max_v3(FragColor.rgb));
  FragColor *= 1.0 - max(0.0, luma - fireflyFactor) / luma;

#else
  /* NOTE(@fclem): textureSize() does not work the same on all implementations
   * when changing the min and max texture levels. Use uniform instead (see T87801). */
  vec2 uvs = gl_FragCoord.xy * texelSize;
  vec4 ofs = texelSize.xyxy * vec4(0.75, 0.75, -0.75, -0.75);
  uvs *= 2.0;

  FragColor = textureLod(source, uvs + ofs.xy, 0.0);
  FragColor += textureLod(source, uvs + ofs.xw, 0.0);
  FragColor += textureLod(source, uvs + ofs.zy, 0.0);
  FragColor += textureLod(source, uvs + ofs.zw, 0.0);
  FragColor *= 0.25;
#endif
}
