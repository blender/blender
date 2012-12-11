//
//  Filename         : VertexRep.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to define the representation of a vertex for
//                     displaying purpose.
//  Date of creation : 03/04/2002
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

#ifndef  VERTEXREP_H
# define VERTEXREP_H

#include "Rep.h"

class LIB_SCENE_GRAPH_EXPORT VertexRep : public Rep
{
public:
  
  inline VertexRep() : Rep() {_vid = 0;_PointSize = 0.f;}
  inline VertexRep(real x, real y, real z, int id = 0) 
    : Rep()
  {
    _coordinates[0] = x;
    _coordinates[1] = y;
    _coordinates[2] = z;

    _vid = id;
    _PointSize = 0.f;
  }

  inline ~VertexRep() {}

  /*! Accept the corresponding visitor */

  virtual void accept(SceneVisitor& v) {
    Rep::accept(v);
    v.visitVertexRep(*this);
  }

  /*! Computes the rep bounding box.
   */
  virtual void ComputeBBox();

  /*! accessors */
  inline const int vid() const {return _vid;}
  inline const real * coordinates() const {return _coordinates;}
  inline real x() const {return _coordinates[0];}
  inline real y() const {return _coordinates[1];}
  inline real z() const {return _coordinates[2];}
  inline float pointSize() const {return _PointSize;}

  /*! modifiers */
  inline void setVid(int id) {_vid = id;}
  inline void setX(real x) {_coordinates[0] = x;}
  inline void setY(real y) {_coordinates[1] = y;}
  inline void setZ(real z) {_coordinates[2] = z;}
  inline void setCoordinates(real x, real y, real z) {_coordinates[0] = x;_coordinates[1] = y; _coordinates[2] = z;}
  inline void setPointSize(float iPointSize) {_PointSize = iPointSize;}

private:
  int _vid; // vertex id
  real _coordinates[3];
  float _PointSize;
};

#endif // VERTEXREP_H
