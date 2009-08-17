//////////////////////////////////////////////////////////////////////
// This file is part of Wavelet Turbulence.
// 
// Wavelet Turbulence is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Wavelet Turbulence is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Wavelet Turbulence.  If not, see <http://www.gnu.org/licenses/>.
// 
// Copyright 2008 Theodore Kim and Nils Thuerey
// 
//////////////////////////////////////////////////////////////////////////////////////////
// Wavelet noise functions
//
// This code is based on the C code provided in the appendices of:
//
// @article{1073264,
//  author = {Robert L. Cook and Tony DeRose},
//  title = {Wavelet noise},
//  journal = {ACM Trans. Graph.},
//  volume = {24},
//  number = {3},
//  year = {2005},
//  issn = {0730-0301},
//  pages = {803--811},
//  doi = {http://doi.acm.org/10.1145/1073204.1073264},
//  publisher = {ACM},
//  address = {New York, NY, USA},
//  }
//
//////////////////////////////////////////////////////////////////////////////////////////

#ifndef WAVELET_NOISE_H
#define WAVELET_NOISE_H

#include <MERSENNETWISTER.h>

#define NOISE_TILE_SIZE 128
static const int noiseTileSize = NOISE_TILE_SIZE;

// warning - noiseTileSize has to be 128^3!
#define modFast128(x) ((x) & 127)
#define modFast64(x)  ((x) & 63)
#define DOWNCOEFFS 0.000334f,-0.001528f, 0.000410f, 0.003545f,-0.000938f,-0.008233f, 0.002172f, 0.019120f, \
                  -0.005040f,-0.044412f, 0.011655f, 0.103311f,-0.025936f,-0.243780f, 0.033979f, 0.655340f, \
                   0.655340f, 0.033979f,-0.243780f,-0.025936f, 0.103311f, 0.011655f,-0.044412f,-0.005040f, \
                   0.019120f, 0.002172f,-0.008233f,-0.000938f, 0.003546f, 0.000410f,-0.001528f, 0.000334f

//////////////////////////////////////////////////////////////////////////////////////////
// Wavelet downsampling -- periodic boundary conditions
//////////////////////////////////////////////////////////////////////////////////////////
static void downsampleX(float *from, float *to, int n){
  // if these values are not local incorrect results are generated
  float downCoeffs[32] = { DOWNCOEFFS };
	const float *a = &downCoeffs[16];
	for (int i = 0; i < n / 2; i++) {
		to[i] = 0;
		for (int k = 2 * i - 16; k <= 2 * i + 16; k++)
			to[i] += a[k - 2 * i] * from[modFast128(k)];
	}
}
static void downsampleY(float *from, float *to, int n){
  // if these values are not local incorrect results are generated
  float downCoeffs[32] = { DOWNCOEFFS };
	const float *a = &downCoeffs[16];
	for (int i = 0; i < n / 2; i++) {
		to[i * n] = 0;
		for (int k = 2 * i - 16; k <= 2 * i + 16; k++)
			to[i * n] += a[k - 2 * i] * from[modFast128(k) * n];
	}
}
static void downsampleZ(float *from, float *to, int n){
  // if these values are not local incorrect results are generated
  float downCoeffs[32] = { DOWNCOEFFS };
	const float *a = &downCoeffs[16];
	for (int i = 0; i < n / 2; i++) {
		to[i * n * n] = 0;
		for (int k = 2 * i - 16; k <= 2 * i + 16; k++)
			to[i * n * n] += a[k - 2 * i] * from[modFast128(k) * n * n];
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Wavelet downsampling -- Neumann boundary conditions
//////////////////////////////////////////////////////////////////////////////////////////
static void downsampleNeumann(const float *from, float *to, int n, int stride)
{
  // if these values are not local incorrect results are generated
  float downCoeffs[32] = { DOWNCOEFFS };
  static const float *const aCoCenter= &downCoeffs[16];
	for (int i = 0; i < n / 2; i++) {
		to[i * stride] = 0;
		for (int k = 2 * i - 16; k < 2 * i + 16; k++) { 
			// handle boundary
			float fromval; 
			if (k < 0) {
				fromval = from[0];
			} else if(k > n - 1) {
				fromval = from[(n - 1) * stride];
			} else {
				fromval = from[k * stride]; 
			} 
			to[i * stride] += aCoCenter[k - 2 * i] * fromval; 
		}
	}
}
static void downsampleXNeumann(float* to, const float* from, int sx,int sy, int sz) {
	for (int iy = 0; iy < sy; iy++) 
		for (int iz = 0; iz < sz; iz++) {
			const int i = iy * sx + iz*sx*sy;
			downsampleNeumann(&from[i], &to[i], sx, 1);
		}
}
static void downsampleYNeumann(float* to, const float* from, int sx,int sy, int sz) {
	for (int ix = 0; ix < sx; ix++) 
		for (int iz = 0; iz < sz; iz++) {
			const int i = ix + iz*sx*sy;
			downsampleNeumann(&from[i], &to[i], sy, sx);
    }
}
static void downsampleZNeumann(float* to, const float* from, int sx,int sy, int sz) {
	for (int ix = 0; ix < sx; ix++) 
		for (int iy = 0; iy < sy; iy++) {
			const int i = ix + iy*sx;
			downsampleNeumann(&from[i], &to[i], sz, sx*sy);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
// Wavelet upsampling - periodic boundary conditions
//////////////////////////////////////////////////////////////////////////////////////////
static float _upCoeffs[4] = {0.25f, 0.75f, 0.75f, 0.25f};
static void upsampleX(float *from, float *to, int n) {
	const float *p = &_upCoeffs[2];

	for (int i = 0; i < n; i++) {
		to[i] = 0;
		for (int k = i / 2; k <= i / 2 + 1; k++)
			to[i] += p[i - 2 * k] * from[modFast64(k)];
	}
}
static void upsampleY(float *from, float *to, int n) {
	const float *p = &_upCoeffs[2];

	for (int i = 0; i < n; i++) {
		to[i * n] = 0;
		for (int k = i / 2; k <= i / 2 + 1; k++)
			to[i * n] += p[i - 2 * k] * from[modFast64(k) * n];
	}
}
static void upsampleZ(float *from, float *to, int n) {
	const float *p = &_upCoeffs[2];

	for (int i = 0; i < n; i++) {
		to[i * n * n] = 0;
		for (int k = i / 2; k <= i / 2 + 1; k++)
			to[i * n * n] += p[i - 2 * k] * from[modFast64(k) * n * n];
	}
}

//////////////////////////////////////////////////////////////////////////////////////////
// Wavelet upsampling - Neumann boundary conditions
//////////////////////////////////////////////////////////////////////////////////////////
static void upsampleNeumann(const float *from, float *to, int n, int stride) {
  static const float *const pCoCenter = &_upCoeffs[2];
	for (int i = 0; i < n; i++) {
		to[i * stride] = 0;
		for (int k = i / 2; k <= i / 2 + 1; k++) {
			float fromval;
			if(k>n/2) {
				fromval = from[(n/2) * stride];
			} else {
				fromval = from[k * stride]; 
			}  
			to[i * stride] += pCoCenter[i - 2 * k] * fromval; 
		}
	}
}
static void upsampleXNeumann(float* to, const float* from, int sx, int sy, int sz) {
	for (int iy = 0; iy < sy; iy++) 
		for (int iz = 0; iz < sz; iz++) {
			const int i = iy * sx + iz*sx*sy;
			upsampleNeumann(&from[i], &to[i], sx, 1);
		}
}
static void upsampleYNeumann(float* to, const float* from, int sx, int sy, int sz) {
	for (int ix = 0; ix < sx; ix++) 
		for (int iz = 0; iz < sz; iz++) {
			const int i = ix + iz*sx*sy;
			upsampleNeumann(&from[i], &to[i], sy, sx);
		}
}
static void upsampleZNeumann(float* to, const float* from, int sx, int sy, int sz) {
	for (int ix = 0; ix < sx; ix++) 
		for (int iy = 0; iy < sy; iy++) {
			const int i = ix + iy*sx;
			upsampleNeumann(&from[i], &to[i], sz, sx*sy);
		}
}


//////////////////////////////////////////////////////////////////////////////////////////
// load in an existing noise tile
//////////////////////////////////////////////////////////////////////////////////////////
static bool loadTile(float* const noiseTileData, std::string filename)
{
	FILE* file;
	file = fopen(filename.c_str(), "rb");

	if (file == NULL) {
		printf("loadTile: No noise tile '%s' found.\n", filename.c_str());
		return false;
	}

	// dimensions
	size_t gridSize = noiseTileSize * noiseTileSize * noiseTileSize;

	// noiseTileData memory is managed by caller
	size_t bread = fread((void*)noiseTileData, sizeof(float), gridSize, file);
	fclose(file);
	printf("Noise tile file '%s' loaded.\n", filename.c_str());

	if (bread != gridSize) {
		printf("loadTile: Noise tile '%s' is wrong size %d.\n", filename.c_str(), (int)bread);
		return false;
	} 
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////
// write out an existing noise tile
//////////////////////////////////////////////////////////////////////////////////////////
static void saveTile(float* const noiseTileData, std::string filename)
{
	FILE* file;
	file = fopen(filename.c_str(), "wb");

	if (file == NULL) {
		printf("saveTile: Noise tile '%s' could not be saved.\n", filename.c_str());
		return;
	} 

	fwrite((void*)noiseTileData, sizeof(float), noiseTileSize * noiseTileSize * noiseTileSize, file);
	fclose(file);

	printf("saveTile: Noise tile file '%s' saved.\n", filename.c_str());
}

//////////////////////////////////////////////////////////////////////////////////////////
// create a new noise tile if necessary
//////////////////////////////////////////////////////////////////////////////////////////
static void generateTile_WAVELET(float* const noiseTileData, std::string filename) {
	// if a tile already exists, just use that
	if (loadTile(noiseTileData, filename)) return;

	const int n = noiseTileSize;
	const int n3 = n*n*n;
	std::cout <<"Generating new 3d noise tile size="<<n<<"^3 \n";
	MTRand twister;

	float *temp13 = new float[n3];
	float *temp23 = new float[n3];
	float *noise3 = new float[n3];

	// initialize
	for (int i = 0; i < n3; i++) {
		temp13[i] = temp23[i] = noise3[i] = 0.;
	}

	// Step 1. Fill the tile with random numbers in the range -1 to 1.
	for (int i = 0; i < n3; i++) 
		noise3[i] = twister.randNorm();

	// Steps 2 and 3. Downsample and upsample the tile
	for (int iy = 0; iy < n; iy++) 
		for (int iz = 0; iz < n; iz++) {
			const int i = iy * n + iz*n*n;
			downsampleX(&noise3[i], &temp13[i], n);
			upsampleX  (&temp13[i], &temp23[i], n);
		}
	for (int ix = 0; ix < n; ix++) 
		for (int iz = 0; iz < n; iz++) {
			const int i = ix + iz*n*n;
			downsampleY(&temp23[i], &temp13[i], n);
			upsampleY  (&temp13[i], &temp23[i], n);
		}
	for (int ix = 0; ix < n; ix++) 
		for (int iy = 0; iy < n; iy++) {
			const int i = ix + iy*n;
			downsampleZ(&temp23[i], &temp13[i], n);
			upsampleZ  (&temp13[i], &temp23[i], n);
		}

	// Step 4. Subtract out the coarse-scale contribution
	for (int i = 0; i < n3; i++) 
		noise3[i] -= temp23[i];

	// Avoid even/odd variance difference by adding odd-offset version of noise to itself.
	int offset = n / 2;
	if (offset % 2 == 0) offset++;

	int icnt=0;
	for (int ix = 0; ix < n; ix++)
		for (int iy = 0; iy < n; iy++)
			for (int iz = 0; iz < n; iz++) { 
				temp13[icnt] = noise3[modFast128(ix+offset) + modFast128(iy+offset)*n + modFast128(iz+offset)*n*n];
				icnt++;
			}

	for (int i = 0; i < n3; i++) 
		noise3[i] += temp13[i];

	for (int i = 0; i < n3; i++) 
		noiseTileData[i] = noise3[i];

	saveTile(noise3, filename); 
	delete[] temp13;
	delete[] temp23;
	delete[] noise3;
	std::cout <<"Generating new 3d noise done\n";
}

//////////////////////////////////////////////////////////////////////////////////////////
// x derivative of noise
//////////////////////////////////////////////////////////////////////////////////////////
static inline float WNoiseDx(Vec3 p, float* data) { 
  int c[3], mid[3], n = noiseTileSize;
  float w[3][3], t, result = 0;
  
  mid[0] = (int)ceil(p[0] - 0.5); 
  t = mid[0] - (p[0] - 0.5);
	w[0][0] = -t;
	w[0][2] = (1.f - t);
	w[0][1] = 2.0f * t - 1.0f;
  
  mid[1] = (int)ceil(p[1] - 0.5); 
  t = mid[1] - (p[1] - 0.5);
  w[1][0] = t * t / 2; 
  w[1][2] = (1 - t) * (1 - t) / 2;
  w[1][1] = 1 - w[1][0] - w[1][2];

  mid[2] = (int)ceil(p[2] - 0.5); 
  t = mid[2] - (p[2] - 0.5);
  w[2][0] = t * t / 2; 
  w[2][2] = (1 - t) * (1 - t)/2; 
  w[2][1] = 1 - w[2][0] - w[2][2];
 
  // to optimize, explicitly unroll this loop
  for (int z = -1; z <=1; z++)
    for (int y = -1; y <=1; y++)
      for (int x = -1; x <=1; x++)
      {
        float weight = 1.0f;
        c[0] = modFast128(mid[0] + x);
        weight *= w[0][x+1];
        c[1] = modFast128(mid[1] + y);
        weight *= w[1][y+1];
        c[2] = modFast128(mid[2] + z);
        weight *= w[2][z+1];
        result += weight * data[c[2]*n*n+c[1]*n+c[0]];
      }
 return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
// y derivative of noise
//////////////////////////////////////////////////////////////////////////////////////////
static inline float WNoiseDy(Vec3 p, float* data) { 
  int c[3], mid[3], n=noiseTileSize; 
  float w[3][3], t, result =0;
  
  mid[0] = (int)ceil(p[0] - 0.5); 
  t = mid[0]-(p[0] - 0.5);
  w[0][0] = t * t / 2; 
  w[0][2] = (1 - t) * (1 - t) / 2;
  w[0][1] = 1 - w[0][0] - w[0][2];
  
  mid[1] = (int)ceil(p[1] - 0.5); 
  t = mid[1]-(p[1] - 0.5);
	w[1][0] = -t;
	w[1][2] = (1.f - t);
	w[1][1] = 2.0f * t - 1.0f;

  mid[2] = (int)ceil(p[2] - 0.5); 
  t = mid[2] - (p[2] - 0.5);
  w[2][0] = t * t / 2; 
  w[2][2] = (1 - t) * (1 - t)/2; 
  w[2][1] = 1 - w[2][0] - w[2][2];
  
  // to optimize, explicitly unroll this loop
  for (int z = -1; z <=1; z++)
    for (int y = -1; y <=1; y++)
      for (int x = -1; x <=1; x++)
      {
        float weight = 1.0f;
        c[0] = modFast128(mid[0] + x);
        weight *= w[0][x+1];
        c[1] = modFast128(mid[1] + y);
        weight *= w[1][y+1];
        c[2] = modFast128(mid[2] + z);
        weight *= w[2][z+1];
        result += weight * data[c[2]*n*n+c[1]*n+c[0]];
      }

  return result;
}

//////////////////////////////////////////////////////////////////////////////////////////
// z derivative of noise
//////////////////////////////////////////////////////////////////////////////////////////
static inline float WNoiseDz(Vec3 p, float* data) { 
  int c[3], mid[3], n=noiseTileSize; 
  float w[3][3], t, result =0;

  mid[0] = (int)ceil(p[0] - 0.5); 
  t = mid[0]-(p[0] - 0.5);
  w[0][0] = t * t / 2; 
  w[0][2] = (1 - t) * (1 - t) / 2;
  w[0][1] = 1 - w[0][0] - w[0][2];
  
  mid[1] = (int)ceil(p[1] - 0.5); 
  t = mid[1]-(p[1] - 0.5);
  w[1][0] = t * t / 2; 
  w[1][2] = (1 - t) * (1 - t) / 2;
  w[1][1] = 1 - w[1][0] - w[1][2];

  mid[2] = (int)ceil(p[2] - 0.5); 
  t = mid[2] - (p[2] - 0.5);
	w[2][0] = -t;
	w[2][2] = (1.f - t);
	w[2][1] = 2.0f * t - 1.0f;

  // to optimize, explicitly unroll this loop
  for (int z = -1; z <=1; z++)
    for (int y = -1; y <=1; y++)
      for (int x = -1; x <=1; x++)
      {
        float weight = 1.0f;
        c[0] = modFast128(mid[0] + x);
        weight *= w[0][x+1];
        c[1] = modFast128(mid[1] + y);
        weight *= w[1][y+1];
        c[2] = modFast128(mid[2] + z);
        weight *= w[2][z+1];
        result += weight * data[c[2]*n*n+c[1]*n+c[0]];
      }
  return result;
}

#endif

