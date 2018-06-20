
in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D colorBuffer;
uniform vec2 invertedViewportSize;

void main()
{
	FragColor = FxaaPixelShader(
	    uvcoordsvar.st,
	    colorBuffer,
	    invertedViewportSize,
	    1.0,
	    0.166,
	    0.0833
	);
}
