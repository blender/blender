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

#include "kernel_differential.h"
#include "kernel_montecarlo.h"
#include "kernel_triangle.h"
#include "kernel_object.h"
#ifdef __QBVH__
#include "kernel_qbvh.h"
#else
#include "kernel_bvh.h"
#endif
#include "kernel_camera.h"
#include "kernel_shader.h"
#include "kernel_light.h"
#include "kernel_emission.h"
#include "kernel_random.h"

CCL_NAMESPACE_BEGIN

#ifdef __MODIFY_TP__
__device float3 path_terminate_modified_throughput(KernelGlobals *kg, __global float3 *buffer, int x, int y, int pass)
{
	/* modify throughput to influence path termination probability, to avoid
	   darker regions receiving fewer samples than lighter regions. also RGB
	   are weighted differently. proper validation still remains to be done. */
	const float3 weights = make_float3(1.0f, 1.33f, 0.66f);
	const float3 one = make_float3(1.0f, 1.0f, 1.0f);
	const int minpass = 5;
	const float minL = 0.1f;

	if(pass >= minpass) {
		float3 L = buffer[x + y*kernel_data.cam.width];
		float3 Lmin = make_float3(minL, minL, minL);
		float correct = (float)(pass+1)/(float)pass;

		L = film_map(L*correct, pass);

		return weights/clamp(L, Lmin, one);
	}

	return weights;
}
#endif

__device float path_terminate_probability(KernelGlobals *kg, int bounce, const float3 throughput)
{
	if(bounce >= kernel_data.integrator.maxbounce)
		return 0.0f;
	else if(bounce <= kernel_data.integrator.minbounce)
		return 1.0f;

	return average(throughput);
}

__device int path_flag_from_label(int path_flag, int label)
{
	/* reflect/transmit */
	if(label & LABEL_REFLECT) {
		path_flag |= PATH_RAY_REFLECT;
		path_flag &= ~PATH_RAY_TRANSMIT;
	}
	else {
		kernel_assert(label & LABEL_TRANSMIT);

		path_flag |= PATH_RAY_TRANSMIT;
		path_flag &= ~PATH_RAY_REFLECT;
	}

	/* diffuse/glossy/singular */
	if(label & LABEL_DIFFUSE) {
		path_flag |= PATH_RAY_DIFFUSE;
		path_flag &= ~(PATH_RAY_GLOSSY|PATH_RAY_SINGULAR);
	}
	else if(label & LABEL_GLOSSY) {
		path_flag |= PATH_RAY_GLOSSY;
		path_flag &= ~(PATH_RAY_DIFFUSE|PATH_RAY_SINGULAR);
	}
	else {
		kernel_assert(label & (LABEL_SINGULAR|LABEL_STRAIGHT));

		path_flag |= PATH_RAY_SINGULAR;
		path_flag &= ~(PATH_RAY_DIFFUSE|PATH_RAY_GLOSSY);
	}
	
	/* ray through transparent is still camera ray */
	if(!(label & LABEL_STRAIGHT))
		path_flag &= ~PATH_RAY_CAMERA;
	
	return path_flag;
}

__device float3 kernel_path_integrate(KernelGlobals *kg, RNG *rng, int pass, Ray ray, float3 throughput)
{
	/* initialize */
	float3 L = make_float3(0.0f, 0.0f, 0.0f);

#ifdef __EMISSION__
	float ray_pdf = 0.0f;
#endif
	int path_flag = PATH_RAY_CAMERA|PATH_RAY_SINGULAR;
	int rng_offset = PRNG_BASE_NUM;

	/* path iteration */
	for(int bounce = 0; ; bounce++, rng_offset += PRNG_BOUNCE_NUM) {
		/* intersect scene */
		Intersection isect;

		if(!scene_intersect(kg, &ray, false, &isect)) {
			/* eval background shader if nothing hit */
#ifdef __BACKGROUND__
			ShaderData sd;
			shader_setup_from_background(kg, &sd, &ray);
			L += throughput*shader_eval_background(kg, &sd, path_flag);
			shader_release(kg, &sd);
#else
			L += make_float3(0.8f, 0.8f, 0.8f);
#endif
			break;
		}

		/* setup shading */
		ShaderData sd;
		shader_setup_from_ray(kg, &sd, &isect, &ray);
		float rbsdf = path_rng(kg, rng, pass, rng_offset + PRNG_BSDF);
		shader_eval_surface(kg, &sd, rbsdf, path_flag);

#ifdef __EMISSION__
		/* emission */
		if(kernel_data.integrator.use_emission) {
			if(sd.flag & SD_EMISSION)
				L += throughput*indirect_emission(kg, &sd, isect.t, path_flag, ray_pdf);

			/* sample illumination from lights to find path contribution */
			if((sd.flag & SD_BSDF_HAS_EVAL) &&
				bounce != kernel_data.integrator.maxbounce) {
				float light_t = path_rng(kg, rng, pass, rng_offset + PRNG_LIGHT);
				float light_o = path_rng(kg, rng, pass, rng_offset + PRNG_LIGHT_F);
				float light_u = path_rng(kg, rng, pass, rng_offset + PRNG_LIGHT_U);
				float light_v = path_rng(kg, rng, pass, rng_offset + PRNG_LIGHT_V);

				Ray light_ray;
				float3 light_L;

				if(direct_emission(kg, &sd, light_t, light_o, light_u, light_v, &light_ray, &light_L)) {
					/* trace shadow ray */
					if(!scene_intersect(kg, &light_ray, true, &isect))
						L += throughput*light_L;
				}
			}
		}
#endif

		/* sample BSDF */
		float bsdf_pdf;
		float3 bsdf_eval;
		float3 bsdf_omega_in;
		differential3 bsdf_domega_in;
		float bsdf_u = path_rng(kg, rng, pass, rng_offset + PRNG_BSDF_U);
		float bsdf_v = path_rng(kg, rng, pass, rng_offset + PRNG_BSDF_V);
		int label;

		label = shader_bsdf_sample(kg, &sd, bsdf_u, bsdf_v, &bsdf_eval,
			&bsdf_omega_in, &bsdf_domega_in, &bsdf_pdf);

		shader_release(kg, &sd);

		if(bsdf_pdf == 0.0f || is_zero(bsdf_eval))
			break;

		/* modify throughput */
		throughput *= bsdf_eval/bsdf_pdf;

		/* set labels */
#ifdef __EMISSION__
		ray_pdf = bsdf_pdf;
#endif

		path_flag = path_flag_from_label(path_flag, label);

		/* path termination */
		float probability = path_terminate_probability(kg, bounce, throughput);
		float terminate = path_rng(kg, rng, pass, rng_offset + PRNG_TERMINATE);

		if(terminate >= probability)
			break;

		throughput /= probability;

		/* setup ray */
		ray.P = ray_offset(sd.P, (label & LABEL_TRANSMIT)? -sd.Ng: sd.Ng);
		ray.D = bsdf_omega_in;
		ray.t = FLT_MAX;
#ifdef __RAY_DIFFERENTIALS__
		ray.dP = sd.dP;
		ray.dD = bsdf_domega_in;
#endif
	}

	return L;
}

__device void kernel_path_trace(KernelGlobals *kg, __global float4 *buffer, __global uint *rng_state, int pass, int x, int y)
{
	/* initialize random numbers */
	RNG rng;

	float filter_u;
	float filter_v;

	path_rng_init(kg, rng_state, pass, &rng, x, y, &filter_u, &filter_v);

	/* sample camera ray */
	Ray ray;

	float lens_u = path_rng(kg, &rng, pass, PRNG_LENS_U);
	float lens_v = path_rng(kg, &rng, pass, PRNG_LENS_V);

	camera_sample(kg, x, y, filter_u, filter_v, lens_u, lens_v, &ray);

	/* integrate */
#ifdef __MODIFY_TP__
	float3 throughput = path_terminate_modified_throughput(kg, buffer, x, y, pass);
	float3 L = kernel_path_integrate(kg, &rng, pass, ray, throughput)/throughput;
#else
	float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
	float3 L = kernel_path_integrate(kg, &rng, pass, ray, throughput);
#endif

	/* accumulate result in output buffer */
	int index = x + y*kernel_data.cam.width;

	float4 result;
	result.x = L.x;
	result.y = L.y;
	result.z = L.z;
	result.w = 1.0f;

	if(pass == 0)
		buffer[index] = result;
	else
		buffer[index] += result;

	path_rng_end(kg, rng_state, rng, x, y);
}

CCL_NAMESPACE_END

