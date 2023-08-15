/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Class to represent a pyramid of images
 */

#include <vector>

#include "../system/FreestyleConfig.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class GrayImage;

class ImagePyramid {
 protected:
  std::vector<GrayImage *> _levels;

 public:
  ImagePyramid() {}
  ImagePyramid(const ImagePyramid &iBrother);
  // ImagePyramid(const GrayImage& level0, uint nbLevels);
  virtual ~ImagePyramid();

  /** Builds the pyramid.
   * must be overloaded by inherited classes.
   * if nbLevels==0, the complete pyramid is built
   */
  virtual void BuildPyramid(const GrayImage &level0, uint nbLevels) = 0;

  /** Builds a pyramid without copying the base level */
  virtual void BuildPyramid(GrayImage *level0, uint nbLevels) = 0;

  virtual GrayImage *getLevel(int l);
  /** Returns the pixel x,y using bilinear interpolation.
   *  \param x:
   *    the abscissa specified in the finest level coordinate system
   *  \param y:
   *    the ordinate specified in the finest level coordinate system
   *  \param level:
   *    the level from which we want the pixel to be evaluated
   */
  virtual float pixel(int x, int y, int level = 0);

  /** Returns the width of the level-th level image */
  virtual int width(int level = 0);

  /** Returns the height of the level-th level image */
  virtual int height(int level = 0);

  /** Returns the number of levels in the pyramid */
  inline int getNumberOfLevels() const
  {
    return _levels.size();
  }

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:ImagePyramid")
#endif
};

class GaussianPyramid : public ImagePyramid {
 protected:
  float _sigma;

 public:
  GaussianPyramid(float iSigma = 1.0f) : ImagePyramid()
  {
    _sigma = iSigma;
  }

  GaussianPyramid(const GrayImage &level0, uint nbLevels, float iSigma = 1.0f);
  GaussianPyramid(GrayImage *level0, uint nbLevels, float iSigma = 1.0f);
  GaussianPyramid(const GaussianPyramid &iBrother);
  virtual ~GaussianPyramid() {}

  virtual void BuildPyramid(const GrayImage &level0, uint nbLevels);
  virtual void BuildPyramid(GrayImage *level0, uint nbLevels);

  /* accessors */
  inline float getSigma() const
  {
    return _sigma;
  }

  /* modifiers */
};

} /* namespace Freestyle */
