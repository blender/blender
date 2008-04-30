
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

#ifndef WIN32
# include "GLRenderer.h"
# include "GLXOffscreenViewer.h"


GLXOffscreenViewer::GLXOffscreenViewer(int w, int h){
    _offscreenArea = new OffScreenArea(OffScreenArea::PIXMAP_OFFSCREEN_TYPE);
    _offscreenArea->AllocateOffScreenArea(w,h);
    _RootNode.SetLightingEnabled(false);
    _RootNode.SetLineWidth(1.0);
    _pGLRenderer = new GLRenderer;
} 

GLXOffscreenViewer::~GLXOffscreenViewer(){
    if(_offscreenArea)
      delete _offscreenArea;
    if(_pGLRenderer)
      delete _pGLRenderer;
    _RootNode.destroy();  
}

void GLXOffscreenViewer::AddNode(Node* iNode){
  _RootNode.AddChild(iNode);
}

void GLXOffscreenViewer::DetachNode(Node* iNode){
  _RootNode.DetachChild(iNode);
}

void GLXOffscreenViewer::init(){
  glClearColor(_clearColor[0],_clearColor[1],_clearColor[2],1);
}

void GLXOffscreenViewer::readPixels(int x,
                                int y,
                                int width,
                                int height,
                                float *pixels){
  _offscreenArea->MakeCurrent();
  glReadBuffer(GL_FRONT);
  GLenum glformat = GL_RED;
  glReadPixels(x,y,width, height, glformat, GL_FLOAT, (GLfloat*)pixels);
} 



void GLXOffscreenViewer::draw()
{
  _offscreenArea->MakeCurrent();
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
  glFlush();
  glPopAttrib();
}

#endif
