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

#ifndef __BSDF_ASHIKHMIN_SHIRLEY_H__
#define __BSDF_ASHIKHMIN_SHIRLEY_H__

/*
ASHIKHMIN SHIRLEY BSDF

Implementation of
Michael Ashikhmin and Peter Shirley: "An Anisotropic Phong BRDF Model" (2000)

The Fresnel factor is missing to get a separable bsdf (intensity*color), as is
the case with all other microfacet-based BSDF implementations in Cycles.

Other than that, the implementation directly follows the paper.
*/

CCL_NAMESPACE_BEGIN

ccl_device int bsdf_ashikhmin_shirley_setup(ShaderClosure *sc)
{
	sc->data0 = clamp(sc->data0, 1e-4f, 1.0f);
	sc->data1 = sc->data0;

	sc->type = CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ID;
	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

ccl_device int bsdf_ashikhmin_shirley_aniso_setup(ShaderClosure *sc)
{
	sc->data0 = clamp(sc->data0, 1e-4f, 1.0f);
	sc->data1 = clamp(sc->data1, 1e-4f, 1.0f);

	sc->type = CLOSURE_BSDF_ASHIKHMIN_SHIRLEY_ANISO_ID;
	return SD_BSDF|SD_BSDF_HAS_EVAL|SD_BSDF_GLOSSY;
}

ccl_device void bsdf_ashikhmin_shirley_blur(ShaderClosure *sc, float roughness)
{
	sc->data0 = fmaxf(roughness, sc->data0); /* clamp roughness */
	sc->data1 = fmaxf(roughness, sc->data1);
}

ccl_device_inline float bsdf_ashikhmin_shirley_roughness_to_exponent(float roughness)
{
	return 2.0f / (roughness*roughness) - 2.0f;
}

ccl_device float3 bsdf_ashikhmin_shirley_eval_reflect(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	float3 N = sc->N;

	float NdotI = dot(N, I);           /* in Cycles/OSL convention I is omega_out    */
	float NdotO = dot(N, omega_in);    /* and consequently we use for O omaga_in ;)  */

	float out = 0.0f;

	if(NdotI > 0.0f && NdotO > 0.0f) {
		NdotI = fmaxf(NdotI, 1e-6f);
		NdotO = fmaxf(NdotO, 1e-6f);
		float3 H = normalize(omega_in + I);
		float HdotI = fmaxf(fabsf(dot(H, I)), 1e-6f);
		float HdotN = fmaxf(dot(H, N), 1e-6f);

		float pump = 1.0f / fmaxf(1e-6f, (HdotI*fmaxf(NdotO, NdotI))); /* pump from original paper (first derivative disc., but cancels the HdotI in the pdf nicely) */
		/*float pump = 1.0f / fmaxf(1e-4f, ((NdotO + NdotI) * (NdotO*NdotI))); */ /* pump from d-brdf paper */

		float n_x = bsdf_ashikhmin_shirley_roughness_to_exponent(sc->data0);
		float n_y = bsdf_ashikhmin_shirley_roughness_to_exponent(sc->data1);

		if(n_x == n_y) {
			/* isotropic */
			float e = n_x;
			float lobe = powf(HdotN, e);
			float norm = (n_x + 1.0f) / (8.0f * M_PI_F);

			out = NdotO * norm * lobe * pump;
			*pdf = norm * lobe / HdotI; /* this is p_h / 4(H.I)  (conversion from 'wh measure' to 'wi measure', eq. 8 in paper) */
		}
		else {
			/* anisotropic */
			float3 X, Y;
			make_orthonormals_tangent(N, sc->T, &X, &Y);

			float HdotX = dot(H, X);
			float HdotY = dot(H, Y);
			float e = (n_x * HdotX*HdotX + n_y * HdotY*HdotY) / (1.0f - HdotN*HdotN);
			float lobe = powf(HdotN, e);
			float norm = sqrtf((n_x + 1.0f)*(n_y + 1.0f)) / (8.0f * M_PI_F);
			
			out = NdotO * norm * lobe * pump;
			*pdf = norm * lobe / HdotI;
		}
	}

	return make_float3(out, out, out);
}

ccl_device float3 bsdf_ashikhmin_shirley_eval_transmit(const ShaderClosure *sc, const float3 I, const float3 omega_in, float *pdf)
{
	return make_float3(0.0f, 0.0f, 0.0f);
}

ccl_device_inline void bsdf_ashikhmin_shirley_sample_first_quadrant(float n_x, float n_y, float randu, float randv, float *phi, float *cos_theta)
{
	*phi = atanf(sqrtf((n_x + 1.0f) / (n_y + 1.0f)) * tanf(M_PI_2_F * randu));
	float cos_phi = cosf(*phi);
	float sin_phi = sinf(*phi);
	*cos_theta = powf(randv, 1.0f / (n_x * cos_phi*cos_phi + n_y * sin_phi*sin_phi + 1.0f));
}

ccl_device int bsdf_ashikhmin_shirley_sample(const ShaderClosure *sc, float3 Ng, float3 I, float3 dIdx, float3 dIdy, float randu, float randv, float3 *eval, float3 *omega_in, float3 *domega_in_dx, float3 *domega_in_dy, float *pdf)
{
	float3 N = sc->N;

	float NdotI = dot(N, I);
	if(NdotI > 0.0f) {

		float n_x = bsdf_ashikhmin_shirley_roughness_to_exponent(sc->data0);
		float n_y = bsdf_ashikhmin_shirley_roughness_to_exponent(sc->data1);

		/* get x,y basis on the surface for anisotropy */
		float3 X, Y;

		if(n_x == n_y)
			make_orthonormals(N, &X, &Y);
		else
			make_orthonormals_tangent(N, sc->T, &X, &Y);

		/* sample spherical coords for h in tangent space */
		float phi;
		float cos_theta;
		if(n_x == n_y) {
			/* isotropic sampling */
			phi = M_2PI_F * randu;
			cos_theta = powf(randv, 1.0f / (n_x + 1.0f));
		}
		else {
			/* anisotropic sampling */
			if(randu < 0.25f) {      /* first quadrant */
				float remapped_randu = 4.0f * randu;
				bsdf_ashikhmin_shirley_sample_first_quadrant(n_x, n_y, remapped_randu, randv, &phi, &cos_theta);
			}
			else if(randu < 0.5f) {  /* second quadrant */
				float remapped_randu = 4.0f * (.5f - randu);
				bsdf_ashikhmin_shirley_sample_first_quadrant(n_x, n_y, remapped_randu, randv, &phi, &cos_theta);
				phi = M_PI_F - phi;
			}
			else if(randu < 0.75f) { /* third quadrant */
				float remapped_randu = 4.0f * (randu - 0.5f);
				bsdf_ashikhmin_shirley_sample_first_quadrant(n_x, n_y, remapped_randu, randv, &phi, &cos_theta);
				phi = M_PI_F + phi;
			}
			else {                   /* fourth quadrant */
				float remapped_randu = 4.0f * (1.0f - randu);
				bsdf_ashikhmin_shirley_sample_first_quadrant(n_x, n_y, remapped_randu, randv, &phi, &cos_theta);
				phi = 2.0f * M_PI_F - phi;
			}
		}

		/* get half vector in tangent space */
		float sin_theta = sqrtf(fmaxf(0.0f, 1.0f - cos_theta*cos_theta));
		float cos_phi = cosf(phi);
		float sin_phi = sinf(phi); /* no sqrt(1-cos^2) here b/c it causes artifacts */
		float3 h = make_float3(
			sin_theta * cos_phi,
			sin_theta * sin_phi,
			cos_theta
			);

		/* half vector to world space */
		float3 H = h.x*X + h.y*Y + h.z*N;
		float HdotI = dot(H, I);
		if(HdotI < 0.0f) H = -H;

		/* reflect I on H to get omega_in */
		*omega_in = -I + (2.0f * HdotI) * H;

		/* leave the rest to eval_reflect */
		*eval = bsdf_ashikhmin_shirley_eval_reflect(sc, I, *omega_in, pdf);

#ifdef __RAY_DIFFERENTIALS__
		/* just do the reflection thing for now */
		*domega_in_dx = (2.0f * dot(N, dIdx)) * N - dIdx;
		*domega_in_dy = (2.0f * dot(N, dIdy)) * N - dIdy;
#endif
	}

	return LABEL_REFLECT|LABEL_GLOSSY;
}


CCL_NAMESPACE_END

#endif /* __BSDF_ASHIKHMIN_SHIRLEY_H__ */
