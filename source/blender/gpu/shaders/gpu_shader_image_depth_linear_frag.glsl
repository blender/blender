
in vec2 texCoord_interp;
out vec4 fragColor;

uniform float znear;
uniform float zfar;
uniform sampler2D image;

void main()
{
	float depth = texture(image, texCoord_interp).r;

	/* normalize */
	fragColor.rgb = vec3((2.0f * znear) / (zfar + znear - (depth * (zfar - znear))));
	fragColor.a = 1.0f;
}
