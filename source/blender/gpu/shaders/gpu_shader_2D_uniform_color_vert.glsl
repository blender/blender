
#if __VERSION__ == 120
  attribute vec2 pos;
#else
  in vec2 pos;
#endif

void main()
{
	gl_Position = gl_ModelViewMatrix * vec4(pos, 0.0, 1.0);
}
