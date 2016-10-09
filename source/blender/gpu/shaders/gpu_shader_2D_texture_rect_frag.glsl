#if __VERSION__ == 120
  varying vec2 texture_coord;
  #define fragColor gl_FragColor
#else
  in vec2 texture_coord;
  out vec4 fragColor;
  #define texture2DRect texture
#endif

uniform float alpha;
uniform sampler2DRect texture_map;

void main()
{
	fragColor = texture2DRect(texture_map, texture_coord);
	fragColor.a *= alpha;
}
