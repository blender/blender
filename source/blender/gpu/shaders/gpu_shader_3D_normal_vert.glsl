#ifndef USE_GPU_SHADER_CREATE_INFO
uniform mat4 ModelViewProjectionMatrix;
uniform mat3 NormalMatrix;

in vec3 pos;
in vec3 nor;
out vec3 normal;
#endif

void main()
{
  normal = normalize(NormalMatrix * nor);
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
}
