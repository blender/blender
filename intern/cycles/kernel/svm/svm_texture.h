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

CCL_NAMESPACE_BEGIN

/* Voronoi Distances */

#if 0
ccl_device float voronoi_distance(NodeDistanceMetric distance_metric, float3 d, float e)
{
#if 0
	if(distance_metric == NODE_VORONOI_DISTANCE_SQUARED)
#endif
		return dot(d, d);
#if 0
	if(distance_metric == NODE_VORONOI_ACTUAL_DISTANCE)
		return len(d);
	if(distance_metric == NODE_VORONOI_MANHATTAN)
		return fabsf(d.x) + fabsf(d.y) + fabsf(d.z);
	if(distance_metric == NODE_VORONOI_CHEBYCHEV)
		return fmaxf(fabsf(d.x), fmaxf(fabsf(d.y), fabsf(d.z)));
	if(distance_metric == NODE_VORONOI_MINKOVSKY_H)
		return sqrtf(fabsf(d.x)) + sqrtf(fabsf(d.y)) + sqrtf(fabsf(d.y));
	if(distance_metric == NODE_VORONOI_MINKOVSKY_4)
		return sqrtf(sqrtf(dot(d*d, d*d)));
	if(distance_metric == NODE_VORONOI_MINKOVSKY)
		return powf(powf(fabsf(d.x), e) + powf(fabsf(d.y), e) + powf(fabsf(d.z), e), 1.0f/e);
	
	return 0.0f;
#endif
}

/* Voronoi / Worley like */
ccl_device_inline float4 voronoi_Fn(float3 p, float e, int n1, int n2)
{
	float da[4];
	float3 pa[4];
	NodeDistanceMetric distance_metric = NODE_VORONOI_DISTANCE_SQUARED;

	/* returns distances in da and point coords in pa */
	int xx, yy, zz, xi, yi, zi;

	xi = floor_to_int(p.x);
	yi = floor_to_int(p.y);
	zi = floor_to_int(p.z);

	da[0] = 1e10f;
	da[1] = 1e10f;
	da[2] = 1e10f;
	da[3] = 1e10f;

	pa[0] = make_float3(0.0f, 0.0f, 0.0f);
	pa[1] = make_float3(0.0f, 0.0f, 0.0f);
	pa[2] = make_float3(0.0f, 0.0f, 0.0f);
	pa[3] = make_float3(0.0f, 0.0f, 0.0f);

	for(xx = xi-1; xx <= xi+1; xx++) {
		for(yy = yi-1; yy <= yi+1; yy++) {
			for(zz = zi-1; zz <= zi+1; zz++) {
				float3 ip = make_float3((float)xx, (float)yy, (float)zz);
				float3 vp = cellnoise_color(ip);
				float3 pd = p - (vp + ip);
				float d = voronoi_distance(distance_metric, pd, e);

				vp += ip;

				if(d < da[0]) {
					da[3] = da[2];
					da[2] = da[1];
					da[1] = da[0];
					da[0] = d;

					pa[3] = pa[2];
					pa[2] = pa[1];
					pa[1] = pa[0];
					pa[0] = vp;
				}
				else if(d < da[1]) {
					da[3] = da[2];
					da[2] = da[1];
					da[1] = d;

					pa[3] = pa[2];
					pa[2] = pa[1];
					pa[1] = vp;
				}
				else if(d < da[2]) {
					da[3] = da[2];
					da[2] = d;

					pa[3] = pa[2];
					pa[2] = vp;
				}
				else if(d < da[3]) {
					da[3] = d;
					pa[3] = vp;
				}
			}
		}
	}

	float4 result = make_float4(pa[n1].x, pa[n1].y, pa[n1].z, da[n1]);

	if(n2 != -1)
		result = make_float4(pa[n2].x, pa[n2].y, pa[n2].z, da[n2]) - result;

	return result;
}
#endif

ccl_device float voronoi_F1_distance(float3 p)
{
	/* returns squared distance in da */
	float da = 1e10f;

#ifndef __KERNEL_SSE2__
	int ix = floor_to_int(p.x), iy = floor_to_int(p.y), iz = floor_to_int(p.z);

	for (int xx = -1; xx <= 1; xx++) {
		for (int yy = -1; yy <= 1; yy++) {
			for (int zz = -1; zz <= 1; zz++) {
				float3 ip = make_float3(ix + xx, iy + yy, iz + zz);
				float3 vp = ip + cellnoise_color(ip);
				float d = len_squared(p - vp);
				da = min(d, da);
			}
		}
	}
#else
	ssef vec_p = load4f(p);
	ssei xyzi = quick_floor_sse(vec_p);

	for (int xx = -1; xx <= 1; xx++) {
		for (int yy = -1; yy <= 1; yy++) {
			for (int zz = -1; zz <= 1; zz++) {
				ssef ip = ssef(xyzi + ssei(xx, yy, zz, 0));
				ssef vp = ip + cellnoise_color(ip);
				float d = len_squared<1, 1, 1, 0>(vec_p - vp);
				da = min(d, da);
			}
		}
	}
#endif

	return da;
}

ccl_device float3 voronoi_F1_color(float3 p)
{
	/* returns color of the nearest point */
	float da = 1e10f;

#ifndef __KERNEL_SSE2__
	float3 pa;
	int ix = floor_to_int(p.x), iy = floor_to_int(p.y), iz = floor_to_int(p.z);

	for (int xx = -1; xx <= 1; xx++) {
		for (int yy = -1; yy <= 1; yy++) {
			for (int zz = -1; zz <= 1; zz++) {
				float3 ip = make_float3(ix + xx, iy + yy, iz + zz);
				float3 vp = ip + cellnoise_color(ip);
				float d = len_squared(p - vp);

				if(d < da) {
					da = d;
					pa = vp;
				}
			}
		}
	}

	return cellnoise_color(pa);
#else
	ssef pa, vec_p = load4f(p);
	ssei xyzi = quick_floor_sse(vec_p);

	for (int xx = -1; xx <= 1; xx++) {
		for (int yy = -1; yy <= 1; yy++) {
			for (int zz = -1; zz <= 1; zz++) {
				ssef ip = ssef(xyzi + ssei(xx, yy, zz, 0));
				ssef vp = ip + cellnoise_color(ip);
				float d = len_squared<1, 1, 1, 0>(vec_p - vp);

				if(d < da) {
					da = d;
					pa = vp;
				}
			}
		}
	}

	ssef color = cellnoise_color(pa);
	return (float3 &)color;
#endif
}

#if 0
ccl_device float voronoi_F1(float3 p) { return voronoi_Fn(p, 0.0f, 0, -1).w; }
ccl_device float voronoi_F2(float3 p) { return voronoi_Fn(p, 0.0f, 1, -1).w; }
ccl_device float voronoi_F3(float3 p) { return voronoi_Fn(p, 0.0f, 2, -1).w; }
ccl_device float voronoi_F4(float3 p) { return voronoi_Fn(p, 0.0f, 3, -1).w; }
ccl_device float voronoi_F1F2(float3 p) { return voronoi_Fn(p, 0.0f, 0, 1).w; }

ccl_device float voronoi_Cr(float3 p)
{
	/* crackle type pattern, just a scale/clamp of F2-F1 */
	float t = 10.0f*voronoi_F1F2(p);
	return (t > 1.0f)? 1.0f: t;
}

ccl_device float voronoi_F1S(float3 p) { return 2.0f*voronoi_F1(p) - 1.0f; }
ccl_device float voronoi_F2S(float3 p) { return 2.0f*voronoi_F2(p) - 1.0f; }
ccl_device float voronoi_F3S(float3 p) { return 2.0f*voronoi_F3(p) - 1.0f; }
ccl_device float voronoi_F4S(float3 p) { return 2.0f*voronoi_F4(p) - 1.0f; }
ccl_device float voronoi_F1F2S(float3 p) { return 2.0f*voronoi_F1F2(p) - 1.0f; }
ccl_device float voronoi_CrS(float3 p) { return 2.0f*voronoi_Cr(p) - 1.0f; }
#endif

/* Noise Bases */

ccl_device float noise_basis(float3 p, NodeNoiseBasis basis)
{
	/* Only Perlin enabled for now, others break CUDA compile by making kernel
	 * too big, with compile using > 4GB, due to everything being inlined. */

#if 0
	if(basis == NODE_NOISE_PERLIN)
#endif
		return noise(p);
#if 0
	if(basis == NODE_NOISE_VORONOI_F1)
		return voronoi_F1S(p);
	if(basis == NODE_NOISE_VORONOI_F2)
		return voronoi_F2S(p);
	if(basis == NODE_NOISE_VORONOI_F3)
		return voronoi_F3S(p);
	if(basis == NODE_NOISE_VORONOI_F4)
		return voronoi_F4S(p);
	if(basis == NODE_NOISE_VORONOI_F2_F1)
		return voronoi_F1F2S(p);
	if(basis == NODE_NOISE_VORONOI_CRACKLE)
		return voronoi_CrS(p);
	if(basis == NODE_NOISE_CELL_NOISE)
		return cellnoise(p);
	
	return 0.0f;
#endif
}

/* Soft/Hard Noise */

ccl_device float noise_basis_hard(float3 p, NodeNoiseBasis basis, int hard)
{
	float t = noise_basis(p, basis);
	return (hard)? fabsf(2.0f*t - 1.0f): t;
}

/* Turbulence */

ccl_device_noinline float noise_turbulence(float3 p, NodeNoiseBasis basis, float octaves, int hard)
{
	float fscale = 1.0f;
	float amp = 1.0f;
	float sum = 0.0f;
	int i, n;

	octaves = clamp(octaves, 0.0f, 16.0f);
	n = float_to_int(octaves);

	for(i = 0; i <= n; i++) {
		float t = noise_basis(fscale*p, basis);

		if(hard)
			t = fabsf(2.0f*t - 1.0f);

		sum += t*amp;
		amp *= 0.5f;
		fscale *= 2.0f;
	}

	float rmd = octaves - floorf(octaves);

	if(rmd != 0.0f) {
		float t = noise_basis(fscale*p, basis);

		if(hard)
			t = fabsf(2.0f*t - 1.0f);

		float sum2 = sum + t*amp;

		sum *= ((float)(1 << n)/(float)((1 << (n+1)) - 1));
		sum2 *= ((float)(1 << (n+1))/(float)((1 << (n+2)) - 1));

		return (1.0f - rmd)*sum + rmd*sum2;
	}
	else {
		sum *= ((float)(1 << n)/(float)((1 << (n+1)) - 1));
		return sum;
	}
}

CCL_NAMESPACE_END

