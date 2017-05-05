
uniform mat4 ModelViewProjectionMatrix;

in vec3 pos;
in int select;

flat out int finalSelect;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	finalSelect = select;
}
