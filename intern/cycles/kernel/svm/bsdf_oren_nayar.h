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

/*
 *	An implementation of Oren-Nayar reflectance model, public domain
 *		http://www1.cs.columbia.edu/CAVE/publications/pdfs/Oren_SIGGRAPH94.pdf
 *
 *	NOTE:
 *		BSDF = A + B * cos() * sin() * tan()
 *
 *		The parameter sigma means different from original.
 *		A and B are calculated by the following formula:
 *			0 <= sigma <= 1
 *			A =     1 / ((1 + sigma / 2) * pi);
 *			B = sigma / ((1 + sigma / 2) * pi);
 *
 *		This formula is derived as following:
 *
 *		0. Normalize A-term and B-term of BSDF *individually*.
 *		   B-term is normalized at maximum point: dot(L, N) = 0.
 *			A = (1/pi) * A'
 *			B = (2/pi) * B'
 *
 *		1. Solve the following equation:
 *			A' + B' = 1
 *			B / A = sigma
 */

#ifndef __BSDF_OREN_NAYAR_H__
#define __BSDF_OREN_NAYAR_H__

CCL_NAMESPACE_BEGIN

typedef struct BsdfOrenNayarClosure {
	float m_a;
	float m_b;
} BsdfOrenNayarClosure;

__device float3 bsdf_oren_nayar_get_intensity(const ShaderClosure *sc, float3 n, float3 v, float3 l)
{
	float nl = max(dot(n, l), 0.0f);
	float nv = max(dot(n, v), 0.0f);

	float3 al = normalize(l - nl * n);
	float3 av = normalize(v - nv * n);
	float t = max(dot(al, av), 0.0f);

	float cos_a, cos_b;
	if(nl < nv) {
		cos_a = nl;
		cos_b = nv;
	}
	else {
		cos_a = nv;
		cos_b = nl;
	}

	float sin_a = sqrtf(max(1.0f - cos_a * cos_a, 0.0f));
	float tan_b = sqrtf(max(1.0f - cos_b * cos_b, 0.0f)) / max(cos_b, 1e-8f);

	float is = nl * (sc->data0 + sc->data1 * t * sin_a * tan_b);
	return make_float3(is, is, is);
}

__device void bsdf_oren_nayar_setup(ShaderData *sd, ShaderClosure *sc, float sigma)
{
	sc->type = CLOSURE_BSDF_OREN_NAYAR_ID;
	sd->flag |= SD_BSDF | SD_BSDF_HAS_EVAL;

	sigma = clamp(sigma, 0.0f, 1.0f);

	float div = 1.0f / ((1.0f + 0.5f * sigma) * M_PI_F);

	sc->data0 =  1.0f * div;
	sc->data1 = sigma * div;
}

__device void bsdf_oren_nayar_blur(ShaderClosure *sc, float roughness)
{
}

__device float3 bsdf_oren_nayar_eval_reflect(const ShaderData *sd, const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	if (dot(sd->N, omega_in) > 0.0f) {
		*pdf = 0.5f * M_1_PI_F;
		return bsdf_oren_nayar_get_intensity(sc, sd->N, I, omega_in);
	}
	else {
		*pdf = 0.0f;
		return make_float3(0.0f, 0.0f, 0.0f);
	}
}

__device float3 bsdf_oren_nayar_eval_transmit(const ShaderData *sd, const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

__device float bsdf_oren_nayar_albedo(const ShaderData *sd, const ShaderClosure *sc, const float3 I)
{
	return 1.0f;
}

__device int bsdf_oren_nayar_sample(const ShaderData *sd, const ShaderClosure *sc, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	sample_uniform_hemisphere(sd->N, randu, randv, omega_in, pdf);

	if (dot(sd->Ng, *omega_in) > 0.0f) {
		*eval = bsdf_oren_nayar_get_intensity(sc, sd->N, sd->I, *omega_in);

#ifdef __RAY_DIFFERENTIALS__
		// TODO: find a better approximation for the bounce
		*domega_in_dx = (2.0f * dot(sd->N, sd->dI.dx)) * sd->N - sd->dI.dx;
		*domega_in_dy = (2.0f * dot(sd->N, sd->dI.dy)) * sd->N - sd->dI.dy;
		*domega_in_dx *= 125.0f;
		*domega_in_dy *= 125.0f;
#endif
	}
	else {
		*pdf = 0.0f;
		*eval = make_float3(0.0f, 0.0f, 0.0f);
	}

	return LABEL_REFLECT | LABEL_DIFFUSE;
}


CCL_NAMESPACE_END

#endif /* __BSDF_OREN_NAYAR_H__ */
