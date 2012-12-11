
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

#include "LineRep.h"

void LineRep::ComputeBBox()
{
  real XMax = _vertices.front()[0];
  real YMax = _vertices.front()[1];
  real ZMax = _vertices.front()[2];

  real XMin = _vertices.front()[0];
  real YMin = _vertices.front()[1];
  real ZMin = _vertices.front()[2];

  // parse all the coordinates to find 
  // the XMax, YMax, ZMax
  vector<Vec3r>::iterator v;
  for(v=_vertices.begin(); v!=_vertices.end(); v++) {
    // X
    if((*v)[0] > XMax)
      XMax = (*v)[0];
    if((*v)[0] < XMin)
      XMin = (*v)[0];

    // Y
    if((*v)[1] > YMax)
      YMax = (*v)[1];
    if((*v)[1] < YMin)
      YMin = (*v)[1];

    // Z
    if((*v)[2] > ZMax)
      ZMax = (*v)[2];
    if((*v)[2] < ZMin)
      ZMin = (*v)[2];
  }

  setBBox(BBox<Vec3r>(Vec3r(XMin, YMin, ZMin), Vec3r(XMax, YMax, ZMax)));
}
