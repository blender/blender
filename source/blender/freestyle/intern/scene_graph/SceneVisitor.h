//
//  Filename         : SceneVisitor.h
//  Author(s)        : Emmanuel Turquin
//  Purpose          : Class to visit (without doing anything)
//                     a scene graph structure
//  Date of creation : 26/04/2003
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

#ifndef  SCENE_VISITOR_H
# define SCENE_VISITOR_H

# define VISIT_COMPLETE_DEF(type)		\
virtual void visit##type(type&) {}		\
virtual void visit##type##Before(type&) {}	\
virtual void visit##type##After(type&) {}

# define VISIT_DECL(type) virtual void visit##type(type&);

# define VISIT_COMPLETE_DECL(type) \
    virtual void visit##type##Before(type&); \
    virtual void visit##type(type&); \
    virtual void visit##type##After(type&); 

#include "../system/FreestyleConfig.h"

class Node;
class NodeShape;
class NodeGroup;
class NodeLight;
class NodeCamera;
class NodeDrawingStyle;
class NodeTransform;

class Rep;
class LineRep;
class OrientedLineRep;
class TriangleRep;
class VertexRep;
class IndexedFaceSet;
class DrawingStyle;
class FrsMaterial;

class LIB_SCENE_GRAPH_EXPORT SceneVisitor
{
public:
  
  SceneVisitor() {}
  virtual ~SceneVisitor() {}

  virtual void beginScene() {}
  virtual void endScene() {}

  //
  // visitClass methods
  //
  //////////////////////////////////////////////

  VISIT_COMPLETE_DEF(Node)
  VISIT_COMPLETE_DEF(NodeShape)
  VISIT_COMPLETE_DEF(NodeGroup)
  VISIT_COMPLETE_DEF(NodeLight)
  VISIT_COMPLETE_DEF(NodeCamera)
  VISIT_COMPLETE_DEF(NodeDrawingStyle)
  VISIT_COMPLETE_DEF(NodeTransform)

  VISIT_COMPLETE_DEF(Rep)
  VISIT_COMPLETE_DEF(LineRep)
  VISIT_COMPLETE_DEF(OrientedLineRep)
  VISIT_COMPLETE_DEF(TriangleRep)
  VISIT_COMPLETE_DEF(VertexRep)
  VISIT_COMPLETE_DEF(IndexedFaceSet)
  VISIT_COMPLETE_DEF(DrawingStyle)
  VISIT_COMPLETE_DEF(FrsMaterial)
};

#endif // SCENEVISITOR_H
