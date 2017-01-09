
/* Keep these in sync with GPU_shader.h */
#define INTERLACE_ROW                      0
#define INTERLACE_COLUMN                   1
#define INTERLACE_CHECKERBOARD             2

#if __VERSION__ == 120
  varying vec2 texCoord_interp;
  #define fragColor gl_FragColor
#else
  in vec2 texCoord_interp;
  out vec4 fragColor;
  #define texture2DRect texture
#endif

uniform int interlace_id;
uniform sampler2DRect image_a;
uniform sampler2DRect image_b;

bool interlace()
{
#if __VERSION__ == 120
	if (interlace_id == INTERLACE_CHECKERBOARD) {
		return int(mod(gl_FragCoord.x + gl_FragCoord.y, 2)) != 0;
	}
	else if (interlace_id == INTERLACE_ROW) {
		return int(mod(gl_FragCoord.y, 2)) != 0;
	}
	else if (interlace_id == INTERLACE_COLUMN) {
		return int(mod(gl_FragCoord.x, 2)) != 0;
	}
#else
	if (interlace_id == INTERLACE_CHECKERBOARD) {
		return (int(gl_FragCoord.x + gl_FragCoord.y) & 1) != 0;
	}
	else if (interlace_id == INTERLACE_ROW) {
		return (int(gl_FragCoord.y) & 1) != 0;
	}
	else if (interlace_id == INTERLACE_COLUMN) {
		return (int(gl_FragCoord.x) & 1) != 0;
	}
#endif
}

void main()
{
	if (interlace()) {
		fragColor = texture2DRect(image_a, texCoord_interp);
	} else {
		fragColor = texture2DRect(image_b, texCoord_interp);
	}
}
