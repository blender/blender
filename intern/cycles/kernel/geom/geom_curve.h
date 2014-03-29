/*
 * Copyright 2011-2013 Blender Foundation
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

#ifdef __HAIR__

/* curve attributes */

ccl_device float curve_attribute_float(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float *dx, float *dy)
{
	if(elem == ATTR_ELEMENT_CURVE) {
#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = 0.0f;
		if(dy) *dy = 0.0f;
#endif

		return kernel_tex_fetch(__attributes_float, offset + sd->prim);
	}
	else if(elem == ATTR_ELEMENT_CURVE_KEY) {
		float4 curvedata = kernel_tex_fetch(__curves, sd->prim);
		int k0 = __float_as_int(curvedata.x) + sd->segment;
		int k1 = k0 + 1;

		float f0 = kernel_tex_fetch(__attributes_float, offset + k0);
		float f1 = kernel_tex_fetch(__attributes_float, offset + k1);

#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = sd->du.dx*(f1 - f0);
		if(dy) *dy = 0.0f;
#endif

		return (1.0f - sd->u)*f0 + sd->u*f1;
	}
	else {
#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = 0.0f;
		if(dy) *dy = 0.0f;
#endif

		return 0.0f;
	}
}

ccl_device float3 curve_attribute_float3(KernelGlobals *kg, const ShaderData *sd, AttributeElement elem, int offset, float3 *dx, float3 *dy)
{
	if(elem == ATTR_ELEMENT_CURVE) {
		/* idea: we can't derive any useful differentials here, but for tiled
		 * mipmap image caching it would be useful to avoid reading the highest
		 * detail level always. maybe a derivative based on the hair density
		 * could be computed somehow? */
#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = make_float3(0.0f, 0.0f, 0.0f);
		if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);
#endif

		return float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + sd->prim));
	}
	else if(elem == ATTR_ELEMENT_CURVE_KEY) {
		float4 curvedata = kernel_tex_fetch(__curves, sd->prim);
		int k0 = __float_as_int(curvedata.x) + sd->segment;
		int k1 = k0 + 1;

		float3 f0 = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + k0));
		float3 f1 = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + k1));

#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = sd->du.dx*(f1 - f0);
		if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);
#endif

		return (1.0f - sd->u)*f0 + sd->u*f1;
	}
	else {
#ifdef __RAY_DIFFERENTIALS__
		if(dx) *dx = make_float3(0.0f, 0.0f, 0.0f);
		if(dy) *dy = make_float3(0.0f, 0.0f, 0.0f);
#endif

		return make_float3(0.0f, 0.0f, 0.0f);
	}
}

/* hair info node functions */

ccl_device float curve_thickness(KernelGlobals *kg, ShaderData *sd)
{
	float r = 0.0f;

	if(sd->segment != ~0) {
		float4 curvedata = kernel_tex_fetch(__curves, sd->prim);
		int k0 = __float_as_int(curvedata.x) + sd->segment;
		int k1 = k0 + 1;

		float4 P1 = kernel_tex_fetch(__curve_keys, k0);
		float4 P2 = kernel_tex_fetch(__curve_keys, k1);
		r = (P2.w - P1.w) * sd->u + P1.w;
	}

	return r*2.0f;
}

ccl_device float3 curve_tangent_normal(KernelGlobals *kg, ShaderData *sd)
{	
	float3 tgN = make_float3(0.0f,0.0f,0.0f);

	if(sd->segment != ~0) {

		tgN = -(-sd->I - sd->dPdu * (dot(sd->dPdu,-sd->I) / len_squared(sd->dPdu)));
		tgN = normalize(tgN);

		/* need to find suitable scaled gd for corrected normal */
#if 0
		tgN = normalize(tgN - gd * sd->dPdu);
#endif
	}

	return tgN;
}

#endif

CCL_NAMESPACE_END

