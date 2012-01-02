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

__device void kernel_shader_evaluate(KernelGlobals *kg, uint4 *input, float3 *output, ShaderEvalType type, int i)
{
	ShaderData sd;
	uint4 in = input[i];
	float3 out;

	if(type == SHADER_EVAL_DISPLACE) {
		/* setup shader data */
		int object = in.x;
		int prim = in.y;
		float u = __int_as_float(in.z);
		float v = __int_as_float(in.w);

		shader_setup_from_displace(kg, &sd, object, prim, u, v);

		/* evaluate */
		float3 P = sd.P;
		shader_eval_displacement(kg, &sd);
		out = sd.P - P;
	}
	else { // SHADER_EVAL_BACKGROUND
		/* setup ray */
		Ray ray;

		ray.P = make_float3(0.0f, 0.0f, 0.0f);
		ray.D = make_float3(__int_as_float(in.x), __int_as_float(in.y), __int_as_float(in.z));
		ray.t = 0.0f;

#ifdef __RAY_DIFFERENTIALS__
		ray.dD.dx = make_float3(0.0f, 0.0f, 0.0f);
		ray.dD.dy = make_float3(0.0f, 0.0f, 0.0f);
		ray.dP.dx = make_float3(0.0f, 0.0f, 0.0f);
		ray.dP.dy = make_float3(0.0f, 0.0f, 0.0f);
#endif

		/* setup shader data */
		shader_setup_from_background(kg, &sd, &ray);

		/* evaluate */
		int flag = 0; /* we can't know which type of BSDF this is for */
		out = shader_eval_background(kg, &sd, flag);
	}
	
	/* write output */
	output[i] = out;
}

CCL_NAMESPACE_END

