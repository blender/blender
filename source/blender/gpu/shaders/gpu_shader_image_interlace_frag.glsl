
/* Keep these in sync with GPU_shader.h */
#define INTERLACE_ROW                      0
#define INTERLACE_COLUMN                   1
#define INTERLACE_CHECKERBOARD             2

in vec2 texCoord_interp;
out vec4 fragColor;

uniform int interlace_id;
uniform sampler2D image_a;
uniform sampler2D image_b;

bool interlace()
{
	if (interlace_id == INTERLACE_CHECKERBOARD) {
		return (int(gl_FragCoord.x + gl_FragCoord.y) & 1) != 0;
	}
	else if (interlace_id == INTERLACE_ROW) {
		return (int(gl_FragCoord.y) & 1) != 0;
	}
	else if (interlace_id == INTERLACE_COLUMN) {
		return (int(gl_FragCoord.x) & 1) != 0;
	}
}

void main()
{
	if (interlace()) {
		fragColor = texture(image_a, texCoord_interp);
	} else {
		fragColor = texture(image_b, texCoord_interp);
	}
}
