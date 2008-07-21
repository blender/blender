//
//  Filename         : AppGL2DCurvesViewer.h
//  Author           : Stephane Grabli
//  Purpose          : 2D GL Curves viewer
//  Date of creation : 14/03/2004
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

#ifndef  APPGL2DCURVESVIEWER_H
# define APPGL2DCURVESVIEWER_H

# include <QGLViewer/qglviewer.h>
# include "../scene_graph/NodeDrawingStyle.h"
# include <qstring.h>

class GLRenderer;

class AppGL2DCurvesViewer : public QGLViewer
{
  Q_OBJECT
    
public:

  AppGL2DCurvesViewer(QWidget *iParent, const char *iName = 0);
  virtual ~AppGL2DCurvesViewer();
  
  /*! Sets the ranges.
   */
  void setRange(const Vec2d& vmin, const Vec2d& vmax, const char * xlabel, const char *ylabel);
  void setCurve(const Vec2d& vmin, const Vec2d& vmax, const vector<Vec3r>& iPoints, const char *xlabel, const char *ylabel);
  void AddNode(Node* iNode);
  void DetachNode(Node* iNode);
  void RetrieveNodes(vector<Node*>& oNodes);
  
  virtual QSize sizeHint() const {return QSize(200,200);}
  virtual QSizePolicy sizePolicy() const {return QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);}
protected:
    virtual void init();
    virtual void draw();

private:
  NodeDrawingStyle _RootNode;
  GLRenderer     *_pGLRenderer;
  vector<Vec3r> _curve;
  Vec2d _vmin; // curve bbox min
  Vec2d _vmax; // curve bbox max
  double _left; // frustum clipping planes (slightly differemt from the bbox for a clear view)
  double _right;
  double _bottom;
  float _xmargin; // margin around plot in x direction
  float _ymargin; // margin around plot in y direction
  double _top;
  QString _xlabel;
  QString _ylabel;
  
};


#endif // APPGL2DCURVESVIEWER_H

