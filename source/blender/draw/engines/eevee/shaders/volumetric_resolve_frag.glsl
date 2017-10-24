
/* Based on Frosbite Unified Volumetric.
 * https://www.ea.com/frostbite/news/physically-based-unified-volumetric-rendering-in-frostbite */

/* Step 4 : Apply final integration on top of the scene color.
 * Note that we do the blending ourself instead of relying
 * on hardware blending which would require 2 pass. */

uniform sampler2D inSceneColor;
uniform sampler2D inSceneDepth;

out vec4 FragColor;

void main()
{
	vec2 uvs = gl_FragCoord.xy / vec2(textureSize(inSceneDepth, 0));
	vec4 scene_color = texture(inSceneColor, uvs);
	float scene_depth = texture(inSceneDepth, uvs).r;

	FragColor = volumetric_resolve(scene_color, uvs, scene_depth);
}
