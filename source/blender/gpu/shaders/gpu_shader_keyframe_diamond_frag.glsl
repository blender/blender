
in vec4 radii;
in vec4 finalColor;
in vec4 finalOutlineColor;
out vec4 fragColor;

void main() {
	vec2 quad = abs(gl_PointCoord - vec2(0.5));
	float dist = quad.x + quad.y;

//	transparent outside of point
// --- 0 ---
//	smooth transition
// --- 1 ---
//	pure outline color
// --- 2 ---
//	smooth transition
// --- 3 ---
//	pure point color
// ...
// dist = 0 at center of point

	float mid_stroke = 0.5 * (radii[1] + radii[2]);

	vec4 backgroundColor = vec4(finalOutlineColor.rgb, 0.0);

	if (dist > mid_stroke)
		fragColor = mix(finalOutlineColor, backgroundColor, smoothstep(radii[1], radii[0], dist));
	else
		fragColor = mix(finalColor, finalOutlineColor, smoothstep(radii[3], radii[2], dist));
}
