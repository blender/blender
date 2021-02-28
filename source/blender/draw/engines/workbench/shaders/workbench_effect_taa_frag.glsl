
uniform sampler2D colorBuffer;
uniform float samplesWeights[9];

out vec4 fragColor;

void main()
{
  vec2 texel_size = 1.0 / vec2(textureSize(colorBuffer, 0));
  vec2 uv = gl_FragCoord.xy * texel_size;

  fragColor = vec4(0.0);
  int i = 0;
  for (int x = -1; x <= 1; x++) {
    for (int y = -1; y <= 1; y++, i++) {
      /* Use log2 space to avoid highlights creating too much aliasing. */
      vec4 color = log2(texture(colorBuffer, uv + vec2(x, y) * texel_size) + 0.5);

      fragColor += color * samplesWeights[i];
    }
  }
}
