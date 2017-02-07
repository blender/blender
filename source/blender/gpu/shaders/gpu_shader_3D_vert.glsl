
uniform mat4 ModelViewProjectionMatrix;
#ifdef USE_NORMALS
uniform mat3 NormalMatrix;
#endif

#if __VERSION__ == 120
  attribute vec3 pos;
#ifdef USE_NORMALS
  attribute vec3 nor;
  varying vec3 normal;
#endif
#else
  in vec3 pos;
#ifdef USE_NORMALS
  in vec3 nor;
  out vec3 normal;
#endif
#endif

void main()
{
#ifdef USE_NORMALS
	normal = normalize(NormalMatrix * nor);
#endif
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
}
