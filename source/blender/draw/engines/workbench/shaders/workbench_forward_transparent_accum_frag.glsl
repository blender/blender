#ifdef V3D_SHADING_TEXTURE_COLOR
uniform sampler2D image;
uniform float ImageTransparencyCutoff = 0.1;

#endif
uniform mat4 ProjectionMatrix;
uniform mat3 normalWorldMatrix;
uniform float alpha = 0.5;
uniform vec2 invertedViewportSize;
uniform vec4 viewvecs[3];

uniform vec4 materialDiffuseColor;
uniform vec4 materialSpecularColor;
uniform float materialRoughness;

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
in vec3 normal_viewport;
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */
#ifdef V3D_SHADING_TEXTURE_COLOR
in vec2 uv_interp;
#endif
#ifdef V3D_LIGHTING_MATCAP
uniform sampler2D matcapImage;
#endif

layout(std140) uniform world_block {
	WorldData world_data;
};

layout(location=0) out vec4 transparentAccum;
layout(location=1) out float revealageAccum; /* revealage actually stored in transparentAccum.a */

void main()
{
	vec4 diffuse_color;
	vec3 diffuse_light = vec3(1.0);

#ifdef V3D_SHADING_TEXTURE_COLOR
	diffuse_color = texture(image, uv_interp);
	if (diffuse_color.a < ImageTransparencyCutoff) {
		discard;
	}
#else
	diffuse_color = materialDiffuseColor;
#endif /* V3D_SHADING_TEXTURE_COLOR */

	vec2 uv_viewport = gl_FragCoord.xy * invertedViewportSize;
	vec3 I_vs = view_vector_from_screen_uv(uv_viewport, viewvecs, ProjectionMatrix);

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
	vec3 nor = normalize(normal_viewport);
#endif

#ifdef V3D_LIGHTING_MATCAP
	bool flipped = world_data.matcap_orientation != 0;
	vec2 matcap_uv = matcap_uv_compute(I_vs, nor, flipped);
	diffuse_light = texture(matcapImage, matcap_uv).rgb;
#endif

#ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
	vec3 specular_color = get_world_specular_lights(world_data, vec4(materialSpecularColor.rgb, materialRoughness), nor, I_vs);
#else
	vec3 specular_color = vec3(0.0);
#endif

#ifdef V3D_LIGHTING_STUDIO
#  ifdef STUDIOLIGHT_ORIENTATION_CAMERA
	diffuse_light = get_camera_diffuse_light(world_data, nor);
#  endif
#  ifdef STUDIOLIGHT_ORIENTATION_WORLD
	vec3 normal_world = normalWorldMatrix * nor;
	diffuse_light = get_world_diffuse_light(world_data, normal_world);
#  endif
#endif

	vec3 shaded_color = diffuse_light * diffuse_color.rgb + specular_color;

	/* Based on :
	 * McGuire and Bavoil, Weighted Blended Order-Independent Transparency, Journal of
	 * Computer Graphics Techniques (JCGT), vol. 2, no. 2, 122–141, 2013
	 */
	/* Listing 4 */
	float z = linear_zdepth(gl_FragCoord.z, viewvecs, ProjectionMatrix);
	float weight = calculate_transparent_weight(z, alpha);
	transparentAccum = vec4(shaded_color * weight, alpha);
	revealageAccum = weight;
}
