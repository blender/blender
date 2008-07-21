//
//  Filename         : NodeTransform.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to represent a transform node. A Transform node 
//                     contains one or several children, all affected by the 
//                     transformation.
//  Date of creation : 06/02/2002
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

#ifndef  NODETRANSFORM_H
# define NODETRANSFORM_H

# include "../geometry/Geom.h"
# include "../system/FreestyleConfig.h"
# include "NodeGroup.h"

using namespace Geometry;

class LIB_SCENE_GRAPH_EXPORT NodeTransform : public NodeGroup
{
public:
  
  inline NodeTransform() : NodeGroup() {
    _Matrix = Matrix44r::identity();
    _Scaled=false;
  }

  virtual ~NodeTransform() {}

  /*! multiplys the current matrix by the 
   * x, y, z translation matrix.
   */
  void Translate(real x, real y, real z);

  /*! multiplys the current matrix by a 
   *  rotation matrix
   *    iAngle
   *      The rotation angle
   *    x, y, z
   *      The rotation axis
   */
  void Rotate(real iAngle, real x, real y, real z);

  /*! multiplys the current matrix by a 
   *  scaling matrix.
   *    x, y, z
   *      The scaling coefficients
   *      with respect to the x,y,z axis
   */
  void Scale(real x, real y, real z);

  /*! Multiplys the current matrix
   *  by iMatrix
   */
  void MultiplyMatrix(const Matrix44r &iMatrix);

  /*! Sets the current matrix to iMatrix */
  void setMatrix(const Matrix44r &iMatrix);

  /*! Accept the corresponding visitor */
  virtual void accept(SceneVisitor& v);

  /*! Overloads the Node::AddBBox in order to take care 
   *  about the transformation
   */
  virtual void AddBBox(const BBox<Vec3r>& iBBox);

  /*! Checks whether a matrix contains a scale factor 
   *  or not.
   *  Returns true if yes.
   *    iMatrix
   *      The matrix to check
   */
  bool isScaled(const Matrix44r &M);

  /*! accessors */
  inline const Matrix44r& matrix() const { return _Matrix; }
  inline bool scaled() const {return _Scaled;}

private:
  Matrix44r _Matrix;
  bool _Scaled;
};

#endif // NODETRANSFORM_H
