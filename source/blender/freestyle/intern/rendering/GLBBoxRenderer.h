//
//  Filename         : GLBBoxRenderer.h
//  Author(s)        : Stephane Grabli
//  Purpose          : Class to render BBoxes of a 3D scene thanks to OpenGL
//  Date of creation : 07/02/2002
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

#ifndef  GLBBOXRENDERER_H
# define GLBBOXRENDERER_H

# include "../system/FreestyleConfig.h"
# include "GLRenderer.h"
# include "../geometry/BBox.h"

class Rep;
class Node;

class LIB_RENDERING_EXPORT GLBBoxRenderer : public GLRenderer
{
public:

  inline GLBBoxRenderer() : GLRenderer() {}
  virtual ~GLBBoxRenderer() {}

  VISIT_DECL(NodeGroup)
  VISIT_DECL(NodeTransform)
  VISIT_DECL(IndexedFaceSet)
  VISIT_DECL(DrawingStyle)

 protected:

  void RenderRep(const Rep& iRep) const ;
  void RenderNode(const Node& iNode) const ;
  void RenderBBox(const BBox<Vec3r>& iBox) const ;
};

#endif // GLBBOXRENDERER_H
