
#ifndef USE_GPU_SHADER_CREATE_INFO
uniform mat4 ModelViewProjectionMatrix;

in vec2 texCoord;
in vec3 pos;
out vec2 texCoord_interp;
#endif

void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos.xyz, 1.0f);
  texCoord_interp = texCoord;
}
