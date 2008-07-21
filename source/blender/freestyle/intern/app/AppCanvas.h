#ifndef ARTCANVAS_H
#define ARTCANVAS_H

//------------------------------------------------------------------------------------------//
//
//                        FileName          : AppCanvas.h
//                        Author            : Stephane Grabli
//                        Purpose           : Class to define the App Canvas.
//                        Date Of Creation  : 05/01/2003
//
//------------------------------------------------------------------------------------------//

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

#include "../stroke/Canvas.h"

class AppGLWidget;
class AppCanvas : public Canvas
{
private:
  mutable AppGLWidget *_pViewer;
  bool _blendEquation;
public:
  AppCanvas();
  AppCanvas(AppGLWidget *iViewer);
  AppCanvas(const AppCanvas& iBrother);
  virtual ~AppCanvas();

  /*! operations that need to be done before a draw */
  virtual void preDraw();
  
  /*! operations that need to be done after a draw */
  virtual void postDraw();
  
  /*! Erases the layers and clears the canvas */
  virtual void Erase(); 
  
  /* init the canvas */
  virtual void init();
  
  /*! Reads a pixel area from the canvas */
  virtual void readColorPixels(int x,int y,int w, int h, RGBImage& oImage) const;
  /*! Reads a depth pixel area from the canvas */
  virtual void readDepthPixels(int x,int y,int w, int h, GrayImage& oImage) const;

  virtual BBox<Vec3r> scene3DBBox() const ;

  /*! update the canvas (display) */
  virtual void update() ;

  /*! Renders the created strokes */
  virtual void Render(const StrokeRenderer *iRenderer);
  virtual void RenderBasic(const StrokeRenderer *iRenderer);
  virtual void RenderStroke(Stroke *iStroke) ;

  /*! accessors */
  virtual int width() const ;
  virtual int height() const ;
  inline const AppGLWidget * viewer() const {return _pViewer;}

  /*! modifiers */
  void setViewer(AppGLWidget *iViewer) ;
};


#endif
