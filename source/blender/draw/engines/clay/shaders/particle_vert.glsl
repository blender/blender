
uniform mat4 ModelViewProjectionMatrix;
uniform mat3 NormalMatrix;
uniform mat4 ModelViewMatrix;

in vec3 pos;
in vec3 nor;
in int ind;
out vec3 normal;
out vec3 viewPosition;
flat out float colRand;

/* TODO: This function yields great distribution, but might be a bit inefficient because of the 4 trig ops.
 * Something more efficient would be nice */
float rand(int seed)
{
	vec4 nums = vec4(0.0);
	nums.x = mod(tan(mod(float(seed + 1) * 238965.0, 342.0)), 1.0) + 0.01;
	nums.y = mod(tan(mod(float(seed + 1) * 34435643.0, 756.0)), 1.0) + 0.01;
	nums.z = mod(tan(mod(float(seed + 1) * 4356757.0, 456.0)), 1.0) + 0.01;
	nums.w = mod(tan(mod(float(seed + 1) * 778679.0, 987.0)), 1.0) + 0.01;

	float num = mod((nums.x / nums.y) + 1 - (nums.z / nums.w), 1.0);
	num += 0.5;
	return mod(num, 1.0);
}

void main()
{
	gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
	normal = normalize(NormalMatrix * nor);
	viewPosition = (ModelViewMatrix * vec4(pos, 1.0)).xyz;
	colRand = rand(ind);
}
