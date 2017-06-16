
uniform int probeIdx;
uniform sampler2DArray probePlanars;

layout(std140) uniform planar_block {
	PlanarData planars_data[MAX_PLANAR];
};

in vec3 worldPosition;

out vec4 FragColor;

void main()
{
	PlanarData pd = planars_data[probeIdx];

	/* Fancy fast clipping calculation */
	vec2 dist_to_clip;
	dist_to_clip.x = dot(pd.pl_clip_pos_x, worldPosition);
	dist_to_clip.y = dot(pd.pl_clip_pos_y, worldPosition);
	float fac = dot(step(pd.pl_clip_edges, dist_to_clip.xxyy), vec2(-1.0, 1.0).xyxy); /* compare and add all tests */

	if (fac != 2.0) {
		discard;
	}

	vec4 refco = pd.reflectionmat * vec4(worldPosition, 1.0);
	refco.xy /= refco.w;
	FragColor = vec4(textureLod(probePlanars, vec3(refco.xy, float(probeIdx)), 0.0).rgb, 1.0);
}
