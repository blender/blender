uniform int object_id = 0;

uniform vec4 materialDiffuseColor;
uniform vec4 materialSpecularColor;
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
layout(location=1) out vec4 diffuseColor;
#ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
layout(location=2) out vec4 specularColor;
#endif
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

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
	vec3 n = (gl_FrontFacing) ? normal_viewport : -normal_viewport;
	n = normalize(n);
#endif

#ifdef V3D_SHADING_TEXTURE_COLOR
	diffuseColor = texture(image, uv_interp);
	if (diffuseColor.a < ImageTransparencyCutoff) {
		discard;
	}
#else
	diffuseColor = vec4(materialDiffuseColor.rgb, 0.0);
#endif /* V3D_SHADING_TEXTURE_COLOR */

#ifdef HAIR_SHADER
	float hair_color_variation = hair_rand * 0.1;
	diffuseColor.rgb = clamp(diffuseColor.rgb - hair_color_variation, 0.0, 1.0);
#endif

#ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
	specularColor = vec4(materialSpecularColor.rgb, materialRoughness);
#  ifdef HAIR_SHADER
	specularColor.rgb = clamp(specularColor.rgb - hair_color_variation, 0.0, 1.0);
#  endif
#endif

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
#  ifdef WORKBENCH_ENCODE_NORMALS
	diffuseColor.a = float(gl_FrontFacing);
	normalViewport = normal_encode(n);
#  else /* WORKBENCH_ENCODE_NORMALS */
	normalViewport = n;
#  endif /* WORKBENCH_ENCODE_NORMALS */
#  ifdef HAIR_SHADER
	diffuseColor.a = 0.5;
#  endif
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */
}
