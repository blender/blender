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

__device void svm_node_set_bump(ShaderData *sd, float *stack, uint c_offset, uint x_offset, uint y_offset)
{
#ifdef __RAY_DIFFERENTIALS__
	float h_c = stack_load_float(stack, c_offset);
	float h_x = stack_load_float(stack, x_offset);
	float h_y = stack_load_float(stack, y_offset);

	float3 Rx = cross(sd->dP.dy, sd->N);
	float3 Ry = cross(sd->N, sd->dP.dx);

	float det = dot(sd->dP.dx, Rx);
	float3 surfgrad = (h_x - h_c)*Rx + (h_y - h_c)*Ry;

	surfgrad *= 0.1f; /* todo: remove this factor */

	float absdet = fabsf(det);
	sd->N = normalize(absdet*sd->N - signf(det)*surfgrad);
#endif
}

/* Displacement Node */

__device void svm_node_set_displacement(ShaderData *sd, float *stack, uint fac_offset)
{
	float d = stack_load_float(stack, fac_offset);
	sd->P += sd->N*d*0.1f; /* todo: get rid of this factor */
}

CCL_NAMESPACE_END

