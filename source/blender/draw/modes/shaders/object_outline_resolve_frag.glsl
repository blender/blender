
in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D outlineBluredColor;

void main()
{
	FragColor = texture(outlineBluredColor, uvcoordsvar.st).rgba;
}
