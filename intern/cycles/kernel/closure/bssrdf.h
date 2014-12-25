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

ccl_device int bssrdf_setup(ShaderClosure *sc, ClosureType type)
{
	if(sc->data0 < BSSRDF_MIN_RADIUS) {
		/* revert to diffuse BSDF if radius too small */
		sc->data0 = 0.0f;
		sc->data1 = 0.0f;
		int flag = bsdf_diffuse_setup(sc);
		sc->type = CLOSURE_BSDF_BSSRDF_ID;
		return flag;
	}
	else {
		sc->data1 = clamp(sc->data1, 0.0f, 1.0f); /* texture blur */
		sc->T.x = clamp(sc->T.x, 0.0f, 1.0f); /* sharpness */
		sc->type = type;

		return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSSRDF;
	}
}

/* Planar Truncated Gaussian
 *
 * Note how this is different from the typical gaussian, this one integrates
 * to 1 over the plane (where you get an extra 2*pi*x factor). We are lucky
 * that integrating x*exp(-x) gives a nice closed form solution. */

/* paper suggests 1/12.46 which is much too small, suspect it's *12.46 */
#define GAUSS_TRUNCATE 12.46f

ccl_device float bssrdf_gaussian_eval(ShaderClosure *sc, float r)
{
	/* integrate (2*pi*r * exp(-r*r/(2*v)))/(2*pi*v)) from 0 to Rm
	 * = 1 - exp(-Rm*Rm/(2*v)) */
	const float v = sc->data0*sc->data0*(0.25f*0.25f);
	const float Rm = sqrtf(v*GAUSS_TRUNCATE);

	if(r >= Rm)
		return 0.0f;

	return expf(-r*r/(2.0f*v))/(2.0f*M_PI_F*v);
}

ccl_device float bssrdf_gaussian_pdf(ShaderClosure *sc, float r)
{
	/* 1.0 - expf(-Rm*Rm/(2*v)) simplified */
	const float area_truncated = 1.0f - expf(-0.5f*GAUSS_TRUNCATE);

	return bssrdf_gaussian_eval(sc, r) * (1.0f/(area_truncated));
}

ccl_device void bssrdf_gaussian_sample(ShaderClosure *sc, float xi, float *r, float *h)
{
	/* xi = integrate (2*pi*r * exp(-r*r/(2*v)))/(2*pi*v)) = -exp(-r^2/(2*v))
	 * r = sqrt(-2*v*logf(xi)) */

	const float v = sc->data0*sc->data0*(0.25f*0.25f);
	const float Rm = sqrtf(v*GAUSS_TRUNCATE);

	/* 1.0 - expf(-Rm*Rm/(2*v)) simplified */
	const float area_truncated = 1.0f - expf(-0.5f*GAUSS_TRUNCATE);

	/* r(xi) */
	const float r_squared = -2.0f*v*logf(1.0f - xi*area_truncated);
	*r = sqrtf(r_squared);

	 /* h^2 + r^2 = Rm^2 */
	 *h = sqrtf(Rm*Rm - r_squared);
}

/* Planar Cubic BSSRDF falloff
 *
 * This is basically (Rm - x)^3, with some factors to normalize it. For sampling
 * we integrate 2*pi*x * (Rm - x)^3, which gives us a quintic equation that as
 * far as I can tell has no closed form solution. So we get an iterative solution
 * instead with newton-raphson. */

ccl_device float bssrdf_cubic_eval(ShaderClosure *sc, float r)
{
	const float sharpness = sc->T.x;

	if(sharpness == 0.0f) {
		const float Rm = sc->data0;

		if(r >= Rm)
			return 0.0f;

		/* integrate (2*pi*r * 10*(R - r)^3)/(pi * R^5) from 0 to R = 1 */
		const float Rm5 = (Rm*Rm) * (Rm*Rm) * Rm;
		const float f = Rm - r;
		const float num = f*f*f;

		return (10.0f * num) / (Rm5 * M_PI_F);

	}
	else {
		float Rm = sc->data0*(1.0f + sharpness);

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

ccl_device float bssrdf_cubic_pdf(ShaderClosure *sc, float r)
{
	return bssrdf_cubic_eval(sc, r);
}

/* solve 10x^2 - 20x^3 + 15x^4 - 4x^5 - xi == 0 */
ccl_device float bssrdf_cubic_quintic_root_find(float xi)
{
	/* newton-raphson iteration, usually succeeds in 2-4 iterations, except
	 * outside 0.02 ... 0.98 where it can go up to 10, so overall performance
	 * should not be too bad */
	const float tolerance = 1e-6f;
	const int max_iteration_count = 10;
	float x = 0.25f;
	int i;

	for (i = 0; i < max_iteration_count; i++) {
		float x2 = x*x;
		float x3 = x2*x;
		float nx = (1.0f - x);

		float f = 10.0f*x2 - 20.0f*x3 + 15.0f*x2*x2 - 4.0f*x2*x3 - xi;
		float f_ = 20.0f*(x*nx)*(nx*nx);

		if(fabsf(f) < tolerance || f_ == 0.0f)
			break;

		x = clamp(x - f/f_, 0.0f, 1.0f);
	}

	return x;
}

ccl_device void bssrdf_cubic_sample(ShaderClosure *sc, float xi, float *r, float *h)
{
	float Rm = sc->data0;
	float r_ = bssrdf_cubic_quintic_root_find(xi);

	const float sharpness = sc->T.x;
	if(sharpness != 0.0f) {
		r_ = powf(r_, 1.0f + sharpness);
		Rm *= (1.0f + sharpness);
	}
	
	r_ *= Rm;
	*r = r_;

	/* h^2 + r^2 = Rm^2 */
	*h = sqrtf(Rm*Rm - r_*r_);
}

/* None BSSRDF falloff
 * 
 * Samples distributed over disk with no falloff, for reference. */

ccl_device float bssrdf_none_eval(ShaderClosure *sc, float r)
{
	const float Rm = sc->data0;
	return (r < Rm)? 1.0f: 0.0f;
}

ccl_device float bssrdf_none_pdf(ShaderClosure *sc, float r)
{
	/* integrate (2*pi*r)/(pi*Rm*Rm) from 0 to Rm = 1 */
	const float Rm = sc->data0;
	const float area = (M_PI_F*Rm*Rm);

	return bssrdf_none_eval(sc, r) / area;
}

ccl_device void bssrdf_none_sample(ShaderClosure *sc, float xi, float *r, float *h)
{
	/* xi = integrate (2*pi*r)/(pi*Rm*Rm) = r^2/Rm^2
	 * r = sqrt(xi)*Rm */
	const float Rm = sc->data0;
	const float r_ = sqrtf(xi)*Rm;

	*r = r_;

	/* h^2 + r^2 = Rm^2 */
	*h = sqrtf(Rm*Rm - r_*r_);
}

/* Generic */

ccl_device void bssrdf_sample(ShaderClosure *sc, float xi, float *r, float *h)
{
	if(sc->type == CLOSURE_BSSRDF_CUBIC_ID)
		bssrdf_cubic_sample(sc, xi, r, h);
	else
		bssrdf_gaussian_sample(sc, xi, r, h);
}

ccl_device float bssrdf_pdf(ShaderClosure *sc, float r)
{
	if(sc->type == CLOSURE_BSSRDF_CUBIC_ID)
		return bssrdf_cubic_pdf(sc, r);
	else
		return bssrdf_gaussian_pdf(sc, r);
}

CCL_NAMESPACE_END

#endif /* __KERNEL_BSSRDF_H__ */

