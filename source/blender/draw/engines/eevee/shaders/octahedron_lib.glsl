
vec2 mapping_octahedron(vec3 cubevec, vec2 texel_size)
{
	/* projection onto octahedron */
	cubevec /= dot( vec3(1), abs(cubevec) );

	/* out-folding of the downward faces */
	if ( cubevec.z < 0.0 ) {
		cubevec.xy = (1.0 - abs(cubevec.yx)) * sign(cubevec.xy);
	}

	/* mapping to [0;1]ˆ2 texture space */
	vec2 uvs = cubevec.xy * (0.5) + 0.5;

	/* edge filtering fix */
	uvs = (1.0 - 2.0 * texel_size) * uvs + texel_size;

	return uvs;
}

vec4 textureLod_octahedron(sampler2DArray tex, vec4 cubevec, float lod, float lod_max)
{
	vec2 texelSize = 1.0 / vec2(textureSize(tex, int(lod_max)));

	vec2 uvs = mapping_octahedron(cubevec.xyz, texelSize);

	return textureLod(tex, vec3(uvs, cubevec.w), lod);
}

vec4 texture_octahedron(sampler2DArray tex, vec4 cubevec)
{
	vec2 texelSize = 1.0 / vec2(textureSize(tex, 0));

	vec2 uvs = mapping_octahedron(cubevec.xyz, texelSize);

	return texture(tex, vec3(uvs, cubevec.w));
}
