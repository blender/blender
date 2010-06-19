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
/////////////////////////////////////////////////////////////////////////
//

#ifndef FFT_NOISE_H_
#define FFT_NOISE_H_

#if FFTW3==1
#include <iostream>
#include <fftw3.h>
#include <MERSENNETWISTER.h>

#include "WAVELET_NOISE.h"

#ifndef M_PI
#define M_PI 3.14159265
#endif

/////////////////////////////////////////////////////////////////////////
// shift spectrum to the format that FFTW expects
/////////////////////////////////////////////////////////////////////////
static void shift3D(float*& field, int xRes, int yRes, int zRes)
{
  int xHalf = xRes / 2;
  int yHalf = yRes / 2;
  int zHalf = zRes / 2;
 // int slabSize = xRes * yRes;
  for (int z = 0; z < zHalf; z++)
    for (int y = 0; y < yHalf; y++)
      for (int x = 0; x < xHalf; x++)
      {
        int index = x + y * xRes + z * xRes * yRes;
        float temp;
        int xSwap = xHalf;
        int ySwap = yHalf * xRes;
        int zSwap = zHalf * xRes * yRes;
        
        // [0,0,0] to [1,1,1]
        temp = field[index];
        field[index] = field[index + xSwap + ySwap + zSwap];
        field[index + xSwap + ySwap + zSwap] = temp;

        // [1,0,0] to [0,1,1]
        temp = field[index + xSwap];
        field[index + xSwap] = field[index + ySwap + zSwap];
        field[index + ySwap + zSwap] = temp;

        // [0,1,0] to [1,0,1]
        temp = field[index + ySwap];
        field[index + ySwap] = field[index + xSwap + zSwap];
        field[index + xSwap + zSwap] = temp;
        
        // [0,0,1] to [1,1,0]
        temp = field[index + zSwap];
        field[index + zSwap] = field[index + xSwap + ySwap];
        field[index + xSwap + ySwap] = temp;
      }
}

static void generatTile_FFT(float* const noiseTileData, std::string filename)
{
	if (loadTile(noiseTileData, filename)) return;
	
	int res = NOISE_TILE_SIZE;
	int xRes = res;
	int yRes = res;
	int zRes = res;
	int totalCells = xRes * yRes * zRes;
	
	// create and shift the filter
	float* filter = new float[totalCells];
	for (int z = 0; z < zRes; z++)
		for (int y = 0; y < yRes; y++)
			for (int x = 0; x < xRes; x++)
			{
				int index = x + y * xRes + z * xRes * yRes;
				float diff[] = {abs(x - xRes/2), 
				abs(y - yRes/2), 
				abs(z - zRes/2)};
				float radius = sqrtf(diff[0] * diff[0] + 
				diff[1] * diff[1] + 
				diff[2] * diff[2]) / (xRes / 2);
				radius *= M_PI;
				float H = cos((M_PI / 2.0f) * log(4.0f * radius / M_PI) / log(2.0f));
				H = H * H;
				float filtered = H;
				
				// clamp everything outside the wanted band
				if (radius >= M_PI / 2.0f)
					filtered = 0.0f;
				
				// make sure to capture all low frequencies
				if (radius <= M_PI / 4.0f)
					filtered = 1.0f;
				
				filter[index] = filtered;
			}
	shift3D(filter, xRes, yRes, zRes);
	
	// create the noise
	float* noise = new float[totalCells];
	int index = 0;
	MTRand twister;
	for (int z = 0; z < zRes; z++)
	for (int y = 0; y < yRes; y++)
	  for (int x = 0; x < xRes; x++, index++)
		noise[index] = twister.randNorm();
	
	// create padded field
	fftw_complex* forward = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * totalCells);
	
	// init padded field
	index = 0;
	for (int z = 0; z < zRes; z++)
	for (int y = 0; y < yRes; y++)
	  for (int x = 0; x < xRes; x++, index++)
	  {
		forward[index][0] = noise[index];
		forward[index][1] = 0.0f;
	  }
	
	// forward FFT 
	fftw_complex* backward = (fftw_complex*)fftw_malloc(sizeof(fftw_complex) * totalCells);
	fftw_plan forwardPlan = fftw_plan_dft_3d(xRes, yRes, zRes, forward, backward, FFTW_FORWARD, FFTW_ESTIMATE);  
	fftw_execute(forwardPlan);
	fftw_destroy_plan(forwardPlan);
	
	// apply filter
	index = 0;
	for (int z = 0; z < zRes; z++)
		for (int y = 0; y < yRes; y++)
			for (int x = 0; x < xRes; x++, index++)
			{
				backward[index][0] *= filter[index];
				backward[index][1] *= filter[index];
			}
	
	// backward FFT
	fftw_plan backwardPlan = fftw_plan_dft_3d(xRes, yRes, zRes, backward, forward, FFTW_BACKWARD, FFTW_ESTIMATE);  
	fftw_execute(backwardPlan);
	fftw_destroy_plan(backwardPlan);
	
	// subtract out the low frequency components
	index = 0;
	for (int z = 0; z < zRes; z++)
		for (int y = 0; y < yRes; y++)
			for (int x = 0; x < xRes; x++, index++)
				noise[index] -= forward[index][0] / totalCells;

	// save out the noise tile
	saveTile(noise, filename);
	
	fftw_free(forward);
	fftw_free(backward);
	delete[] filter;
	delete[] noise;
}

#endif

#endif /* FFT_NOISE_H_ */
