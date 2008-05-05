
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
#include <qtextstream.h>
#include <qimage.h>
#include <qtabwidget.h>
#include <qtextedit.h>
#include <QMouseEvent>
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

// glut.h must be included last to avoid a conflict with stdlib.h on vc .net 2003 and 2005
#ifdef __MACH__
# include <GLUT/glut.h>
#else
# include <GL/glut.h>
#endif

GLuint texture = 0;

bool AppGLWidget::_frontBufferFlag = false;
bool AppGLWidget::_backBufferFlag = true;

AppGLWidget::AppGLWidget(QWidget *iParent, const char *iName)
  : QGLViewer(iParent)
{
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

  camera()->setZNearCoefficient(0.1);

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
  _Draw3DScene = true;
  _drawEnvMap = false;
  _currentEnvMap = 1;
  _maxId = 0;
  _blendFunc = 0;

  const QString sep(Config::DIR_SEP.c_str());
  const QString filename = Config::Path::getInstance()->getHomeDir() + sep +
    Config::OPTIONS_DIR + sep + Config::OPTIONS_QGLVIEWER_FILE;
  setStateFileName(filename);  

  //get camera frame:
  qglviewer::Camera * cam = camera();
  qglviewer::ManipulatedFrame *  fr = cam->frame() ;
  _enableUpdateSilhouettes = false;
  connect(fr, SIGNAL(modified()), this, SLOT(updateSilhouettes()));
  _captureMovie = false;
  //  _frontBufferFlag = false;
  //  _backBufferFlag = true;
  _record = false;
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

void AppGLWidget::SetMainWindow(QMainWindow *iMainWindow) {
	_pMainWindow = iMainWindow;
}
void AppGLWidget::captureMovie()
{
  _captureMovie = true;
  setSnapshotFormat("BMP");
  setSnapshotFileName("anim");
  camera()->playPath(0);
  //_captureMovie = false;
}

void
AppGLWidget::updateSilhouettes()
{
  if(!_enableUpdateSilhouettes || !g_pController)
    return;
  g_pController->ComputeViewMap();
  g_pController->DrawStrokes();
  if(_captureMovie)
  {
    if(!camera()->keyFrameInterpolator(0)->interpolationIsStarted())
    {
      _captureMovie = false;
      return;
    }
    saveSnapshot(true);
  }
}

void
AppGLWidget::select(const QMouseEvent *e) {

  // 3D Shape selection

  if (_selection_mode) {

    // Make openGL context current
    makeCurrent();
  
    const unsigned SENSITIVITY = 4;
    const unsigned NB_HITS_MAX = 64;
  
    // Prepare the selection mode
    static GLuint hits[NB_HITS_MAX];
  
    glSelectBuffer(NB_HITS_MAX, hits);
    glRenderMode(GL_SELECT);
    glInitNames();

    // Loads the matrices
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    GLint viewport[4];
    camera()->getViewport(viewport);
    gluPickMatrix(static_cast<GLdouble>(e->x()), static_cast<GLdouble>(e->y()), SENSITIVITY, SENSITIVITY, viewport);

    // loadProjectionMatrix() first resets the GL_PROJECTION matrix with a glLoadIdentity.
    // Give false as a parameter in order to prevent this and to combine the matrices.
    camera()->loadProjectionMatrix(false);

    camera()->loadModelViewMatrix();
  
    // Render scene with objects ids
    _pSelectRenderer->setSelectRendering(true);
    DrawScene(_pSelectRenderer);
    glFlush();

    // Get the results
    GLint nb_hits = glRenderMode(GL_RENDER);

    if (nb_hits <= 0) {
      _pSelectRenderer->setSelectedId(-1);
      return;
    }
  
    // Interpret results
    unsigned int zMin = hits[1];
    unsigned int selected = hits[3];
    for (int i=1; i<nb_hits; ++i)
      if (hits[i*4+1] < zMin)
	{
	  zMin = hits[i*4+1];
	  selected = hits[i*4+3];
	}
    _pSelectRenderer->setSelectedId(selected);

    cout << "SHAPE" << endl;
    cout << "-----" << endl;
    cout << "Id: " << _pSelectRenderer->getSelectedId() << endl;
    cout << endl;

    return;
  }

  // ViewMap selection

  FEdge *fe = g_pController->SelectFEdge(e->x(), height()-e->y());
  if (!fe)
    return;
  ViewEdge * ve = fe->viewedge();

  if (ve) {
    cout << "VIEWEDGE" << endl;
    cout << "--------" << endl;
    cout << "ViewEdge Id: " << ve->getId().getFirst() << ", " << ve->getId().getSecond() << endl;
    cout << "Shape Id: " << ve->shape_id() << endl;
    cout << "Nature: " << ve->getNature() << endl;
    cout << "QI: " << ve->qi() << endl;
    if(ve->aShape())
      cout << "Occludee: " << ve->aShape()->getId() << endl;
    else
      cout << "Occludee: NULL" << endl ;
    cout << endl;
    
    cout << "FEDGE" << endl;
    cout << "-----" << endl;
    cout << "FEdge Id: " << fe->getId().getFirst() << ", " << fe->getId().getSecond() << endl;
    cout << "Vertex A Id: " << fe->vertexA()->getId() << endl;
    cout << "Vertex B Id: " << fe->vertexB()->getId() << endl;
    cout << endl;
    
    vector<ViewEdge*> vedges;
    vedges.push_back(ve);
    _p2DSelectionNode->AddChild(g_pController->BuildRep(vedges.begin(), vedges.end()));
    // FEdge
    LineRep * fedgeRep = new LineRep(fe->vertexA()->point2d(), fe->vertexB()->point2d());
    fedgeRep->SetWidth(3.f);
    NodeShape * fedgeNode = new NodeShape;
    fedgeNode->AddRep(fedgeRep);
    fedgeNode->material().SetDiffuse(0.2, 1, 0.2, 1.0);
    _p2DSelectionNode->AddChild(fedgeNode);
    //SVertex A
    Vec3r A(fe->vertexA()->point2d());
    VertexRep * aVertexRep = new VertexRep(A.x(), A.y(), A.z());
    aVertexRep->SetPointSize(3.f);
    NodeShape * aVertexNode = new NodeShape;
    aVertexNode->AddRep(aVertexRep);
    aVertexNode->material().SetDiffuse(1, 0, 0, 1.0);
    _p2DSelectionNode->AddChild(aVertexNode);
    // and its fedges
    const vector<FEdge*>& afedges = fe->vertexA()->fedges();
    vector<FEdge*>::const_iterator f=afedges.begin(), fend=afedges.end();
    for(;
    f!=fend;
    ++f)
    {
      LineRep * lrep = new LineRep((*f)->vertexA()->point2d(), (*f)->vertexB()->point2d());
      lrep->SetWidth(1.f);
      aVertexNode->AddRep(lrep);
    }
    //SVertex B
    Vec3r B(fe->vertexB()->point2d());
    VertexRep * bVertexRep = new VertexRep(B.x(), B.y(), B.z());
    bVertexRep->SetPointSize(3.f);
    NodeShape * bVertexNode = new NodeShape;
    bVertexNode->AddRep(bVertexRep);
    bVertexNode->material().SetDiffuse(0, 0, 1, 1.0);
    _p2DSelectionNode->AddChild(bVertexNode);
    // and its fedges
    const vector<FEdge*>& bfedges = fe->vertexB()->fedges();
    f=bfedges.begin();
    fend=bfedges.end();
    for(;
    f!=fend;
    ++f)
    {
      LineRep * lrep = new LineRep((*f)->vertexA()->point2d(), (*f)->vertexB()->point2d());
      lrep->SetWidth(1.f);
      bVertexNode->AddRep(lrep);
    }

  }
}


void
AppGLWidget::mousePressEvent(QMouseEvent *e)
{
  _p2DSelectionNode->destroy();
  if (e->button() == Qt::LeftButton)
  {
	  if(e->modifiers() == Qt::ShiftModifier)
	  {
	      select(e);
      }
	  else if(e->modifiers() == Qt::ControlModifier)
	  {
      // Density Observation
      g_pController->displayDensityCurves(e->x(), height()-1-e->y());
	    }else{
      QGLViewer::mousePressEvent(e);
    }
    updateGL();
  }
  else
    QGLViewer::mousePressEvent(e);
}

void
AppGLWidget::mouseReleaseEvent  (  QMouseEvent *    e  ) 
{
  //  if(g_pController)
  //    g_pController->ComputeViewMap();
  //  g_pController->DrawStrokes();
  QGLViewer::mouseReleaseEvent(e);
}

void
AppGLWidget::keyPressEvent(QKeyEvent* e)
{
  switch (e->key()) {

  case Qt::Key_U:
    _enableUpdateSilhouettes = !_enableUpdateSilhouettes;
    break;
  case Qt::Key_Escape:
    break;
  case Qt::Key_V:
    g_pController->toggleVisibilityAlgo();
    break;
  case Qt::Key_R:
	  if(e->modifiers() == Qt::ShiftModifier){
      _record = !_record;
      if(_record){
        setSnapshotFormat("JPEG");
        setSnapshotFileName("anim");
        g_pController->displayMessage("record", true);
      }else{
        g_pController->displayMessage("");
      }
      
    }
    else if(_cameraStateSaved) {
      setCameraState(_cameraPosition, _cameraOrientation);
      updateGL();
    }
    break;
  case Qt::Key_M: 
    _drawEnvMap = !_drawEnvMap ; 
    updateGL(); break;
  case Qt::Key_Plus:
    Canvas::getInstance()->changePaperTexture(true);updateGL();
    break;
  case Qt::Key_Minus:
    Canvas::getInstance()->changePaperTexture(false);updateGL();
    break;
  case Qt::Key_P:
    Canvas::getInstance()->togglePaperTexture();updateGL();
    break;
  case Qt::Key_PageUp:
	  if(e->modifiers() == Qt::ControlModifier)
      _blendFunc = (_blendFunc + 1) % 2;
    else {
      _currentEnvMap++;
      if(_currentEnvMap > _maxId)
	_currentEnvMap = 1;
    }
    updateGL();
    break;
  case Qt::Key_PageDown:
	  if(e->modifiers() == Qt::ControlModifier)
      _blendFunc = (_blendFunc + 1) % 2;
    else {
      _currentEnvMap--;
      if(_currentEnvMap < 1)
	_currentEnvMap = _maxId;
    }
    updateGL();
    break;
  case Qt::Key_1: _ModelRootNode->SetStyle(DrawingStyle::FILLED); updateGL(); break;
  case Qt::Key_2: _ModelRootNode->SetStyle(DrawingStyle::LINES); _ModelRootNode->SetLineWidth(1.0); updateGL(); break;
  case Qt::Key_3: _ModelRootNode->SetStyle(DrawingStyle::INVISIBLE); updateGL(); break;
  case Qt::Key_B:
    {
//       if(e->state() == ShiftButton)
//         {
//           g_pController->toggleEdgeTesselationNature(Nature::BORDER); updateGL(); break;
//         }
//       else
        {
          _drawBBox == true ? _drawBBox = false : _drawBBox = true; updateGL(); break;
        }
    }
//   case Key_C:
//     if(e->state() == ShiftButton)
//       {
// 	g_pController->toggleEdgeTesselationNature(Nature::CREASE); updateGL(); break;
//       }
//     break;
  case Qt::Key_S:
    {
//       if(e->state() == ShiftButton)
//         {
//           g_pController->toggleEdgeTesselationNature(Nature::SILHOUETTE); updateGL(); break;
//         }
//       else
        {
          _silhouette == true ? _silhouette = false : _silhouette = true; updateGL(); break;
        }
    }
  case Qt::Key_L: 
    {
        _selection_mode = !_selection_mode; updateGL(); break;
    }
    break;
  case Qt::Key_E: 
    {
        _fedges == true ? _fedges = false : _fedges = true; updateGL(); break;
    }
    break;
  case Qt::Key_D: 
    {
      _debug == true ? _debug = false : _debug = true; updateGL();
    }
    break;
  case Qt::Key_F2: _Draw2DScene == true ? _Draw2DScene = false : _Draw2DScene = true; updateGL(); break; 
  case Qt::Key_F3: _Draw3DScene == true ? _Draw3DScene = false : _Draw3DScene = true; updateGL(); break; 
  default:
    QGLViewer::keyPressEvent(e);
  }
}

void AppGLWidget::LoadEnvMap(const char *filename)
{
  GLuint textureId;
  GLubyte *data;
  //sgiImage img;
  //cout << filename << endl;
  QImage img(filename, "PNG");
  QImage glImage = QGLWidget::convertToGLFormat(img);
  int d = glImage.depth();
  //data = img.read(filename); // tres beau bleu gris mauve!!
  // allocate a texture name
  glGenTextures( 1, &textureId );
  if(textureId > _maxId)
    _maxId = textureId;

  // select our current texture
  glBindTexture( GL_TEXTURE_2D, textureId );
  
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 
                   GL_NEAREST);

  glTexImage2D(GL_TEXTURE_2D, 0,GL_RGBA, glImage.width(), glImage.height(), 0,
    GL_RGBA, GL_UNSIGNED_BYTE, glImage.bits() );
}

void AppGLWidget::help(){
  emit helpRequired();

  bool resize = false;
  int width=600;
  int height=400;

  static QString label[] = {" &Keyboard ", " &Mouse "};

  QTabWidget * hWidget = helpWidget();
  if (!hWidget){
    hWidget = new QTabWidget(NULL);
    hWidget->setWindowTitle("Control Bindings");
    resize = true;
    for (int i=0; i<2; ++i){
	    QTextEdit* tab = new QTextEdit(hWidget);
	    //tab->setAcceptRichText(true); // FIXME: commented because qt 4.0 is incomplete
#if QT_VERSION >= 300
	    tab->setReadOnly(true);
#endif
	    hWidget->insertTab(i, tab, label[i]);
    }
  }

#if QT_VERSION < 300
  const int currentPageIndex = hWidget->currentPageIndex();
#endif

  for (int i=0; i<2; ++i)
    {
      QString text;
      switch (i)
	{
	case 0 : text = keyboardString(); break;
	case 1 : text = mouseString();	  break;
	default : break;
	}

#if QT_VERSION < 300
    hWidget->setCurrentPage(i);
    QTextEdit* textEdit = (QTextEdit*)(hWidget->currentPage());
#else
    hWidget->setCurrentIndex(i);
    QTextEdit* textEdit = (QTextEdit*)(hWidget->currentWidget());
#endif
    textEdit->setHtml(text);

    if (resize && (textEdit->heightForWidth(width) > height))
	height = textEdit->heightForWidth(width);
    }
  
#if QT_VERSION < 300
  hWidget->setCurrentPage(currentPageIndex);
#endif

  if (resize)
    hWidget->resize(width, height+40); // 40 is tabs' height
  hWidget->show();
  hWidget->raise();  
}
  
QString AppGLWidget::helpString() const{
  QString pdir(Config::Path::getInstance()->getProjectDir());
  QString text = "<a href=\"" + pdir + "/doc/html/index.html\">help content</a>";
  return text;
}

QString AppGLWidget::mouseString() const{
  QString text("<table border=\"1\" cellspacing=\"0\">\n");
  text += "<tr bgcolor=\"#eebf00\"><th align=\"center\">Button</th><th align=\"center\">Description</th></tr>\n";
  text += "<tr><td><b>Shift+Left</b></td><td>If view map exists, selects a view edge.<br> If in selection mode, selects a shape</td></tr>";
	text += "</table>";
  text += QGLViewer::mouseString();
  return text;
}

QString AppGLWidget::keyboardString() const {

  QString text("<table border=\"1\" cellspacing=\"0\">\n");
  text += "<tr bgcolor=\"#eebf00\"><th align=\"center\">Key</th><th align=\"center\">Description</th></tr>\n";
  text += "<tr><td><b>F2</b></td><td>Toggles 2D Scene display</td></tr>";
  text += "<tr><td><b>F3</b></td><td>Toggles 3D Scene display</td></tr>";

  text += "<tr><td><b>1</b></td><td>Filled display mode</td></tr>";
  text += "<tr><td><b>2</b></td><td>Lines display mode</td></tr>";
  text += "<tr><td><b>3</b></td><td>Invisible display mode</td></tr>";

  text += "<tr><td><b>E</b></td><td>Toggles ViewMap display</td></tr>";
  text += "<tr><td><b>B</b></td><td>Toggles bounding boxes display</td></tr>";
  text += "<tr><td><b>S</b></td><td>Toggles GL silhouettes display</td></tr>";
  text += "<tr><td><b>D</b></td><td>Toggles debug information display</td></tr>";
  text += "<tr><td><b>L</b></td><td>Toggles shape selection mode</td></tr>";
  text += "<tr><td><b>P</b></td><td>Toggles paper texture display</td></tr>";
  text += "<tr><td><b>M</b></td><td>Toggles toon shading</td></tr>";
  text += "<tr><td><b>V</b></td><td>Toggles visibility algorithm</td></tr>";

  text += "<tr><td><b>R</b></td><td>Reset camera to the latest ViewMap computation settings</td></tr>";
  text += "<tr><td><b>Shift+R</b></td><td>Toggles snapshots mode</td></tr>";

  text += "<tr><td><b>U</b></td><td>Recomputes the ViewMap when the view changes</td></tr>";

  text += "<tr><td><b>+/-</b></td><td>Change paper texture</td></tr>";
  text += "<tr><td><b>PgUp/PgDn</b></td><td>Changes EnvMap</td></tr>";
  text += "<tr><td><b>Ctrl+PgUp/PgDn</b></td><td>Changes blending function</td></tr>";
  text += "</table>";
  text += QGLViewer::keyboardString();
  return text;
}

void AppGLWidget::init()
{
  setShortcut(QGLViewer::EXIT_VIEWER, 0);
//  setShortcut(QGLViewer::DISPLAY_Z_BUFFER, 0);
  setShortcut(QGLViewer::STEREO, 0);
  setShortcut(QGLViewer::ANIMATION, 0);
  setShortcut(QGLViewer::EDIT_CAMERA, 0);

  restoreStateFromFile();

  //trackball().fitBBox(_ModelRootNode->bbox().getMin(), _ModelRootNode->bbox().getMax(), _Fovy);

   glClearColor(1,1,1,0);
   glShadeModel(GL_SMOOTH);
  
   glCullFace(GL_BACK);
   glEnable(GL_CULL_FACE);
   glEnable(GL_DEPTH_TEST);

   // open and read texture data
   Config::Path * cpath = Config::Path::getInstance();
   QString envmapDir = cpath->getEnvMapDir();
   LoadEnvMap((envmapDir + QString("gray00.png")).toAscii().data());
   //LoadEnvMap(Config::ENV_MAP_DIR + "gray01.bmp");
   LoadEnvMap((envmapDir + QString("gray02.png")).toAscii().data());
   LoadEnvMap((envmapDir + QString("gray03.png")).toAscii().data());
   LoadEnvMap((envmapDir + QString("brown00.png")).toAscii().data());
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
   setShortcut(QGLViewer::SAVE_SCREENSHOT, Qt::CTRL + Qt::Key_W);
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
   if(_captureMovie)
  {
    if(!camera()->keyFrameInterpolator(0)->interpolationIsStarted())
    {
      _captureMovie = false;
      return;
    }
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

  if(_frontBufferFlag){
    if(_backBufferFlag)
      glDrawBuffer(GL_FRONT_AND_BACK);
    else
      glDrawBuffer(GL_FRONT);
  }
  else if(_backBufferFlag)
    glDrawBuffer(GL_BACK);
  
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
  glDrawBuffer(GL_BACK);
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

