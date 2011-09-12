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

typedef struct LightSample {
	float3 P;
	float3 Ng;
	int object;
	int prim;
	int shader;
	float weight;
} LightSample;

/* Point Light */

__device void point_light_sample(KernelGlobals *kg, int point,
	float randu, float randv, float3 P, LightSample *ls)
{
	float4 f = kernel_tex_fetch(__light_point, point);

	ls->P = make_float3(f.x, f.y, f.z);
	ls->Ng = normalize(ls->P - P);
	ls->shader = __float_as_int(f.w);
	ls->object = ~0;
	ls->prim = ~0;
}

__device float point_light_pdf(KernelGlobals *kg, float t)
{
	return t*t*kernel_data.integrator.pdf_lights;
}

/* Triangle Light */

__device void triangle_light_sample(KernelGlobals *kg, int prim, int object,
	float randu, float randv, LightSample *ls)
{
	/* triangle, so get position, normal, shader */
	ls->P = triangle_sample_MT(kg, prim, randu, randv);
	ls->Ng = triangle_normal_MT(kg, prim, &ls->shader);
	ls->object = object;
	ls->prim = prim;

#ifdef __INSTANCING__
	/* instance transform */
	if(ls->object >= 0) {
		object_position_transform(kg, ls->object, &ls->P);
		object_normal_transform(kg, ls->object, &ls->Ng);
	}
#endif
}

__device float triangle_light_pdf(KernelGlobals *kg,
	const float3 Ng, const float3 I, float t)
{
	float cos_pi = fabsf(dot(Ng, I));

	if(cos_pi == 0.0f)
		return 0.0f;
	
	return (t*t*kernel_data.integrator.pdf_triangles)/cos_pi;
}

/* Light Distribution */

__device int light_distribution_sample(KernelGlobals *kg, float randt)
{
	/* this is basically std::upper_bound as used by pbrt, to find a point light or
	   triangle to emit from, proportional to area. a good improvement would be to
	   also sample proportional to power, though it's not so well defined with
	   OSL shaders. */
	int first = 0;
	int len = kernel_data.integrator.num_distribution + 1;

	while(len > 0) {
		int half_len = len >> 1;
		int middle = first + half_len;

		if(randt < kernel_tex_fetch(__light_distribution, middle).x) {
			len = half_len;
		}
		else {
			first = middle + 1;
			len = len - half_len - 1;
		}
	}

	first = max(0, first-1);
	kernel_assert(first >= 0 && first < kernel_data.integrator.num_distribution);

	return first;
}

/* Generic Light */

__device void light_sample(KernelGlobals *kg, float randt, float randu, float randv, float3 P, LightSample *ls)
{
	/* sample index */
	int index = light_distribution_sample(kg, randt);

	/* fetch light data */
	float4 l = kernel_tex_fetch(__light_distribution, index);
	int prim = __float_as_int(l.y);
	ls->weight = l.z;

	if(prim >= 0) {
		int object = __float_as_int(l.w);
		triangle_light_sample(kg, prim, object, randu, randv, ls);
	}
	else {
		int point = -prim-1;
		point_light_sample(kg, point, randu, randv, P, ls);
	}
}

__device float light_sample_pdf(KernelGlobals *kg, LightSample *ls, float3 I, float t)
{
	float pdf;

	if(ls->prim != ~0)
		pdf = triangle_light_pdf(kg, ls->Ng, I, t);
	else
		pdf = point_light_pdf(kg, t);
	
	return pdf;
}

__device void light_select(KernelGlobals *kg, int index, float randu, float randv, float3 P, LightSample *ls)
{
	point_light_sample(kg, index, randu, randv, P, ls);
}

__device float light_select_pdf(KernelGlobals *kg, LightSample *ls, float3 I, float t)
{
	return point_light_pdf(kg, t);
}

CCL_NAMESPACE_END

