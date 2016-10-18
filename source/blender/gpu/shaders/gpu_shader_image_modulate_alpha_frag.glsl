
#if __VERSION__ == 120
  varying vec2 texCoord_interp;
  #define fragColor gl_FragColor
#else
  in vec2 texCoord_interp;
  out vec4 fragColor;
  #define texture2D texture
#endif

uniform float alpha;
uniform sampler2D image;

void main()
{
	fragColor = texture2D(image, texCoord_interp);
	fragColor.a *= alpha;
}
