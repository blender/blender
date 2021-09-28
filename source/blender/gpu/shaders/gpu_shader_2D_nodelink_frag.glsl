
in float colorGradient;
in vec4 finalColor;
in float lineU;
flat in float lineLength;
flat in float dashFactor;
flat in int isMainLine;

out vec4 fragColor;

#define DASH_WIDTH 20.0
#define ANTIALIAS 1.0

void main()
{
  fragColor = finalColor;

  if ((isMainLine != 0) && (dashFactor < 1.0)) {
    float distance_along_line = lineLength * lineU;
    float normalized_distance = fract(distance_along_line / DASH_WIDTH);

    /* Checking if `normalized_distance <= dashFactor` is already enough for a basic
     * dash, however we want to handle a nice antialias. */

    float dash_center = DASH_WIDTH * dashFactor * 0.5;
    float normalized_distance_triangle =
        1.0 - abs((fract((distance_along_line - dash_center) / DASH_WIDTH)) * 2.0 - 1.0);
    float t = ANTIALIAS / DASH_WIDTH;
    float slope = 1.0 / (2.0 * t);

    float alpha = min(1.0, max(0.0, slope * (normalized_distance_triangle - dashFactor + t)));

    if (alpha < 0.0) {
      discard;
    }

    fragColor.a *= 1.0 - alpha;
  }

  fragColor.a *= smoothstep(1.0, 0.1, abs(colorGradient));
}
