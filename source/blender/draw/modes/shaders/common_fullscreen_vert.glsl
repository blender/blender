
in vec2 pos;
in vec2 uvs;
out vec4 uvcoordsvar;

void main()
{
	uvcoordsvar = vec4(uvs, 0.0, 0.0);
	gl_Position = vec4(pos, 0.0, 1.0);
}
