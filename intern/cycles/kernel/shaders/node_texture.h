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
 * limitations under the License
 */

/* Voronoi Distances */

float voronoi_distance(string distance_metric, vector d, float e)
{
#if 0
	if (distance_metric == "Distance Squared")
#endif
		return dot(d, d);
#if 0
	if (distance_metric == "Actual Distance")
		return length(d);
	if (distance_metric == "Manhattan")
		return fabs(d[0]) + fabs(d[1]) + fabs(d[2]);
	if (distance_metric == "Chebychev")
		return max(fabs(d[0]), max(fabs(d[1]), fabs(d[2])));
	if (distance_metric == "Minkovsky 1/2")
		return sqrt(fabs(d[0])) + sqrt(fabs(d[1])) + sqrt(fabs(d[1]));
	if (distance_metric == "Minkovsky 4")
		return sqrt(sqrt(dot(d * d, d * d)));
	if (distance_metric == "Minkovsky")
		return pow(pow(fabs(d[0]), e) + pow(fabs(d[1]), e) + pow(fabs(d[2]), e), 1.0 / e);
	
	return 0.0;
#endif
}

/* Voronoi / Worley like */

color cellnoise_color(point p)
{
	float r = cellnoise(p);
	float g = cellnoise(point(p[1], p[0], p[2]));
	float b = cellnoise(point(p[1], p[2], p[0]));

	return color(r, g, b);
}

void voronoi(point p, string distance_metric, float e, float da[4], point pa[4])
{
	/* returns distances in da and point coords in pa */
	int xx, yy, zz, xi, yi, zi;

	xi = (int)floor(p[0]);
	yi = (int)floor(p[1]);
	zi = (int)floor(p[2]);

	da[0] = 1e10;
	da[1] = 1e10;
	da[2] = 1e10;
	da[3] = 1e10;

	for (xx = xi - 1; xx <= xi + 1; xx++) {
		for (yy = yi - 1; yy <= yi + 1; yy++) {
			for (zz = zi - 1; zz <= zi + 1; zz++) {
				point ip = point(xx, yy, zz);
				point vp = (point)cellnoise_color(ip);
				point pd = p - (vp + ip);
				float d = voronoi_distance(distance_metric, pd, e);

				vp += point(xx, yy, zz);

				if (d < da[0]) {
					da[3] = da[2];
					da[2] = da[1];
					da[1] = da[0];
					da[0] = d;

					pa[3] = pa[2];
					pa[2] = pa[1];
					pa[1] = pa[0];
					pa[0] = vp;
				}
				else if (d < da[1]) {
					da[3] = da[2];
					da[2] = da[1];
					da[1] = d;

					pa[3] = pa[2];
					pa[2] = pa[1];
					pa[1] = vp;
				}
				else if (d < da[2]) {
					da[3] = da[2];
					da[2] = d;

					pa[3] = pa[2];
					pa[2] = vp;
				}
				else if (d < da[3]) {
					da[3] = d;
					pa[3] = vp;
				}
			}
		}
	}
}

float voronoi_Fn(point p, int n)
{
	float da[4];
	point pa[4];

	voronoi(p, "Distance Squared", 0, da, pa);

	return da[n];
}

float voronoi_FnFn(point p, int n1, int n2)
{
	float da[4];
	point pa[4];

	voronoi(p, "Distance Squared", 0, da, pa);

	return da[n2] - da[n1];
}

float voronoi_F1(point p) { return voronoi_Fn(p, 0); }
float voronoi_F2(point p) { return voronoi_Fn(p, 1); }
float voronoi_F3(point p) { return voronoi_Fn(p, 2); }
float voronoi_F4(point p) { return voronoi_Fn(p, 3); }
float voronoi_F1F2(point p) { return voronoi_FnFn(p, 0, 1); }

float voronoi_Cr(point p)
{
	/* crackle type pattern, just a scale/clamp of F2-F1 */
	float t = 10.0 * voronoi_F1F2(p);
	return (t > 1.0) ? 1.0 : t;
}

float voronoi_F1S(point p) { return 2.0 * voronoi_F1(p) - 1.0; }
float voronoi_F2S(point p) { return 2.0 * voronoi_F2(p) - 1.0; }
float voronoi_F3S(point p) { return 2.0 * voronoi_F3(p) - 1.0; }
float voronoi_F4S(point p) { return 2.0 * voronoi_F4(p) - 1.0; }
float voronoi_F1F2S(point p) { return 2.0 * voronoi_F1F2(p) - 1.0; }
float voronoi_CrS(point p) { return 2.0 * voronoi_Cr(p) - 1.0; }

/* Noise Bases */

float safe_noise(point p, int type)
{
	float f = 0.0;
	
	/* Perlin noise in range -1..1 */
	if (type == 0)
		f = noise("perlin", p);
	
	/* Perlin noise in range 0..1 */
	else
		f = noise(p);

	/* can happen for big coordinates, things even out to 0.5 then anyway */
	if (!isfinite(f))
		return 0.5;
	
	return f;
}

float noise_basis(point p, string basis)
{
	if (basis == "Perlin")
		return safe_noise(p, 1);
	if (basis == "Voronoi F1")
		return voronoi_F1S(p);
	if (basis == "Voronoi F2")
		return voronoi_F2S(p);
	if (basis == "Voronoi F3")
		return voronoi_F3S(p);
	if (basis == "Voronoi F4")
		return voronoi_F4S(p);
	if (basis == "Voronoi F2-F1")
		return voronoi_F1F2S(p);
	if (basis == "Voronoi Crackle")
		return voronoi_CrS(p);
	if (basis == "Cell Noise")
		return cellnoise(p);
	
	return 0.0;
}

/* Soft/Hard Noise */

float noise_basis_hard(point p, string basis, int hard)
{
	float t = noise_basis(p, basis);
	return (hard) ? fabs(2.0 * t - 1.0) : t;
}

/* Turbulence */

float noise_turbulence(point p, string basis, float details, int hard)
{
	float fscale = 1.0;
	float amp = 1.0;
	float sum = 0.0;
	int i, n;
	
	float octaves = clamp(details, 0.0, 16.0);
	n = (int)octaves;

	for (i = 0; i <= n; i++) {
		float t = noise_basis(fscale * p, basis);

		if (hard)
			t = fabs(2.0 * t - 1.0);

		sum += t * amp;
		amp *= 0.5;
		fscale *= 2.0;
	}
	
	float rmd = octaves - floor(octaves);

	if (rmd != 0.0) {
		float t = noise_basis(fscale * p, basis);

		if (hard)
			t = fabs(2.0 * t - 1.0);

		float sum2 = sum + t * amp;

		sum *= ((float)(1 << n) / (float)((1 << (n + 1)) - 1));
		sum2 *= ((float)(1 << (n + 1)) / (float)((1 << (n + 2)) - 1));

		return (1.0 - rmd) * sum + rmd * sum2;
	}
	else {
		sum *= ((float)(1 << n) / (float)((1 << (n + 1)) - 1));
		return sum;
	}
}

/* Utility */

float nonzero(float f, float eps)
{
	float r;

	if (abs(f) < eps)
		r = sign(f) * eps;
	else
		r = f;
	
	return r;
}

