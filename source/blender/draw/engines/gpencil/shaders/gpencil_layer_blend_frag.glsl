
uniform sampler2D colorBuf;
uniform sampler2D revealBuf;
uniform sampler2D maskBuf;
uniform int blendMode;
uniform float blendOpacity;

in vec4 uvcoordsvar;

/* Reminder: This is considered SRC color in blend equations.
 * Same operation on all buffers. */
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragRevealage;

void main()
{
  vec4 color;

  /* Remember, this is associated alpha (aka. premult). */
  color.rgb = textureLod(colorBuf, uvcoordsvar.xy, 0).rgb;
  /* Stroke only render mono-chromatic revealage. We convert to alpha. */
  color.a = 1.0 - textureLod(revealBuf, uvcoordsvar.xy, 0).r;

  float mask = textureLod(maskBuf, uvcoordsvar.xy, 0).r;
  mask *= blendOpacity;

  fragColor = vec4(1.0, 0.0, 1.0, 1.0);
  fragRevealage = vec4(1.0, 0.0, 1.0, 1.0);

  blend_mode_output(blendMode, color, mask, fragColor, fragRevealage);
}
