
in vec3 pos;

uniform mat4 ModelViewProjectionMatrix;
uniform mat4 ModelMatrix;

out vec3 worldPosition;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	worldPosition = (ModelMatrix * vec4(pos, 1.0)).xyz;
}