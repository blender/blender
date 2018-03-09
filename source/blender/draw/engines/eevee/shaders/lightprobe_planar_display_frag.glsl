
uniform sampler2DArray probePlanars;

in vec3 worldPosition;
flat in int probeIdx;

out vec4 FragColor;

void main()
{
	vec4 refco = ViewProjectionMatrix * vec4(worldPosition, 1.0);
	refco.xy /= refco.w;
	FragColor = vec4(textureLod(probePlanars, vec3(refco.xy * 0.5 + 0.5, float(probeIdx)), 0.0).rgb, 1.0);
}
