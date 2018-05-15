uniform int object_id = 0;
uniform vec3 object_color = vec3(1.0, 0.0, 1.0);
#ifdef OB_TEXTURE
uniform sampler2D image;
#endif

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
in vec3 normal_viewport;
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */
#ifdef OB_TEXTURE
in vec2 uv_interp;
#endif /* OB_TEXTURE */

layout(location=0) out uint objectId;
layout(location=1) out vec4 diffuseColor;
#ifdef NORMAL_VIEWPORT_PASS_ENABLED
	#ifdef WORKBENCH_ENCODE_NORMALS
layout(location=2) out vec2 normalViewport;
	#else /* WORKBENCH_ENCODE_NORMALS */
layout(location=2) out vec3 normalViewport;
	#endif /* WORKBENCH_ENCODE_NORMALS */
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */

void main()
{
	objectId = uint(object_id);
#ifdef OB_SOLID
	diffuseColor = vec4(object_color, 0.0);
#endif /* OB_SOLID */
#ifdef OB_TEXTURE
	diffuseColor = texture(image, uv_interp);
#endif /* OB_TEXTURE */

#ifdef NORMAL_VIEWPORT_PASS_ENABLED
	#ifdef WORKBENCH_ENCODE_NORMALS
	if (!gl_FrontFacing) {
		normalViewport = normal_encode(normalize(-normal_viewport));
		diffuseColor.a = 1.0;
	}
	else {
		normalViewport = normal_encode(normalize(normal_viewport));
		diffuseColor.a = 0.0;
	}
	#else /* WORKBENCH_ENCODE_NORMALS */
	normalViewport = normal_viewport;
	#endif /* WORKBENCH_ENCODE_NORMALS */
#endif /* NORMAL_VIEWPORT_PASS_ENABLED */
}
