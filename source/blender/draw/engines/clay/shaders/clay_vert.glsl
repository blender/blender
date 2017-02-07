uniform mat4 ModelViewProjectionMatrix;
uniform mat3 NormalMatrix;

#if __VERSION__ == 120
attribute vec3 pos;
attribute vec3 nor;
varying vec3 normal;
#else
in vec3 pos;
in vec3 nor;
out vec3 normal;
#endif


void main()
{
	normal = normalize(NormalMatrix * nor);
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
}

