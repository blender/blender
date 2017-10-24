
/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Step 4 : Apply final integration on top of the scene color.
 * Note that we do the blending ourself instead of relying
 * on hardware blending which would require 2 pass. */

uniform sampler3D inScattering;
uniform sampler3D inTransmittance;

uniform sampler2D inSceneColor;
uniform sampler2D inSceneDepth;

out vec4 FragColor;

void main()
{
	vec2 uvs = gl_FragCoord.xy / vec2(textureSize(inSceneDepth, 0));
	vec3 volume_cos = ndc_to_volume(vec3(uvs, texture(inSceneDepth, uvs).r));

	vec3 scene_color = texture(inSceneColor, uvs).rgb;
	vec3 scattering = texture(inScattering, volume_cos).rgb;
	vec3 transmittance = texture(inTransmittance, volume_cos).rgb;

	FragColor = vec4(scene_color * transmittance + scattering, 1.0);
}
