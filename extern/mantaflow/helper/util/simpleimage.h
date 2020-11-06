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

#ifndef MANTA_SIMPLEIMAGE_H
#define MANTA_SIMPLEIMAGE_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "manta.h"
#include "vectorbase.h"

namespace Manta {

//*****************************************************************************
// simple 2d image class
// template<class Scalar>
class SimpleImage {
 public:
  // cons/des
  SimpleImage() : mSize(-1), mpData(nullptr), mAbortOnError(true){};
  virtual ~SimpleImage()
  {
    if (mpData)
      delete[] mpData;
  };

  //! set to constant
  void reset(Real val = 0.)
  {
    const Vec3 v = Vec3(val);
    for (int i = 0; i < mSize[0] * mSize[1]; i++)
      mpData[i] = v;
  }
  //! init memory & reset to zero
  void init(int x, int y)
  {
    mSize = Vec3i(x, y, 0);
    mpData = new Vec3[x * y];
    reset();
  };

  inline bool checkIndex(int x, int y)
  {
    if ((x < 0) || (y < 0) || (x > mSize[0] - 1) || (y > mSize[1] - 1)) {
      errMsg("SimpleImage::operator() Invalid access to " << x << "," << y << ", size=" << mSize);
      return false;
    }
    return true;
  }

  // access element
  inline Vec3 &operator()(int x, int y)
  {
    DEBUG_ONLY(checkIndex(x, y));
    return mpData[y * mSize[0] + x];
  };
  inline Vec3 &get(int x, int y)
  {
    return (*this)(x, y);
  }
  inline Vec3 &getMap(int x, int y, int z, int axis)
  {
    int i = x, j = y;
    if (axis == 1)
      j = z;
    if (axis == 0) {
      i = y;
      j = z;
    }
    return get(i, j);
  }

  // output as string, debug
  std::string toString()
  {
    std::ostringstream out;

    for (int j = 0; j < mSize[1]; j++) {
      for (int i = 0; i < mSize[0]; i++) {
        // normal zyx order */
        out << (*this)(i, j);
        out << " ";
      }
      // if (format)
      out << std::endl;
    }

    return out.str();
  }

  // multiply all values by f
  void add(Vec3 f)
  {
    for (int j = 0; j < mSize[1]; j++)
      for (int i = 0; i < mSize[0]; i++) {
        get(i, j) += f;
      }
  }
  // multiply all values by f
  void multiply(Real f)
  {
    for (int j = 0; j < mSize[1]; j++)
      for (int i = 0; i < mSize[0]; i++) {
        get(i, j) *= f;
      }
  }
  // map 0-f to 0-1 range, clamp
  void mapRange(Real f)
  {
    for (int j = 0; j < mSize[1]; j++)
      for (int i = 0; i < mSize[0]; i++) {
        get(i, j) /= f;
        for (int c = 0; c < 3; ++c)
          get(i, j)[c] = clamp(get(i, j)[c], (Real)0., (Real)1.);
      }
  }

  // normalize max values
  void normalizeMax()
  {
    Real max = normSquare(get(0, 0));
    for (int j = 0; j < mSize[1]; j++)
      for (int i = 0; i < mSize[0]; i++) {
        if (normSquare(get(i, j)) > max)
          max = normSquare(get(i, j));
      }
    max = sqrt(max);
    Real invMax = 1. / max;
    for (int j = 0; j < mSize[1]; j++)
      for (int i = 0; i < mSize[0]; i++) {
        get(i, j) *= invMax;
      }
  };

  // normalize min and max values
  void normalizeMinMax()
  {
    Real max = normSquare(get(0, 0));
    Real min = max;
    for (int j = 0; j < mSize[1]; j++)
      for (int i = 0; i < mSize[0]; i++) {
        if (normSquare(get(i, j)) > max)
          max = normSquare(get(i, j));
        if (normSquare(get(i, j)) < min)
          min = normSquare(get(i, j));
      }
    max = sqrt(max);
    min = sqrt(min);
    Real factor = 1. / (max - min);
    for (int j = 0; j < mSize[1]; j++)
      for (int i = 0; i < mSize[0]; i++) {
        get(i, j) -= min;
        get(i, j) *= factor;
      }
  };

  void setAbortOnError(bool set)
  {
    mAbortOnError = set;
  }

  // ppm in/output

  // write whole image
  bool writePpm(std::string filename);
  // write rectangle to ppm
  bool writePpm(
      std::string filename, int minx, int miny, int maxx, int maxy, bool invertXY = false);
  // read in a ppm file, and init the image accordingly
  bool initFromPpm(std::string filename);

  // check index is valid
  bool indexIsValid(int i, int j);

  //! access
  inline Vec3i getSize() const
  {
    return mSize;
  }

 protected:
  //! size
  Vec3i mSize;
  //! data
  Vec3 *mpData;
  // make errors fatal, or continue?
  bool mAbortOnError;

};  // SimpleImage

};  // namespace Manta

#endif
