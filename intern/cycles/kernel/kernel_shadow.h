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

ccl_device_inline bool shadow_blocked(KernelGlobals *kg, PathState *state, Ray *ray, float3 *shadow)
{
	*shadow = make_float3(1.0f, 1.0f, 1.0f);

	if(ray->t == 0.0f)
		return false;
	
	Intersection isect;
#ifdef __HAIR__
	bool result = scene_intersect(kg, ray, PATH_RAY_SHADOW_OPAQUE, &isect, NULL, 0.0f, 0.0f);
#else
	bool result = scene_intersect(kg, ray, PATH_RAY_SHADOW_OPAQUE, &isect);
#endif

#ifdef __TRANSPARENT_SHADOWS__
	if(result && kernel_data.integrator.transparent_shadows) {
		/* transparent shadows work in such a way to try to minimize overhead
		 * in cases where we don't need them. after a regular shadow ray is
		 * cast we check if the hit primitive was potentially transparent, and
		 * only in that case start marching. this gives on extra ray cast for
		 * the cases were we do want transparency.
		 *
		 * also note that for this to work correct, multi close sampling must
		 * be used, since we don't pass a random number to shader_eval_surface */
		if(shader_transparent_shadow(kg, &isect)) {
			float3 throughput = make_float3(1.0f, 1.0f, 1.0f);
			float3 Pend = ray->P + ray->D*ray->t;
			int bounce = state->transparent_bounce;

			for(;;) {
				if(bounce >= kernel_data.integrator.transparent_max_bounce) {
					return true;
				}
				else if(bounce >= kernel_data.integrator.transparent_min_bounce) {
					/* todo: get random number somewhere for probabilistic terminate */
#if 0
					float probability = average(throughput);
					float terminate = 0.0f;

					if(terminate >= probability)
						return true;

					throughput /= probability;
#endif
				}

#ifdef __HAIR__
				if(!scene_intersect(kg, ray, PATH_RAY_SHADOW_TRANSPARENT, &isect, NULL, 0.0f, 0.0f)) {
#else
				if(!scene_intersect(kg, ray, PATH_RAY_SHADOW_TRANSPARENT, &isect)) {
#endif
					*shadow *= throughput;
					return false;
				}

				if(!shader_transparent_shadow(kg, &isect))
					return true;

				ShaderData sd;
				shader_setup_from_ray(kg, &sd, &isect, ray, state->bounce+1);
				shader_eval_surface(kg, &sd, 0.0f, PATH_RAY_SHADOW, SHADER_CONTEXT_SHADOW);

				throughput *= shader_bsdf_transparency(kg, &sd);

				ray->P = ray_offset(sd.P, -sd.Ng);
				if(ray->t != FLT_MAX)
					ray->D = normalize_len(Pend - ray->P, &ray->t);

				bounce++;
			}
		}
	}
#endif

	return result;
}

CCL_NAMESPACE_END

