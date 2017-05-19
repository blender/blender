
in vec2 texCoord_interp;
out vec4 fragColor;

uniform vec4 color;
uniform sampler2D image;

void main()
{
	fragColor = texture(image, texCoord_interp) * color;
}
