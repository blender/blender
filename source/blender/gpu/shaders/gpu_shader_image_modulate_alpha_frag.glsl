
in vec2 texCoord_interp;
out vec4 fragColor;

uniform float alpha;
uniform sampler2D image;

void main()
{
	fragColor = texture(image, texCoord_interp);
	fragColor.a *= alpha;
}
