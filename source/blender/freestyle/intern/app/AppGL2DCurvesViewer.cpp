
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

#include "AppGL2DCurvesViewer.h"
#include "../rendering/GLRenderer.h"
#include "../scene_graph/NodeShape.h"
#include "../scene_graph/LineRep.h"
#include "../scene_graph/VertexRep.h"

AppGL2DCurvesViewer::AppGL2DCurvesViewer(QWidget *iParent, const char *iName)
: QGLViewer(iParent)
{
  _RootNode.setLightingEnabled(false);
  _RootNode.setLineWidth(1.0);
  _pGLRenderer = new GLRenderer;
}
AppGL2DCurvesViewer::~AppGL2DCurvesViewer(){
  makeCurrent();
  _RootNode.destroy();
  if(_pGLRenderer)
    delete _pGLRenderer;
}
  
void AppGL2DCurvesViewer::setRange(const Vec2d& vmin, const Vec2d& vmax, const char * xlabel, const char *ylabel){
  _vmin = vmin;
  _vmax = vmax;
  _xmargin = (vmax.x()-vmin.x())/20.0;
  _ymargin = (vmax.y()-vmin.y())/20.0;
  _left = vmin.x()-_xmargin;
  _right = vmax.x()+_xmargin;
  _bottom = vmin.y()- _ymargin;
  _top = vmax.y()+_ymargin;
  if(xlabel)
    _xlabel = xlabel;
  if(ylabel)
    _ylabel = ylabel;
}
void AppGL2DCurvesViewer::setCurve(const Vec2d& vmin, const Vec2d& vmax, const vector<Vec3r>& iPoints, const char *xlabel, const char *ylabel){
  setRange(vmin, vmax, xlabel, ylabel);
  vector<Node*> nodes;
  _RootNode.RetrieveChildren(nodes);
  _RootNode.DetachChildren();
  for(vector<Node*>::iterator n=nodes.begin(), nend=nodes.end();
  n!=nend;
  ++n){
    delete (*n);
  }
  _curve.clear();
  _curve = iPoints;
  NodeGroup * curveNode = new NodeGroup;
  NodeShape * shape = new NodeShape;
  shape->material().setDiffuse(0,0,0,1);
  curveNode->AddChild(shape);
  shape->AddRep(new LineRep(iPoints));
  for(vector<Vec3r>::const_iterator v=iPoints.begin(), vend=iPoints.end();
  v!=vend;
  ++v){
    shape->AddRep(new VertexRep(v->x(), v->y(), v->z()));
  }
  _RootNode.AddChild(curveNode);
  updateGL();
}

void AppGL2DCurvesViewer::AddNode(Node* iNode){
  _RootNode.AddChild(iNode);
}

void AppGL2DCurvesViewer::DetachNode(Node* iNode){
  _RootNode.DetachChild(iNode);
}

void AppGL2DCurvesViewer::RetrieveNodes(vector<Node*>& oNodes){
  _RootNode.RetrieveChildren(oNodes);
}

void AppGL2DCurvesViewer::init(){
  glClearColor(1,1,1,1);
  _left = 0;
  _right = width();
  _bottom = 0; 
  _top = height();
}
void AppGL2DCurvesViewer::draw(){
  glPushAttrib(GL_ALL_ATTRIB_BITS);

//    // Projection Matrix
//    //==================
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(_left,_right, _bottom, _top, -1.0, 1.0);

  //Modelview Matrix
  //================
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  // draw axis
  glColor3f(0.5, 0.5, 0.5);
  // x axis
  glBegin(GL_LINES);
  glVertex2f(_left, _vmin.y());
  glVertex2f(_vmax.x(), _vmin.y());
  glEnd();
  QFont serifFont( "Times", 8);
  if(!_xlabel.isEmpty()){
    renderText(width()-30, height()-1, _xlabel, serifFont);
    //renderText(_vmax.x()-_xmargin, _vmin.y(), 0, _xlabel, serifFont);
  }

  // y axis
  glBegin(GL_LINES);
  glVertex2f(_vmin.x(), _bottom);
  glVertex2f(_vmin.x(), _vmax.y());
  glEnd();
  if(!_ylabel.isEmpty()){
    //renderText(_vmin.x(), _vmax.y()-3*_ymargin, _ylabel, serifFont);
    renderText(12, 10, _ylabel, serifFont);
  }
  _RootNode.accept(*_pGLRenderer);
  serifFont.setPointSize(7);
  for(vector<Vec3r>::iterator v=_curve.begin(), vend=_curve.end();
  v!=vend;
  ++v){
    if(v->y() == 0)
      continue;
    QString label = QString( "(%1, %2)" )
                    .arg( (int)v->x())
                    .arg( v->y(), 0, 'E', 1 );

    renderText(v->x(), v->y(), 0, label, serifFont);
  }
  glPopAttrib();

}