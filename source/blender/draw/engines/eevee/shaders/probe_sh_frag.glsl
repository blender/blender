
uniform samplerCube probeHdr;
uniform int probeSize;
uniform float lodBias;

in vec3 worldPosition;

out vec4 FragColor;

#define M_4PI 12.5663706143591729

const mat3 CUBE_ROTATIONS[6] = mat3[](
	mat3(vec3( 0.0,  0.0, -1.0),
		 vec3( 0.0, -1.0,  0.0),
		 vec3(-1.0,  0.0,  0.0)),
	mat3(vec3( 0.0,  0.0,  1.0),
		 vec3( 0.0, -1.0,  0.0),
		 vec3( 1.0,  0.0,  0.0)),
	mat3(vec3( 1.0,  0.0,  0.0),
		 vec3( 0.0,  0.0,  1.0),
		 vec3( 0.0, -1.0,  0.0)),
	mat3(vec3( 1.0,  0.0,  0.0),
		 vec3( 0.0,  0.0, -1.0),
		 vec3( 0.0,  1.0,  0.0)),
	mat3(vec3( 1.0,  0.0,  0.0),
		 vec3( 0.0, -1.0,  0.0),
		 vec3( 0.0,  0.0, -1.0)),
	mat3(vec3(-1.0,  0.0,  0.0),
		 vec3( 0.0, -1.0,  0.0),
		 vec3( 0.0,  0.0,  1.0)));

vec3 get_cubemap_vector(vec2 co, int face)
{
	return normalize(CUBE_ROTATIONS[face] * vec3(co * 2.0 - 1.0, 1.0));
}

float area_element(float x, float y)
{
    return atan(x * y, sqrt(x * x + y * y + 1));
}
 
float texel_solid_angle(vec2 co, float halfpix)
{
	vec2 v1 = (co - vec2(halfpix)) * 2.0 - 1.0;
	vec2 v2 = (co + vec2(halfpix)) * 2.0 - 1.0;

	return area_element(v1.x, v1.y) - area_element(v1.x, v2.y) - area_element(v2.x, v1.y) + area_element(v2.x, v2.y);
}

void main()
{
	float pixstep = 1.0 / probeSize;
	float halfpix = pixstep / 2.0;

	float weight_accum = 0.0;
	vec3 sh = vec3(0.0);

	int shnbr = int(floor(gl_FragCoord.x));

	for (int face = 0; face < 6; ++face) {
		for (float x = halfpix; x < 1.0; x += pixstep) {
			for (float y = halfpix; y < 1.0; y += pixstep) {
				float shcoef;

				vec2 facecoord = vec2(x,y);
				vec3 cubevec = get_cubemap_vector(facecoord, face);
				float weight = texel_solid_angle(facecoord, halfpix);

				if (shnbr == 0) {
					shcoef = 0.282095;
				}
				else if (shnbr == 1) {
					shcoef = -0.488603 * cubevec.z * 2.0 / 3.0;
				}
				else if (shnbr == 2) {
					shcoef = 0.488603 * cubevec.y * 2.0 / 3.0;
				}
				else if (shnbr == 3) {
					shcoef = -0.488603 * cubevec.x * 2.0 / 3.0;
				}
				else if (shnbr == 4) {
					shcoef = 1.092548 * cubevec.x * cubevec.z * 1.0 / 4.0;
				}
				else if (shnbr == 5) {
					shcoef = -1.092548 * cubevec.z * cubevec.y * 1.0 / 4.0;
				}
				else if (shnbr == 6) {
					shcoef = 0.315392 * (3.0 * cubevec.y * cubevec.y - 1.0) * 1.0 / 4.0;
				}
				else if (shnbr == 7) {
					shcoef = 1.092548 * cubevec.x * cubevec.y * 1.0 / 4.0;
				}
				else { /* (shnbr == 8) */
					shcoef = 0.546274 * (cubevec.x * cubevec.x - cubevec.z * cubevec.z) * 1.0 / 4.0;
				}

				vec4 sample = textureLod(probeHdr, cubevec, lodBias);
				sh += sample.rgb * shcoef * weight;
				weight_accum += weight;
			}
		}
	}

	sh *= M_4PI / weight_accum;

	FragColor = vec4(sh, 1.0);
}