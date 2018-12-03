uniform int object_id = 0;

uniform vec3 materialDiffuseColor;
uniform float materialMetallic;
uniform float materialRoughness;

#ifdef V3D_SHADING_TEXTURE_COLOR
uniform sampler2D image;
uniform float ImageTransparencyCutoff = 0.1;

#endif

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
in vec3 normal_viewport;
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */

#ifdef V3D_SHADING_TEXTURE_COLOR
in vec2 uv_interp;
#endif /* V3D_SHADING_TEXTURE_COLOR */

#ifdef HAIR_SHADER
flat in float hair_rand;
#endif

layout(location=0) out uint objectId;
layout(location=1) out vec4 materialData;
#ifdef NORMAL_VIEWPORT_PASS_ENABLED
#  ifdef WORKBENCH_ENCODE_NORMALS
layout(location=3) out vec2 normalViewport;
#  else /* WORKBENCH_ENCODE_NORMALS */
layout(location=3) out vec3 normalViewport;
#  endif /* WORKBENCH_ENCODE_NORMALS */
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */

void main()
{
	objectId = uint(object_id);

	vec4 color_roughness;
#ifdef V3D_SHADING_TEXTURE_COLOR
	color_roughness = texture(image, uv_interp);
	if (color_roughness.a < ImageTransparencyCutoff) {
		discard;
	}
	color_roughness.a = materialRoughness;
#else
	color_roughness = vec4(materialDiffuseColor, materialRoughness);
#endif /* V3D_SHADING_TEXTURE_COLOR */

#ifdef HAIR_SHADER
	float hair_color_variation = hair_rand * 0.1;
	color_roughness = clamp(color_roughness - hair_color_variation, 0.0, 1.0);
#endif

	float metallic;
#ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
#  ifdef HAIR_SHADER
	metallic = clamp(materialMetallic - hair_color_variation, 0.0, 1.0);
#  else
	metallic = materialMetallic;
#  endif
#elif defined(V3D_LIGHTING_MATCAP)
	/* Encode front facing in metallic channel. */
	metallic = float(gl_FrontFacing);
	color_roughness.a = 0.0;
#endif

	materialData.rgb = color_roughness.rgb;
	materialData.a   = workbench_float_pair_encode(color_roughness.a, metallic);

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
	vec3 n = (gl_FrontFacing) ? normal_viewport : -normal_viewport;
	n = normalize(n);
	normalViewport = workbench_normal_encode(n);
#endif
}
