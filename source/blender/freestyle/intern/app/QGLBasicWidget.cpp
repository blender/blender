
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
#include "QGLBasicWidget.h"
#include "../rendering/GLRenderer.h"
// #ifndef WIN32
// #include "../rendering/pbuffer.h"
// #endif 

QGLBasicWidget::QGLBasicWidget( QWidget* parent, const char* name, int w, int h, const QGLWidget* shareWidget )
    : QGLWidget( parent, shareWidget )
{
  _pGLRenderer = new GLRenderer;
// #ifndef WIN32
//   _Pbuffer = new PBuffer(w,h,
//               PBuffer::SingleBuffer 
// 			  | PBuffer::DepthBuffer 
// 			  | PBuffer::StencilBuffer);

//   _Pbuffer->create(false);
// #endif 
  resizeGL(w,h);
  _RootNode.setLightingEnabled(false);
  _RootNode.setLineWidth(1.0);
}

QGLBasicWidget::QGLBasicWidget( const QGLFormat& format, QWidget* parent, const char* name, 
	      int w, int h, const QGLWidget* shareWidget )
    : QGLWidget( format, parent, shareWidget )
{
  _pGLRenderer = new GLRenderer;
// #ifndef WIN32
//   _Pbuffer = new PBuffer(w,h,
//               PBuffer::SingleBuffer 
// 			  | PBuffer::DepthBuffer 
// 			  | PBuffer::StencilBuffer);
//   _Pbuffer->create(false);
// #endif 
  resizeGL(w,h);
  _RootNode.setLightingEnabled(false);
  _RootNode.setLineWidth(1.0);
}

QGLBasicWidget::~QGLBasicWidget()
{
  _RootNode.destroy();
  if(_pGLRenderer)
    delete _pGLRenderer;
// #ifndef WIN32
//   if(_Pbuffer)
//     delete _Pbuffer;
// #endif 
}

void QGLBasicWidget::AddNode(Node* iNode){
  _RootNode.AddChild(iNode);
}

void QGLBasicWidget::DetachNode(Node* iNode){
  _RootNode.DetachChild(iNode);
}

void QGLBasicWidget::readPixels(int x,
                                int y,
                                int width,
                                int height,
                                float *pixels){
// #ifndef WIN32
// _Pbuffer->makeCurrent();

//    glReadBuffer(GL_FRONT);
//    GLenum e = glGetError();
//    GLenum glformat = GL_RED;
//    glReadPixels(x,y,width, height, glformat, GL_FLOAT, (GLfloat*)pixels);
//    e = glGetError();
// #endif 
} 

void QGLBasicWidget::initializeGL()
{
   glClearColor(_clearColor[0],_clearColor[1],_clearColor[2],1);
}

void QGLBasicWidget::resizeGL( int w, int h )
{
// #ifndef WIN32
// 	_Pbuffer->makeCurrent();
// #endif 

    glViewport( 0, 0, (GLint)w, (GLint)h );
    // Projection Matrix
    //==================
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
// #ifndef WIN32
// 	// FXS- changed order of y bounds for glRead
//     glOrtho(0,w, h, 0, -1.0, 1.0);
// #else
    glOrtho(0,w, 0, h, -1.0, 1.0);
    //#endif 
}

void QGLBasicWidget::paintGL()
{
// #ifndef WIN32
// 	_Pbuffer->makeCurrent();
//    glClearColor(_clearColor[0],_clearColor[1],_clearColor[2],1);
// #endif 

  glDrawBuffer( GL_FRONT);
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
  //Modelview Matrix
  //================
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
    
  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE);
  glDisable(GL_DEPTH_TEST);
  _RootNode.accept(*_pGLRenderer);
  glPopAttrib();
}

