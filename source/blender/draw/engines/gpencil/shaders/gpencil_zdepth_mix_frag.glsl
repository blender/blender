in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;
uniform int tonemapping;
uniform vec4 select_color;
uniform int do_select;

float srgb_to_linearrgb(float c)
{
	if (c < 0.04045) {
		return (c < 0.0) ? 0.0 : c * (1.0 / 12.92);
	}
	else {
		return pow((c + 0.055) * (1.0 / 1.055), 2.4);
	}
}

float linearrgb_to_srgb(float c)
{
	if (c < 0.0031308) {
		return (c < 0.0) ? 0.0 : c * 12.92;
	}
	else {
		return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
	}
}

bool check_borders(ivec2 uv, int size)
{
	for (int x = -size; x <= size; x++) {
		for (int y = -size; y <= size; y++) {
			vec4 stroke_color =  texelFetch(strokeColor, ivec2(uv.x + x, uv.y + y), 0).rgba;
			if (stroke_color.a > 0) {
				return true;
			}
		}
	}

	return false;
}

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);
	float stroke_depth = texelFetch(strokeDepth, uv, 0).r;
	vec4 stroke_color =  texelFetch(strokeColor, uv, 0).rgba;

	/* premult alpha factor to remove double blend effects */
	if (stroke_color.a > 0) {
		stroke_color = vec4(vec3(stroke_color.rgb / stroke_color.a), stroke_color.a);
	}

	/* apply color correction for render only */
	if (tonemapping == 1) {
		stroke_color.r = srgb_to_linearrgb(stroke_color.r);
		stroke_color.g = srgb_to_linearrgb(stroke_color.g);
		stroke_color.b = srgb_to_linearrgb(stroke_color.b);
	}

	FragColor = clamp(stroke_color, 0.0, 1.0);
	gl_FragDepth = clamp(stroke_depth, 0.0, 1.0);

	if (do_select == 1) {
		if (stroke_color.a == 0) {
			if (check_borders(uv, 2)) {
				FragColor = select_color;
				gl_FragDepth = 0.000001;
			}
		}
	}
}
