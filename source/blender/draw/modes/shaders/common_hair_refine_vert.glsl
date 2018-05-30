
/* To be compiled with common_hair_lib.glsl */

out vec4 outData;

void main(void)
{
	float interp_time;
	vec4 data0, data1, data2, data3;
	hair_get_interp_attribs(data0, data1, data2, data3, interp_time);

	/* TODO some interpolation. */

	outData = mix(data1, data2, interp_time);
}
