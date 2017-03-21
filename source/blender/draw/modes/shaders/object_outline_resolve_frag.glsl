
in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D outlineBluredColor;
uniform sampler2D outlineDepth;

void main()
{
	FragColor = texture(outlineBluredColor, uvcoordsvar.st).rgba;

	/* Modulate fill color */
	// float depth = texture(outlineDepth, uvcoordsvar.st).r;
	// if (depth != 1.0)
	// 	FragColor.a *= 0.1;
}
