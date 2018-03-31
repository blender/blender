
in vec2 texCoord_interp;
flat in vec4 finalColor;
out vec4 fragColor;

uniform sampler2D image;

void main()
{
	fragColor = texture(image, texCoord_interp) * finalColor;
}
