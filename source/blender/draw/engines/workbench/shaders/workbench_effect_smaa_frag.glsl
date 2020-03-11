
uniform sampler2D edgesTex;
uniform sampler2D areaTex;
uniform sampler2D searchTex;
uniform sampler2D blendTex;
uniform sampler2D colorTex;
uniform float mixFactor;
uniform float taaSampleCountInv;

in vec2 uvs;
in vec2 pixcoord;
in vec4 offset[3];

#if SMAA_STAGE == 0
out vec2 fragColor;
#else
out vec4 fragColor;
#endif

void main()
{
#if SMAA_STAGE == 0
  /* Detect edges in color and revealage buffer. */
  fragColor = SMAALumaEdgeDetectionPS(uvs, offset, colorTex);
  /* Discard if there is no edge. */
  if (dot(fragColor, float2(1.0, 1.0)) == 0.0) {
    discard;
  }

#elif SMAA_STAGE == 1
  fragColor = SMAABlendingWeightCalculationPS(
      uvs, pixcoord, offset, edgesTex, areaTex, searchTex, vec4(0));

#elif SMAA_STAGE == 2
  fragColor = vec4(0.0);
  if (mixFactor > 0.0) {
    fragColor += SMAANeighborhoodBlendingPS(uvs, offset[0], colorTex, blendTex) * mixFactor;
  }
  if (mixFactor < 1.0) {
    fragColor += texture(colorTex, uvs) * (1.0 - mixFactor);
  }
  fragColor *= taaSampleCountInv;
#endif
}
