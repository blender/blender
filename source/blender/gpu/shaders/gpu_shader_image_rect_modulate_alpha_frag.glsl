
#if __VERSION__ == 120
  varying vec2 texCoord_interp;
  #define fragColor gl_FragColor
#else
  in vec2 texCoord_interp;
  out vec4 fragColor;
  #define texture2DRect texture
#endif

uniform float alpha;
uniform sampler2DRect image;

void main()
{
	fragColor = texture2DRect(image, texCoord_interp);
	fragColor.a *= alpha;
}
