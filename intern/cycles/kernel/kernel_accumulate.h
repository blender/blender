/*
 * Copyright 2011, Blender Foundation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

CCL_NAMESPACE_BEGIN

/* BSDF Eval
 *
 * BSDF evaluation result, split per BSDF type. This is used to accumulate
 * render passes separately. */

__device_inline void bsdf_eval_init(BsdfEval *eval, ClosureType type, float3 value, int use_light_pass)
{
#ifdef __PASSES__
	eval->use_light_pass = use_light_pass;

	if(eval->use_light_pass) {
		eval->diffuse = make_float3(0.0f, 0.0f, 0.0f);
		eval->glossy = make_float3(0.0f, 0.0f, 0.0f);
		eval->transmission = make_float3(0.0f, 0.0f, 0.0f);
		eval->transparent = make_float3(0.0f, 0.0f, 0.0f);

		if(type == CLOSURE_BSDF_TRANSPARENT_ID)
			eval->transparent = value;
		else if(CLOSURE_IS_BSDF_DIFFUSE(type))
			eval->diffuse = value;
		else if(CLOSURE_IS_BSDF_GLOSSY(type))
			eval->glossy = value;
		else
			eval->transmission = value;
	}
	else
		eval->diffuse = value;
#else
	*eval = value;
#endif
}

__device_inline void bsdf_eval_accum(BsdfEval *eval, ClosureType type, float3 value)
{
#ifdef __PASSES__
	if(eval->use_light_pass) {
		if(CLOSURE_IS_BSDF_DIFFUSE(type))
			eval->diffuse += value;
		else if(CLOSURE_IS_BSDF_GLOSSY(type))
			eval->glossy += value;
		else
			eval->transmission += value;

		/* skipping transparent, this function is used by for eval(), will be zero then */
	}
	else
		eval->diffuse += value;
#else
	*eval += value;
#endif
}

__device_inline bool bsdf_eval_is_zero(BsdfEval *eval)
{
#ifdef __PASSES__
	if(eval->use_light_pass) {
		return is_zero(eval->diffuse)
			&& is_zero(eval->glossy)
			&& is_zero(eval->transmission)
			&& is_zero(eval->transparent);
	}
	else
		return is_zero(eval->diffuse);
#else
	return is_zero(*eval);
#endif
}

__device_inline void bsdf_eval_mul(BsdfEval *eval, float3 value)
{
#ifdef __PASSES__
	if(eval->use_light_pass) {
		eval->diffuse *= value;
		eval->glossy *= value;
		eval->transmission *= value;

		/* skipping transparent, this function is used by for eval(), will be zero then */
	}
	else
		eval->diffuse *= value;
#else
	*eval *= value;
#endif
}

/* Path Radiance
 *
 * We accumulate different render passes separately. After summing at the end
 * to get the combined result, it should be identical. We definte directly
 * visible as the first non-transparent hit, while indirectly visible are the
 * bounces after that. */

__device_inline void path_radiance_init(PathRadiance *L, int use_light_pass)
{
	/* clear all */
#ifdef __PASSES__
	L->use_light_pass = use_light_pass;

	if(use_light_pass) {
		L->indirect = make_float3(0.0f, 0.0f, 0.0f);
		L->direct_throughput = make_float3(0.0f, 0.0f, 0.0f);
		L->direct_emission = make_float3(0.0f, 0.0f, 0.0f);

		L->color_diffuse = make_float3(0.0f, 0.0f, 0.0f);
		L->color_glossy = make_float3(0.0f, 0.0f, 0.0f);
		L->color_transmission = make_float3(0.0f, 0.0f, 0.0f);

		L->direct_diffuse = make_float3(0.0f, 0.0f, 0.0f);
		L->direct_glossy = make_float3(0.0f, 0.0f, 0.0f);
		L->direct_transmission = make_float3(0.0f, 0.0f, 0.0f);

		L->indirect_diffuse = make_float3(0.0f, 0.0f, 0.0f);
		L->indirect_glossy = make_float3(0.0f, 0.0f, 0.0f);
		L->indirect_transmission = make_float3(0.0f, 0.0f, 0.0f);

		L->emission = make_float3(0.0f, 0.0f, 0.0f);
		L->background = make_float3(0.0f, 0.0f, 0.0f);
		L->ao = make_float3(0.0f, 0.0f, 0.0f);
		L->shadow = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
	}
	else
		L->emission = make_float3(0.0f, 0.0f, 0.0f);
#else
	*L = make_float3(0.0f, 0.0f, 0.0f);
#endif
}

__device_inline void path_radiance_bsdf_bounce(PathRadiance *L, float3 *throughput,
	BsdfEval *bsdf_eval, float bsdf_pdf, int bounce, int bsdf_label)
{
	float inverse_pdf = 1.0f/bsdf_pdf;

#ifdef __PASSES__
	if(L->use_light_pass) {
		if(bounce == 0 && !(bsdf_label & LABEL_TRANSPARENT)) {
			/* first on directly visible surface */
			float3 value = *throughput*inverse_pdf;

			L->indirect_diffuse = bsdf_eval->diffuse*value;
			L->indirect_glossy = bsdf_eval->glossy*value;
			L->indirect_transmission = bsdf_eval->transmission*value;

			*throughput = L->indirect_diffuse + L->indirect_glossy + L->indirect_transmission;
			
			L->direct_throughput = *throughput;
		}
		else {
			/* transparent bounce before first hit, or indirectly visible through BSDF */
			float3 sum = (bsdf_eval->diffuse + bsdf_eval->glossy + bsdf_eval->transmission + bsdf_eval->transparent)*inverse_pdf;
			*throughput *= sum;
		}
	}
	else
		*throughput *= bsdf_eval->diffuse*inverse_pdf;
#else
	*throughput *= *bsdf_eval*inverse_pdf;
#endif
}

__device_inline void path_radiance_accum_emission(PathRadiance *L, float3 throughput, float3 value, int bounce)
{
#ifdef __PASSES__
	if(L->use_light_pass) {
		if(bounce == 0)
			L->emission += throughput*value;
		else if(bounce == 1)
			L->direct_emission += throughput*value;
		else
			L->indirect += throughput*value;
	}
	else
		L->emission += throughput*value;
#else
	*L += throughput*value;
#endif
}

__device_inline void path_radiance_accum_ao(PathRadiance *L, float3 throughput, float3 bsdf, float3 ao, int bounce)
{
#ifdef __PASSES__
	if(L->use_light_pass) {
		if(bounce == 0) {
			/* directly visible lighting */
			L->direct_diffuse += throughput*bsdf*ao;
			L->ao += throughput*ao;
		}
		else {
			/* indirectly visible lighting after BSDF bounce */
			L->indirect += throughput*bsdf*ao;
		}
	}
	else
		L->emission += throughput*bsdf*ao;
#else
	*L += throughput*bsdf*ao;
#endif
}

__device_inline void path_radiance_accum_light(PathRadiance *L, float3 throughput, BsdfEval *bsdf_eval, float3 shadow, int bounce, bool is_lamp)
{
#ifdef __PASSES__
	if(L->use_light_pass) {
		if(bounce == 0) {
			/* directly visible lighting */
			L->direct_diffuse += throughput*bsdf_eval->diffuse*shadow;
			L->direct_glossy += throughput*bsdf_eval->glossy*shadow;
			L->direct_transmission += throughput*bsdf_eval->transmission*shadow;

			if(is_lamp) {
				float3 sum = throughput*(bsdf_eval->diffuse + bsdf_eval->glossy + bsdf_eval->transmission);

				L->shadow.x += shadow.x;
				L->shadow.y += shadow.y;
				L->shadow.z += shadow.z;
				L->shadow.w += average(sum);
			}
		}
		else {
			/* indirectly visible lighting after BSDF bounce */
			float3 sum = bsdf_eval->diffuse + bsdf_eval->glossy + bsdf_eval->transmission;
			L->indirect += throughput*sum*shadow;
		}
	}
	else
		L->emission += throughput*bsdf_eval->diffuse*shadow;
#else
	*L += throughput*(*bsdf_eval)*shadow;
#endif
}

__device_inline void path_radiance_accum_background(PathRadiance *L, float3 throughput, float3 value, int bounce)
{
#ifdef __PASSES__
	if(L->use_light_pass) {
		if(bounce == 0)
			L->background += throughput*value;
		else if(bounce == 1)
			L->direct_emission += throughput*value;
		else
			L->indirect += throughput*value;
	}
	else
		L->emission += throughput*value;
#else
	*L += throughput*value;
#endif
}

__device_inline float3 path_radiance_sum(PathRadiance *L)
{
#ifdef __PASSES__
	if(L->use_light_pass) {
		/* this division is a bit ugly, but means we only have to keep track of
		   only a single throughput further along the path, here we recover just
		   the indirect parth that is not influenced by any particular BSDF type */
		L->direct_emission = safe_divide_color(L->direct_emission, L->direct_throughput);
		L->direct_diffuse += L->indirect_diffuse*L->direct_emission;
		L->direct_glossy += L->indirect_glossy*L->direct_emission;
		L->direct_transmission += L->indirect_transmission*L->direct_emission;

		L->indirect = safe_divide_color(L->indirect, L->direct_throughput);
		L->indirect_diffuse *= L->indirect;
		L->indirect_glossy *= L->indirect;
		L->indirect_transmission *= L->indirect;

		return L->emission + L->background
			+ L->direct_diffuse + L->direct_glossy + L->direct_transmission
			+ L->indirect_diffuse + L->indirect_glossy + L->indirect_transmission;
	}
	else
		return L->emission;
#else
	return *L;
#endif
}

__device_inline void path_radiance_clamp(PathRadiance *L, float3 *L_sum, float clamp)
{
	float sum = fabsf((*L_sum).x) + fabsf((*L_sum).y) + fabsf((*L_sum).z);

	if(!isfinite(sum)) {
		/* invalid value, reject */
		*L_sum = make_float3(0.0f, 0.0f, 0.0f);

#ifdef __PASSES__
		if(L->use_light_pass) {
			L->direct_diffuse = make_float3(0.0f, 0.0f, 0.0f);
			L->direct_glossy = make_float3(0.0f, 0.0f, 0.0f);
			L->direct_transmission = make_float3(0.0f, 0.0f, 0.0f);

			L->indirect_diffuse = make_float3(0.0f, 0.0f, 0.0f);
			L->indirect_glossy = make_float3(0.0f, 0.0f, 0.0f);
			L->indirect_transmission = make_float3(0.0f, 0.0f, 0.0f);

			L->emission = make_float3(0.0f, 0.0f, 0.0f);
		}
#endif
	}
	else if(sum > clamp) {
		/* value to high, scale down */
		float scale = clamp/sum;

		*L_sum *= scale;

#ifdef __PASSES__
		if(L->use_light_pass) {
			L->direct_diffuse *= scale;
			L->direct_glossy *= scale;
			L->direct_transmission *= scale;

			L->indirect_diffuse *= scale;
			L->indirect_glossy *= scale;
			L->indirect_transmission *= scale;

			L->emission *= scale;
		}
#endif
	}
}

CCL_NAMESPACE_END

