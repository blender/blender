//
//  Filename         : TriangleRep.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to define the represenation of a triangle
//  Date of creation : 16/12/2002
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

#ifndef  TRIANGLEREP_H
# define TRIANGLEREP_H

//! inherits from class Rep 
#include "Rep.h"

/*! Base class for all lines objects */
class LIB_SCENE_GRAPH_EXPORT TriangleRep : public Rep
{
public:
  
  /*! Line description style */
  enum TRIANGLE_STYLE{FILL, LINES};
private:
  TRIANGLE_STYLE _Style;
  Vec3r _vertices[3];
  Vec3r _colors[3];
public:
  inline TriangleRep() : Rep() {_Style = FILL;}

  /*! Builds a triangle from 3 vertices
   *  v0
   *    first vertex
   *  v1
   *    second vertex
   *  v2
   *    third vertex
   */
  inline TriangleRep(const Vec3r& v0, const Vec3r& v1, const Vec3r& v2)
    : Rep()
  {
    _vertices[0] = v0;
    _vertices[1] = v1;
    _vertices[2] = v2;
    _Style = FILL;
  }
  inline TriangleRep( const Vec3r& v0, const Vec3r& c0,
                      const Vec3r& v1, const Vec3r& c1,
                      const Vec3r& v2, const Vec3r& c2)
    : Rep()
  {
    _vertices[0] = v0;
    _vertices[1] = v1;
    _vertices[2] = v2;
    _colors[0] = c0;
    _colors[1] = c1;
    _colors[2] = c2;
    _Style = FILL;
  }

  virtual ~TriangleRep() 
  {} 

  /*! accessors */
  inline const TRIANGLE_STYLE style() const {return _Style;}
  inline const Vec3r& vertex(int index) const {return _vertices[index];}
  inline const Vec3r& color(int index) const {return _colors[index];}
  /*! modifiers */
  inline void setStyle(const TRIANGLE_STYLE iStyle) {_Style = iStyle;}
  inline void setVertex(int index, const Vec3r& iVertex) {_vertices[index] = iVertex;}
  inline void setColor(int index, const Vec3r& iColor) {_colors[index] = iColor;}
  inline void setVertices(const Vec3r& v0, const Vec3r& v1, const Vec3r& v2) {_vertices[0] = v0; _vertices[1] = v1; _vertices[2] = v2;}
  inline void setColors(const Vec3r& c0, const Vec3r& c1, const Vec3r& c2) {_colors[0] = c0; _colors[1] = c1; _colors[2] = c2;}

  /*! Accept the corresponding visitor */
  virtual void accept(SceneVisitor& v) {
    Rep::accept(v);
    v.visitTriangleRep(*this);
  }

  /*! Computes the triangle bounding box.*/
  virtual void ComputeBBox();


};

#endif
