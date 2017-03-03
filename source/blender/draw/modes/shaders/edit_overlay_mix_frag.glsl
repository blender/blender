
out vec4 FragColor;

uniform sampler2D wireColor;
uniform sampler2D wireDepth;
uniform sampler2D faceColor;
uniform sampler2D sceneDepth;
uniform float alpha;

vec4 linear(vec4 col)
{
	const float fac = 0.45454545;
	return vec4(pow(col.r,fac), pow(col.g,fac), pow(col.b,fac), col.a);
}

vec4 srgb(vec4 col)
{
	const float fac = 2.2;
	return vec4(pow(col.r,fac), pow(col.g,fac), pow(col.b,fac), col.a);
}

void main()
{
	ivec2 co = ivec2(gl_FragCoord.xy);
	float wire_depth = texelFetch(wireDepth, co, 0).r;
	float scene_depth = texelFetch(sceneDepth, co, 0).r;
	vec4 wire_color = texelFetch(wireColor, co, 0).rgba;
	vec4 face_color = texelFetch(faceColor, co, 0).rgba;

	/* this works because not rendered depth is 1.0 and the
	 * following test is always true even when no wires */
	if (wire_depth > scene_depth) {
		wire_color.a *= alpha;
		FragColor.rgb = mix(face_color.rgb, wire_color.rgb, wire_color.a);
		FragColor.a = face_color.a + wire_color.a;
	}
	else {
		FragColor = wire_color;
	}
}
