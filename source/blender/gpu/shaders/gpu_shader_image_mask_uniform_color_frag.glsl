
#if __VERSION__ == 120
  varying vec2 texCoord_interp;
  #define fragColor gl_FragColor
#else
  in vec2 texCoord_interp;
  out vec4 fragColor;
  #define texture2D texture
#endif

uniform sampler2D image;
uniform vec4 color;

void main()
{
	fragColor.a = texture2D(image, texCoord_interp).a * color.a;
	fragColor.rgb = color.rgb;
}
