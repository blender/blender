
uniform mat4 ModelViewProjectionMatrix;

in vec2 uv;
in vec3 pos;

out vec2 uv_interp;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	uv_interp = uv;

}
