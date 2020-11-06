/******************************************************************************
 *
 * MantaFlow fluid solver framework
 * Copyright 2014 Tobias Pfaff, Nils Thuerey
 *
 * This program is free software, distributed under the terms of the
 * Apache License, Version 2.0
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Simple image IO
 *
 ******************************************************************************/

#include "vectorbase.h"
#include "simpleimage.h"

namespace Manta {

// write rectangle to ppm
bool SimpleImage::writePpm(
    std::string filename, int minx, int miny, int maxx, int maxy, bool invertXY)
{
  int w = maxx - minx;
  int h = maxy - miny;

  if (w <= 0 || h <= 0 || w > mSize[0] || h > mSize[1]) {
    errMsg("SimpleImage::WritePPM Invalid rect: w="
           << w << ", h=" << h << ", size=" << mSize[0] << "," << mSize[1] << " min/max: " << minx
           << "," << miny << " to " << maxx << "," << maxy << ", resetting... ");
    minx = miny = 0;
    maxx = mSize[0] - 1;
    maxy = mSize[1] - 1;
    w = mSize[0] - 1;
    h = mSize[1] - 1;
  }

  FILE *fp = fopen(filename.c_str(), "wb");
  if (fp == nullptr) {
    errMsg("SimpleImage::WritePPM Unable to open '" << filename << "' for writing");
    return false;
  }
  fprintf(fp, "P6\n%d %d\n255\n", w, h);

  int pixCnt = 0;
  for (int j = maxy - 1; j >= miny; j--)
    for (int i = minx; i < maxx; i++) {
      unsigned char col[3];
      for (int l = 0; l < 3; l++) {
        float val;
        if (invertXY)
          val = (float)get(j, i)[l];
        else
          val = (float)get(i, j)[l];

        val = clamp(val, (float)0., (float)1.);
        col[l] = (unsigned char)(255. * val);
      }
      // col[1] = col[2] = col[0];
      // if (fwrite(col,1,3, fp) != 3) errMsg("SimpleImage::writePpm fwrite failed");
      fwrite(col, 1, 3, fp);
      pixCnt++;
      // fprintf(stderr,"%d %d %d \n",col[0],i,j);
    }

  fclose(fp);
  // debMsg("WritePPM Wrote '"<<filename<<"', region="<<minx<<","<<miny<<" to
  // "<<maxx<<","<<maxy<<"; "<<pixCnt, 1);

  return true;
}

bool SimpleImage::writePpm(std::string filename)
{
  return writePpm(filename, 0, 0, getSize()[0], getSize()[1]);
}

// read in a ppm file, and init the image accordingly
bool SimpleImage::initFromPpm(std::string filename)
{
  // maximum length of a line of text
  const int MAXLINE = 1024;

  int filetype = 0;
  enum { PGM, PPM };  // possible file types

  FILE *fp;
  char line[MAXLINE];
  int size, rowsize;

  // Read in file type
  fp = fopen(filename.c_str(), "rb");
  if (!fp) {
    if (mAbortOnError)
      debMsg("SimpleImage Error - unable to open file '" << filename << "' for reading", 1);
    return 0;
  }

  // 1st line: PPM or PGM
  if (fgets(line, MAXLINE, fp) == nullptr) {
    if (mAbortOnError)
      debMsg("SimpleImage::initFromPpm fgets failed", 1);
    return 0;
  }

  if (line[1] == '5')
    filetype = PGM;
  else if (line[1] == '6')
    filetype = PPM;
  else {
    if (mAbortOnError)
      debMsg("SimpleImage Error: need PPM or PGM file as input!", 1);
    return 0;
  }

  // Read in width and height, & allocate space
  // 2nd line: width height
  if (fgets(line, MAXLINE, fp) == nullptr) {
    if (mAbortOnError)
      errMsg("SimpleImage::initFromPpm fgets failed");
    return 0;
  }
  int windW = 0, windH = 0;  // size of the window on the screen
  int intsFound = sscanf(line, "%d %d", &windW, &windH);
  if (intsFound == 1) {
    // only X found, search on next line as well for Y...
    if (sscanf(line, "%d", &windH) != 1) {
      if (mAbortOnError)
        errMsg("initFromPpm Ppm dimensions not found!" << windW << "," << windH);
      return 0;
    }
    else {
      // ok, found 2 lines
      // debMsg("initFromPpm Ppm dimensions found!"<<windW<<","<<windH, 1);
    }
  }
  else if (intsFound == 2) {
    // ok!
  }
  else {
    if (mAbortOnError)
      errMsg("initFromPpm Ppm dimensions not found at all!" << windW << "," << windH);
    return 0;
  }

  if (filetype == PGM) {
    size = windH * windW;  // greymap: 1 byte per pixel
    rowsize = windW;
  }
  else {
    // filetype == PPM
    size = windH * windW * 3;  // pixmap: 3 bytes per pixel
    rowsize = windW * 3;
  }

  unsigned char *pic = new unsigned char[size];  // (GLubyte *)malloc (size);

  // Read in maximum value (ignore) , could be scanned with sscanf as well, but this should be
  // 255... 3rd line
  if (fgets(line, MAXLINE, fp) == nullptr) {
    if (mAbortOnError)
      errMsg("SimpleImage::initFromPpm fgets failed");
    return 0;
  }

  // Read in the pixel array row-by-row: 1st row = top scanline */
  unsigned char *ptr = nullptr;
  ptr = &pic[(windH - 1) * rowsize];
  for (int i = windH; i > 0; i--) {
    assertMsg(fread((void *)ptr, 1, rowsize, fp) == rowsize,
              "SimpleImage::initFromPpm couldn't read data");
    ptr -= rowsize;
  }

  // init image
  this->init(windW, windH);
  if (filetype == PGM) {
    // grayscale
    for (int i = 0; i < windW; i++) {
      for (int j = 0; j < windH; j++) {
        double r = (double)pic[(j * windW + i) * 1 + 0] / 255.;
        (*this)(i, j) = Vec3(r, r, r);
      }
    }
  }
  else {
    // convert grid to RGB vec's
    for (int i = 0; i < windW; i++) {
      for (int j = 0; j < windH; j++) {
        // return mpData[y*mSize[0]+x];
        double r = (double)pic[(j * windW + i) * 3 + 0] / 255.;
        double g = (double)pic[(j * windW + i) * 3 + 1] / 255.;
        double b = (double)pic[(j * windW + i) * 3 + 2] / 255.;

        //(*this)(i,j) = Vec3(r,g,b);

        // RGB values have to be rotated to get the right colors!?
        // this might also be an artifact of photoshop export...?
        (*this)(i, j) = Vec3(g, b, r);
      }
    }
  }

  delete[] pic;
  fclose(fp);
  return 1;
}

// check index is valid
bool SimpleImage::indexIsValid(int i, int j)
{
  if (i < 0)
    return false;
  if (j < 0)
    return false;
  if (i >= mSize[0])
    return false;
  if (j >= mSize[1])
    return false;
  return true;
}

};  // namespace Manta

//*****************************************************************************

#include "grid.h"
namespace Manta {

// simple shaded output , note requires grid functionality!
static void gridPrecompLight(const Grid<Real> &density, Grid<Real> &L, Vec3 light = Vec3(1, 1, 1))
{
  FOR_IJK(density)
  {
    Vec3 n = getGradient(density, i, j, k) * -1.;
    normalize(n);

    Real d = dot(light, n);
    L(i, j, k) = d;
  }
}

// simple shading with pre-computed gradient
static inline void shadeCell(
    Vec3 &dst, int shadeMode, Real src, Real light, int depthPos, Real depthInv)
{
  switch (shadeMode) {

    case 1: {
      // surfaces
      Vec3 ambient = Vec3(0.1, 0.1, 0.1);
      Vec3 diffuse = Vec3(0.9, 0.9, 0.9);
      Real alpha = src;

      // different color for depth?
      diffuse[0] *= ((Real)depthPos * depthInv) * 0.7 + 0.3;
      diffuse[1] *= ((Real)depthPos * depthInv) * 0.7 + 0.3;

      Vec3 col = ambient + diffuse * light;

      // img( 0+i, j ) = (1.-alpha) * img( 0+i, j ) + alpha * col;
      dst = (1. - alpha) * dst + alpha * col;
    } break;

    default: {
      // volumetrics / smoke
      dst += depthInv * Vec3(src, src, src);
    } break;
  }
}

//! helper to project a grid intro an image (used for ppm export and GUI displauy)
void projectImg(SimpleImage &img, const Grid<Real> &val, int shadeMode = 0, Real scale = 1.)
{
  Vec3i s = val.getSize();
  Vec3 si = Vec3(1. / (Real)s[0], 1. / (Real)s[1], 1. / (Real)s[2]);

  // init image size
  int imgSx = s[0];
  if (val.is3D())
    imgSx += s[2] + s[0];  // mult views in 3D
  img.init(imgSx, std::max(s[0], std::max(s[1], s[2])));

  // precompute lighting
  Grid<Real> L(val);
  gridPrecompLight(val, L, Vec3(1, 1, 1));

  FOR_IJK(val)
  {
    Vec3i idx(i, j, k);
    shadeCell(img(0 + i, j), shadeMode, val(idx), L(idx), k, si[2]);
  }

  if (val.is3D()) {

    FOR_IJK(val)
    {
      Vec3i idx(i, j, k);
      shadeCell(img(s[0] + k, j), shadeMode, val(idx), L(idx), i, si[0]);
    }

    FOR_IJK(val)
    {
      Vec3i idx(i, j, k);
      shadeCell(img(s[0] + s[2] + i, k), shadeMode, val(idx), L(idx), j, si[1]);
    }

  }  // 3d

  img.mapRange(1. / scale);
}

};  // namespace Manta
