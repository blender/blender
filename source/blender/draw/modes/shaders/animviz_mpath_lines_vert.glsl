
uniform mat4 ViewMatrix;
uniform mat4 ViewProjectionMatrix;
uniform vec2 viewportSize;

uniform int frameCurrent;
uniform int frameStart;
uniform int frameEnd;
uniform int cacheStart;
uniform bool selected;
uniform bool useCustomColor;
uniform vec3 customColor;

in vec3 pos;

out vec2 ssPos;
out vec4 finalColor_geom;

/* project to screen space */
vec2 proj(vec4 pos)
{
	return (0.5 * (pos.xy / pos.w) + 0.5) * viewportSize;
}

#define SET_INTENSITY(A, B, C, min, max) (((1.0 - (float(C - B) / float(C - A))) * (max - min)) + min)

void main()
{
	gl_Position = ViewProjectionMatrix * vec4(pos, 1.0);

	ssPos = proj(gl_Position);

	int frame = gl_VertexID + cacheStart;

	float intensity;  /* how faint */

	vec3 blend_base = (abs(frame - frameCurrent) == 1) ? colorCurrentFrame.rgb : colorBackground.rgb; /* "bleed" cframe color to ease color blending */

	/* TODO: We might want something more consistent with custom color and standard colors. */
	if (frame < frameCurrent) {
		if (useCustomColor) {
			/* Custom color: previous frames color is darker than current frame */
			finalColor_geom.rgb = customColor * 0.25;
		}
		else {
			/* black - before frameCurrent */
			if (selected) {
				intensity = SET_INTENSITY(frameStart, frame, frameCurrent, 0.25, 0.75);
			}
			else {
				intensity = SET_INTENSITY(frameStart, frame, frameCurrent, 0.68, 0.92);
			}
			finalColor_geom.rgb = mix(colorWire.rgb, blend_base, intensity);
		}
	}
	else if (frame > frameCurrent) {
		if (useCustomColor) {
			/* Custom color: next frames color is equal to user selected color */
			finalColor_geom.rgb = customColor;
		}
		else {
			/* blue - after frameCurrent */
			if (selected) {
				intensity = SET_INTENSITY(frameCurrent, frame, frameEnd, 0.25, 0.75);
			}
			else {
				intensity = SET_INTENSITY(frameCurrent, frame, frameEnd, 0.68, 0.92);
			}

			finalColor_geom.rgb = mix(colorBonePose.rgb, blend_base, intensity);
		}
	}
	else {
		if (useCustomColor) {
			/* Custom color: current frame color is slightly darker than user selected color */
			finalColor_geom.rgb = customColor * 0.5;
		}
		else {
			/* green - on frameCurrent */
			if (selected) {
				intensity = 0.5f;
			}
			else {
				intensity = 0.99f;
			}
			finalColor_geom.rgb = clamp(mix(colorCurrentFrame.rgb, colorBackground.rgb, intensity) - 0.1, 0.0, 0.1);
		}
	}

	finalColor_geom.a = 1.0;
}
