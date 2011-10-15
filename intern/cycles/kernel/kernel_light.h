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
	float3 D;
	float3 Ng;
	float t;
	int object;
	int prim;
	int shader;
} LightSample;

/* Regular Light */

__device float3 disk_light_sample(float3 v, float randu, float randv)
{
	float3 ru, rv;

	make_orthonormals(v, &ru, &rv);
	to_unit_disk(&randu, &randv);

	return ru*randu + rv*randv;
}

__device float3 distant_light_sample(float3 D, float size, float randu, float randv)
{
	return normalize(D + disk_light_sample(D, randu, randv)*size);
}

__device float3 sphere_light_sample(float3 P, float3 center, float size, float randu, float randv)
{
	return disk_light_sample(normalize(P - center), randu, randv)*size;
}

__device float3 area_light_sample(float3 axisu, float3 axisv, float randu, float randv)
{
	randu = randu - 0.5f;
	randv = randv - 0.5f;

	return axisu*randu + axisv*randv;
}

__device void regular_light_sample(KernelGlobals *kg, int point,
	float randu, float randv, float3 P, LightSample *ls)
{
	float4 data0 = kernel_tex_fetch(__light_data, point*LIGHT_SIZE + 0);
	float4 data1 = kernel_tex_fetch(__light_data, point*LIGHT_SIZE + 1);

	LightType type = (LightType)__float_as_int(data0.x);

	if(type == LIGHT_DISTANT) {
		/* distant light */
		float3 D = make_float3(data0.y, data0.z, data0.w);
		float size = data1.y;

		if(size > 0.0f)
			D = distant_light_sample(D, size, randu, randv);

		ls->P = D;
		ls->Ng = D;
		ls->D = -D;
		ls->t = FLT_MAX;
	}
	else {
		ls->P = make_float3(data0.y, data0.z, data0.w);

		if(type == LIGHT_POINT) {
			float size = data1.y;

			/* sphere light */
			if(size > 0.0f)
				ls->P += sphere_light_sample(P, ls->P, size, randu, randv);

			ls->Ng = normalize(P - ls->P);
		}
		else {
			/* area light */
			float4 data2 = kernel_tex_fetch(__light_data, point*LIGHT_SIZE + 2);
			float4 data3 = kernel_tex_fetch(__light_data, point*LIGHT_SIZE + 3);

			float3 axisu = make_float3(data1.y, data1.z, data2.w);
			float3 axisv = make_float3(data2.y, data2.z, data2.w);
			float3 D = make_float3(data3.y, data3.z, data3.w);

			ls->P += area_light_sample(axisu, axisv, randu, randv);
			ls->Ng = D;
		}

		ls->t = 0.0f;
	}

	ls->shader = __float_as_int(data1.x);
	ls->object = ~0;
	ls->prim = ~0;
}

__device float regular_light_pdf(KernelGlobals *kg,
	const float3 Ng, const float3 I, float t)
{
	float pdf = kernel_data.integrator.pdf_lights;

	if(t == FLT_MAX)
		return pdf;

	float cos_pi = dot(Ng, I);

	if(cos_pi <= 0.0f)
		return 0.0f;

	return t*t*pdf/cos_pi;
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
	ls->t = 0.0f;

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

	if(prim >= 0) {
		int object = __float_as_int(l.w);
		triangle_light_sample(kg, prim, object, randu, randv, ls);
	}
	else {
		int point = -prim-1;
		regular_light_sample(kg, point, randu, randv, P, ls);
	}

	/* compute incoming direction and distance */
	if(ls->t != FLT_MAX)
		ls->D = normalize_len(ls->P - P, &ls->t);
}

__device float light_sample_pdf(KernelGlobals *kg, LightSample *ls, float3 I, float t)
{
	float pdf;

	if(ls->prim != ~0)
		pdf = triangle_light_pdf(kg, ls->Ng, I, t);
	else
		pdf = regular_light_pdf(kg, ls->Ng, I, t);
	
	return pdf;
}

__device void light_select(KernelGlobals *kg, int index, float randu, float randv, float3 P, LightSample *ls)
{
	regular_light_sample(kg, index, randu, randv, P, ls);
}

__device float light_select_pdf(KernelGlobals *kg, LightSample *ls, float3 I, float t)
{
	return regular_light_pdf(kg, ls->Ng, I, t);
}

CCL_NAMESPACE_END

