/*
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

/* Voronoi Distances */

float voronoi_distance(string distance_metric, vector d, float e)
{
	float result = 0.0;

	if(distance_metric == "Distance Squared")
		result = dot(d, d);
	if(distance_metric == "Actual Distance")
		result = length(d);
	if(distance_metric == "Manhattan")
		result = fabs(d[0]) + fabs(d[1]) + fabs(d[2]);
	if(distance_metric == "Chebychev")
		result = max(fabs(d[0]), max(fabs(d[1]), fabs(d[2])));
	if(distance_metric == "Minkovsky 1/2")
		result = sqrt(fabs(d[0])) + sqrt(fabs(d[1])) + sqrt(fabs(d[1]));
	if(distance_metric == "Minkovsky 4")
		result = sqrt(sqrt(dot(d*d, d*d)));
	if(distance_metric == "Minkovsky")
		result = pow(pow(fabs(d[0]), e) + pow(fabs(d[1]), e) + pow(fabs(d[2]), e), 1.0/e);
	
	return result;
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

	for(xx = xi-1; xx <= xi+1; xx++) {
		for(yy = yi-1; yy <= yi+1; yy++) {
			for(zz = zi-1; zz <= zi+1; zz++) {
				point ip = point(xx, yy, zz);
				point vp = (point)cellnoise_color(ip);
				point pd = p - (vp + ip);
				float d = voronoi_distance(distance_metric, pd, e);

				vp += point(xx, yy, zz);

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
	float t = 10.0*voronoi_F1F2(p);
	return (t > 1.0)? 1.0: t;
}

float voronoi_F1S(point p) { return 2.0*voronoi_F1(p) - 1.0; }
float voronoi_F2S(point p) { return 2.0*voronoi_F2(p) - 1.0; }
float voronoi_F3S(point p) { return 2.0*voronoi_F3(p) - 1.0; }
float voronoi_F4S(point p) { return 2.0*voronoi_F4(p) - 1.0; }
float voronoi_F1F2S(point p) { return 2.0*voronoi_F1F2(p) - 1.0; }
float voronoi_CrS(point p) { return 2.0*voronoi_Cr(p) - 1.0; }

/* Noise Bases */

float noise_basis(point p, string basis)
{
	float result = 0.0;

	if(basis == "Perlin")
		result = noise(p);
	if(basis == "Voronoi F1")
		result = voronoi_F1S(p);
	if(basis == "Voronoi F2")
		result = voronoi_F2S(p);
	if(basis == "Voronoi F3")
		result = voronoi_F3S(p);
	if(basis == "Voronoi F4")
		result = voronoi_F4S(p);
	if(basis == "Voronoi F2-F1")
		result = voronoi_F1F2S(p);
	if(basis == "Voronoi Crackle")
		result = voronoi_CrS(p);
	if(basis == "Cell Noise")
		result = cellnoise(p);
	
	return result;
}

/* Soft/Hard Noise */

float noise_basis_hard(point p, string basis, int hard)
{
	float t = noise_basis(p, basis);
	return (hard)? fabs(2.0*t - 1.0): t;
}

/* Waves */

float noise_wave(string wave, float a)
{
	float result = 0.0;

	if(wave == "Sine") {
		result = 0.5 + 0.5*sin(a);
	}
	else if(wave == "Saw") {
		float b = 2*M_PI;
		int n = (int)(a / b);
		a -= n*b;
		if(a < 0) a += b;

		result = a / b;
	}
	else if(wave == "Tri") {
		float b = 2*M_PI;
		float rmax = 1.0;

		result = rmax - 2.0*fabs(floor((a*(1.0/b))+0.5) - (a*(1.0/b)));
	}

	return result;
}

/* Turbulence */

float noise_turbulence(point p, string basis, int octaves, int hard)
{
	float fscale = 1.0;
	float amp = 1.0;
	float sum = 0.0;
	int i;

	for(i = 0; i <= octaves; i++) {
		float t = noise_basis(fscale*p, basis);

		if(hard)
			t = fabs(2.0*t - 1.0);

		sum += t*amp;
		amp *= 0.5;
		fscale *= 2.0;
	}

	sum *= ((float)(1 << octaves)/(float)((1 << (octaves+1)) - 1));

	return sum;
}

/* Utility */

float nonzero(float f, float eps)
{
	float r;

	if(abs(f) < eps)
		r = sign(f)*eps;
	else
		r = f;
	
	return r;
}

