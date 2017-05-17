
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in float size;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	gl_PointSize = size;
}
