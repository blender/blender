/*
 * Copyright 2011-2014 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

CCL_NAMESPACE_BEGIN

ccl_device float average_fac(float3 v)
{
	return (fabsf(v.x) + fabsf(v.y) + fabsf(v.z))/3.0f;
}

ccl_device void svm_vector_math(float *Fac, float3 *Vector, NodeVectorMath type, float3 Vector1, float3 Vector2)
{
	if(type == NODE_VECTOR_MATH_ADD) {
		*Vector = Vector1 + Vector2;
		*Fac = average_fac(*Vector);
	}
	else if(type == NODE_VECTOR_MATH_SUBTRACT) {
		*Vector = Vector1 - Vector2;
		*Fac = average_fac(*Vector);
	}
	else if(type == NODE_VECTOR_MATH_AVERAGE) {
		*Vector = safe_normalize_len(Vector1 + Vector2, Fac);
	}
	else if(type == NODE_VECTOR_MATH_DOT_PRODUCT) {
		*Fac = dot(Vector1, Vector2);
		*Vector = make_float3(0.0f, 0.0f, 0.0f);
	}
	else if(type == NODE_VECTOR_MATH_CROSS_PRODUCT) {
		*Vector = safe_normalize_len(cross(Vector1, Vector2), Fac);
	}
	else if(type == NODE_VECTOR_MATH_NORMALIZE) {
		*Vector = safe_normalize_len(Vector1, Fac);
	}
	else {
		*Fac = 0.0f;
		*Vector = make_float3(0.0f, 0.0f, 0.0f);
	}
}

ccl_device float svm_math(NodeMath type, float Fac1, float Fac2)
{
	float Fac;

	if(type == NODE_MATH_ADD)
		Fac = Fac1 + Fac2;
	else if(type == NODE_MATH_SUBTRACT)
		Fac = Fac1 - Fac2;
	else if(type == NODE_MATH_MULTIPLY)
		Fac = Fac1*Fac2;
	else if(type == NODE_MATH_DIVIDE)
		Fac = safe_divide(Fac1, Fac2);
	else if(type == NODE_MATH_SINE)
		Fac = sinf(Fac1);
	else if(type == NODE_MATH_COSINE)
		Fac = cosf(Fac1);
	else if(type == NODE_MATH_TANGENT)
		Fac = tanf(Fac1);
	else if(type == NODE_MATH_ARCSINE)
		Fac = safe_asinf(Fac1);
	else if(type == NODE_MATH_ARCCOSINE)
		Fac = safe_acosf(Fac1);
	else if(type == NODE_MATH_ARCTANGENT)
		Fac = atanf(Fac1);
	else if(type == NODE_MATH_POWER)
		Fac = safe_powf(Fac1, Fac2);
	else if(type == NODE_MATH_LOGARITHM)
		Fac = safe_logf(Fac1, Fac2);
	else if(type == NODE_MATH_MINIMUM)
		Fac = fminf(Fac1, Fac2);
	else if(type == NODE_MATH_MAXIMUM)
		Fac = fmaxf(Fac1, Fac2);
	else if(type == NODE_MATH_ROUND)
		Fac = floorf(Fac1 + 0.5f);
	else if(type == NODE_MATH_LESS_THAN)
		Fac = Fac1 < Fac2;
	else if(type == NODE_MATH_GREATER_THAN)
		Fac = Fac1 > Fac2;
	else if(type == NODE_MATH_MODULO)
		Fac = safe_modulo(Fac1, Fac2);
	else if(type == NODE_MATH_ABSOLUTE)
		Fac = fabsf(Fac1);
	else if(type == NODE_MATH_ARCTAN2)
		Fac = atan2f(Fac1, Fac2);
	else if (type == NODE_MATH_FLOOR)
		Fac = floorf(Fac1);
	else if (type == NODE_MATH_CEIL)
		Fac = ceilf(Fac1);
	else if (type == NODE_MATH_FRACT)
		Fac = Fac1 - floorf(Fac1);
	else if (type == NODE_MATH_SQRT)
		Fac = safe_sqrtf(Fac1);
	else if(type == NODE_MATH_CLAMP)
		Fac = saturate(Fac1);
	else
		Fac = 0.0f;

	return Fac;
}

/* Calculate color in range 800..12000 using an approximation
 * a/x+bx+c for R and G and ((at + b)t + c)t + d) for B
 * Max absolute error for RGB is (0.00095, 0.00077, 0.00057),
 * which is enough to get the same 8 bit/channel color.
 */

ccl_static_constant float blackbody_table_r[6][3] = {
	{  2.52432244e+03f, -1.06185848e-03f, 3.11067539e+00f },
	{  3.37763626e+03f, -4.34581697e-04f, 1.64843306e+00f },
	{  4.10671449e+03f, -8.61949938e-05f, 6.41423749e-01f },
	{  4.66849800e+03f,  2.85655028e-05f, 1.29075375e-01f },
	{  4.60124770e+03f,  2.89727618e-05f, 1.48001316e-01f },
	{  3.78765709e+03f,  9.36026367e-06f, 3.98995841e-01f },
};

ccl_static_constant float blackbody_table_g[6][3] = {
	{ -7.50343014e+02f,  3.15679613e-04f, 4.73464526e-01f },
	{ -1.00402363e+03f,  1.29189794e-04f, 9.08181524e-01f },
	{ -1.22075471e+03f,  2.56245413e-05f, 1.20753416e+00f },
	{ -1.42546105e+03f, -4.01730887e-05f, 1.44002695e+00f },
	{ -1.18134453e+03f, -2.18913373e-05f, 1.30656109e+00f },
	{ -5.00279505e+02f, -4.59745390e-06f, 1.09090465e+00f },
};

ccl_static_constant float blackbody_table_b[6][4] = {
	{ 0.0f, 0.0f, 0.0f, 0.0f }, /* zeros should be optimized by compiler */
	{ 0.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 0.0f },
	{ -2.02524603e-11f,  1.79435860e-07f, -2.60561875e-04f, -1.41761141e-02f },
	{ -2.22463426e-13f, -1.55078698e-08f,  3.81675160e-04f, -7.30646033e-01f },
	{  6.72595954e-13f, -2.73059993e-08f,  4.24068546e-04f, -7.52204323e-01f },
};


ccl_device float3 svm_math_blackbody_color(float t)
{
	/* TODO(lukas): Reimplement in XYZ. */

	if(t >= 12000.0f) {
		return make_float3(0.826270103f, 0.994478524f, 1.56626022f);
	}
	else if(t < 965.0f) {
		/* For 800 <= t < 965 color does not change in OSL implementation, so keep color the same */
		return make_float3(4.70366907f, 0.0f, 0.0f);
	}

	int i = (t >= 6365.0f)? 5:
		(t >= 3315.0f)? 4:
		(t >= 1902.0f)? 3:
		(t >= 1449.0f)? 2:
		(t >= 1167.0f)? 1: 0;

	ccl_constant float *r = blackbody_table_r[i];
	ccl_constant float *g = blackbody_table_g[i];
	ccl_constant float *b = blackbody_table_b[i];

	const float t_inv = 1.0f / t;
	return make_float3(r[0] * t_inv + r[1] * t + r[2],
	                   g[0] * t_inv + g[1] * t + g[2],
	                   ((b[0] * t + b[1]) * t + b[2]) * t + b[3]);
}

ccl_device_inline float3 svm_math_gamma_color(float3 color, float gamma)
{
	if(gamma == 0.0f)
		return make_float3(1.0f, 1.0f, 1.0f);

	if(color.x > 0.0f)
		color.x = powf(color.x, gamma);
	if(color.y > 0.0f)
		color.y = powf(color.y, gamma);
	if(color.z > 0.0f)
		color.z = powf(color.z, gamma);

	return color;
}

CCL_NAMESPACE_END
