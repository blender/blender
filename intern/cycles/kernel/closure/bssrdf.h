/*
 * Copyright 2013, Blender Foundation.
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

#ifndef __KERNEL_BSSRDF_H__
#define __KERNEL_BSSRDF_H__

CCL_NAMESPACE_BEGIN

__device int bssrdf_setup(ShaderClosure *sc)
{
	if(sc->data0 < BSSRDF_MIN_RADIUS) {
		/* revert to diffuse BSDF if radius too small */
		sc->data0 = 0.0f;
		sc->data1 = 0.0f;
		return bsdf_diffuse_setup(sc);
	}
	else {
		/* radius + IOR params */
		sc->data0 = max(sc->data0, 0.0f);
		sc->data1 = max(sc->data1, 1.0f);
		sc->type = CLOSURE_BSSRDF_ID;

		return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSSRDF;
	}
}

/* Simple Cubic BSSRDF falloff */

__device float bssrdf_cubic(float ld, float r)
{
	if(ld == 0.0f)
		return (r == 0.0f)? 1.0f: 0.0f;

	return powf(ld - min(r, ld), 3.0f) * 4.0f/powf(ld, 4.0f);
}

/* Original BSSRDF fallof function */

typedef struct BSSRDFParams {
	float eta;		/* index of refraction */
	float sigma_t_; /* reduced extinction coefficient */
	float sigma_tr;	/* effective extinction coefficient */
	float Fdr;		/* diffuse fresnel reflectance */
	float D;		/* diffusion constant */
	float A;
	float alpha_;	/* reduced albedo */
	float zr;		/* distance of virtual lightsource above surface */
	float zv;		/* distance of virtual lightsource below surface */
	float ld;		/* mean free path */
	float ro;		/* diffuse reflectance */
} BSSRDFParams;

__device float bssrdf_reduced_albedo_Rd(float alpha_, float A, float ro)
{
	float sq;

	sq = sqrt(3.0f*(1.0f - alpha_));
	return (alpha_/2.0f)*(1.0f + expf((-4.0f/3.0f)*A*sq))*expf(-sq) - ro;
}

__device float bssrdf_compute_reduced_albedo(float A, float ro)
{
	const float tolerance = 1e-8;
	const int max_iteration_count = 20;
	float d, fsub, xn_1 = 0.0f, xn = 1.0f, fxn, fxn_1;
	int i;

	/* use secant method to compute reduced albedo using Rd function inverse
	 * with a given reflectance */
	fxn = bssrdf_reduced_albedo_Rd(xn, A, ro);
	fxn_1 = bssrdf_reduced_albedo_Rd(xn_1, A, ro);

	for (i= 0; i < max_iteration_count; i++) {
		fsub = (fxn - fxn_1);
		if (fabsf(fsub) < tolerance)
			break;
		d = ((xn - xn_1)/fsub)*fxn;
		if (fabsf(d) < tolerance)
			break;

		xn_1 = xn;
		fxn_1 = fxn;
		xn = xn - d;

		if (xn > 1.0f) xn = 1.0f;
		if (xn_1 > 1.0f) xn_1 = 1.0f;
		
		fxn = bssrdf_reduced_albedo_Rd(xn, A, ro);
	}

	/* avoid division by zero later */
	if (xn <= 0.0f)
		xn = 0.00001f;

	return xn;
}

__device void bssrdf_setup_params(BSSRDFParams *ss, float refl, float radius, float ior)
{
	ss->eta = ior;
	ss->Fdr = -1.440f/ior*ior + 0.710f/ior + 0.668f + 0.0636f*ior;
	ss->A = (1.0f + ss->Fdr)/(1.0f - ss->Fdr);
	ss->ld = radius;
	ss->ro = min(refl, 0.999f);

	ss->alpha_ = bssrdf_compute_reduced_albedo(ss->A, ss->ro);

	ss->sigma_tr = 1.0f/ss->ld;
	ss->sigma_t_ = ss->sigma_tr/sqrtf(3.0f*(1.0f - ss->alpha_));

	ss->D = 1.0f/(3.0f*ss->sigma_t_);

	ss->zr = 1.0f/ss->sigma_t_;
	ss->zv = ss->zr + 4.0f*ss->A*ss->D;
}

/* exponential falloff function */

__device float bssrdf_original(const BSSRDFParams *ss, float r)
{
	if(ss->ld == 0.0f)
		return (r == 0.0f)? 1.0f: 0.0f;

	float rr = r*r;
	float sr, sv, Rdr, Rdv;

	sr = sqrt(rr + ss->zr*ss->zr);
	sv = sqrt(rr + ss->zv*ss->zv);

	Rdr = ss->zr*(1.0f + ss->sigma_tr*sr)*expf(-ss->sigma_tr*sr)/(sr*sr*sr);
	Rdv = ss->zv*(1.0f + ss->sigma_tr*sv)*expf(-ss->sigma_tr*sv)/(sv*sv*sv);

	return ss->alpha_*(1.0f/(4.0f*(float)M_PI))*(Rdr + Rdv);
}

CCL_NAMESPACE_END

#endif /* __KERNEL_BSSRDF_H__ */

