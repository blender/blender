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

/* Bump Node */

__device void svm_node_set_bump(KernelGlobals *kg, ShaderData *sd, float *stack, uint4 node)
{
#ifdef __RAY_DIFFERENTIALS__
	/* get normal input */
	float3 normal_in = stack_valid(node.y)? stack_load_float3(stack, node.y): sd->N;

	/* get surface tangents from normal */
	float3 Rx = cross(sd->dP.dy, normal_in);
	float3 Ry = cross(normal_in, sd->dP.dx);

	/* get bump values */
	uint c_offset, x_offset, y_offset, intensity_offset;
	decode_node_uchar4(node.z, &c_offset, &x_offset, &y_offset, &intensity_offset);

	float h_c = stack_load_float(stack, c_offset);
	float h_x = stack_load_float(stack, x_offset);
	float h_y = stack_load_float(stack, y_offset);

	/* compute surface gradient and determinant */
	float det = dot(sd->dP.dx, Rx);
	float3 surfgrad = (h_x - h_c)*Rx + (h_y - h_c)*Ry;
	float intensity = stack_load_float(stack, intensity_offset);

	surfgrad *= intensity;
	float absdet = fabsf(det);

	/* compute and output perturbed normal */
	float3 outN = normalize(absdet*normal_in - signf(det)*surfgrad);
	stack_store_float3(stack, node.w, outN);
#endif
}

/* Displacement Node */

__device void svm_node_set_displacement(ShaderData *sd, float *stack, uint fac_offset)
{
	float d = stack_load_float(stack, fac_offset);
	sd->P += sd->N*d*0.1f; /* todo: get rid of this factor */
}

CCL_NAMESPACE_END

