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
 * limitations under the License
 */

CCL_NAMESPACE_BEGIN

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
	else if(type == NODE_MATH_CLAMP)
		Fac = clamp(Fac1, 0.0f, 1.0f);
	else
		Fac = 0.0f;
	
	return Fac;
}

CCL_NAMESPACE_END

