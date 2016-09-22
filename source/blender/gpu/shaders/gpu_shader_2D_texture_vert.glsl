#if __VERSION__ == 120
  varying vec2 texture_coord;
#else
  out vec2 texture_coord;
#endif

in vec2 texcoord;
in vec3 position;

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * vec4(position.xyz, 1.0f);
	texture_coord = texcoord;
}
