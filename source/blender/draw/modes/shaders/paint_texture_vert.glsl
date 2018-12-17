
uniform mat4 ModelViewProjectionMatrix;

in vec2 u; /* active uv map */
in vec3 pos;

out vec2 uv_interp;

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);

	uv_interp = u;
}
