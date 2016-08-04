
#if __VERSION__ == 120
  attribute vec2 pos;
  attribute vec4 color;

  varying vec4 finalColor;
#else
  in vec2 pos;
  in vec4 color;

  noperspective out vec4 finalColor;
#endif

void main()
{
	gl_Position = gl_ModelViewMatrix * vec4(pos, 0.0, 1.0);
	finalColor = color;
}
