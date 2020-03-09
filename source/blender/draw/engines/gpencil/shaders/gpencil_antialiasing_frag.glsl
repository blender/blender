
uniform sampler2D edgesTex;
uniform sampler2D areaTex;
uniform sampler2D searchTex;
uniform sampler2D blendTex;
uniform sampler2D colorTex;
uniform sampler2D revealTex;
uniform bool onlyAlpha;
uniform bool doAntiAliasing;

in vec2 uvs;
in vec2 pixcoord;
in vec4 offset[3];

#if SMAA_STAGE == 0
out vec2 fragColor;
#elif SMAA_STAGE == 1
out vec4 fragColor;
#elif SMAA_STAGE == 2
/* Reminder: Blending func is fragRevealage * DST + fragColor .*/
layout(location = 0, index = 0) out vec4 outColor;
layout(location = 0, index = 1) out vec4 outReveal;
#endif

void main()
{
#if SMAA_STAGE == 0
  /* Detect edges in color and revealage buffer. */
  fragColor = SMAALumaEdgeDetectionPS(uvs, offset, colorTex);
  fragColor = max(fragColor, SMAALumaEdgeDetectionPS(uvs, offset, revealTex));
  /* Discard if there is no edge. */
  if (dot(fragColor, float2(1.0, 1.0)) == 0.0) {
    discard;
  }

#elif SMAA_STAGE == 1
  fragColor = SMAABlendingWeightCalculationPS(
      uvs, pixcoord, offset, edgesTex, areaTex, searchTex, vec4(0));

#elif SMAA_STAGE == 2
  /* Resolve both buffers. */
  if (doAntiAliasing) {
    outColor = SMAANeighborhoodBlendingPS(uvs, offset[0], colorTex, blendTex);
    outReveal = SMAANeighborhoodBlendingPS(uvs, offset[0], revealTex, blendTex);
  }
  else {
    outColor = texture(colorTex, uvs);
    outReveal = texture(revealTex, uvs);
  }

  /* Revealage, how much light passes through. */
  /* Average for alpha channel. */
  outReveal.a = clamp(dot(outReveal.rgb, vec3(0.333334)), 0.0, 1.0);
  /* Color buf is already premultiplied. Just add it to the color. */
  /* Add the alpha. */
  outColor.a = 1.0 - outReveal.a;

  if (onlyAlpha) {
    /* Special case in wireframe xray mode. */
    outColor = vec4(0.0);
    outReveal.rgb = outReveal.aaa;
  }
#endif
}
