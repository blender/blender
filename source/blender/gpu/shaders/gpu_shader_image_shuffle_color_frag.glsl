
in vec2 texCoord_interp;
out vec4 fragColor;
#define texture2D texture

uniform sampler2D image;
uniform vec4 color;
uniform vec4 shuffle;

void main()
{
	vec4 sample = texture2D(image, texCoord_interp);
	fragColor = vec4(sample.r * shuffle.r +
	                 sample.g * shuffle.g +
	                 sample.b * shuffle.b +
	                 sample.a * shuffle.a) * color;
}
