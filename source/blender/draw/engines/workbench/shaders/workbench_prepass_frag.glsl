uniform int object_id = 0;

layout(std140) uniform material_block {
	MaterialData material_data;
};

#ifdef OB_TEXTURE
uniform sampler2D image;
#endif

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
in vec3 normal_viewport;
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */

#ifdef OB_TEXTURE
in vec2 uv_interp;
#endif /* OB_TEXTURE */

#ifdef HAIR_SHADER
flat in float hair_rand;
#endif

layout(location=0) out uint objectId;
layout(location=1) out vec4 diffuseColor;
layout(location=2) out vec4 specularColor;
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

#ifdef OB_SOLID
	diffuseColor = vec4(material_data.diffuse_color.rgb, 0.0);
#  ifdef STUDIOLIGHT_ORIENTATION_VIEWNORMAL
	specularColor = vec4(material_data.diffuse_color.rgb, material_data.matcap_texture_index);
#  endif
#endif /* OB_SOLID */

#ifdef OB_TEXTURE
	diffuseColor = texture(image, uv_interp);
#endif /* OB_TEXTURE */
#ifdef HAIR_SHADER
	float hair_color_variation = hair_rand * 0.1;
	diffuseColor.rgb = clamp(diffuseColor.rgb - hair_color_variation, 0.0, 1.0);
#endif

#ifdef V3D_SHADING_SPECULAR_HIGHLIGHT
	specularColor = vec4(material_data.specular_color.rgb, material_data.roughness);
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
