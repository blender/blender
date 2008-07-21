
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

#include "AppGLWidget.h"
#include "../image/Image.h"
#include "../system/TimeStamp.h"
#include "Controller.h"
#include "../stroke/StrokeRenderer.h"
#include "AppCanvas.h"
#include "../rendering/GLRenderer.h"
#include "../rendering/GLStrokeRenderer.h"
#include "../rendering/GLUtils.h"
#include "AppConfig.h"

#include "../system/StringUtils.h"

#ifdef WIN32
# include <windows.h>
# include "../rendering/extgl.h"
#endif
#ifdef __MACH__
# include <OpenGL/gl.h>
#else
# include <GL/gl.h>
#endif

AppCanvas::AppCanvas()
:Canvas()
{
  _pViewer = 0;
  _blendEquation = true;
	_MapsPath = StringUtils::toAscii( Config::Path::getInstance()->getMapsDir() ).c_str();
}

AppCanvas::AppCanvas(AppGLWidget* iViewer)
:Canvas()
{
  _pViewer = iViewer;
  _blendEquation = true;
}

AppCanvas::AppCanvas(const AppCanvas& iBrother)
:Canvas(iBrother)
{
  _pViewer = iBrother._pViewer;
  _blendEquation = iBrother._blendEquation;
}

AppCanvas::~AppCanvas()
{
  _pViewer = 0;
}

void AppCanvas::setViewer(AppGLWidget *iViewer)
{
  _pViewer = iViewer;
}  

int AppCanvas::width() const 
{
  return _pViewer->width();
}

int AppCanvas::height() const
{
  return _pViewer->height();
}

BBox<Vec3r> AppCanvas::scene3DBBox() const 
{
  return _pViewer->scene3DBBox();
}

void AppCanvas::preDraw()
{
  Canvas::preDraw();

  _pViewer->prepareCanvas();
  glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);
	glDisable(GL_LIGHTING);
	glPolygonMode(GL_FRONT, GL_FILL);
	glShadeModel(GL_SMOOTH);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
}

void AppCanvas::init() 
{
#ifdef WIN32
  static bool firsttime = true;
  if (firsttime)
    {
      if (extgl_Initialize() != 0)
	      cerr << "Error: problem occurred while initializing GL extensions" << endl;			
      else
	      cout << "GL extensions initialized" << endl;

      if(!glutils_extgl_GetProcAddress("glBlendEquation")){
        _blendEquation = false;
        cout << "glBlendEquation unavailable on this hardware -> switching to strokes basic rendering mode" << endl;
      }
      firsttime=false;
    }
#endif

  _Renderer = new GLStrokeRenderer;
  if(!StrokeRenderer::loadTextures())
    {
      cerr << "unable to load stroke textures" << endl;
      return;
    }
}

void AppCanvas::postDraw()
{
  //inverse frame buffer
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
  _pViewer->releaseCanvas();

  Canvas::postDraw();
}

void AppCanvas::Erase()
{
  Canvas::Erase();
  //_pViewer->clear();
}

#include "../image/GaussianFilter.h"
void AppCanvas::readColorPixels(int x,int y,int w, int h, RGBImage& oImage) const
{
  //static unsigned number = 0;
  float *rgb = new float[3*w*h];
  _pViewer->readPixels(x,y,w,h,AppGLWidget::RGB,rgb);
  oImage.setArray(rgb, width(), height(), w,h, x, y, false);
  // FIXME
  //  QImage qtmp(w, h, 32);
  //  for(unsigned py=0;py<h;++py){
  //    for(unsigned px=0;px<w;++px){
  //      int r = (int)255*(oImage.getR(x+px,y+py));
  //      int g = (int)255*(oImage.getG(x+px,y+py));
  //      int b = (int)255*(oImage.getB(x+px,y+py));
  //      qtmp.setPixel(px,py,qRgb(r,g,b));
  //    }
  //  }
  //  qtmp.save("densityQuery"+QString::number(number)+".png", "PNG");
  //  if(number == 1090){
  //    RGBImage img;
  //    float *rgbtmp = new float[3*width()*height()];
  //    _pViewer->readPixels(0,0,width(),height(),AppGLWidget::RGB,rgbtmp);
  //    img.setArray(rgbtmp, width(), height(), width(), height(), 0, 0, false);
  //    QImage qtmp(width(), height(), 32);
  //    for(unsigned py=0;py<height();++py){
  //      for(unsigned px=0;px<width();++px){
  //        int r = (int)255*(img.getR(px,py));
  //        int g = (int)255*(img.getG(px,py));
  //        int b = (int)255*(img.getB(px,py));
  //        qtmp.setPixel(px,height()-1-py,qRgb(r,g,b));
  //      }
  //    }
  //    qtmp.save("densityQuery"+QString::number(number)+".png", "PNG");
  //    
  //    GaussianFilter filter;
  //    filter.setSigma(4.0);
  //    int bound = filter.getBound();
  //    QImage qtmp2(width(), height(), 32);
  //    for(int py2=0;py2<height();++py2){
  //      for(int px2=0;px2<width();++px2){  
  //        if( (px2-bound < 0) || (px2+bound>width())
  //	      || (py2-bound < 0) || (py2+bound>height()))
  //          continue;
  //        int g = 255*filter.getSmoothedPixel<RGBImage>(&img, px2,py2);
  //        qtmp2.setPixel(px2,height()-1-py2,qRgb(g,g,g));
  //      }
  //    }
  //    qtmp2.save("blurredCausalDensity"+QString::number(number)+".png", "PNG");
  //  }
  //  cout << number << endl;
  //  ++number;
}

void AppCanvas::readDepthPixels(int x,int y,int w, int h, GrayImage& oImage) const
{
  float *rgb = new float[w*h];
  _pViewer->readPixels(x,y,w,h,AppGLWidget::DEPTH,rgb);
  oImage.setArray(rgb, width(), height(), w,h, x, y, false);
}

void AppCanvas::update()
{
//  static int counter = 0;
//  char fileName[100] = "framebuffer";
//  char number[10];
//
  _pViewer->updateGL();
  //_pViewer->swapBuffers();
  //QImage fb = _pViewer->grabFrameBuffer();
  //  sprintf(number, "%3d", counter);
  //  strcat(fileName, number);
  //  strcat(fileName, ".bmp");
  //  fb.save(fileName, "BMP");
  //counter++;
}

void AppCanvas::Render(const StrokeRenderer *iRenderer)
{
  if(!_blendEquation){
    RenderBasic(iRenderer);
    return;
  }

  glClearColor(1,1,1,1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_LIGHTING);
  glPolygonMode(GL_FRONT, GL_FILL);
  glShadeModel(GL_SMOOTH);
  
  if(_pViewer->draw3DsceneEnabled())
  {
    glClearColor(1,1,1,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    _pViewer->set3DContext();
    _pViewer->DrawScene(_pViewer->glRenderer());
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
  }
  
  
  glDisable(GL_DEPTH_TEST);
  glBlendEquation(GL_ADD);
  
  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  
  if(_drawPaper)
  {
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    float zfar = _pViewer->zfar();
    zfar = zfar+0.1*zfar;
    //draw background paper // FIXME
    //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, StrokeRenderer::_textureManager->getPaperTextureIndex(_paperTextureIndex)); 
    glColor4f(1,1,1,0.0);
    glBegin(GL_TRIANGLE_STRIP);
    { 
      glTexCoord2f(0,0); glVertex3f(0, 0, -1);
      glTexCoord2f(4,0); glVertex3f(2048, 0, -1);
      glTexCoord2f(0,4); glVertex3f(0, 2048, -1);
      glTexCoord2f(4,4); glVertex3f(2048, 2048, -1);
    } 
    glEnd();
  }
  
  glPushAttrib(GL_COLOR_BUFFER_BIT);
  glBlendEquation(GL_FUNC_SUBTRACT);
  glBlendFunc(GL_ONE, GL_ONE);
  
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glColor4f(1,1,1,1);
  glBegin(GL_TRIANGLE_STRIP);
  {  
    glVertex2f(0, 0);
    glVertex2f(2048, 0);
    glVertex2f(0, 2048);
    glVertex2f(2048, 2048);
  }  
  glEnd();
  glPopAttrib();
  
  glDisable(GL_DEPTH_TEST);
  glBlendEquation(GL_ADD);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  
  glEnable(GL_TEXTURE_2D);

  Canvas::Render(iRenderer);
  //  
  glPushAttrib(GL_COLOR_BUFFER_BIT);
  glBlendEquation(GL_FUNC_SUBTRACT);
  glBlendFunc(GL_ONE, GL_ONE);
  
  glDisable(GL_TEXTURE_2D);
  glEnable(GL_BLEND);
  glColor3f(1,1,1);
  glBegin(GL_TRIANGLE_STRIP);
  { 
    glVertex2f(0, 0);
    glVertex2f(2048, 0);
    glVertex2f(0, 2048);
    glVertex2f(2048, 2048);
  } 
  glEnd();
  glPopAttrib();
  
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
}

void AppCanvas::RenderBasic(const StrokeRenderer *iRenderer)
{
  glClearColor(1,1,1,1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glDisable(GL_LIGHTING);
  glPolygonMode(GL_FRONT, GL_FILL);
  glShadeModel(GL_SMOOTH);
  
  if(_pViewer->draw3DsceneEnabled())
  {
    glClearColor(1,1,1,0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    
    glEnable(GL_LIGHTING);
    glEnable(GL_DEPTH_TEST);
    _pViewer->set3DContext();
    _pViewer->DrawScene(_pViewer->glRenderer());
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
  }

  glBlendFunc(GL_DST_COLOR, GL_ZERO);
  if(_drawPaper)
  {
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    float zfar = _pViewer->zfar();
    zfar = zfar+0.1*zfar;
    //draw background paper // FIXME
    //glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glBindTexture(GL_TEXTURE_2D, StrokeRenderer::_textureManager->getPaperTextureIndex(_paperTextureIndex)); 
    glColor4f(1,1,1,0.0);
    glBegin(GL_TRIANGLE_STRIP);
    { 
      glTexCoord2f(0,0); glVertex3f(0, 0, -1);
      glTexCoord2f(4,0); glVertex3f(2048, 0, -1);
      glTexCoord2f(0,4); glVertex3f(0, 2048, -1);
      glTexCoord2f(4,4); glVertex3f(2048, 2048, -1);
    } 
    glEnd();
  }

  glDisable(GL_DEPTH_TEST);
  glPushAttrib(GL_COLOR_BUFFER_BIT);
  glEnable(GL_BLEND);
  glPopAttrib();

  glDisable(GL_DEPTH_TEST);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);
  
  glEnable(GL_TEXTURE_2D);
  Canvas::RenderBasic(iRenderer);

  glDisable(GL_TEXTURE_2D);
  glDisable(GL_BLEND);
}


void AppCanvas::RenderStroke(Stroke *iStroke) {
  iStroke->Render(_Renderer);
  if(_pViewer->getRecordFlag()){
    //Sleep(1000);
     _pViewer->saveSnapshot(true);
  }
}
