
/* To be compiled with common_hair_lib.glsl */

out vec4 outData;

vec4 get_weights_cardinal(float t)
{
	float t2 = t * t;
	float t3 = t2 * t;
#if defined(CARDINAL)
	float fc = 0.71;
#else /* defined(CATMULL_ROM) */
	float fc = 0.5;
#endif

	vec4 weights;
	/* GLSL Optimized version of key_curve_position_weights() */
	float fct  = t  * fc;
	float fct2 = t2 * fc;
	float fct3 = t3 * fc;
	weights.x = ( fct2 * 2.0 - fct3) - fct;
	weights.y = ( t3 * 2.0 - fct3) + (-t2 * 3.0 + fct2) + 1.0;
	weights.z = (-t3 * 2.0 + fct3) + ( t2 * 3.0 - (2.0 * fct2)) + fct;
	weights.w =   fct3 - fct2;
	return weights;
}

/* TODO(fclem): This one is buggy, find why. (it's not the optimization!!) */
vec4 get_weights_bspline(float t)
{
	float t2 = t * t;
	float t3 = t2 * t;

	vec4 weights;
	/* GLSL Optimized version of key_curve_position_weights() */
	weights.xz = vec2(-0.16666666, -0.5) * t3 + (0.5 * t2 + 0.5 * vec2(-t, t) + 0.16666666);
	weights.y  = ( 0.5        * t3 - t2 + 0.66666666);
	weights.w  = ( 0.16666666 * t3);
	return weights;
}

vec4 interp_data(vec4 v0, vec4 v1, vec4 v2, vec4 v3, vec4 w)
{
	return v0 * w.x + v1 * w.y + v2 * w.z + v3 * w.w;
}

void main(void)
{
	float interp_time;
	vec4 data0, data1, data2, data3;
	hair_get_interp_attrs(data0, data1, data2, data3, interp_time);

	vec4 weights = get_weights_cardinal(interp_time);
	outData = interp_data(data0, data1, data2, data3, weights);
}
