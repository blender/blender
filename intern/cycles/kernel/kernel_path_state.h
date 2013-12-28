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

typedef struct PathState {
	int flag;
	int bounce;

	int diffuse_bounce;
	int glossy_bounce;
	int transmission_bounce;
	int transparent_bounce;

#ifdef __VOLUME__
	int volume_shader;
#endif
} PathState;

ccl_device_inline void path_state_init(KernelGlobals *kg, PathState *state)
{
	state->flag = PATH_RAY_CAMERA|PATH_RAY_SINGULAR|PATH_RAY_MIS_SKIP;
	state->bounce = 0;
	state->diffuse_bounce = 0;
	state->glossy_bounce = 0;
	state->transmission_bounce = 0;
	state->transparent_bounce = 0;

#ifdef __VOLUME__
	/* todo: this assumes camera is always in air, need to detect when it isn't */
	state->volume_shader = kernel_data.background.volume_shader;
#endif
}

ccl_device_inline void path_state_next(KernelGlobals *kg, PathState *state, int label)
{
	/* ray through transparent keeps same flags from previous ray and is
	 * not counted as a regular bounce, transparent has separate max */
	if(label & LABEL_TRANSPARENT) {
		state->flag |= PATH_RAY_TRANSPARENT;
		state->transparent_bounce++;

		if(!kernel_data.integrator.transparent_shadows)
			state->flag |= PATH_RAY_MIS_SKIP;

		return;
	}

	state->bounce++;

	/* reflection/transmission */
	if(label & LABEL_REFLECT) {
		state->flag |= PATH_RAY_REFLECT;
		state->flag &= ~(PATH_RAY_TRANSMIT|PATH_RAY_CAMERA|PATH_RAY_TRANSPARENT);

		if(label & LABEL_DIFFUSE)
			state->diffuse_bounce++;
		else
			state->glossy_bounce++;
	}
	else {
		kernel_assert(label & LABEL_TRANSMIT);

		state->flag |= PATH_RAY_TRANSMIT;
		state->flag &= ~(PATH_RAY_REFLECT|PATH_RAY_CAMERA|PATH_RAY_TRANSPARENT);

		state->transmission_bounce++;
	}

	/* diffuse/glossy/singular */
	if(label & LABEL_DIFFUSE) {
		state->flag |= PATH_RAY_DIFFUSE|PATH_RAY_DIFFUSE_ANCESTOR;
		state->flag &= ~(PATH_RAY_GLOSSY|PATH_RAY_SINGULAR|PATH_RAY_MIS_SKIP);
	}
	else if(label & LABEL_GLOSSY) {
		state->flag |= PATH_RAY_GLOSSY|PATH_RAY_GLOSSY_ANCESTOR;
		state->flag &= ~(PATH_RAY_DIFFUSE|PATH_RAY_SINGULAR|PATH_RAY_MIS_SKIP);
	}
	else {
		kernel_assert(label & LABEL_SINGULAR);

		state->flag |= PATH_RAY_GLOSSY|PATH_RAY_SINGULAR|PATH_RAY_MIS_SKIP;
		state->flag &= ~PATH_RAY_DIFFUSE;
	}
}

ccl_device_inline uint path_state_ray_visibility(KernelGlobals *kg, PathState *state)
{
	uint flag = state->flag & PATH_RAY_ALL_VISIBILITY;

	/* for visibility, diffuse/glossy are for reflection only */
	if(flag & PATH_RAY_TRANSMIT)
		flag &= ~(PATH_RAY_DIFFUSE|PATH_RAY_GLOSSY);
	/* for camera visibility, use render layer flags */
	if(flag & PATH_RAY_CAMERA)
		flag |= kernel_data.integrator.layer_flag;

	return flag;
}

ccl_device_inline float path_state_terminate_probability(KernelGlobals *kg, PathState *state, const float3 throughput)
{
	if(state->flag & PATH_RAY_TRANSPARENT) {
		/* transparent rays treated separately */
		if(state->transparent_bounce >= kernel_data.integrator.transparent_max_bounce)
			return 0.0f;
		else if(state->transparent_bounce <= kernel_data.integrator.transparent_min_bounce)
			return 1.0f;
	}
	else {
		/* other rays */
		if((state->bounce >= kernel_data.integrator.max_bounce) ||
		   (state->diffuse_bounce >= kernel_data.integrator.max_diffuse_bounce) ||
		   (state->glossy_bounce >= kernel_data.integrator.max_glossy_bounce) ||
		   (state->transmission_bounce >= kernel_data.integrator.max_transmission_bounce))
		{
			return 0.0f;
		}
		else if(state->bounce <= kernel_data.integrator.min_bounce) {
			return 1.0f;
		}
	}

	/* probalistic termination */
	return average(throughput); /* todo: try using max here */
}

CCL_NAMESPACE_END

