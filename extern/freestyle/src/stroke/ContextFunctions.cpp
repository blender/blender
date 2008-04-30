
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

#include "ContextFunctions.h"
#include "../view_map/SteerableViewMap.h"
#include "../system/TimeStamp.h"
namespace ContextFunctions {
  
  unsigned GetTimeStampCF(){
    return TimeStamp::instance()->getTimeStamp();
  }

  unsigned GetCanvasWidthCF(){
    return Canvas::getInstance()->width();
  }

  unsigned GetCanvasHeightCF(){
    return Canvas::getInstance()->height();
  }
  void LoadMapCF(const char *iFileName, const char *iMapName, unsigned iNbLevels, float iSigma ){
    return Canvas::getInstance()->loadMap(iFileName, iMapName, iNbLevels,iSigma);
  }

  float ReadMapPixelCF(const char *iMapName, int level, unsigned x, unsigned y){
    Canvas * canvas = Canvas::getInstance();
    return canvas->readMapPixel(iMapName, level, x,y);
  }

  float ReadCompleteViewMapPixelCF(int level, unsigned x, unsigned y){
	  SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
    return svm->readCompleteViewMapPixel(level,x,y);
  }

  float ReadDirectionalViewMapPixelCF(int iOrientation, int level, unsigned x, unsigned y){
    SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
    return svm->readSteerableViewMapPixel(iOrientation, level,x,y);
  }

  FEdge * GetSelectedFEdgeCF(){
    return Canvas::getInstance()->selectedFEdge();
  }
}
