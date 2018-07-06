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

/* Motion Triangle Primitive
 *
 * These are stored as regular triangles, plus extra positions and normals at
 * times other than the frame center. Computing the triangle vertex positions
 * or normals at a given ray time is a matter of interpolation of the two steps
 * between which the ray time lies.
 *
 * The extra positions and normals are stored as ATTR_STD_MOTION_VERTEX_POSITION
 * and ATTR_STD_MOTION_VERTEX_NORMAL mesh attributes.
 */

CCL_NAMESPACE_BEGIN

/* Time interpolation of vertex positions and normals */

ccl_device_inline int find_attribute_motion(KernelGlobals *kg, int object, uint id, AttributeElement *elem)
{
	/* todo: find a better (faster) solution for this, maybe store offset per object */
	uint attr_offset = object_attribute_map_offset(kg, object);
	uint4 attr_map = kernel_tex_fetch(__attributes_map, attr_offset);

	while(attr_map.x != id) {
		attr_offset += ATTR_PRIM_TYPES;
		attr_map = kernel_tex_fetch(__attributes_map, attr_offset);
	}

	*elem = (AttributeElement)attr_map.y;

	/* return result */
	return (attr_map.y == ATTR_ELEMENT_NONE) ? (int)ATTR_STD_NOT_FOUND : (int)attr_map.z;
}

ccl_device_inline void motion_triangle_verts_for_step(KernelGlobals *kg, uint4 tri_vindex, int offset, int numverts, int numsteps, int step, float3 verts[3])
{
	if(step == numsteps) {
		/* center step: regular vertex location */
		verts[0] = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+0));
		verts[1] = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+1));
		verts[2] = float4_to_float3(kernel_tex_fetch(__prim_tri_verts, tri_vindex.w+2));
	}
	else {
		/* center step not store in this array */
		if(step > numsteps)
			step--;

		offset += step*numverts;

		verts[0] = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + tri_vindex.x));
		verts[1] = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + tri_vindex.y));
		verts[2] = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + tri_vindex.z));
	}
}

ccl_device_inline void motion_triangle_normals_for_step(KernelGlobals *kg, uint4 tri_vindex, int offset, int numverts, int numsteps, int step, float3 normals[3])
{
	if(step == numsteps) {
		/* center step: regular vertex location */
		normals[0] = float4_to_float3(kernel_tex_fetch(__tri_vnormal, tri_vindex.x));
		normals[1] = float4_to_float3(kernel_tex_fetch(__tri_vnormal, tri_vindex.y));
		normals[2] = float4_to_float3(kernel_tex_fetch(__tri_vnormal, tri_vindex.z));
	}
	else {
		/* center step is not stored in this array */
		if(step > numsteps)
			step--;

		offset += step*numverts;

		normals[0] = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + tri_vindex.x));
		normals[1] = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + tri_vindex.y));
		normals[2] = float4_to_float3(kernel_tex_fetch(__attributes_float3, offset + tri_vindex.z));
	}
}

ccl_device_inline void motion_triangle_vertices(KernelGlobals *kg, int object, int prim, float time, float3 verts[3])
{
	/* get motion info */
	int numsteps, numverts;
	object_motion_info(kg, object, &numsteps, &numverts, NULL);

	/* figure out which steps we need to fetch and their interpolation factor */
	int maxstep = numsteps*2;
	int step = min((int)(time*maxstep), maxstep-1);
	float t = time*maxstep - step;

	/* find attribute */
	AttributeElement elem;
	int offset = find_attribute_motion(kg, object, ATTR_STD_MOTION_VERTEX_POSITION, &elem);
	kernel_assert(offset != ATTR_STD_NOT_FOUND);

	/* fetch vertex coordinates */
	float3 next_verts[3];
	uint4 tri_vindex = kernel_tex_fetch(__tri_vindex, prim);

	motion_triangle_verts_for_step(kg, tri_vindex, offset, numverts, numsteps, step, verts);
	motion_triangle_verts_for_step(kg, tri_vindex, offset, numverts, numsteps, step+1, next_verts);

	/* interpolate between steps */
	verts[0] = (1.0f - t)*verts[0] + t*next_verts[0];
	verts[1] = (1.0f - t)*verts[1] + t*next_verts[1];
	verts[2] = (1.0f - t)*verts[2] + t*next_verts[2];
}

ccl_device_inline float3 motion_triangle_smooth_normal(KernelGlobals *kg, float3 Ng, int object, int prim, float u, float v, float time)
{
	/* get motion info */
	int numsteps, numverts;
	object_motion_info(kg, object, &numsteps, &numverts, NULL);

	/* figure out which steps we need to fetch and their interpolation factor */
	int maxstep = numsteps*2;
	int step = min((int)(time*maxstep), maxstep-1);
	float t = time*maxstep - step;

	/* find attribute */
	AttributeElement elem;
	int offset = find_attribute_motion(kg, object, ATTR_STD_MOTION_VERTEX_NORMAL, &elem);
	kernel_assert(offset != ATTR_STD_NOT_FOUND);

	/* fetch normals */
	float3 normals[3], next_normals[3];
	uint4 tri_vindex = kernel_tex_fetch(__tri_vindex, prim);

	motion_triangle_normals_for_step(kg, tri_vindex, offset, numverts, numsteps, step, normals);
	motion_triangle_normals_for_step(kg, tri_vindex, offset, numverts, numsteps, step+1, next_normals);

	/* interpolate between steps */
	normals[0] = (1.0f - t)*normals[0] + t*next_normals[0];
	normals[1] = (1.0f - t)*normals[1] + t*next_normals[1];
	normals[2] = (1.0f - t)*normals[2] + t*next_normals[2];

	/* interpolate between vertices */
	float w = 1.0f - u - v;
	float3 N = safe_normalize(u*normals[0] + v*normals[1] + w*normals[2]);

	return is_zero(N)? Ng: N;
}

CCL_NAMESPACE_END
