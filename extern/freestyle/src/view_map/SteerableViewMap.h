//
//  Filename         : SteerbaleViewMap.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Convenient access to the steerable ViewMap
//                     to which any element of the ViewMap belongs to.
//  Date of creation : 01/07/2003
//
///////////////////////////////////////////////////////////////////////////////


//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////
#ifndef  STEERABLEVIEWMAP_H
# define STEERABLEVIEWMAP_H
 
#include <map>
#include "../system/FreestyleConfig.h"
#include "../geometry/Geom.h"
using namespace Geometry;

using namespace std;

class FEdge;
class ImagePyramid;
class GrayImage;
/*! This class checks for every FEdge in which steerable 
 *  it belongs and stores the mapping allowing to retrieve 
 *  this information from the FEdge Id 
 */
class LIB_VIEW_MAP_EXPORT SteerableViewMap{
protected:
  map<unsigned int, double* > _mapping; // for each vector the list of nbOrientations weigths corresponding to its contributions to the nbOrientations directional maps
  unsigned _nbOrientations;
  ImagePyramid **_imagesPyramids; // the pyramids of images storing the different SVM

  // internal
  double _bound; // cos(Pi/N)
  vector<Vec2d> _directions;

public:
  SteerableViewMap(unsigned int nbOrientations = 4);
  SteerableViewMap(const SteerableViewMap& iBrother);
  virtual ~SteerableViewMap();

  /*! Resets everything */
  virtual void Reset();

  /*! Adds a FEdge to steerable VM.
   *  Returns the nbOrientations weigths corresponding to 
   *  the FEdge contributions to the nbOrientations directional maps.
   */
  double* AddFEdge(FEdge *iFEdge);

  /*! Compute the weight of direction dir for orientation iNOrientation */
  double ComputeWeight(const Vec2d& dir, unsigned iNOrientation);

  /*! Returns the number of the SVM to which a direction belongs 
   *  to.
   *  \param dir
   *    The direction
   */
  unsigned getSVMNumber(const Vec2f& dir);

  /*! Returns the number of the SVM to which a FEdge belongs 
   *  most.
   *  \param id
   *    The First element of the Id struct of the FEdge 
   *    we're intersted in.
   */
  unsigned getSVMNumber(unsigned id);

  /*! Builds _nbOrientations+1 pyramids of images from the _nbOrientations+1 base images 
   *  of the steerable viewmap.
   *  \param steerableBases
   *    The _nbOrientations+1 images constituing the basis for the steerable
   *    pyramid.
   *  \param copy
   *    If false, the data is not duplicated, and Canvas deals
   *    with the memory management of these _nbOrientations+1 images. If true, data 
   *    is copied, and it's up to the caller to delete the images.
   *  \params iNbLevels
   *    The number of levels desired for each pyramid.
   *    If iNbLevels == 0, the complete pyramid is built.
   *  \param iSigma
   *    The sigma that will be used for the gaussian blur
   */
  void buildImagesPyramids(GrayImage **steerableBases, bool copy = false, unsigned iNbLevels=4, float iSigma = 1.f);

  /*! Reads a pixel value in one of the VewMap density steerable pyramids.
   *  Returns a value between 0 and 1.
   *  \param iOrientation
   *    the number telling which orientation we need to check.
   *    There are _nbOrientations+1 oriented ViewMaps:
   *    0 -> the ViewMap containing every horizontal lines
   *    1 -> the ViewMap containing every lines whose orientation is around PI/4
   *    2 -> the ViewMap containing every vertical lines
   *    3 -> the ViewMap containing every lines whose orientation is around 3PI/4
   *    4 -> the complete ViewMap
   *  \param iLevel
   *    The level of the pyramid we want to read
   *  \param x
   *    The abscissa of the desired pixel specified in level0 coordinate 
   *    system. The origin is the lower left corner.
   *  \param y
   *    The ordinate of the desired pixel specified in level0 coordinate 
   *    system. The origin is the lower left corner.
   */
  float readSteerableViewMapPixel(unsigned iOrientation, int iLevel, int x, int y);

  /*! Reads a pixel in the one of the level of the 
   *  pyramid containing the images of the complete 
   *  ViewMap.
   *  Returns a value between 0 and 1.
   *  Equivalent to : readSteerableViewMapPixel(nbOrientations, x,y)
   */
  float readCompleteViewMapPixel(int iLevel, int x, int y);

  /*! Returns the number of levels in the pyramids */
  unsigned int getNumberOfPyramidLevels() const;
  
  /*! Returns the number of orientations */
  unsigned int getNumberOfOrientations() const{
    return _nbOrientations;
  }

  /*! for debug purposes */
  void saveSteerableViewMap() const ;

protected:
  void Clear();
  void Build();


};

#endif // STEERABLEVIEWMAP_H
