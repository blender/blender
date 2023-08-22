/* SPDX-FileCopyrightText: 2008-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup freestyle
 * \brief Class to represent a pyramid of images
 */

#include <iostream>

#include "GaussianFilter.h"
#include "Image.h"
#include "ImagePyramid.h"

#include "BLI_sys_types.h"

using namespace std;

namespace Freestyle {

#if 0
ImagePyramid::ImagePyramid(const GrayImage &level0, uint nbLevels)
{
  BuildPyramid(level0, nbLevels);
}
#endif

ImagePyramid::ImagePyramid(const ImagePyramid & /*iBrother*/)
{
  if (!_levels.empty()) {
    for (vector<GrayImage *>::iterator im = _levels.begin(), imend = _levels.end(); im != imend;
         ++im) {
      _levels.push_back(new GrayImage(**im));
    }
  }
}

ImagePyramid::~ImagePyramid()
{
  if (!_levels.empty()) {
    for (vector<GrayImage *>::iterator im = _levels.begin(), imend = _levels.end(); im != imend;
         ++im) {
      delete (*im);
    }
    _levels.clear();
  }
}

GrayImage *ImagePyramid::getLevel(int l)
{
  return _levels[l];
}

float ImagePyramid::pixel(int x, int y, int level)
{
  GrayImage *img = _levels[level];
  if (0 == level) {
    return img->pixel(x, y);
  }
  uint i = 1 << level;
  uint sx = x >> level;
  uint sy = y >> level;
  if (sx >= img->width()) {
    sx = img->width() - 1;
  }
  if (sy >= img->height()) {
    sy = img->height() - 1;
  }

  // bilinear interpolation
  float A = i * (sx + 1) - x;
  float B = x - i * sx;
  float C = i * (sy + 1) - y;
  float D = y - i * sy;

  float P1(0), P2(0);
  P1 = A * img->pixel(sx, sy);
  if (sx < img->width() - 1) {
    if (x % i != 0) {
      P1 += B * img->pixel(sx + 1, sy);
    }
  }
  else {
    P1 += B * img->pixel(sx, sy);
  }
  if (sy < img->height() - 1) {
    if (y % i != 0) {
      P2 = A * img->pixel(sx, sy + 1);
      if (sx < img->width() - 1) {
        if (x % i != 0) {
          P2 += B * img->pixel(sx + 1, sy + 1);
        }
      }
      else {
        P2 += B * img->pixel(sx, sy + 1);
      }
    }
  }
  else {
    P2 = P1;
  }
  return (1.0f / float(1 << (2 * level))) * (C * P1 + D * P2);
}

int ImagePyramid::width(int level)
{
  return _levels[level]->width();
}

int ImagePyramid::height(int level)
{
  return _levels[level]->height();
}

GaussianPyramid::GaussianPyramid(const GrayImage &level0, uint nbLevels, float iSigma)
{
  _sigma = iSigma;
  BuildPyramid(level0, nbLevels);
}

GaussianPyramid::GaussianPyramid(GrayImage *level0, uint nbLevels, float iSigma)
{
  _sigma = iSigma;
  BuildPyramid(level0, nbLevels);
}

GaussianPyramid::GaussianPyramid(const GaussianPyramid &iBrother) : ImagePyramid(iBrother)
{
  _sigma = iBrother._sigma;
}

void GaussianPyramid::BuildPyramid(const GrayImage &level0, uint nbLevels)
{
  GrayImage *pLevel = new GrayImage(level0);
  BuildPyramid(pLevel, nbLevels);
}

void GaussianPyramid::BuildPyramid(GrayImage *level0, uint nbLevels)
{
  GrayImage *pLevel = level0;
  _levels.push_back(pLevel);
  GaussianFilter gf(_sigma);
  // build the nbLevels:
  uint w = pLevel->width();
  uint h = pLevel->height();
  if (nbLevels != 0) {
    for (uint i = 0; i < nbLevels; ++i) {  // soc
      w = pLevel->width() >> 1;
      h = pLevel->height() >> 1;
      GrayImage *img = new GrayImage(w, h);
      for (uint y = 0; y < h; ++y) {
        for (uint x = 0; x < w; ++x) {
          float v = gf.getSmoothedPixel<GrayImage>(pLevel, 2 * x, 2 * y);
          img->setPixel(x, y, v);
        }
      }
      _levels.push_back(img);
      pLevel = img;
    }
  }
  else {
    while ((w > 1) && (h > 1)) {
      w = pLevel->width() >> 1;
      h = pLevel->height() >> 1;
      GrayImage *img = new GrayImage(w, h);
      for (uint y = 0; y < h; ++y) {
        for (uint x = 0; x < w; ++x) {
          float v = gf.getSmoothedPixel<GrayImage>(pLevel, 2 * x, 2 * y);
          img->setPixel(x, y, v);
        }
      }
      _levels.push_back(img);
      pLevel = img;
    }
  }
}

} /* namespace Freestyle */
