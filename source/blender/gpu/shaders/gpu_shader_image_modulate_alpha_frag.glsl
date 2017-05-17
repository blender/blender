
in vec2 texCoord_interp;
out vec4 fragColor;
#define texture2D texture

uniform float alpha;
uniform sampler2D image;

void main()
{
	fragColor = texture2D(image, texCoord_interp);
	fragColor.a *= alpha;
}
