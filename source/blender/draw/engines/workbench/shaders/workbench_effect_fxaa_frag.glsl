
in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D colorBuffer;
uniform vec2 invertedViewportSize;

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);
  float alpha = texelFetch(colorBuffer, texel, 0).a;
  FragColor = FxaaPixelShader(
      uvcoordsvar.st, colorBuffer, invertedViewportSize, 1.0, 0.166, 0.0833);
  FragColor.a = alpha;
}
