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
//
#ifndef IMAGE_H
#define IMAGE_H

#include <stdlib.h>
#include <string>
#include <fstream>
#include <sstream>
#include <zlib.h>

//////////////////////////////////////////////////////////////////////
// NT helper functions
//////////////////////////////////////////////////////////////////////
template < class T > inline T ABS( T a ) {
	return (0 < a) ? a : -a ;
}

template < class T > inline void SWAP_POINTERS( T &a, T &b ) {
	T temp = a;
	a = b;
	b = temp;
}

template < class T > inline void CLAMP( T &a, T b=0., T c=1.) {
	if(a<b) { a=b; return; }
	if(a>c) { a=c; return; }
}

template < class T > inline T MIN( T a, T b) {
	return (a < b) ? a : b;
}

template < class T > inline T MAX( T a, T b) {
	return (a > b) ? a : b;
}

template < class T > inline T MAX3( T a, T b, T c) {
	T max = (a > b) ? a : b;
	max = (max > c) ? max : c;
	return max;
}

template < class T > inline float MAX3V( T vec) {
	float max = (vec[0] > vec[1]) ? vec[0] : vec[1];
	max = (max > vec[2]) ? max : vec[2];
	return max;
}

template < class T > inline float MIN3V( T vec) {
	float min = (vec[0] < vec[1]) ? vec[0] : vec[1];
	min = (min < vec[2]) ? min : vec[2];
	return min;
}

//////////////////////////////////////////////////////////////////////
// PNG, POV-Ray, and PBRT output functions
//////////////////////////////////////////////////////////////////////
#ifdef WIN32
#include "png.h"
#else
#include <png.h>
#endif

namespace IMAGE {
	/*
  static int writePng(const char *fileName, unsigned char **rowsp, int w, int h)
  {
    // defaults
    const int colortype = PNG_COLOR_TYPE_RGBA;
    const int bitdepth = 8;
    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    png_bytep *rows = rowsp;

    FILE *fp = NULL;
    std::string doing = "open for writing";
    if (!(fp = fopen(fileName, "wb"))) goto fail;

    if(!png_ptr) {
      doing = "create png write struct";
      if (!(png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL))) goto fail;
    }
    if(!info_ptr) {
      doing = "create png info struct";
      if (!(info_ptr = png_create_info_struct(png_ptr))) goto fail;
    }

    if (setjmp(png_jmpbuf(png_ptr))) goto fail;
    doing = "init IO";
    png_init_io(png_ptr, fp);
    doing = "write header";
    png_set_IHDR(png_ptr, info_ptr, w, h, bitdepth, colortype, PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    doing = "write info";
    png_write_info(png_ptr, info_ptr);
    doing = "write image";
    png_write_image(png_ptr, rows);
    doing = "write end";
    png_write_end(png_ptr, NULL);
    doing = "write destroy structs";
    png_destroy_write_struct(&png_ptr, &info_ptr);

    fclose( fp );
    return 0;

  fail:
    std::cerr << "writePng: could not "<<doing<<" !\n";
    if(fp) fclose( fp );
    if(png_ptr || info_ptr) png_destroy_write_struct(&png_ptr, &info_ptr);
    return -1;
  }
  */

  /////////////////////////////////////////////////////////////////////////////////
  // write a numbered PNG file out, padded with zeros up to three zeros
  /////////////////////////////////////////////////////////////////////////////////
  /*
  static void dumpNumberedPNG(int counter, std::string prefix, float* field, int xRes, int yRes)
  {
	char buffer[256];
    sprintf(buffer,"%04i", counter);
    std::string number = std::string(buffer);

    unsigned char pngbuf[xRes*yRes*4];
    unsigned char *rows[yRes];
    float *pfield = field;
    for (int j=0; j<yRes; j++) {
      for (int i=0; i<xRes; i++) {
        float val = *pfield;
        if(val>1.) val=1.;
        if(val<0.) val=0.;
        pngbuf[(j*xRes+i)*4+0] = (unsigned char)(val*255.);
        pngbuf[(j*xRes+i)*4+1] = (unsigned char)(val*255.);
        pngbuf[(j*xRes+i)*4+2] = (unsigned char)(val*255.);
        pfield++;
        pngbuf[(j*xRes+i)*4+3] = 255;
      }
      rows[j] = &pngbuf[(yRes-j-1)*xRes*4];
    }
    std::string filenamePNG = prefix + number + std::string(".png");
    writePng(filenamePNG.c_str(), rows, xRes, yRes, false);
    printf("Writing %s\n", filenamePNG.c_str());
   
  }
*/
  /////////////////////////////////////////////////////////////////////////////////
  // export pbrt volumegrid geometry object
  /////////////////////////////////////////////////////////////////////////////////
	/*
  static void dumpPBRT(int counter, std::string prefix, float* fieldOrg, int xRes, int yRes, int zRes)
  {
    char buffer[256];
    sprintf(buffer,"%04i", counter);
    std::string number = std::string(buffer);

    std::string filenamePbrt = prefix + number + std::string(".pbrt.gz");
    printf("Writing PBRT %s\n", filenamePbrt.c_str());

    float *field = new float[xRes*yRes*zRes];
    // normalize values
    float maxDensVal = ABS(fieldOrg[0]);
    float targetNorm = 0.5;
    for (int i = 0; i < xRes * yRes * zRes; i++) {
      if(ABS(fieldOrg[i])>maxDensVal) maxDensVal = ABS(fieldOrg[i]);
      field[i] = 0.;
    }
    if(maxDensVal>0.) {
      for (int i = 0; i < xRes * yRes * zRes; i++) {
        field[i] = ABS(fieldOrg[i]) / maxDensVal * targetNorm;
      }
    }

    std::fstream fout;
    fout.open(filenamePbrt.c_str(), std::ios::out);

    int maxRes = (xRes > yRes) ? xRes : yRes;
    maxRes = (maxRes > zRes) ? maxRes : zRes;

    const float xSize = 1.0 / (float)maxRes * (float)xRes;
    const float ySize = 1.0 / (float)maxRes * (float)yRes;
    const float zSize = 1.0 / (float)maxRes * (float)zRes;

    gzFile file;
    file = gzopen(filenamePbrt.c_str(), "wb1");
    if (file == NULL) {
      std::cerr << " Couldn't write file " << filenamePbrt << "!!!" << std::endl;
      return;
    }

    // dimensions
    gzprintf(file, "Volume \"volumegrid\" \n");
    gzprintf(file, " \"integer nx\" %i\n", xRes);
    gzprintf(file, " \"integer ny\" %i\n", yRes);
    gzprintf(file, " \"integer nz\" %i\n", zRes);
    gzprintf(file, " \"point p0\" [ 0.0 0.0 0.0 ] \"point p1\" [%f %f %f ] \n", xSize, ySize, zSize);
    gzprintf(file, " \"float density\" [ \n");
    for (int i = 0; i < xRes * yRes * zRes; i++)
      gzprintf(file, "%f ", field[i]);
    gzprintf(file, "] \n \n");

    gzclose(file);
    delete[] field;
  }
  */

  /////////////////////////////////////////////////////////////////////////////////
  // 3D df3 export
  /////////////////////////////////////////////////////////////////////////////////
/*
  static void dumpDF3(int counter, std::string prefix, float* fieldOrg, int xRes, int yRes, int zRes)
  {
    char buffer[256];

    // do deferred copying to final directory, better for network directories
    sprintf(buffer,"%04i", counter);
    std::string number = std::string(buffer);
    std::string filenameDf3 = prefix + number + std::string(".df3.gz");
    printf("Writing DF3 %s\n", filenameDf3.c_str());

    gzFile file;
    file = gzopen(filenameDf3.c_str(), "wb1");
    if (file == NULL) {
      std::cerr << " Couldn't write file " << filenameDf3 << "!!!" << std::endl;
      return;
    }

    // dimensions
    const int byteSize = 2;
    const unsigned short int onx=xRes,ony=yRes,onz=zRes;
    unsigned short int nx,ny,nz;
    nx = onx >> 8;
    ny = ony >> 8;
    nz = onz >> 8;
    nx += (onx << 8);
    ny += (ony << 8);
    nz += (onz << 8);
    gzwrite(file, (void*)&nx, sizeof(short));
    gzwrite(file, (void*)&ny, sizeof(short));
    gzwrite(file, (void*)&nz, sizeof(short));
    const int nitems = onx*ony*onz;
    const float mul = (float)( (1<<(8*byteSize))-1);

    unsigned short int *buf = new unsigned short int[nitems];
    for (int k = 0; k < onz; k++)
      for (int j = 0; j < ony; j++)
        for (int i = 0; i < onx; i++) {
          float val = fieldOrg[k*(onx*ony)+j*onx+i] ;
          CLAMP(val);
          buf[k*(onx*ony)+j*onx+i] = (short int)(val*mul);
        }
    gzwrite(file, (void*)buf, sizeof(unsigned short int)* nitems);

    gzclose(file);
    delete[] buf;
  }
  */

};


#endif

