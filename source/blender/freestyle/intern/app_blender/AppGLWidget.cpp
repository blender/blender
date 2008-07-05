
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

#include <iostream>
#include "../stroke/Canvas.h"
#include "AppGLWidget.h"
#include "../scene_graph/NodeLight.h"
#include "../rendering/GLRenderer.h"
#include "../rendering/GLSelectRenderer.h"
#include "../rendering/GLBBoxRenderer.h"
#include "../rendering/GLMonoColorRenderer.h"
#include "Controller.h"
#include "../view_map/Silhouette.h"
#include "../view_map/ViewMap.h"
#include "../scene_graph/LineRep.h"
#include "../scene_graph/NodeShape.h"
#include "../scene_graph/VertexRep.h"
#include "AppConfig.h"

#include "../system/StringUtils.h"

extern "C" {
#include "BLI_blenlib.h"
#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"
}

// glut.h must be included last to avoid a conflict with stdlib.h on vc .net 2003 and 2005
#ifdef __MACH__
# include <GLUT/glut.h>
#else
# include <GL/glut.h>
#endif

GLuint texture = 0;

bool AppGLWidget::_frontBufferFlag = false;
bool AppGLWidget::_backBufferFlag = true;

AppGLWidget::AppGLWidget(const char *iName)
{
  //soc
  _camera = new AppGLWidget_Camera;	

  _Fovy        = 30.f;
  //_SceneDepth = 2.f;
  _RenderStyle = LINE;
  //_ModelRootNode->SetBBox(BBox<Vec3f>(Vec3f(-10.f, -10.f, -10.f), Vec3f(10.f, 10.f, 10.f)));
  _ModelRootNode = new NodeDrawingStyle;
  _SilhouetteRootNode = new NodeDrawingStyle;
  _DebugRootNode = new NodeDrawingStyle;
  
  _RootNode.AddChild(_ModelRootNode);
  _SilhouetteRootNode->SetStyle(DrawingStyle::LINES);
  _SilhouetteRootNode->SetLightingEnabled(false);
  _SilhouetteRootNode->SetLineWidth(2.f);
  _SilhouetteRootNode->SetPointSize(3.f);

  _RootNode.AddChild(_SilhouetteRootNode);

  _DebugRootNode->SetStyle(DrawingStyle::LINES);
  _DebugRootNode->SetLightingEnabled(false);
  _DebugRootNode->SetLineWidth(1.f);
  
  _RootNode.AddChild(_DebugRootNode);

  _minBBox = __min(__min(_ModelRootNode->bbox().getMin()[0], 
                            _ModelRootNode->bbox().getMin()[1]),
                      _ModelRootNode->bbox().getMin()[2]);
  _maxBBox = __max(__max(_ModelRootNode->bbox().getMax()[0], 
                            _ModelRootNode->bbox().getMax()[1]),
                      _ModelRootNode->bbox().getMax()[2]);

  _maxAbs = __max(rabs(_minBBox), rabs(_maxBBox));
  _minAbs = __min(rabs(_minBBox), rabs(_maxBBox));

  _camera->setZNearCoefficient(0.1);

  // 2D Scene
  //  _pFENode = new NodeDrawingStyle;
  //  _pFENode->SetStyle(DrawingStyle::LINES);
  //  _pFENode->SetLightingEnabled(false);
  //  _pFENode->SetLineWidth(1.f);
  //
  //  _p2DNode.AddChild(_pFENode);
  //  
  //  _pVisibleSilhouetteNode = new NodeDrawingStyle;
  //  _pVisibleSilhouetteNode->SetStyle(DrawingStyle::LINES);
  //  _pVisibleSilhouetteNode->SetLightingEnabled(false);
  //  _pVisibleSilhouetteNode->SetLineWidth(3.f);
  //
  //  _p2DNode.AddChild(_pVisibleSilhouetteNode);
  //  
  _p2DSelectionNode = new NodeDrawingStyle;
  _p2DSelectionNode->SetLightingEnabled(false);
  _p2DSelectionNode->SetStyle(DrawingStyle::LINES);
  _p2DSelectionNode->SetLineWidth(5.f);
  
  _p2DNode.AddChild(_p2DSelectionNode);

  _pGLRenderer = new GLRenderer;
  _pSelectRenderer = new GLSelectRenderer;
  _pBBoxRenderer = new GLBBoxRenderer;
  _pMonoColorRenderer = new GLMonoColorRenderer;
  _pDebugRenderer = new GLDebugRenderer;

  _pMainWindow = NULL;
  _cameraStateSaved = false;
  _drawBBox = false;
  _silhouette = false;
  _fedges = false;
  _debug = false;
  _selection_mode = false;
  _Draw2DScene = true;
  _Draw3DScene = false;
  _drawEnvMap = false;
  _currentEnvMap = 1;
  _maxId = 0;
  _blendFunc = 0;

  const string sep(Config::DIR_SEP);
  const string filename = Config::Path::getInstance()->getHomeDir() + sep +
    Config::OPTIONS_DIR + sep + Config::OPTIONS_QGLVIEWER_FILE;
  setStateFileName(filename);  

  //get camera frame:
  //qglviewer::Camera * cam = camera();
  //qglviewer::ManipulatedFrame *  fr = cam->frame() ;
  
  //soc _enableupdateSilhouettes = false;

  _captureMovie = false;
  //  _frontBufferFlag = false;
  //  _backBufferFlag = true;
  _record = false;


	workingBuffer = GL_BACK; //soc

}

AppGLWidget::~AppGLWidget()
{
  int ref = _RootNode.destroy();
  
  _Light.destroy();
  ref = _p2DNode.destroy();
  
  if(NULL != _pGLRenderer)
    {
      delete _pGLRenderer;
      _pGLRenderer = NULL;
    }

  if(NULL != _pSelectRenderer)
    {
      delete _pSelectRenderer;
      _pSelectRenderer = NULL;
    }

  if(NULL != _pBBoxRenderer)
    {
      delete _pBBoxRenderer;
      _pBBoxRenderer = NULL;
    }

  if(NULL != _pMonoColorRenderer)
    {
      delete _pMonoColorRenderer;
      _pMonoColorRenderer = NULL;
    }

  if(NULL != _pDebugRenderer)
    {
      delete _pDebugRenderer;
      _pDebugRenderer = NULL;
    }

  makeCurrent();
  //saveToFile(filename);
}



void AppGLWidget::LoadEnvMap(const char *filename)
{
  GLuint textureId;
  //sgiImage img;
  //cout << filename << endl;
	ImBuf *image = IMB_loadiffname(filename, 0);

  //data = img.read(filename); // tres beau bleu gris mauve!!
  // allocate a texture name
  glGenTextures( 1, &textureId );
  if(textureId > (GLuint) _maxId)
    _maxId = textureId;

  // select our current texture
  glBindTexture( GL_TEXTURE_2D, textureId );
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
                   GL_NEAREST);

  glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA, image->x, image->y, 0,
    GL_RGBA, GL_UNSIGNED_BYTE, image->rect );
}

void AppGLWidget::init()
{
  //setShortcut(QGLViewer::EXIT_VIEWER, 0);
//  setShortcut(QGLViewer::DISPLAY_Z_BUFFER, 0);
  //setShortcut(QGLViewer::STEREO, 0);
  //setShortcut(QGLViewer::ANIMATION, 0);
  //setShortcut(QGLViewer::EDIT_CAMERA, 0);

  //restoreStateFromFile();

  //trackball().fitBBox(_ModelRootNode->bbox().getMin(), _ModelRootNode->bbox().getMax(), _Fovy);

   glClearColor(1,1,1,0);
   glShadeModel(GL_SMOOTH);
  
   glCullFace(GL_BACK);
   glEnable(GL_CULL_FACE);
   glEnable(GL_DEPTH_TEST);

   // open and read texture data
   Config::Path * cpath = Config::Path::getInstance();
   string envmapDir = cpath->getEnvMapDir();
	LoadEnvMap( StringUtils::toAscii(envmapDir + string("gray00.png")).c_str() );
   //LoadEnvMap(Config::ENV_MAP_DIR + "gray01.bmp");
   LoadEnvMap( StringUtils::toAscii(envmapDir + string("gray02.png")).c_str() );
   LoadEnvMap( StringUtils::toAscii(envmapDir + string("gray03.png")).c_str() );
   LoadEnvMap( StringUtils::toAscii(envmapDir + string("brown00.png")).c_str() );
   glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP) ;
   glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP) ;

   // gl settings for Environmental Texturing:
   glColor3f(1, 1, 1);

   // Use GL auto-computed enviroment texture coordinates
   //glEnable(GL_TEXTURE_GEN_S);
   //glEnable(GL_TEXTURE_GEN_T);

   // Bind the texture to use
   //glBindTexture(GL_TEXTURE_2D,texture);
   glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
   //glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
   
   // parametres de melange
   //glBlendFunc(GL_ONE, GL_ONE);
   glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
   //glBlendEquatio(GL_FUNC_ADD);
     
   //glEnable(GL_BLEND);
   NodeLight *light = new NodeLight;
   _Light.AddChild(light);

   // Change QGLViewer's default shortcut for snapshots
   //setShortcut(QGLViewer::SAVE_SCREENSHOT, Qt::CTRL + Qt::Key_W);
   //   setShortcutKey (QGLViewer::SAVE_SCREENSHOT, Key_W);
   //   setShortcutStateKey(QGLViewer::SAVE_SCREENSHOT, ControlButton);
  
   cout << "Renderer (GL)    : " << glGetString(GL_RENDERER) << endl
	<< "Vendor (GL)      : " << glGetString(GL_VENDOR) << endl << endl;
}

void AppGLWidget::draw()
{
  if (true == _Draw3DScene)
    {
      if (true == _selection_mode) {
	_pSelectRenderer->setSelectRendering(false);
	_pSelectRenderer->resetColor();
	DrawScene(_pSelectRenderer);
      } else
 	DrawScene(_pGLRenderer);
  
      if (true == _silhouette)
	DrawSilhouette();
  
      if (true == _drawBBox) {
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	_ModelRootNode->accept(*_pBBoxRenderer);
	glPopAttrib();
      }

      if (true == _debug) {
	glPushAttrib(GL_ALL_ATTRIB_BITS);
	_DebugRootNode->accept(*_pDebugRenderer);
	glPopAttrib();
      }
    }

  if (true == _Draw2DScene) {
    Draw2DScene(_pGLRenderer);
    Set3DContext();
  }
  if(_record){
    saveSnapshot(true);
  }
}

void AppGLWidget::DrawScene(SceneVisitor *iRenderer)
{
  glPushAttrib(GL_ALL_ATTRIB_BITS);

  if(_drawEnvMap)
  {
    _ModelRootNode->SetLightingEnabled(false);
    glEnable(GL_COLOR_MATERIAL);

    glEnable(GL_TEXTURE_2D);
    // Bind the texture to use
    glBindTexture(GL_TEXTURE_2D,_currentEnvMap); 
    switch(_blendFunc)
    {
    case 0:
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE) ;
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
      glEnable(GL_BLEND);
      break;
    case 1:
      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE) ;
      glDisable(GL_BLEND);
      break;
      //    case 2:
      //      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE) ;
      //      glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
      //      glEnable(GL_BLEND);
      //      break;
      //    case 3:
      //      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE) ;
      //      glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE_MINUS_SRC_COLOR); 
      //      glEnable(GL_BLEND);
      //      break;
      //    case 4:
      //      glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE) ;
      //      glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA); 
      //      glEnable(GL_BLEND);
      //      break;
    default:
      break;
    }

    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
  }

  // FIXME
  //  //_ModelRootNode->SetLightingEnabled(true);
  //  if(_ModelRootNode->style() == DrawingStyle::LINES){
  //    glPushAttrib(GL_ALL_ATTRIB_BITS);
  //    //glDisable(GL_COLOR_MATERIAL);
  //    _ModelRootNode->SetStyle(DrawingStyle::FILLED);
  //    _ModelRootNode->SetLightingEnabled(true);
  //    _ModelRootNode->accept(*iRenderer);  
  //    _ModelRootNode->SetStyle(DrawingStyle::LINES);
  //    _ModelRootNode->SetLightingEnabled(false);
  //    _ModelRootNode->accept(*iRenderer);  
  //    glPopAttrib();
  //  }
  //  else
  _ModelRootNode->accept(*iRenderer);

  glDisable(GL_TEXTURE_GEN_S);
  glDisable(GL_TEXTURE_GEN_T);
  glDisable(GL_TEXTURE_2D);
  glDisable(GL_COLOR_MATERIAL);
  _ModelRootNode->SetLightingEnabled(true);

  if(_fedges == true)
    _SilhouetteRootNode->accept(*iRenderer);

  // FIXME: deprecated
//   if(_debug == true)
//     _DebugRootNode->accept(*iRenderer);
  
  glPopAttrib();
}

void AppGLWidget::prepareCanvas()
{
  makeCurrent();
  glPushAttrib(GL_ALL_ATTRIB_BITS);

  // if(_frontBufferFlag){
  //   if(_backBufferFlag)
  //     glDrawBuffer(GL_FRONT_AND_BACK);
  //   else
  //     glDrawBuffer(GL_FRONT);
  // }
  // else if(_backBufferFlag)
  //   glDrawBuffer(GL_BACK);
	//glDrawBuffer( workingBuffer ); //soc

  // Projection Matrix
  //==================
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  
  glOrtho(0,width(), 0, height(), -1.0, 1.0);
  
  //Modelview Matrix
  //================
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
}

void AppGLWidget::releaseCanvas()
{
  makeCurrent();
  //glDrawBuffer( workingBuffer ); //soc
  glPopAttrib();
}

void AppGLWidget::Draw2DScene(SceneVisitor *iRenderer)
{
  static bool first = 1;
  glPushAttrib(GL_ALL_ATTRIB_BITS);

//    // Projection Matrix
//    //==================
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0,width(), 0, height(), -1.0, 1.0);

//    //Modelview Matrix
//    //================
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  //  glBegin(GL_LINE_LOOP);
  //  glVertex2f(0,0);
  //  glVertex2f(100,0);
  //  glVertex2f(100,100);
  //  glVertex2f(0,100);
  //  glEnd();

  //glDrawBuffer(GL_FRONT_AND_BACK);
  // Draw visible silhouette
  //_pVisibleSilhouetteNode->Render(iRenderer);
  Canvas * canvas = Canvas::getInstance();
  if((canvas) && (!canvas->isEmpty()))
  {
    if (first)
    {
      canvas->init();
      first = false;
    }
    canvas->Render(canvas->renderer());
  }
  
  glLoadIdentity();
  //  glColor3f(0.f,1.f,0.f);
  //  glLineWidth(5.f);
  //glPolygonOffset(0.5f, 0.5f);
  glPushAttrib(GL_DEPTH_BUFFER_BIT);
  glDisable(GL_DEPTH_TEST);
  _p2DSelectionNode->accept(*iRenderer);
  glPopAttrib();
  // Draw Feature edges
  //  if(_fedges == true)
  //  {
  //    _pFENode->Render(iRenderer);
  //  }
 
  glPopAttrib();
}

void AppGLWidget::DrawSilhouette()
{
  glPushAttrib(GL_ALL_ATTRIB_BITS);
  
  glDepthFunc(GL_LESS);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  DrawScene(_pMonoColorRenderer);

  glCullFace(GL_FRONT);
  glDepthFunc(GL_LEQUAL);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glLineWidth(3.0);
  //glPolygonOffset(10.f, 10.f);
  glPolygonOffset(0.5f, 0.5f);
 
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  _pMonoColorRenderer->setColor(0.f, 0.f, 0.f);
  DrawScene(_pMonoColorRenderer);

  //Restore old context
  glPopAttrib();

}

void AppGLWidget::ReInitRenderers()
{
  // Debug Renderer
  if(NULL != _pDebugRenderer)
    _pDebugRenderer->ReInit(rabs(_ModelRootNode->bbox().getMax()[1] -
				 _ModelRootNode->bbox().getMin()[1]));
}

void AppGLWidget::setFrontBufferFlag(bool iBool){
  _frontBufferFlag = iBool;
}
bool AppGLWidget::getFrontBufferFlag() {
  return _frontBufferFlag;
}
void AppGLWidget::setBackBufferFlag(bool iBool){
  _backBufferFlag = iBool;
}
bool AppGLWidget::getBackBufferFlag() {
  return _backBufferFlag;
}

//void AppGLWidget::DrawLines()
//{
//  //Antialiasing:
//  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
//  glEnable(GL_BLEND);
//  glEnable(GL_LINE_SMOOTH);
//  glPolygonMode(GL_FRONT, GL_LINE);
//
//  glColor3f(0.f, 0.f, 0.f);
//  glLineWidth(2.f);
//
//  DrawScene();
//}
//
//void AppGLWidget::DrawSurfacic()
//{
//  glPolygonMode(GL_FRONT, GL_FILL);
//  glShadeModel(GL_SMOOTH);
//  
//  glEnable(GL_LIGHTING);
//  glEnable(GL_LIGHT0);
//
//  
//  GLreal diffuseV[] = {0.5, 0.7, 0.5, 1.0};
//  glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuseV);
//
//  //glColor3f(0.f, 0.f, 0.f);
//
//  DrawScene();
//
//  glDisable(GL_LIGHTING);
//}
//
//void AppGLWidget::DrawDepthBuffer()
//{
//  GLint w = width();
//  GLint h = height();
//
//  glPolygonMode(GL_FRONT, GL_FILL);
//  
//  //Disable the writing in the frame buffer
//  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
//
//  //This rendering will only fills the depth buffer
//  DrawScene();
//
//  //Re-enable the frame buffer writing
//  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
//
//
//  GLreal *zPixels = new real[w*h];
//  GLreal *colorPixels = new real[4*w*h];
//
// // glReadBuffer(GL_FRONT); //in reality: glReadBuffer and glDrawBuffer are both set to GL_BACK
//  glReadPixels(0,0,w, h, GL_DEPTH_COMPONENT, GL_real, (GLreal*)zPixels);
//
//  real *tmpZ = zPixels;
//  real *tmpColor = colorPixels;
//
//  for(int i=0; i<h; i++)
//  {
//    for(int j=0; j<w; j++)
//    {
//      //fprintf(test, " %.5f ", pixels[i*w+j]);
//      tmpColor[0] = *tmpZ;
//      tmpColor[1] = *tmpZ;
//      tmpColor[2] = *tmpZ;
//      tmpColor[3] = 1.f;
//
//      tmpColor += 4;
//      tmpZ++;
//    }
//  }
//  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
// // glDrawBuffer(GL_FRONT_AND_BACK);
//  //glRasterPos2i(0, 0);
//  //glLoadIdentity();
//  glDrawPixels(w, h, GL_RGBA, GL_real, (GLreal *)colorPixels);
//    
//  delete [] zPixels;
//  delete [] colorPixels;  
//}


//*******************************
// COPIED FROM LIBQGLVIEWER
//*******************************

	// inherited 	
	//Updates the display. Do not call draw() directly, use this method instead. 
	void AppGLWidget::updateGL() {}

	//Makes this widget's rendering context the current OpenGL rendering context. Useful with several viewers
	void AppGLWidget::makeCurrent() { }

	// not-inherited
	 void AppGLWidget::setStateFileName(const string& name) { stateFileName_ = name; };
	void AppGLWidget::saveSnapshot(bool b) {}
