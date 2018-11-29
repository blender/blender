
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in int data;

flat out int finalFlag;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	finalFlag = data;
}
