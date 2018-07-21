/*
 * Copyright 2011-2016 Blender Foundation
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

/* Setup of motion triangle specific parts of ShaderData, moved into this one
 * function to more easily share computation of interpolated positions and
 * normals */

/* return 3 triangle vertex normals */
ccl_device_noinline void motion_triangle_shader_setup(KernelGlobals *kg,
                                                      ShaderData *sd, const
                                                      Intersection *isect,
                                                      const Ray *ray,
                                                      bool is_local)
{
	/* Get shader. */
	sd->shader = kernel_tex_fetch(__tri_shader, sd->prim);
	/* Get motion info. */
	/* TODO(sergey): This logic is really similar to motion_triangle_vertices(),
	 * can we de-duplicate something here?
	 */
	int numsteps, numverts;
	object_motion_info(kg, sd->object, &numsteps, &numverts, NULL);
	/* Figure out which steps we need to fetch and their interpolation factor. */
	int maxstep = numsteps*2;
	int step = min((int)(sd->time*maxstep), maxstep-1);
	float t = sd->time*maxstep - step;
	/* Find attribute. */
	AttributeElement elem;
	int offset = find_attribute_motion(kg, sd->object,
	                                   ATTR_STD_MOTION_VERTEX_POSITION,
	                                   &elem);
	kernel_assert(offset != ATTR_STD_NOT_FOUND);
	/* Fetch vertex coordinates. */
	float3 verts[3], next_verts[3];
	uint4 tri_vindex = kernel_tex_fetch(__tri_vindex, sd->prim);
	motion_triangle_verts_for_step(kg, tri_vindex, offset, numverts, numsteps, step, verts);
	motion_triangle_verts_for_step(kg, tri_vindex, offset, numverts, numsteps, step+1, next_verts);
	/* Interpolate between steps. */
	verts[0] = (1.0f - t)*verts[0] + t*next_verts[0];
	verts[1] = (1.0f - t)*verts[1] + t*next_verts[1];
	verts[2] = (1.0f - t)*verts[2] + t*next_verts[2];
	/* Compute refined position. */
#ifdef __BVH_LOCAL__
	if(is_local) {
		sd->P = motion_triangle_refine_local(kg,
		                                     sd,
		                                     isect,
		                                     ray,
		                                     verts);
	}
	else
#endif  /*  __BVH_LOCAL__*/
	{
		sd->P = motion_triangle_refine(kg, sd, isect, ray, verts);
	}
	/* Compute face normal. */
	float3 Ng;
	if(sd->object_flag & SD_OBJECT_NEGATIVE_SCALE_APPLIED) {
		Ng = normalize(cross(verts[2] - verts[0], verts[1] - verts[0]));
	}
	else {
		Ng = normalize(cross(verts[1] - verts[0], verts[2] - verts[0]));
	}
	sd->Ng = Ng;
	sd->N = Ng;
	/* Compute derivatives of P w.r.t. uv. */
#ifdef __DPDU__
	sd->dPdu = (verts[0] - verts[2]);
	sd->dPdv = (verts[1] - verts[2]);
#endif
	/* Compute smooth normal. */
	if(sd->shader & SHADER_SMOOTH_NORMAL) {
		/* Find attribute. */
		AttributeElement elem;
		int offset = find_attribute_motion(kg,
		                                   sd->object,
		                                   ATTR_STD_MOTION_VERTEX_NORMAL,
		                                   &elem);
		kernel_assert(offset != ATTR_STD_NOT_FOUND);
		/* Fetch vertex coordinates. */
		float3 normals[3], next_normals[3];
		motion_triangle_normals_for_step(kg, tri_vindex, offset, numverts, numsteps, step, normals);
		motion_triangle_normals_for_step(kg, tri_vindex, offset, numverts, numsteps, step+1, next_normals);
		/* Interpolate between steps. */
		normals[0] = (1.0f - t)*normals[0] + t*next_normals[0];
		normals[1] = (1.0f - t)*normals[1] + t*next_normals[1];
		normals[2] = (1.0f - t)*normals[2] + t*next_normals[2];
		/* Interpolate between vertices. */
		float u = sd->u;
		float v = sd->v;
		float w = 1.0f - u - v;
		sd->N = (u*normals[0] + v*normals[1] + w*normals[2]);
	}
}

CCL_NAMESPACE_END
