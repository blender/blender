
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in vec4 color;

out vec4 finalColor;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	finalColor = color;
}
