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
//////////////////////////////////////////////////////////////////////

#include <iostream>
#include "FLUID_3D.h"

using namespace std;

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
  cout << "=========================================================================" << endl;
  cout << " Wavelet Turbulence simulator " << endl;
  cout << "=========================================================================" << endl;
  cout << " This code is Copyright 2008 Theodore Kim and Nils Thuerey and released" << endl;
  cout << " under the GNU public license. For more information see:" << endl << endl;
  cout << "   http://www.cs.cornell.edu/~tedkim/WTURB" << endl;
  cout << "=========================================================================" << endl;

  int xRes = 48;
  int yRes = 64;
  int zRes = 48;
  int amplify = 4;
  int totalCells = xRes * yRes * zRes;
  int amplifiedCells = totalCells * amplify * amplify * amplify;
  
  // print out memory requirements
  long long int coarseSize = sizeof(float) * totalCells * 22 + 
                   sizeof(unsigned char) * totalCells;
  long long int fineSize = sizeof(float) * amplifiedCells * 7 + // big grids
                 sizeof(float) * totalCells * 8 +     // small grids
                 sizeof(float) * 128 * 128 * 128;     // noise tile
  long long int totalMB = (coarseSize + fineSize) / 1048576;
  cout << " Current coarse resolution: " << xRes << " x " << yRes << " x " << zRes << endl;
  cout << " Current amplified resolution: " << xRes * amplify << " x " << yRes * amplify 
                                            << " x " << zRes * amplify << endl;
  cout << " At least " << totalMB << " MB of RAM needed " << endl;
  cout << "=========================================================================" << endl;
  cout.flush();

  // create output directories
  system("mkdir original.preview");
  system("mkdir amplified.preview");
  system("mkdir pbrt");
  
	FLUID_3D fluid(xRes, yRes, zRes, amplify);
  for (int x = 0; x < 300; x++)
  {
    fluid.addSmokeColumn();
    fluid.step();
  }

	return EXIT_SUCCESS;
}
