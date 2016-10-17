#if __VERSION__ == 120
  varying vec2 texture_coord;
  #define fragColor gl_FragColor
#else
  in vec2 texture_coord;
  out vec4 fragColor;
  #define texture2D texture
#endif

uniform float alpha;
uniform sampler2D texture_map;

void main()
{
	fragColor = texture2D(texture_map, texture_coord);
	fragColor.a *= alpha;
}
