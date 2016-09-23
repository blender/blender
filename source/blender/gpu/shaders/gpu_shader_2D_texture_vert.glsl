#if __VERSION__ == 120
  attribute vec2 texcoord;
  attribute vec3 position;
  varying vec2 texture_coord;
#else
  in vec2 texcoord;
  in vec3 position;
  out vec2 texture_coord;
#endif


void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * vec4(position.xyz, 1.0f);
	texture_coord = texcoord;
}
