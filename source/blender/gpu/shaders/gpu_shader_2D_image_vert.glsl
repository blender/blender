
uniform mat4 ModelViewProjectionMatrix;

#if __VERSION__ == 120
  attribute vec2 texCoord;
  attribute vec2 pos;
  varying vec2 texCoord_interp;
#else
  in vec2 texCoord;
  in vec2 pos;
  out vec2 texCoord_interp;
#endif

void main()
{
    gl_Position = ModelViewProjectionMatrix * vec4(pos.xy, 0.0f, 1.0f);
    texCoord_interp = texCoord;
}
