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

#define MBVH_OBJECT_SENTINEL 0x76543210
#define MBVH_NODE_SIZE 8
#define MBVH_STACK_SIZE 1024
#define MBVH_RAY_STACK_SIZE 10000

typedef struct MBVHTask {
	int node;
	int index;
	int num;
	int object;
} MBVHTask;

typedef struct MVBHRay {
	float3 P;
	float u;
	float3 idir;
	float v;
	float t;
	int index;
	int object;

	float3 origP;
	float3 origD;
	float tmax;
} MBVHRay;

__device float3 mbvh_inverse_direction(float3 dir)
{
	// Avoid divide by zero (ooeps = exp2f(-80.0f))
	float ooeps = 0.00000000000000000000000082718061255302767487140869206996285356581211090087890625f;
	float3 idir;

	idir.x = 1.0f / (fabsf(dir.x) > ooeps ? dir.x : copysignf(ooeps, dir.x));
	idir.y = 1.0f / (fabsf(dir.y) > ooeps ? dir.y : copysignf(ooeps, dir.y));
	idir.z = 1.0f / (fabsf(dir.z) > ooeps ? dir.z : copysignf(ooeps, dir.z));

	return idir;
}

__device void mbvh_instance_push(KernelGlobals *kg, int object, MBVHRay *ray)
{
	Transform tfm = object_fetch_transform(kg, object, OBJECT_INVERSE_TRANSFORM);

	ray->P = transform_point(&tfm, ray->origP);

	float3 dir = ray->origD;

	if(ray->t != ray->tmax) dir *= ray->t;

	dir = transform_direction(&tfm, dir);
	ray->idir = mbvh_inverse_direction(normalize(dir));

	if(ray->t != ray->tmax) ray->t = len(dir);
}

__device void mbvh_instance_pop(KernelGlobals *kg, int object, MBVHRay *ray)
{
	Transform tfm = object_fetch_transform(kg, object, OBJECT_TRANSFORM);

	if(ray->t != ray->tmax)
		ray->t = len(transform_direction(&tfm, (1.0f/(ray->idir)) * (ray->t)));

	ray->P = ray->origP;
	ray->idir = mbvh_inverse_direction(ray->origD);
}

/* Sven Woop's algorithm */
__device void mbvh_triangle_intersect(KernelGlobals *kg, MBVHRay *ray, int object, int triAddr)
{
	float3 P = ray->P;
	float3 idir = ray->idir;

	/* compute and check intersection t-value */
	float4 v00 = kernel_tex_fetch(__tri_woop, triAddr*MBVH_NODE_SIZE+0);
	float4 v11 = kernel_tex_fetch(__tri_woop, triAddr*MBVH_NODE_SIZE+1);
	float3 dir = 1.0f/idir;

	float Oz = v00.w - P.x*v00.x - P.y*v00.y - P.z*v00.z;
	float invDz = 1.0f/(dir.x*v00.x + dir.y*v00.y + dir.z*v00.z);
	float t = Oz * invDz;

	if(t > 0.0f && t < ray->t) {
		/* compute and check barycentric u */
		float Ox = v11.w + P.x*v11.x + P.y*v11.y + P.z*v11.z;
		float Dx = dir.x*v11.x + dir.y*v11.y + dir.z*v11.z;
		float u = Ox + t*Dx;

		if(u >= 0.0f) {
			/* compute and check barycentric v */
			float4 v22 = kernel_tex_fetch(__tri_woop, triAddr*MBVH_NODE_SIZE+2);
			float Oy = v22.w + P.x*v22.x + P.y*v22.y + P.z*v22.z;
			float Dy = dir.x*v22.x + dir.y*v22.y + dir.z*v22.z;
			float v = Oy + t*Dy;

			if(v >= 0.0f && u + v <= 1.0f) {
				/* record intersection */
				ray->index = triAddr;
				ray->object = object;
				ray->u = u;
				ray->v = v;
				ray->t = t;
			}
		}
	}
}

__device void mbvh_node_intersect(KernelGlobals *kg, __m128 *traverseChild,
	__m128 *tHit, float3 P, float3 idir, float t, int nodeAddr)
{
	/* X axis */
	const __m128 bminx = kernel_tex_fetch_m128(__bvh_nodes, nodeAddr*MBVH_NODE_SIZE+0);
	const __m128 t0x = _mm_mul_ps(_mm_sub_ps(bminx, _mm_set_ps1(P.x)), _mm_set_ps1(idir.x));
	const __m128 bmaxx = kernel_tex_fetch_m128(__bvh_nodes, nodeAddr*MBVH_NODE_SIZE+1);
	const __m128 t1x = _mm_mul_ps(_mm_sub_ps(bmaxx, _mm_set_ps1(P.x)), _mm_set_ps1(idir.x));

	__m128 tmin = _mm_max_ps(_mm_min_ps(t0x, t1x), _mm_setzero_ps());
	__m128 tmax = _mm_min_ps(_mm_max_ps(t0x, t1x), _mm_set_ps1(t));

	/* Y axis */
	const __m128 bminy = kernel_tex_fetch_m128(__bvh_nodes, nodeAddr*MBVH_NODE_SIZE+2);
	const __m128 t0y = _mm_mul_ps(_mm_sub_ps(bminy, _mm_set_ps1(P.y)), _mm_set_ps1(idir.y));
	const __m128 bmaxy = kernel_tex_fetch_m128(__bvh_nodes, nodeAddr*MBVH_NODE_SIZE+3);
	const __m128 t1y = _mm_mul_ps(_mm_sub_ps(bmaxy, _mm_set_ps1(P.y)), _mm_set_ps1(idir.y));

	tmin = _mm_max_ps(_mm_min_ps(t0y, t1y), tmin);
	tmax = _mm_min_ps(_mm_max_ps(t0y, t1y), tmax);

	/* Z axis */
	const __m128 bminz = kernel_tex_fetch_m128(__bvh_nodes, nodeAddr*MBVH_NODE_SIZE+4);
	const __m128 t0z = _mm_mul_ps(_mm_sub_ps(bminz, _mm_set_ps1(P.z)), _mm_set_ps1(idir.z));
	const __m128 bmaxz = kernel_tex_fetch_m128(__bvh_nodes, nodeAddr*MBVH_NODE_SIZE+5);
	const __m128 t1z = _mm_mul_ps(_mm_sub_ps(bmaxz, _mm_set_ps1(P.z)), _mm_set_ps1(idir.z));

	tmin = _mm_max_ps(_mm_min_ps(t0z, t1z), tmin);
	tmax = _mm_min_ps(_mm_max_ps(t0z, t1z), tmax);

	/* compare and get mask */
	*traverseChild = _mm_cmple_ps(tmin, tmax);

	/* get distance XXX probably wrong */
	*tHit = tmin;
}

static void mbvh_sort_by_length(int id[4], float len[4])
{
	for(int i = 1; i < 4; i++) {
		int j = i - 1;

		while(j >= 0 && len[j] > len[j+1]) {
			swap(len[j], len[j+1]);
			swap(id[j], id[j+1]);
			j--;
		}
	}
}

__device void scene_intersect(KernelGlobals *kg, MBVHRay *rays, int numrays)
{
	/* traversal stacks */
	MBVHTask task_stack[MBVH_STACK_SIZE];
	int active_ray_stacks[4][MBVH_RAY_STACK_SIZE];
	int num_task, num_active[4] = {0, 0, 0, 0};
	__m128i one_mm = _mm_set1_epi32(1);

	/* push root node task on stack */
	task_stack[0].node = kernel_data.bvh.root;
	task_stack[0].index = 0;
	task_stack[0].num = numrays;
	task_stack[0].object = ~0;
	num_task = 1;

	/* push all rays in first SIMD lane */
	for(int i = 0; i < numrays; i++)
		active_ray_stacks[0][i] = i;
	num_active[0] = numrays;
	
	while(num_task >= 1) {
		/* pop task */
		MBVHTask task = task_stack[--num_task];

		if(task.node == MBVH_OBJECT_SENTINEL) {
			/* instance pop */

			/* pop rays from stack */
			num_active[task.index] -= task.num;
			int ray_offset = num_active[task.index];

			/* transform rays */
			for(int i = 0; i < task.num; i++) {
				MBVHRay *ray = &rays[active_ray_stacks[task.index][ray_offset + i]];
				mbvh_instance_pop(kg, task.object, ray);
			}
		}
		else if(task.node >= 0) {
			/* inner node? */

			/* pop rays from stack*/
			num_active[task.index] -= task.num;
			int ray_offset = num_active[task.index];

			/* initialze simd values */
			__m128i num_active_mm = _mm_load_si128((__m128i*)num_active);
			__m128 len_mm = _mm_set_ps1(0.0f);

			for(int i = 0; i < task.num; i++) {
				int rayid = active_ray_stacks[task.index][ray_offset + i];
				MVBHRay *ray = rays + rayid;

				/* intersect 4 QBVH node children */
				__m128 result;
				__m128 thit;

				mbvh_node_intersect(kg, &result, &thit, ray->P, ray->idir, ray->t, task.node);

				/* update length for sorting */
				len_mm = _mm_add_ps(len_mm, _mm_and_ps(thit, result));

				/* push rays on stack */
				for(int j = 0; j < 4; j++)
					active_ray_stacks[j][num_active[j]] = rayid;

				/* update num active */
				__m128i resulti = _mm_and_si128(*((__m128i*)&result), one_mm);
				num_active_mm = _mm_add_epi32(resulti, num_active_mm);
				_mm_store_si128((__m128i*)num_active, num_active_mm);
			}

			if(num_active[0] || num_active[1] || num_active[2] || num_active[3]) {
				/* load child node addresses */
				float4 cnodes = kernel_tex_fetch(__bvh_nodes, task.node);
				int child[4] = {
					__float_as_int(cnodes.x),
					__float_as_int(cnodes.y),
					__float_as_int(cnodes.z),
					__float_as_int(cnodes.w)};

				/* sort nodes by average intersection distance */
				int ids[4] = {0, 1, 2, 3};
				float len[4];

				_mm_store_ps(len, len_mm);
				mbvh_sort_by_length(ids, len);

				/* push new tasks on stack */
				for(int j = 0; j < 4; j++) {
					if(num_active[j]) {
						int id = ids[j];

						task_stack[num_task].node = child[id];
						task_stack[num_task].index = id;
						task_stack[num_task].num = num_active[id];
						task_stack[num_task].object = task.object;
						num_task++;
					}
				}
			}
		}
		else {
			/* fetch leaf node data */
			float4 leaf = kernel_tex_fetch(__bvh_nodes, (-task.node-1)*MBVH_NODE_SIZE+(MBVH_NODE_SIZE-2));
			int triAddr = __float_as_int(leaf.x);
			int triAddr2 = __float_as_int(leaf.y);

			/* pop rays from stack*/
			num_active[task.index] -= task.num;
			int ray_offset = num_active[task.index];

			/* triangles */
			if(triAddr >= 0) {
				int i, numq = (task.num >> 2) << 2;

				/* SIMD ray leaf intersection */
				for(i = 0; i < numq; i += 4) {
					MBVHRay *ray4[4] = {
						&rays[active_ray_stacks[task.index][ray_offset + i + 0]],
						&rays[active_ray_stacks[task.index][ray_offset + i + 1]],
						&rays[active_ray_stacks[task.index][ray_offset + i + 2]],
						&rays[active_ray_stacks[task.index][ray_offset + i + 3]]};

					/* load SoA */

					while(triAddr < triAddr2) {
						mbvh_triangle_intersect(ray4[0], task.object, task.node);
						mbvh_triangle_intersect(ray4[1], task.object, task.node);
						mbvh_triangle_intersect(ray4[2], task.object, task.node);
						mbvh_triangle_intersect(ray4[3], task.object, task.node);
						triAddr++;

						/* some shadow ray optim could be done by setting t=0 */
					}

					/* store AoS */
				}

				/* mono ray leaf intersection */
				for(; i < task.num; i++) {
					MBVHRay *ray = &rays[active_ray_stacks[task.index][ray_offset + i]];

					while(triAddr < triAddr2) {
						mbvh_triangle_intersect(kg, ray, task.object, task.node);
						triAddr++;
					}
				}
			}
			else {
				/* instance push */
				int object = -triAddr-1;
				int node = triAddr;

				/* push instance pop task */
				task_stack[num_task].node = MBVH_OBJECT_SENTINEL;
				task_stack[num_task].index = task.index;
				task_stack[num_task].num = task.num;
				task_stack[num_task].object = object;
				num_task++;

				num_active[task.index] += task.num;

				/* push node task */
				task_stack[num_task].node = node;
				task_stack[num_task].index = task.index;
				task_stack[num_task].num = task.num;
				task_stack[num_task].object = object;
				num_task++;

				for(int i = 0; i < task.num; i++) {
					int rayid = active_ray_stacks[task.index][ray_offset + i];

					/* push on stack for last task */
					active_ray_stacks[task.index][num_active[task.index]] = rayid;
					num_active[task.index]++;

					/* transform ray */
					MBVHRay *ray = &rays[rayid];
					mbvh_instance_push(kg, object, ray);
				}
			}
		}
	}
}

__device void mbvh_set_ray(MBVHRay *rays, int i, Ray *ray, float tmax)
{
	MBVHRay *mray = &rays[i];

	/* ray parameters in registers */
	mray->P = ray->P;
	mray->idir = mbvh_inverse_direction(ray->D);
	mray->t = tmax;
}

__device bool mbvh_get_intersection(MVBHRay *rays, int i, Intersection *isect, float tmax)
{
	MBVHRay *mray = &rays[i];

	if(mray->t == tmax)
		return false;
	
	isect->t = mray->t;
	isect->u = mray->u;
	isect->v = mray->v;
	isect->index = mray->index;
	isect->object = mray->object;

	return true;
}

__device bool mbvh_get_shadow(MBVHRay *rays, int i, float tmax)
{
	return (rays[i].t == tmax);
}

CCL_NAMESPACE_END

