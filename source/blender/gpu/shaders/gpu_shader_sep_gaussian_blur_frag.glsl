uniform vec2 ScaleU;
uniform sampler2D textureSource;

#if __VERSION__ == 120
  varying vec2 texCoord_interp;
  #define fragColor gl_FragColor
#else
  in vec2 texCoord_interp;
  out vec4 fragColor;
  #define texture2D texture
#endif

void main()
{
	vec4 color = vec4(0.0);
	color += texture2D(textureSource, texCoord_interp.st + vec2(-3.0 * ScaleU.x, -3.0 * ScaleU.y)) * 0.015625;
	color += texture2D(textureSource, texCoord_interp.st + vec2(-2.0 * ScaleU.x, -2.0 * ScaleU.y)) * 0.09375;
	color += texture2D(textureSource, texCoord_interp.st + vec2(-1.0 * ScaleU.x, -1.0 * ScaleU.y)) * 0.234375;
	color += texture2D(textureSource, texCoord_interp.st + vec2(0.0, 0.0)) * 0.3125;
	color += texture2D(textureSource, texCoord_interp.st + vec2(1.0 * ScaleU.x,  1.0 * ScaleU.y)) * 0.234375;
	color += texture2D(textureSource, texCoord_interp.st + vec2(2.0 * ScaleU.x,  2.0 * ScaleU.y)) * 0.09375;
	color += texture2D(textureSource, texCoord_interp.st + vec2(3.0 * ScaleU.x,  3.0 * ScaleU.y)) * 0.015625;

	fragColor = color;
}
