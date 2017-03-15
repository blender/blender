
#if __VERSION__ == 120
	attribute vec2 pos;
	attribute vec2 uvs;
	varying vec4 uvcoordsvar;
#else
	in vec2 pos;
	in vec2 uvs;
	out vec4 uvcoordsvar;
#endif

void main()
{
	uvcoordsvar = vec4(uvs, 0.0, 0.0);
	gl_Position = vec4(pos, 0.0, 1.0);
}
