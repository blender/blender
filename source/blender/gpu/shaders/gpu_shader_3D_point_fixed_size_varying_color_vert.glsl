
#if __VERSION__ == 120
  attribute vec3 pos;
  attribute vec4 color;
  varying vec4 finalColor;
#else
  in vec3 pos;
  in vec4 color;
  out vec4 finalColor;
#endif

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * vec4(pos, 1.0);
	finalColor = color;
}
