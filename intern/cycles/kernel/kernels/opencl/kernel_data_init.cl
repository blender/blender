/*
 * Copyright 2011-2015 Blender Foundation
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

#include "split/kernel_data_init.h"

__kernel void kernel_ocl_path_trace_data_init(
        ccl_global char *kg,
        ccl_global char *sd,
        ccl_global char *sd_DL_shadow,

        ccl_global float3 *P_sd,
        ccl_global float3 *P_sd_DL_shadow,

        ccl_global float3 *N_sd,
        ccl_global float3 *N_sd_DL_shadow,

        ccl_global float3 *Ng_sd,
        ccl_global float3 *Ng_sd_DL_shadow,

        ccl_global float3 *I_sd,
        ccl_global float3 *I_sd_DL_shadow,

        ccl_global int *shader_sd,
        ccl_global int *shader_sd_DL_shadow,

        ccl_global int *flag_sd,
        ccl_global int *flag_sd_DL_shadow,

        ccl_global int *prim_sd,
        ccl_global int *prim_sd_DL_shadow,

        ccl_global int *type_sd,
        ccl_global int *type_sd_DL_shadow,

        ccl_global float *u_sd,
        ccl_global float *u_sd_DL_shadow,

        ccl_global float *v_sd,
        ccl_global float *v_sd_DL_shadow,

        ccl_global int *object_sd,
        ccl_global int *object_sd_DL_shadow,

        ccl_global float *time_sd,
        ccl_global float *time_sd_DL_shadow,

        ccl_global float *ray_length_sd,
        ccl_global float *ray_length_sd_DL_shadow,

        /* Ray differentials. */
        ccl_global differential3 *dP_sd,
        ccl_global differential3 *dP_sd_DL_shadow,

        ccl_global differential3 *dI_sd,
        ccl_global differential3 *dI_sd_DL_shadow,

        ccl_global differential *du_sd,
        ccl_global differential *du_sd_DL_shadow,

        ccl_global differential *dv_sd,
        ccl_global differential *dv_sd_DL_shadow,

        /* Dp/Du */
        ccl_global float3 *dPdu_sd,
        ccl_global float3 *dPdu_sd_DL_shadow,

        ccl_global float3 *dPdv_sd,
        ccl_global float3 *dPdv_sd_DL_shadow,

        /* Object motion. */
        ccl_global Transform *ob_tfm_sd,
        ccl_global Transform *ob_tfm_sd_DL_shadow,

        ccl_global Transform *ob_itfm_sd,
        ccl_global Transform *ob_itfm_sd_DL_shadow,

        ShaderClosure *closure_sd,
        ShaderClosure *closure_sd_DL_shadow,

        ccl_global int *num_closure_sd,
        ccl_global int *num_closure_sd_DL_shadow,

        ccl_global float *randb_closure_sd,
        ccl_global float *randb_closure_sd_DL_shadow,

        ccl_global float3 *ray_P_sd,
        ccl_global float3 *ray_P_sd_DL_shadow,

        ccl_global differential3 *ray_dP_sd,
        ccl_global differential3 *ray_dP_sd_DL_shadow,

        ccl_constant KernelData *data,
        ccl_global float *per_sample_output_buffers,
        ccl_global uint *rng_state,
        ccl_global uint *rng_coop,                   /* rng array to store rng values for all rays */
        ccl_global float3 *throughput_coop,          /* throughput array to store throughput values for all rays */
        ccl_global float *L_transparent_coop,        /* L_transparent array to store L_transparent values for all rays */
        PathRadiance *PathRadiance_coop,             /* PathRadiance array to store PathRadiance values for all rays */
        ccl_global Ray *Ray_coop,                    /* Ray array to store Ray information for all rays */
        ccl_global PathState *PathState_coop,        /* PathState array to store PathState information for all rays */
        ccl_global char *ray_state,                  /* Stores information on current state of a ray */

#define KERNEL_TEX(type, ttype, name)                                   \
        ccl_global type *name,
#include "../../kernel_textures.h"

        int start_sample, int sx, int sy, int sw, int sh, int offset, int stride,
        int rng_state_offset_x,
        int rng_state_offset_y,
        int rng_state_stride,
        ccl_global int *Queue_data,                  /* Memory for queues */
        ccl_global int *Queue_index,                 /* Tracks the number of elements in queues */
        int queuesize,                               /* size (capacity) of the queue */
        ccl_global char *use_queues_flag,            /* flag to decide if scene-intersect kernel should use queues to fetch ray index */
        ccl_global unsigned int *work_array,         /* work array to store which work each ray belongs to */
#ifdef __WORK_STEALING__
        ccl_global unsigned int *work_pool_wgs,      /* Work pool for each work group */
        unsigned int num_samples,                    /* Total number of samples per pixel */
#endif
#ifdef __KERNEL_DEBUG__
        DebugData *debugdata_coop,
#endif
        int parallel_samples)                        /* Number of samples to be processed in parallel */
{
	kernel_data_init((KernelGlobals *)kg,
	                 (ShaderData *)sd,
	                 (ShaderData *)sd_DL_shadow,
	                 P_sd,
	                 P_sd_DL_shadow,
	                 N_sd,
	                 N_sd_DL_shadow,
	                 Ng_sd,
	                 Ng_sd_DL_shadow,
	                 I_sd,
	                 I_sd_DL_shadow,
	                 shader_sd,
	                 shader_sd_DL_shadow,
	                 flag_sd,
	                 flag_sd_DL_shadow,
	                 prim_sd,
	                 prim_sd_DL_shadow,
	                 type_sd,
	                 type_sd_DL_shadow,
	                 u_sd,
	                 u_sd_DL_shadow,
	                 v_sd,
	                 v_sd_DL_shadow,
	                 object_sd,
	                 object_sd_DL_shadow,
	                 time_sd,
	                 time_sd_DL_shadow,
	                 ray_length_sd,
	                 ray_length_sd_DL_shadow,

	                 /* Ray differentials. */
	                 dP_sd,
	                 dP_sd_DL_shadow,
	                 dI_sd,
	                 dI_sd_DL_shadow,
	                 du_sd,
	                 du_sd_DL_shadow,
	                 dv_sd,
	                 dv_sd_DL_shadow,

	                 /* Dp/Du */
	                 dPdu_sd,
	                 dPdu_sd_DL_shadow,
	                 dPdv_sd,
	                 dPdv_sd_DL_shadow,

	                 /* Object motion. */
	                 ob_tfm_sd,
	                 ob_tfm_sd_DL_shadow,
	                 ob_itfm_sd,
	                 ob_itfm_sd_DL_shadow,

	                 closure_sd,
	                 closure_sd_DL_shadow,
	                 num_closure_sd,
	                 num_closure_sd_DL_shadow,
	                 randb_closure_sd,
	                 randb_closure_sd_DL_shadow,
	                 ray_P_sd,
	                 ray_P_sd_DL_shadow,
	                 ray_dP_sd,
	                 ray_dP_sd_DL_shadow,
	                 data,
	                 per_sample_output_buffers,
	                 rng_state,
	                 rng_coop,
	                 throughput_coop,
	                 L_transparent_coop,
	                 PathRadiance_coop,
	                 Ray_coop,
	                 PathState_coop,
	                 ray_state,

#define KERNEL_TEX(type, ttype, name) name,
#include "../../kernel_textures.h"

	                 start_sample, sx, sy, sw, sh, offset, stride,
	                 rng_state_offset_x,
	                 rng_state_offset_y,
	                 rng_state_stride,
	                 Queue_data,
	                 Queue_index,
	                 queuesize,
	                 use_queues_flag,
	                 work_array,
#ifdef __WORK_STEALING__
	                 work_pool_wgs,
	                 num_samples,
#endif
#ifdef __KERNEL_DEBUG__
	                 debugdata_coop,
#endif
	                 parallel_samples);
}
