
/* Draw Curve Normals */

uniform mat4 ModelViewProjectionMatrix;
uniform float normalSize;

in vec3 pos;
in vec3 nor;
in vec3 tan;
in float rad;

void main()
{
	vec3 final_pos = pos;

	float flip = (gl_InstanceID != 0) ? -1.0 : 1.0;

	if (gl_VertexID % 2 == 0) {
		final_pos += normalSize * rad * (flip * nor - tan);
	}

	gl_Position = ModelViewProjectionMatrix * vec4(final_pos, 1.0);
}
