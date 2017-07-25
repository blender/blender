/**
 * Simple downsample shader. Takes the average of the 4 texels of lower mip.
 **/

uniform sampler2D source;

out vec4 FragColor;

void main()
{
	/* Reconstructing Target uvs like this avoid missing pixels if NPO2 */
	vec2 uvs = gl_FragCoord.xy * 2.0 / vec2(textureSize(source, 0));

	FragColor = textureLod(source, uvs, 0.0);
}