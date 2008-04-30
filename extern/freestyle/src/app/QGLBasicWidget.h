//
//  Filename         : QGLBasicWidget.h
//  Author           : Stephane Grabli
//  Purpose          : A basic qgl widget designed to be used as 
//                     a 2D offscreen buffer. (no interactive function)
//  Date of creation : 26/12/2003
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

#ifndef QGLBASICWIDGET_H
#define QGLBASICWIDGET_H

#include <qgl.h>
#include "../geometry/Geom.h"
//#include "../rendering/pbuffer.h"
#include "../scene_graph/NodeDrawingStyle.h"
using namespace Geometry;

class GLRenderer;
class ViewMap;
// #ifndef WIN32
// class PBuffer;
// #endif
class QGLBasicWidget : public QGLWidget
{
    Q_OBJECT

public:

    QGLBasicWidget( QWidget* parent, const char* name, int w, int h, const QGLWidget* shareWidget=0 );
    QGLBasicWidget( const QGLFormat& format, QWidget* parent, const char* name, 
	  int w, int h, const QGLWidget* shareWidget=0 );
    ~QGLBasicWidget();

    /*! Adds a node directly under the root node */
    void AddNode(Node* iNode);
    /*! Detach the node iNode which must 
     *  be directly under the root node.
     */
    void DetachNode(Node *iNode);

    /*! reads the frame buffer pixels as luminance .
     *  \param x 
     *    The lower-left corner x-coordinate of the 
     *    rectangle we want to grab.
     *  \param y
     *    The lower-left corner y-coordinate of the 
     *    rectangle we want to grab.
     *  \param width
     *    The width of the rectangle we want to grab.
     *  \param height
     *    The height of the rectangle we want to grab.
     *  \params pixels
     *    The array of float (of size width*height) in which 
     *    the read values are stored.
     */
    void readPixels(int x,int y,int width,int height,float *pixels) ;
// #ifndef WIN32
// 	void draw() { paintGL(); }
// #endif 

    inline void SetClearColor(const Vec3f& c) {_clearColor = c;}
    inline Vec3f getClearColor() const {return _clearColor;}

protected:

    virtual void		initializeGL();
    virtual void		paintGL();
    virtual void		resizeGL(int w, int h);

private:
// #ifndef WIN32
//   PBuffer	*_Pbuffer;
// #endif 
  NodeDrawingStyle       _RootNode;
  Vec3f           _clearColor;
  GLRenderer     *_pGLRenderer;
};


#endif // QGLBASICWIDGET_H
