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

__device void kernel_displace(KernelGlobals *kg, uint4 *input, float3 *offset, int i)
{
	/* setup shader data */
	ShaderData sd;
	uint4 in = input[i];
	shader_setup_from_displace(kg, &sd, in.x, in.y, __int_as_float(in.z), __int_as_float(in.w));

	/* evaluate */
	float3 P = sd.P;
	shader_eval_displacement(kg, &sd);
	offset[i] = sd.P - P;
}

CCL_NAMESPACE_END

