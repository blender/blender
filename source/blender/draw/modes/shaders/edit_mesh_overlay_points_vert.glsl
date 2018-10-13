
uniform mat3 NormalMatrix;
uniform mat4 ProjectionMatrix;
uniform mat4 ModelViewMatrix;
uniform mat4 ModelViewProjectionMatrix;
uniform float ofs = 1e-3;

in vec3 pos;
in ivec4 data;
#ifdef VERTEX_FACING
in vec3 vnor;
#endif

out vec4 finalColor;

void main()
{
	finalColor = colorVertex;
	finalColor = ((data.x & VERTEX_SELECTED) != 0) ? colorVertexSelect : finalColor;
	finalColor = ((data.x & VERTEX_ACTIVE) != 0) ? vec4(colorEditMeshActive.xyz, 1.0) : finalColor;

	gl_PointSize = sizeVertex * 2.0;
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	gl_Position.z -= ofs;

#ifdef VERTEX_FACING
	vec4 vPos = ModelViewMatrix * vec4(pos, 1.0);
	vec3 view_normal = normalize(NormalMatrix * vnor);
	vec3 view_vec = (ProjectionMatrix[3][3] == 0.0)
		? normalize(vPos.xyz)
		: vec3(0.0, 0.0, 1.0);
	float facing = dot(view_vec, view_normal);

	finalColor.a *= 1.0 - abs(facing) * 0.4;
#endif

	if ((data.x & VERTEX_EXISTS) == 0) {
		gl_Position = vec4(0.0);
		gl_PointSize = 0.0;
	}
}
