uniform int object_id = 0;
uniform vec3 object_color = vec3(1.0, 0.0, 1.0);

in vec3 normal_viewport;

out uint objectId;
out vec4 diffuseColor;
#ifdef WORKBENCH_ENCODE_NORMALS
out vec2 normalViewport;
#else
out vec3 normalViewport;
#endif

void main()
{
	objectId = uint(object_id);
	diffuseColor = vec4(object_color, 0.0);
#ifdef WORKBENCH_ENCODE_NORMALS
	if (!gl_FrontFacing) {
		normalViewport = normal_encode(-normal_viewport);
		diffuseColor.a = 1.0;
	} else {
		normalViewport = normal_encode(normal_viewport);
	}
#else /* WORKBENCH_ENCODE_NORMALS */
	normalViewport = normal_viewport;
#endif /* WORKBENCH_ENCODE_NORMALS */
}
