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
	/* Reconstructing Target uvs like this avoid missing pixels */
	vec2 uvs = floor(gl_FragCoord.xy) * 2.0 * texelSize + texelSize;

	/* Downsample with a 4x4 box filter */
	vec4 d = texelSize.xyxy * vec4(-1, -1, +1, +1);

	FragColor  = texture(source, vec3(uvs + d.xy, layer)).rgba;
	FragColor += texture(source, vec3(uvs + d.zy, layer)).rgba;
	FragColor += texture(source, vec3(uvs + d.xw, layer)).rgba;
	FragColor += texture(source, vec3(uvs + d.zw, layer)).rgba;

	FragColor /= 4.0;
}