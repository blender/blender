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

__device void svm_node_fresnel(ShaderData *sd, float *stack, uint ior_offset, uint ior_value, uint out_offset)
{
	float eta = (stack_valid(ior_offset))? stack_load_float(stack, ior_offset): __int_as_float(ior_value);
	eta = fmaxf(eta, 1.0f + 1e-5f);
	eta = (sd->flag & SD_BACKFACING)? 1.0f/eta: eta;

	float f = fresnel_dielectric_cos(dot(sd->I, sd->N), eta);

	stack_store_float(stack, out_offset, f);
}

/* Blend Weight Node */

__device void svm_node_layer_weight(ShaderData *sd, float *stack, uint4 node)
{
	uint blend_offset = node.y;
	uint blend_value = node.z;
	float blend = (stack_valid(blend_offset))? stack_load_float(stack, blend_offset): __int_as_float(blend_value);

	uint type, out_offset;
	decode_node_uchar4(node.w, &type, &out_offset, NULL, NULL);

	float f;

	if(type == NODE_LAYER_WEIGHT_FRESNEL) {
		float eta = fmaxf(1.0f - blend, 1e-5f);
		eta = (sd->flag & SD_BACKFACING)? eta: 1.0f/eta;

		f = fresnel_dielectric_cos(dot(sd->I, sd->N), eta);
	}
	else {
		f = fabsf(dot(sd->I, sd->N));

		if(blend != 0.5f) {
			blend = clamp(blend, 0.0f, 1.0f-1e-5f);
			blend = (blend < 0.5f)? 2.0f*blend: 0.5f/(1.0f - blend);

			f = powf(f, blend);
		}

		f = 1.0f - f;
	}

	stack_store_float(stack, out_offset, f);
}

CCL_NAMESPACE_END

