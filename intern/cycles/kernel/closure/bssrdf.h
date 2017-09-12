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
 * limitations under the License.
 */

#ifndef __KERNEL_BSSRDF_H__
#define __KERNEL_BSSRDF_H__

CCL_NAMESPACE_BEGIN

typedef ccl_addr_space struct Bssrdf {
	SHADER_CLOSURE_BASE;

	float radius;
	float sharpness;
	float d;
	float texture_blur;
	float albedo;
	float roughness;
} Bssrdf;

/* Planar Truncated Gaussian
 *
 * Note how this is different from the typical gaussian, this one integrates
 * to 1 over the plane (where you get an extra 2*pi*x factor). We are lucky
 * that integrating x*exp(-x) gives a nice closed form solution. */

/* paper suggests 1/12.46 which is much too small, suspect it's *12.46 */
#define GAUSS_TRUNCATE 12.46f

ccl_device float bssrdf_gaussian_eval(const ShaderClosure *sc, float r)
{
	/* integrate (2*pi*r * exp(-r*r/(2*v)))/(2*pi*v)) from 0 to Rm
	 * = 1 - exp(-Rm*Rm/(2*v)) */
	const Bssrdf *bssrdf = (const Bssrdf*)sc;
	const float v = bssrdf->radius*bssrdf->radius*(0.25f*0.25f);
	const float Rm = sqrtf(v*GAUSS_TRUNCATE);

	if(r >= Rm)
		return 0.0f;

	return expf(-r*r/(2.0f*v))/(2.0f*M_PI_F*v);
}

ccl_device float bssrdf_gaussian_pdf(const ShaderClosure *sc, float r)
{
	/* 1.0 - expf(-Rm*Rm/(2*v)) simplified */
	const float area_truncated = 1.0f - expf(-0.5f*GAUSS_TRUNCATE);

	return bssrdf_gaussian_eval(sc, r) * (1.0f/(area_truncated));
}

ccl_device void bssrdf_gaussian_sample(const ShaderClosure *sc, float xi, float *r, float *h)
{
	/* xi = integrate (2*pi*r * exp(-r*r/(2*v)))/(2*pi*v)) = -exp(-r^2/(2*v))
	 * r = sqrt(-2*v*logf(xi)) */
	const Bssrdf *bssrdf = (const Bssrdf*)sc;
	const float v = bssrdf->radius*bssrdf->radius*(0.25f*0.25f);
	const float Rm = sqrtf(v*GAUSS_TRUNCATE);

	/* 1.0 - expf(-Rm*Rm/(2*v)) simplified */
	const float area_truncated = 1.0f - expf(-0.5f*GAUSS_TRUNCATE);

	/* r(xi) */
	const float r_squared = -2.0f*v*logf(1.0f - xi*area_truncated);
	*r = sqrtf(r_squared);

	/* h^2 + r^2 = Rm^2 */
	*h = safe_sqrtf(Rm*Rm - r_squared);
}

/* Planar Cubic BSSRDF falloff
 *
 * This is basically (Rm - x)^3, with some factors to normalize it. For sampling
 * we integrate 2*pi*x * (Rm - x)^3, which gives us a quintic equation that as
 * far as I can tell has no closed form solution. So we get an iterative solution
 * instead with newton-raphson. */

ccl_device float bssrdf_cubic_eval(const ShaderClosure *sc, float r)
{
	const Bssrdf *bssrdf = (const Bssrdf*)sc;
	const float sharpness = bssrdf->sharpness;

	if(sharpness == 0.0f) {
		const float Rm = bssrdf->radius;

		if(r >= Rm)
			return 0.0f;

		/* integrate (2*pi*r * 10*(R - r)^3)/(pi * R^5) from 0 to R = 1 */
		const float Rm5 = (Rm*Rm) * (Rm*Rm) * Rm;
		const float f = Rm - r;
		const float num = f*f*f;

		return (10.0f * num) / (Rm5 * M_PI_F);

	}
	else {
		float Rm = bssrdf->radius*(1.0f + sharpness);

		if(r >= Rm)
			return 0.0f;

		/* custom variation with extra sharpness, to match the previous code */
		const float y = 1.0f/(1.0f + sharpness);
		float Rmy, ry, ryinv;

		if(sharpness == 1.0f) {
			Rmy = sqrtf(Rm);
			ry = sqrtf(r);
			ryinv = (ry > 0.0f)? 1.0f/ry: 0.0f;
		}
		else {
			Rmy = powf(Rm, y);
			ry = powf(r, y);
			ryinv = (r > 0.0f)? powf(r, 2.0f*y - 2.0f): 0.0f;
		}

		const float Rmy5 = (Rmy*Rmy) * (Rmy*Rmy) * Rmy;
		const float f = Rmy - ry;
		const float num = f*(f*f)*(y*ryinv);

		return (10.0f * num) / (Rmy5 * M_PI_F);
	}
}

ccl_device float bssrdf_cubic_pdf(const ShaderClosure *sc, float r)
{
	return bssrdf_cubic_eval(sc, r);
}

/* solve 10x^2 - 20x^3 + 15x^4 - 4x^5 - xi == 0 */
ccl_device_forceinline float bssrdf_cubic_quintic_root_find(float xi)
{
	/* newton-raphson iteration, usually succeeds in 2-4 iterations, except
	 * outside 0.02 ... 0.98 where it can go up to 10, so overall performance
	 * should not be too bad */
	const float tolerance = 1e-6f;
	const int max_iteration_count = 10;
	float x = 0.25f;
	int i;

	for(i = 0; i < max_iteration_count; i++) {
		float x2 = x*x;
		float x3 = x2*x;
		float nx = (1.0f - x);

		float f = 10.0f*x2 - 20.0f*x3 + 15.0f*x2*x2 - 4.0f*x2*x3 - xi;
		float f_ = 20.0f*(x*nx)*(nx*nx);

		if(fabsf(f) < tolerance || f_ == 0.0f)
			break;

		x = saturate(x - f/f_);
	}

	return x;
}

ccl_device void bssrdf_cubic_sample(const ShaderClosure *sc, float xi, float *r, float *h)
{
	const Bssrdf *bssrdf = (const Bssrdf*)sc;
	const float sharpness = bssrdf->sharpness;
	float Rm = bssrdf->radius;
	float r_ = bssrdf_cubic_quintic_root_find(xi);

	if(sharpness != 0.0f) {
		r_ = powf(r_, 1.0f + sharpness);
		Rm *= (1.0f + sharpness);
	}

	r_ *= Rm;
	*r = r_;

	/* h^2 + r^2 = Rm^2 */
	*h = safe_sqrtf(Rm*Rm - r_*r_);
}

/* Approximate Reflectance Profiles
 * http://graphics.pixar.com/library/ApproxBSSRDF/paper.pdf
 */

/* This is a bit arbitrary, just need big enough radius so it matches
 * the mean free length, but still not too big so sampling is still
 * effective. Might need some further tweaks.
 */
#define BURLEY_TRUNCATE     16.0f
#define BURLEY_TRUNCATE_CDF 0.9963790093708328f // cdf(BURLEY_TRUNCATE)

ccl_device_inline float bssrdf_burley_fitting(float A)
{
	/* Diffuse surface transmission, equation (6). */
	return 1.9f - A + 3.5f * (A - 0.8f) * (A - 0.8f);
}

/* Scale mean free path length so it gives similar looking result
 * to Cubic and Gaussian models.
 */
ccl_device_inline float bssrdf_burley_compatible_mfp(float r)
{
	return 0.25f * M_1_PI_F * r;
}

ccl_device void bssrdf_burley_setup(Bssrdf *bssrdf)
{
	/* Mean free path length. */
	const float l = bssrdf_burley_compatible_mfp(bssrdf->radius);
	/* Surface albedo. */
	const float A = bssrdf->albedo;
	const float s = bssrdf_burley_fitting(A);
	const float d = l / s;

	bssrdf->d = d;
}

ccl_device float bssrdf_burley_eval(const ShaderClosure *sc, float r)
{
	const Bssrdf *bssrdf = (const Bssrdf*)sc;
	const float d = bssrdf->d;
	const float Rm = BURLEY_TRUNCATE * d;

	if(r >= Rm)
		return 0.0f;

	/* Burley refletance profile, equation (3).
	 *
	 * NOTES:
	 * - Surface albedo is already included into sc->weight, no need to
	 *   multiply by this term here.
	 * - This is normalized diffuse model, so the equation is mutliplied
	 *   by 2*pi, which also matches cdf().
	 */
	float exp_r_3_d = expf(-r / (3.0f * d));
	float exp_r_d = exp_r_3_d * exp_r_3_d * exp_r_3_d;
	return (exp_r_d + exp_r_3_d) / (4.0f*d);
}

ccl_device float bssrdf_burley_pdf(const ShaderClosure *sc, float r)
{
	return bssrdf_burley_eval(sc, r) * (1.0f/BURLEY_TRUNCATE_CDF);
}

/* Find the radius for desired CDF value.
 * Returns scaled radius, meaning the result is to be scaled up by d.
 * Since there's no closed form solution we do Newton-Raphson method to find it.
 */
ccl_device_forceinline float bssrdf_burley_root_find(float xi)
{
	const float tolerance = 1e-6f;
	const int max_iteration_count = 10;
	/* Do initial guess based on manual curve fitting, this allows us to reduce
	 * number of iterations to maximum 4 across the [0..1] range. We keep maximum
	 * number of iteration higher just to be sure we didn't miss root in some
	 * corner case.
	 */
	float r;
	if(xi <= 0.9f) {
		r = expf(xi * xi * 2.4f) - 1.0f;
	}
	else {
		/* TODO(sergey): Some nicer curve fit is possible here. */
		r = 15.0f;
	}
	/* Solve against scaled radius. */
	for(int i = 0; i < max_iteration_count; i++) {
		float exp_r_3 = expf(-r / 3.0f);
		float exp_r = exp_r_3 * exp_r_3 * exp_r_3;
		float f = 1.0f - 0.25f * exp_r - 0.75f * exp_r_3 - xi;
		float f_ = 0.25f * exp_r + 0.25f * exp_r_3;

		if(fabsf(f) < tolerance || f_ == 0.0f) {
			break;
		}

		r = r - f/f_;
		if(r < 0.0f) {
			r = 0.0f;
		}
	}
	return r;
}

ccl_device void bssrdf_burley_sample(const ShaderClosure *sc,
                                     float xi,
                                     float *r,
                                     float *h)
{
	const Bssrdf *bssrdf = (const Bssrdf*)sc;
	const float d = bssrdf->d;
	const float Rm = BURLEY_TRUNCATE * d;
	const float r_ = bssrdf_burley_root_find(xi * BURLEY_TRUNCATE_CDF) * d;

	*r = r_;

	/* h^2 + r^2 = Rm^2 */
	*h = safe_sqrtf(Rm*Rm - r_*r_);
}

/* None BSSRDF falloff
 *
 * Samples distributed over disk with no falloff, for reference. */

ccl_device float bssrdf_none_eval(const ShaderClosure *sc, float r)
{
	const Bssrdf *bssrdf = (const Bssrdf*)sc;
	const float Rm = bssrdf->radius;
	return (r < Rm)? 1.0f: 0.0f;
}

ccl_device float bssrdf_none_pdf(const ShaderClosure *sc, float r)
{
	/* integrate (2*pi*r)/(pi*Rm*Rm) from 0 to Rm = 1 */
	const Bssrdf *bssrdf = (const Bssrdf*)sc;
	const float Rm = bssrdf->radius;
	const float area = (M_PI_F*Rm*Rm);

	return bssrdf_none_eval(sc, r) / area;
}

ccl_device void bssrdf_none_sample(const ShaderClosure *sc, float xi, float *r, float *h)
{
	/* xi = integrate (2*pi*r)/(pi*Rm*Rm) = r^2/Rm^2
	 * r = sqrt(xi)*Rm */
	const Bssrdf *bssrdf = (const Bssrdf*)sc;
	const float Rm = bssrdf->radius;
	const float r_ = sqrtf(xi)*Rm;

	*r = r_;

	/* h^2 + r^2 = Rm^2 */
	*h = safe_sqrtf(Rm*Rm - r_*r_);
}

/* Generic */

ccl_device_inline Bssrdf *bssrdf_alloc(ShaderData *sd, float3 weight)
{
	Bssrdf *bssrdf = (Bssrdf*)closure_alloc(sd, sizeof(Bssrdf), CLOSURE_NONE_ID, weight);

	if(bssrdf == NULL) {
		return NULL;
	}

	float sample_weight = fabsf(average(weight));
	bssrdf->sample_weight = sample_weight;
	return (sample_weight >= CLOSURE_WEIGHT_CUTOFF) ? bssrdf : NULL;
}

ccl_device int bssrdf_setup(Bssrdf *bssrdf, ClosureType type)
{
	if(bssrdf->radius < BSSRDF_MIN_RADIUS) {
		/* revert to diffuse BSDF if radius too small */
		int flag;
#ifdef __PRINCIPLED__
		if(type == CLOSURE_BSSRDF_PRINCIPLED_ID) {
			float roughness = bssrdf->roughness;
			float3 N = bssrdf->N;
			float3 weight = bssrdf->weight;
			float sample_weight = bssrdf->sample_weight;

			PrincipledDiffuseBsdf *bsdf = (PrincipledDiffuseBsdf*)bssrdf;

			bsdf->N = N;
			bsdf->roughness = roughness;
			bsdf->weight = weight;
			bsdf->sample_weight = sample_weight;
			flag = bsdf_principled_diffuse_setup(bsdf);
			bsdf->type = CLOSURE_BSDF_BSSRDF_PRINCIPLED_ID;
		}
		else
#endif  /* __PRINCIPLED__ */
		{
			DiffuseBsdf *bsdf = (DiffuseBsdf*)bssrdf;
			bsdf->N = bssrdf->N;
			flag = bsdf_diffuse_setup(bsdf);
			bsdf->type = CLOSURE_BSDF_BSSRDF_ID;
		}
		
		return flag;
	}
	else {
		bssrdf->texture_blur = saturate(bssrdf->texture_blur);
		bssrdf->sharpness = saturate(bssrdf->sharpness);
		bssrdf->type = type;

		if(type == CLOSURE_BSSRDF_BURLEY_ID ||
		   type == CLOSURE_BSSRDF_PRINCIPLED_ID)
		{
			bssrdf_burley_setup(bssrdf);
		}

		return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSSRDF;
	}
}

ccl_device void bssrdf_sample(const ShaderClosure *sc, float xi, float *r, float *h)
{
	if(sc->type == CLOSURE_BSSRDF_CUBIC_ID)
		bssrdf_cubic_sample(sc, xi, r, h);
	else if(sc->type == CLOSURE_BSSRDF_GAUSSIAN_ID)
		bssrdf_gaussian_sample(sc, xi, r, h);
	else /*if(sc->type == CLOSURE_BSSRDF_BURLEY_ID || sc->type == CLOSURE_BSSRDF_PRINCIPLED_ID)*/
		bssrdf_burley_sample(sc, xi, r, h);
}

ccl_device_forceinline float bssrdf_pdf(const ShaderClosure *sc, float r)
{
	if(sc->type == CLOSURE_BSSRDF_CUBIC_ID)
		return bssrdf_cubic_pdf(sc, r);
	else if(sc->type == CLOSURE_BSSRDF_GAUSSIAN_ID)
		return bssrdf_gaussian_pdf(sc, r);
	else /*if(sc->type == CLOSURE_BSSRDF_BURLEY_ID || sc->type == CLOSURE_BSSRDF_PRINCIPLED_ID)*/
		return bssrdf_burley_pdf(sc, r);
}

CCL_NAMESPACE_END

#endif /* __KERNEL_BSSRDF_H__ */

