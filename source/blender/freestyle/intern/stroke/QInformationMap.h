//
//  Filename         : QInformationMap.h
//  Author           : Stephane Grabli
//  Purpose          : Class defining an information map using a QImage
//  Date of creation : 04/01/2003
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

#ifndef  QINFORMATIONMAP_H
# define QINFORMATIONMAP_H

# include <qimage.h>
# include "InformationMap.h"

class QInformationMap : public InformationMap
{
private:
  QImage _map; // the image or a piece of image

public:
  QInformationMap();
  QInformationMap(const QImage&);
  QInformationMap(const QInformationMap&);
  QInformationMap& operator=(const QInformationMap&);

  //float getSmoothedPixel(int x, int y, float sigma = 0.2f)
  virtual float getMean(int x, int y) ;
  virtual void retrieveMeanAndVariance(int x, int y, float &oMean, float &oVariance) ;

  inline const QImage& map() const {return _map;}
  inline void setMap(const QImage& iMap, float iw, float ih) {_map = iMap.copy();_w=iw;_h=ih;}

protected:
  virtual float computeGaussian(int x, int y);
};

#endif // QINFORMATIONMAP_H
