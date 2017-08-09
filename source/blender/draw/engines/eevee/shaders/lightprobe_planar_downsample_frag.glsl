/**
 * Simple downsample shader. Takes the average of the 4 texels of lower mip.
 **/

uniform sampler2DArray source;
uniform vec2 texelSize;

in vec2 uvs;
flat in float layer;

out vec4 FragColor;

void main()
{
#if 0
	/* Reconstructing Target uvs like this avoid missing pixels if NPO2 */
	vec2 uvs = gl_FragCoord.xy * 2.0 / vec2(textureSize(source, 0));

	FragColor = textureLod(source, vec3(uvs, layer), 0.0);
#else
	vec2 texel_size = 1.0 / vec2(textureSize(source, 0));
	vec2 uvs = gl_FragCoord.xy * 2.0 * texel_size;
	vec4 ofs = texel_size.xyxy * vec4(0.75, 0.75, -0.75, -0.75);

	FragColor  = textureLod(source, vec3(uvs + ofs.xy, layer), 0.0);
	FragColor += textureLod(source, vec3(uvs + ofs.xw, layer), 0.0);
	FragColor += textureLod(source, vec3(uvs + ofs.zy, layer), 0.0);
	FragColor += textureLod(source, vec3(uvs + ofs.zw, layer), 0.0);
	FragColor *= 0.25;
#endif
}