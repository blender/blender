
uniform mat4 ModelViewProjectionMatrix;
uniform mat3 NormalMatrix;
uniform mat4 ModelViewMatrix;

in vec3 pos;
in vec3 nor;
in int ind;
out vec3 tangent;
out vec3 viewPosition;
flat out float colRand;

float rand(int s)
{
	int seed = s * 1023423;

	seed = (seed ^ 61) ^ (seed >> 16);
	seed *= 9;
	seed = seed ^ (seed >> 4);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15);

	float value = float(seed);
	value *= 1.0 / 42596.0;
	return fract(value);
}

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	tangent = normalize(NormalMatrix * nor);
	viewPosition = (ModelViewMatrix * vec4(pos, 1.0)).xyz;
	colRand = rand(ind);
}
