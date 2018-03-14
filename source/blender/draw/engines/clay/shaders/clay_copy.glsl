
in vec4 uvcoordsvar;
out vec4 fragColor;

uniform sampler2D colortex;

void main()
{
	fragColor = texture(colortex, uvcoordsvar.st);
}
