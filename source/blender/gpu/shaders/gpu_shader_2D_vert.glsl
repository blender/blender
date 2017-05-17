
uniform mat4 ModelViewProjectionMatrix;

in vec2 pos;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
}
