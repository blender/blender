/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to perform gaussian filtering operations on an image
 */

#include <cstdlib>   // for abs
#include <string.h>  // for memcpy

#include "../system/FreestyleConfig.h"

#include "BLI_math.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class GaussianFilter {
 protected:
  /* The mask is a symmetrical 2d array (with respect to the middle point).
   * Thus: `M(i,j) = M(-i,j) = M(i,-j) = M(-i,-j)`.
   * For this reason, to represent a NxN array (N odd),
   * we only store a `((N+1)/2)x((N+1)/2)` array. */

  /** The sigma value of the gaussian function. */
  float _sigma;
  float *_mask;
  int _bound;
  /* the real mask size (must be odd)(the size of the mask we store is:
   * `((_maskSize+1)/2)*((_maskSize+1)/2))`. */
  int _maskSize;
  int _storedMaskSize;  // (_maskSize+1)/2)

 public:
  GaussianFilter(float iSigma = 1.0f);
  GaussianFilter(const GaussianFilter &);
  GaussianFilter &operator=(const GaussianFilter &);
  virtual ~GaussianFilter();

  /** Returns the value for pixel x,y of image "map" after a gaussian blur, made using the sigma
   * value. The sigma value determines the mask size (~ 2 x sigma).
   * \param map: The image we wish to work on.
   * The Map template must implement the following methods:
   * - float pixel(unsigned int x,unsigned int y) const;
   * - unsigned width() const;
   * - unsigned height() const;
   *  \param x:
   *    The abscissa of the pixel where we want to evaluate the gaussian blur.
   *  \param y:
   *    The ordinate of the pixel where we want to evaluate the gaussian blur.
   */
  template<class Map> float getSmoothedPixel(Map *map, int x, int y);

  /** Compute the mask size and returns the REAL mask size ((2*_maskSize)-1)
   *  This method is provided for convenience.
   */
  static int computeMaskSize(float sigma);

  /** accessors */
  inline float sigma() const
  {
    return _sigma;
  }

  inline int maskSize() const
  {
    return _maskSize;
  }

  inline int getBound()
  {
    return _bound;
  }

  /** modifiers */
  void setSigma(float sigma);
#if 0
  void SetMaskSize(int size)
  {
    _maskSize = size;
    _storedMaskSize = (_maskSize + 1) >> 1;
  }
#endif

 protected:
  void computeMask();

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:GaussianFilter")
#endif
};

/*
 * #############################################
 * #############################################
 * #############################################
 * ######                                 ######
 * ######   I M P L E M E N T A T I O N   ######
 * ######                                 ######
 * #############################################
 * #############################################
 * #############################################
 */

template<class Map> float GaussianFilter::getSmoothedPixel(Map *map, int x, int y)
{
  // float sum = 0.0f;
  float L = 0.0f;
  int w = (int)map->width();   // soc
  int h = (int)map->height();  // soc

  // Current pixel is x,y
  // Sum surrounding pixels L value:
  for (int i = -_bound; i <= _bound; ++i) {
    if ((y + i < 0) || (y + i >= h)) {
      continue;
    }
    for (int j = -_bound; j <= _bound; ++j) {
      if ((x + j < 0) || (x + j >= w)) {
        continue;
      }

      float tmpL = map->pixel(x + j, y + i);
      float m = _mask[abs(i) * _storedMaskSize + abs(j)];
      L += m * tmpL;
      // sum += m;
    }
  }
  // L /= sum;
  return L;
}

} /* namespace Freestyle */
