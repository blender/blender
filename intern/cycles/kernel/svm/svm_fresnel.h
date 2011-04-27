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

/* Fresnel Node */

__device void svm_node_fresnel(ShaderData *sd, float *stack, uint fresnel_offset, uint fresnel_value, uint out_offset)
{
	float fresnel = (stack_valid(fresnel_offset))? stack_load_float(stack, fresnel_offset): __int_as_float(fresnel_value);
	fresnel = fmaxf(1.0f - fresnel, 0.00001f);

	float f = fresnel_dielectric_cos(dot(sd->I, sd->N), (sd->flag & SD_BACKFACING)? fresnel: 1.0f/fresnel);

	stack_store_float(stack, out_offset, f);
}

CCL_NAMESPACE_END

