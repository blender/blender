
uniform vec3 color;
uniform vec3 outlineColor;
uniform sampler1D ramp;

in vec4 radii;
flat in float finalVal;

out vec4 fragColor;

void main() {
	float dist = length(gl_PointCoord - vec2(0.5));

// transparent outside of point
// --- 0 ---
// smooth transition
// --- 1 ---
// pure outline color
// --- 2 ---
// smooth transition
// --- 3 ---
// pure point color
// ...
// dist = 0 at center of point

	float midStroke = 0.5 * (radii[1] + radii[2]);

	if (dist > midStroke) {
		if (finalVal < 0.0) {
			fragColor.rgb = outlineColor;
		}
		else {
			fragColor.rgb = texture(ramp, finalVal).rgb;
		}

		fragColor.a = mix(1.0, 0.0, smoothstep(radii[1], radii[0], dist));
	}
	else {
		if (finalVal < 0.0) {
			fragColor.rgb = mix(color, outlineColor, smoothstep(radii[3], radii[2], dist));
		}
		else {
			fragColor.rgb = texture(ramp, finalVal).rgb;
		}

		fragColor.a = 1.0;
	}

	if (fragColor.a == 0.0) {
		discard;
	}
}
