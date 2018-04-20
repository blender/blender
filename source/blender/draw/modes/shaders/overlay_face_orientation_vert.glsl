
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in vec3 nor;

uniform mat4 ProjectionMatrix;
uniform mat4 ModelViewMatrix;
uniform mat3 NormalMatrix;

flat out float facing;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	vec3 view_normal = normalize(NormalMatrix * nor);
	vec3 view_vec = (ProjectionMatrix[3][3] == 0.0)
		? normalize((ModelViewMatrix * vec4(pos, 1.0)).xyz)
		: vec3(0.0, 0.0, 1.0);
	facing = dot(view_vec, view_normal);
}
