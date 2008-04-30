
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

# include "../view_map/Functions0D.h"
# include "AdvancedFunctions0D.h"
# include "../view_map/SteerableViewMap.h"
# include "Canvas.h"

namespace Functions0D {

  double DensityF0D::operator()(Interface0DIterator& iter) {
    Canvas* canvas = Canvas::getInstance();
    int bound = _filter.getBound();
    if( (iter->getProjectedX()-bound < 0) || (iter->getProjectedX()+bound>canvas->width())
	|| (iter->getProjectedY()-bound < 0) || (iter->getProjectedY()+bound>canvas->height()))
      return 0.0;
    RGBImage image;
    canvas->readColorPixels((int)iter->getProjectedX() - bound,
			    (int)iter->getProjectedY() - bound,
			    _filter.maskSize(),
			    _filter.maskSize(),
			    image);
    return _filter.getSmoothedPixel<RGBImage>(&image, (int)iter->getProjectedX(),
					(int)iter->getProjectedY());
  }


  double LocalAverageDepthF0D::operator()(Interface0DIterator& iter) {
    Canvas * iViewer = Canvas::getInstance();
    int bound = _filter.getBound();
    
    if( (iter->getProjectedX()-bound < 0) || (iter->getProjectedX()+bound>iViewer->width())
	|| (iter->getProjectedY()-bound < 0) || (iter->getProjectedY()+bound>iViewer->height()))
      return 0.0;
    GrayImage image ;
    iViewer->readDepthPixels((int)iter->getProjectedX()-bound,(int)iter->getProjectedY()-bound,_filter.maskSize(),_filter.maskSize(),image);
    return _filter.getSmoothedPixel(&image, (int)iter->getProjectedX(), (int)iter->getProjectedY());
  }

  float ReadMapPixelF0D::operator()(Interface0DIterator& iter) {
    Canvas * canvas = Canvas::getInstance();
    return canvas->readMapPixel(_mapName, _level, (int)iter->getProjectedX(), (int)iter->getProjectedY());
  }

  float ReadSteerableViewMapPixelF0D::operator()(Interface0DIterator& iter) {
    SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
    float v = svm->readSteerableViewMapPixel(_orientation, _level,(int)iter->getProjectedX(), (int)iter->getProjectedY());
    return v;
  }

  float ReadCompleteViewMapPixelF0D::operator()(Interface0DIterator& iter) {
    SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
    float v = svm->readCompleteViewMapPixel(_level,(int)iter->getProjectedX(), (int)iter->getProjectedY());
    return v;
  }

  float GetViewMapGradientNormF0D::operator()(Interface0DIterator& iter){
    SteerableViewMap *svm = Canvas::getInstance()->getSteerableViewMap();
    float pxy = svm->readCompleteViewMapPixel(_level,(int)iter->getProjectedX(), (int)iter->getProjectedY());
    float gx = svm->readCompleteViewMapPixel(_level,(int)iter->getProjectedX()+_step, (int)iter->getProjectedY())
      - pxy;
    float gy = svm->readCompleteViewMapPixel(_level,(int)iter->getProjectedX(), (int)iter->getProjectedY()+_step)
      - pxy;
	float f = Vec2f(gx,gy).norm();
    return f;
  }
} // end of namespace Functions0D
