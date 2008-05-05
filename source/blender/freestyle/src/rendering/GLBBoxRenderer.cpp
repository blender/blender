
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

#include "GLBBoxRenderer.h"
#include "../scene_graph/IndexedFaceSet.h"
#include "../scene_graph/NodeDrawingStyle.h"
#include "../scene_graph/NodeShape.h"
#include "../scene_graph/NodeLight.h"
#include "../scene_graph/NodeTransform.h"

#include "../scene_graph/Rep.h"
#include "../scene_graph/Node.h"

void GLBBoxRenderer::visitIndexedFaceSet(IndexedFaceSet& iFaceSet)  
{
  RenderRep(iFaceSet);
}

void GLBBoxRenderer::visitNodeGroup(NodeGroup& iGroupNode)  
{
  RenderNode(iGroupNode);
}

void GLBBoxRenderer::visitNodeTransform(NodeTransform& iTransformNode)
{
  RenderNode(iTransformNode);
}

void GLBBoxRenderer::visitDrawingStyle(DrawingStyle& iDrawingStyle) 
{
  if(DrawingStyle::INVISIBLE == iDrawingStyle.style())
    return ;

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_BLEND);
  glEnable(GL_LINE_SMOOTH);
  glPolygonMode(GL_FRONT, GL_LINE);
  glLineWidth(1.f);

  glDisable(GL_LIGHTING);
}

void GLBBoxRenderer::RenderRep(const Rep& iRep) const 
{
  RenderBBox(iRep.bbox());
}

void GLBBoxRenderer::RenderNode(const Node& iNode) const 
{
  RenderBBox(iNode.bbox());
}

void GLBBoxRenderer::RenderBBox(const BBox<Vec3r>& iBox) const 
{
  if(iBox.empty())
    return;

  Vec3r m = iBox.getMin();
  Vec3r M = iBox.getMax();

  glColor3f(0.f, 0.f, 0.f);
  glBegin(GL_LINE_LOOP);
  ((GLBBoxRenderer*)(this))->glVertex3r(m[0], m[1], m[2]);
  ((GLBBoxRenderer*)(this))->glVertex3r(M[0], m[1], m[2]);
  ((GLBBoxRenderer*)(this))->glVertex3r(M[0], M[1], m[2]);
  ((GLBBoxRenderer*)(this))->glVertex3r(m[0], M[1], m[2]);
  glEnd();
  
  glBegin(GL_LINE_LOOP);
  ((GLBBoxRenderer*)(this))->glVertex3r(m[0], m[1], M[2]);
  ((GLBBoxRenderer*)(this))->glVertex3r(M[0], m[1], M[2]);
  ((GLBBoxRenderer*)(this))->glVertex3r(M[0], M[1], M[2]);
  ((GLBBoxRenderer*)(this))->glVertex3r(m[0], M[1], M[2]);
  glEnd();
  
  glBegin(GL_LINE_LOOP);
  ((GLBBoxRenderer*)(this))->glVertex3r(m[0], m[1], m[2]);
  ((GLBBoxRenderer*)(this))->glVertex3r(m[0], m[1], M[2]);
  ((GLBBoxRenderer*)(this))->glVertex3r(m[0], M[1], M[2]);
  ((GLBBoxRenderer*)(this))->glVertex3r(m[0], M[1], m[2]);
  glEnd();
  
  glBegin(GL_LINE_LOOP);
  ((GLBBoxRenderer*)(this))->glVertex3r(M[0], m[1], m[2]);
  ((GLBBoxRenderer*)(this))->glVertex3r(M[0], m[1], M[2]);
  ((GLBBoxRenderer*)(this))->glVertex3r(M[0], M[1], M[2]);
  ((GLBBoxRenderer*)(this))->glVertex3r(M[0], M[1], m[2]);
  glEnd();

}
