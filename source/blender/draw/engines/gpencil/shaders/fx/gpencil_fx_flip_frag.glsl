out vec4 FragColor;

uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;
uniform vec2 wsize;
uniform int flipmode;

void main()
{
	vec2 mode = vec2(0,0);
	/* horz. */
	if (flipmode >= 110) {
		mode[0] = 1;
	}
	/* vert. */
	if ((flipmode == 101) || (flipmode == 111)) {
		mode[1] = 1;
	}

	vec2 uv = vec2(gl_FragCoord.xy);
	float stroke_depth;
	vec4 outcolor;

	if (mode[0] > 0) {
		uv.x = wsize.x - uv.x;
	}
	if (mode[1] > 0) {
		uv.y = wsize.y - uv.y;
	}

	ivec2 iuv = ivec2(uv.x, uv.y);
	stroke_depth = texelFetch(strokeDepth, iuv, 0).r;
	outcolor = texelFetch(strokeColor, iuv, 0);

	gl_FragDepth = stroke_depth;
	FragColor = outcolor;
}
