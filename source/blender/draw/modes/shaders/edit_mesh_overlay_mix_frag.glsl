
out vec4 FragColor;

uniform sampler2D wireColor;
uniform sampler2D wireDepth;
uniform sampler2D sceneDepth;
uniform float alpha;

void main()
{
	ivec2 uv = ivec2(gl_FragCoord.xy);
	float wire_depth = texelFetch(wireDepth, uv, 0).r;
	float scene_depth = texelFetch(sceneDepth, uv, 0).r;
	vec4 wire_color = texelFetch(wireColor, uv, 0).rgba;

	FragColor = wire_color;

	/* Modulate alpha if occluded */
	if (wire_depth > scene_depth) {
		FragColor.a *= alpha;
	}
}
