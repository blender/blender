
uniform mat4 ModelViewProjectionMatrix;

#if __VERSION__ == 120
  attribute vec2 pos;
  attribute vec2 uvs;
  varying vec2 texCoord_interp;
#else
  in vec2 pos;
  in vec2 uvs;
  out vec2 texCoord_interp;
#endif

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 0.0, 1.0);
	texCoord_interp = uvs;
}
