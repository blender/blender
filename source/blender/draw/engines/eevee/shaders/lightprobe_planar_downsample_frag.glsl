/**
 * Simple down-sample shader. Takes the average of the 4 texels of lower mip.
 */

uniform sampler2DArray source;
uniform float fireflyFactor;

in vec2 uvs;
flat in float layer;

out vec4 FragColor;

float brightness(vec3 c)
{
  return max(max(c.r, c.g), c.b);
}

void main()
{
#if 0
  /* Reconstructing Target uvs like this avoid missing pixels if NPO2 */
  vec2 uvs = gl_FragCoord.xy * 2.0 / vec2(textureSize(source, 0));

  FragColor = textureLod(source, vec3(uvs, layer), 0.0);
#else
  vec2 texel_size = 1.0 / vec2(textureSize(source, 0));
  vec2 uvs = gl_FragCoord.xy * 2.0 * texel_size;
  vec4 ofs = texel_size.xyxy * vec4(0.75, 0.75, -0.75, -0.75);

  FragColor = textureLod(source, vec3(uvs + ofs.xy, layer), 0.0);
  FragColor += textureLod(source, vec3(uvs + ofs.xw, layer), 0.0);
  FragColor += textureLod(source, vec3(uvs + ofs.zy, layer), 0.0);
  FragColor += textureLod(source, vec3(uvs + ofs.zw, layer), 0.0);
  FragColor *= 0.25;

  /* Clamped brightness. */
  float luma = max(1e-8, brightness(FragColor.rgb));
  FragColor *= 1.0 - max(0.0, luma - fireflyFactor) / luma;
#endif
}
