/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief Convenient access to the steerable ViewMap to which any element of the ViewMap belongs
 * to.
 */

#include <map>

#include "../geometry/Geom.h"

#include "../system/FreestyleConfig.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

using namespace Geometry;

class FEdge;
class ImagePyramid;
class GrayImage;

/** This class checks for every FEdge in which steerable it belongs and stores the mapping
 *  allowing to retrieve this information from the FEdge Id.
 */
class SteerableViewMap {
 protected:
  // for each vector the list of nbOrientations weights corresponding to its contributions
  // to the nbOrientations directional maps
  map<uint, double *> _mapping;
  uint _nbOrientations;
  ImagePyramid **_imagesPyramids;  // the pyramids of images storing the different SVM

  // internal
  double _bound;  // cos(Pi/N)
  vector<Vec2d> _directions;

 public:
  SteerableViewMap(uint nbOrientations = 4);
  SteerableViewMap(const SteerableViewMap &iBrother);
  virtual ~SteerableViewMap();

  /** Resets everything */
  virtual void Reset();

  /** Adds a FEdge to steerable VM.
   *  Returns the nbOrientations weights corresponding to the FEdge contributions to the
   *  nbOrientations directional maps.
   */
  double *AddFEdge(FEdge *iFEdge);

  /** Compute the weight of direction dir for orientation iNOrientation */
  double ComputeWeight(const Vec2d &dir, uint iNOrientation);

  /** Returns the number of the SVM to which a direction belongs to.
   *  \param dir:
   *    The direction
   */
  uint getSVMNumber(Vec2f dir);

  /** Returns the number of the SVM to which a FEdge belongs most.
   *  \param id:
   *    The First element of the Id struct of the FEdge we're interested in.
   */
  uint getSVMNumber(uint id);

  /** Builds _nbOrientations+1 pyramids of images from the _nbOrientations+1 base images of the
   *  steerable viewmap.
   *  \param steerableBases:
   *    The _nbOrientations+1 images constituting the basis for the steerable pyramid.
   *  \param copy:
   *    If false, the data is not duplicated, and Canvas deals with the memory management of these
   *    _nbOrientations+1 images. If true, data is copied, and it's up to the caller to delete
   *    the images.
   *  \param iNbLevels:
   *    The number of levels desired for each pyramid.
   *    If iNbLevels == 0, the complete pyramid is built.
   *  \param iSigma:
   *    The sigma that will be used for the gaussian blur
   */
  void buildImagesPyramids(GrayImage **steerableBases,
                           bool copy = false,
                           uint iNbLevels = 4,
                           float iSigma = 1.0f);

  /** Reads a pixel value in one of the VewMap density steerable pyramids.
   *  Returns a value between 0 and 1.
   *  \param iOrientation:
   *    the number telling which orientation we need to check.
   *    There are _nbOrientations+1 oriented ViewMaps:
   *    0 -> the ViewMap containing every horizontal lines
   *    1 -> the ViewMap containing every lines whose orientation is around PI/4
   *    2 -> the ViewMap containing every vertical lines
   *    3 -> the ViewMap containing every lines whose orientation is around 3PI/4
   *    4 -> the complete ViewMap
   *  \param iLevel:
   *    The level of the pyramid we want to read
   *  \param x:
   *    The abscissa of the desired pixel specified in level0 coordinate system.
   *    The origin is the lower left corner.
   *  \param y:
   *    The ordinate of the desired pixel specified in level0 coordinate system.
   *    The origin is the lower left corner.
   */
  float readSteerableViewMapPixel(uint iOrientation, int iLevel, int x, int y);

  /** Reads a pixel in the one of the level of the pyramid containing the images
   *  of the complete ViewMap.
   *  Returns a value between 0 and 1.
   *  Equivalent to : readSteerableViewMapPixel(nbOrientations, x, y)
   */
  float readCompleteViewMapPixel(int iLevel, int x, int y);

  /** Returns the number of levels in the pyramids */
  uint getNumberOfPyramidLevels() const;

  /** Returns the number of orientations */
  uint getNumberOfOrientations() const
  {
    return _nbOrientations;
  }

  /** for debug purposes */
  void saveSteerableViewMap() const;

 protected:
  void Clear();
  void Build();

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:SteerableViewMap")
#endif
};

} /* namespace Freestyle */
