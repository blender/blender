
in float colorGradient;
in vec4 finalColor;
in float lineU;
flat in float lineLength;
flat in float dashFactor;
flat in float dashAlpha;
flat in int isMainLine;

out vec4 fragColor;

#define DASH_WIDTH 10.0
#define ANTIALIAS 1.0
#define MINIMUM_ALPHA 0.5

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

    float unclamped_alpha = 1.0 - slope * (normalized_distance_triangle - dashFactor + t);
    float alpha = max(dashAlpha, min(unclamped_alpha, 1.0));

    fragColor.a *= alpha;
  }

  fragColor.a *= smoothstep(1.0, 0.1, abs(colorGradient));
}
