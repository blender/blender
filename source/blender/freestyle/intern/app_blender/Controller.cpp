
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

// Must be included before any QT header, because of moc
#include "../system/PythonInterpreter.h"

#include <string>
#include <fstream>
#include <float.h>

#include "AppGLWidget.h"
#include "AppCanvas.h"
#include "AppConfig.h"


#include "../system/StringUtils.h"
#include "../scene_graph/MaxFileLoader.h"
#include "../scene_graph/NodeShape.h"
#include "../scene_graph/NodeTransform.h"
#include "../scene_graph/NodeDrawingStyle.h"
#include "../winged_edge/WingedEdgeBuilder.h"
#include "../winged_edge/WEdge.h"
#include "../scene_graph/VertexRep.h"
#include "../winged_edge/WXEdgeBuilder.h"
#include "../scene_graph/ScenePrettyPrinter.h"
#include "../winged_edge/WFillGrid.h"

#include "../view_map/ViewMapTesselator.h"
#include "../stroke/StrokeTesselator.h"
#include "../view_map/ViewMapIO.h"
#include "Controller.h"
#include "../view_map/ViewMap.h"
#include "../winged_edge/Curvature.h"
//#include "QGLBasicWidget.h"
//#include <qimage.h>
#include "../image/Image.h"
#include "../view_map/SteerableViewMap.h"
#include "../stroke/PSStrokeRenderer.h"
#include "../stroke/TextStrokeRenderer.h"
#include "../stroke/StyleModule.h"

#ifndef WIN32
//# include "GLXOffscreenBuffer.h"
//# include "GLXOffscreenBuffer.h"
#endif

#include "../system/StringUtils.h"


Controller::Controller()
{
	
  const string sep(Config::DIR_SEP.c_str());
  //const string filename = Config::Path::getInstance()->getHomeDir() + sep + Config::OPTIONS_DIR + sep + Config::OPTIONS_CURRENT_DIRS_FILE;
  //_current_dirs = new ConfigIO(filename, Config::APPLICATION_NAME + "CurrentDirs", true);

  _RootNode = new NodeGroup;
  _RootNode->addRef();
  
  _SilhouetteNode = NULL;
  //_ProjectedSilhouette = NULL;
  //_VisibleProjectedSilhouette = NULL;
  
  _DebugNode = new NodeGroup;
  _DebugNode->addRef();

  _winged_edge = NULL;
  
  _pView = NULL;

  _edgeTesselationNature = (Nature::SILHOUETTE | Nature::BORDER | Nature::CREASE);

  _ProgressBar = new ProgressBar;
  _SceneNumFaces = 0;
  _minEdgeSize = DBL_MAX;
  _bboxDiag = 0;
  
  _ViewMap = 0;

  _Canvas = 0;

  _VisibilityAlgo = ViewMapBuilder::ray_casting;
  //_VisibilityAlgo = ViewMapBuilder::ray_casting_fast;

  _Canvas = new AppCanvas;

  _inter = new PythonInterpreter;
  _EnableQI = true;
  _ComputeRidges = true;
  _ComputeSteerableViewMap = false;
  _ComputeSuggestive = true;
  _sphereRadius = 1.0;

	init_options();
}

Controller::~Controller()
{
  if(NULL != _RootNode)
    {
      int ref = _RootNode->destroy();
      if(0 == ref)
	delete _RootNode;
    }
  
  if(NULL != _SilhouetteNode)
    {
      int ref = _SilhouetteNode->destroy();
      if(0 == ref)
	delete _SilhouetteNode;
    }
  
  if(NULL != _DebugNode)
    {
      int ref = _DebugNode->destroy();
      if(0 == ref)
	delete _DebugNode;
    }

  if(_winged_edge) {
    delete _winged_edge;
    _winged_edge = NULL;
  }

  if(0 != _ViewMap)
    {
      delete _ViewMap;
      _ViewMap = 0;
    }

  if(0 != _Canvas)
    {
      delete _Canvas;
      _Canvas = 0;
    }

  if (_inter) {
    delete _inter;
    _inter = NULL;
  }

  //delete _current_dirs;
}

void Controller::SetView(AppGLWidget *iView)
{
  if(NULL == iView)
    return;
  
  _pView = iView;
  _Canvas->SetViewer(_pView);
}


int Controller::Load3DSFile(const char *iFileName)
{
  if (_pView)
    _pView->setUpdateMode(false);
  
  MaxFileLoader loader3DS(iFileName);
  //_RootNode->AddChild(BuildSceneTest());
  
  _Chrono.start();
  
  NodeGroup *maxScene = loader3DS.Load();

  if (maxScene == NULL) {
	cout << "Cannot load scene" << endl;
    return 1;
  }

  cout << "Scene loaded\n" << endl;

  printf("Mesh cleaning    : %lf\n", _Chrono.stop());
  _SceneNumFaces += loader3DS.numFacesRead();

  if(loader3DS.minEdgeSize() < _minEdgeSize)
    {
      _minEdgeSize = loader3DS.minEdgeSize();
      _EPSILON = _minEdgeSize*1e-6;
      if(_EPSILON < DBL_MIN)
	_EPSILON = 0.0;
    }

  cout << "Epsilon computed : " << _EPSILON << endl;

  // DEBUG
//   ScenePrettyPrinter spp;
//   maxScene->accept(spp);
	
  _RootNode->AddChild(maxScene);
  _RootNode->UpdateBBox(); // FIXME: Correct that by making a Renderer to compute the bbox

  _pView->SetModel(_RootNode);
  _pView->FitBBox();


  _Chrono.start();

  
  WXEdgeBuilder wx_builder;
  maxScene->accept(wx_builder);
  _winged_edge = wx_builder.getWingedEdge();

  printf("WEdge building   : %lf\n", _Chrono.stop());

 _Chrono.start();

  _Grid.clear();
  Vec3r size;
  for(unsigned int i=0; i<3; i++)
    {
      size[i] = fabs(_RootNode->bbox().getMax()[i] - _RootNode->bbox().getMin()[i]);
      size[i] += size[i]/10.0; // let make the grid 1/10 bigger to avoid numerical errors while computing triangles/cells intersections
      if(size[i]==0){
          cout << "Warning: the bbox size is 0 in dimension "<<i<<endl;
      }
    }
  _Grid.configure(Vec3r(_RootNode->bbox().getMin() - size / 20.0), size,
		  _SceneNumFaces);

  // Fill in the grid:
  WFillGrid fillGridRenderer(&_Grid, _winged_edge);
  fillGridRenderer.fillGrid();

  printf("Grid building    : %lf\n", _Chrono.stop());
  
  // DEBUG
//   _Grid.displayDebug();
   
  _pView->SetDebug(_DebugNode);

  //delete stuff
  //  if(0 != ws_builder)
  //    {
  //      delete ws_builder;
  //      ws_builder = 0;
  //    }
  _pView->updateGL();
  

	//soc QFileInfo qfi(iFileName);
	//soc string basename((const char*)qfi.fileName().toAscii().data());
	char cleaned[FILE_MAX];
	BLI_strncpy(cleaned, iFileName, FILE_MAX);
	BLI_cleanup_file(NULL, cleaned);
	string basename = StringUtils::toAscii( string(cleaned) );

  _ListOfModels.push_back(basename);

  cout << "Triangles nb     : " << _SceneNumFaces << endl;
  _bboxDiag = (_RootNode->bbox().getMax()-_RootNode->bbox().getMin()).norm();
  cout << "Bounding Box     : " << _bboxDiag << endl;
  return 0;
}

void Controller::CloseFile()
{
  WShape::SetCurrentId(0);
  _pView->DetachModel();
  _ListOfModels.clear();
  if(NULL != _RootNode)
    {
      int ref = _RootNode->destroy();
      if(0 == ref)
	_RootNode->addRef();
    
      _RootNode->clearBBox();
    }
  
  _pView->DetachSilhouette();
  if (NULL != _SilhouetteNode)
    {
      int ref = _SilhouetteNode->destroy();
      if(0 == ref)
	{
	  delete _SilhouetteNode;
	  _SilhouetteNode = NULL;
	}
    }
  //  if(NULL != _ProjectedSilhouette)
  //    {
  //      int ref = _ProjectedSilhouette->destroy();
  //      if(0 == ref)
  //	{
  //	  delete _ProjectedSilhouette;
  //	  _ProjectedSilhouette = NULL;
  //	}
  //    }
  //  if(NULL != _VisibleProjectedSilhouette)
  //    {
  //      int ref = _VisibleProjectedSilhouette->destroy();
  //      if(0 == ref)
  //	{
  //	  delete _VisibleProjectedSilhouette;
  //	  _VisibleProjectedSilhouette = NULL;
  //	}
  //  }
  
  _pView->DetachDebug();
  if(NULL != _DebugNode)
    {
      int ref = _DebugNode->destroy();
      if(0 == ref)
	_DebugNode->addRef();
    }
  
  if(_winged_edge) {
    delete _winged_edge;
    _winged_edge = NULL;
  }

  // We deallocate the memory:
  if(NULL != _ViewMap)
    {
      delete _ViewMap;
      _ViewMap = 0;
    }

  // clears the canvas
  _Canvas->Erase();

  // clears the grid
  _Grid.clear();
  _SceneNumFaces = 0;
  _minEdgeSize = DBL_MAX;
  //  _pView2D->DetachScene();
  //  if(NULL != _SRoot)
  //  {
  //    int ref = _SRoot->destroy();
  //    if(0 == ref)
  //    {
  //      //_SRoot->addRef();
  //      delete _SRoot;
  //      _SRoot = NULL;
  //    }
  //  }
}

//  static const streamsize buffer_size = 512 * 1024;

void Controller::SaveViewMapFile(const char *oFileName)
{
  if (!_ViewMap)
    return;

  ofstream ofs(oFileName, ios::binary);
  if (!ofs.is_open()) {
    cerr << "Error: Cannot save this file" << endl;
    return;
  }
//    char buffer[buffer_size];
//  #if defined(__GNUC__) && (__GNUC__ < 3)
//    ofs.rdbuf()->setbuf(buffer, buffer_size);
//  # else
//    ofs.rdbuf()->pubsetbuf(buffer, buffer_size);
//  #endif
  _Chrono.start();

  ofs << Config::VIEWMAP_MAGIC << endl << Config::VIEWMAP_VERSION << endl;

  // Write the models filenames
  ofs << _ListOfModels.size() << endl;
  for (vector<string>::const_iterator i = _ListOfModels.begin(); i != _ListOfModels.end(); i++)
    ofs << *i << "\n";

  // Save the camera position
  float position[3];
  float orientation[4];
  _pView->getCameraState(position, orientation);
  ofs.write((char*)position, 3 * sizeof(*position));
  ofs.write((char*)orientation, 4 * sizeof(*orientation));

  // Write ViewMap
  if (ViewMapIO::save(ofs, _ViewMap, 0)) {
    _Chrono.stop();
    cerr << "Error: Cannot save this file" << endl;
    return;
  }

  real d = _Chrono.stop();
  cout << "ViewMap saving   : " << d << endl;
}

void Controller::LoadViewMapFile(const char *iFileName, bool only_camera)
{
  ifstream ifs(iFileName, ios::binary);
  if (!ifs.is_open()) {
    cerr << "Error: Cannot load this file" << endl;
    return;
  }
//    char buffer[buffer_size];
//  #if defined(__GNUC__) && (__GNUC__ < 3)
//    ifs.rdbuf()->setbuf(buffer, buffer_size);
//  # else
//    ifs.rdbuf()->pubsetbuf(buffer, buffer_size);
//  #endif

  // Test File Magic and version
  char tmp_buffer[256];
  string test;
  
  ifs.getline(tmp_buffer, 255);
  test = tmp_buffer;
  if (test != Config::VIEWMAP_MAGIC) {
    cerr << "Error: This is not a valid ." << Config::VIEWMAP_EXTENSION << " file" << endl;
    return;
  }
  ifs.getline(tmp_buffer, 255);
  test = tmp_buffer;
  if (test != Config::VIEWMAP_VERSION && !only_camera) {
    cerr << "Error: This version of the ." << Config::VIEWMAP_EXTENSION << " file format is no longer supported" << endl;
    return;
  }

  // Read the models filenames and open them (if not already done)
  string tmp;
  vector<string> tmp_vec;
  unsigned models_nb, i;

  ifs.getline(tmp_buffer, 255);
  models_nb = atoi(tmp_buffer);
  for (i = 0; i < models_nb; i++) {
    ifs.getline(tmp_buffer, 255);
    tmp = tmp_buffer;
    tmp_vec.push_back(tmp);
  }
  if (_ListOfModels != tmp_vec && !only_camera) {
    CloseFile();
    vector<string> pathnames;
    int err = 0;
    for (vector<string>::const_iterator i = tmp_vec.begin(); i != tmp_vec.end(); i++)
      {
	pathnames.clear();
	StringUtils::getPathName(ViewMapIO::Options::getModelsPath(), *i, pathnames);
	for (vector<string>::const_iterator j = pathnames.begin(); j != pathnames.end(); j++)
	  if (!(err = Load3DSFile(j->c_str())))
	    break;
	if (err) {
	  cerr << "Error: cannot find model \"" << *i << "\" - check the path in the Options" << endl;
	  return;
	}
      }
  }

  // Set the camera position
  float position[3];
  float orientation[4];
  ifs.read((char*)position, 3 * sizeof(*position));
  ifs.read((char*)orientation, 4 * sizeof(*orientation));
  _pView->setCameraState(position, orientation);
  _pView->saveCameraState();

  if (only_camera) {
    return;
  }

  // Reset ViewMap
  if(NULL != _ViewMap)
    {
      delete _ViewMap;
      _ViewMap = 0;
    }
  _pView->DetachSilhouette();
  if (NULL != _SilhouetteNode)
    {
      int ref = _SilhouetteNode->destroy();
      if(0 == ref)
	delete _SilhouetteNode;
    }
  //  if(NULL != _ProjectedSilhouette)
  //    {
  //      int ref = _ProjectedSilhouette->destroy();
  //      if(0 == ref)
  //	delete _ProjectedSilhouette;
  //    }
  //  if(NULL != _VisibleProjectedSilhouette)
  //    {
  //      int ref = _VisibleProjectedSilhouette->destroy();
  //      if(0 == ref)
  //	{
  //	  delete _VisibleProjectedSilhouette;
  //	  _VisibleProjectedSilhouette = 0;
  //	}
   // }
  _ViewMap = new ViewMap();

  // Read ViewMap
  _Chrono.start();
  if (ViewMapIO::load(ifs, _ViewMap, 0)) {
    _Chrono.stop();

    cerr << "Error: This is not a valid ." << Config::VIEWMAP_EXTENSION << " file" << endl;
    return;
  }

  // Update display
  ViewMapTesselator3D sTesselator3d;
  //ViewMapTesselator2D sTesselator2d;
  //sTesselator2d.SetNature(_edgeTesselationNature);
  sTesselator3d.SetNature(_edgeTesselationNature);
  
  // Tesselate the 3D edges:
  _SilhouetteNode = sTesselator3d.Tesselate(_ViewMap);
  _SilhouetteNode->addRef();
  
  // Tesselate 2D edges
  //  _ProjectedSilhouette = sTesselator2d.Tesselate(_ViewMap);
  //  _ProjectedSilhouette->addRef();
  //  
  _pView->AddSilhouette(_SilhouetteNode);
  //_pView->Add2DSilhouette(_ProjectedSilhouette);

  // Update options window
  //_pOptionsWindow->updateViewMapFormat();

  real d = _Chrono.stop();
  cout << "ViewMap loading  : " << d << endl;

  // Compute the Directional ViewMap:
  if(_ComputeSteerableViewMap){
    ComputeSteerableViewMap();
  }

  // Reset Style modules modification flags
  resetModified(true);
}

void Controller::ComputeViewMap()
{

  if (!_ListOfModels.size())
    return;
  
  if(NULL != _ViewMap)
    {
      delete _ViewMap;
      _ViewMap = 0;
    }

  _pView->DetachDebug();
  if(NULL != _DebugNode)
    {
      int ref = _DebugNode->destroy();
      if(0 == ref)
	_DebugNode->addRef();
    }
  

  _pView->DetachSilhouette();
  if (NULL != _SilhouetteNode)
    {
      int ref = _SilhouetteNode->destroy();
      if(0 == ref)
	delete _SilhouetteNode;
    }
  //  if(NULL != _ProjectedSilhouette)
  //    {
  //      int ref = _ProjectedSilhouette->destroy();
  //      if(0 == ref)
  //	delete _ProjectedSilhouette;
  //    }
  //  if(NULL != _VisibleProjectedSilhouette)
  //    {
  //      int ref = _VisibleProjectedSilhouette->destroy();
  //      if(0 == ref)
  //	{
  //	  delete _VisibleProjectedSilhouette;
  //	  _VisibleProjectedSilhouette = 0;
  //	}
  //  }
  
  // retrieve the 3D viewpoint and transformations information
  //----------------------------------------------------------
  // Save the viewpoint context at the view level in order 
  // to be able to restore it later:
  _pView->saveCameraState();

  // Restore the context of view:
  // we need to perform all these operations while the 
  // 3D context is on.
  _pView->Set3DContext();
  float src[3] = { 0, 0, 0 };
  float vp_tmp[3] = { 0, 0, 0 };
  _pView->_camera->getWorldCoordinatesOf(src, vp_tmp);
  Vec3r vp(vp_tmp[0], vp_tmp[1], vp_tmp[2]);

  real mv[4][4];
  _pView->RetriveModelViewMatrix((real *)mv);
  // retrieve the projection matrix:
  real proj[4][4];
  _pView->RetrieveProjectionMatrix((real *)proj);
  int viewport[4];
  _pView->RetrieveViewport(viewport);  
  real focalLength = _pView->GetFocalLength();

  // Flag the WXEdge structure for silhouette edge detection:
  //----------------------------------------------------------

  _Chrono.start();
 
  edgeDetector.SetViewpoint(Vec3r(vp));
  edgeDetector.enableRidgesAndValleysFlag(_ComputeRidges);
  edgeDetector.enableSuggestiveContours(_ComputeSuggestive);
  edgeDetector.setSphereRadius(_sphereRadius);
  edgeDetector.setSuggestiveContourKrDerivativeEpsilon(_suggestiveContourKrDerivativeEpsilon);
  edgeDetector.processShapes(*_winged_edge);

  real duration = _Chrono.stop();
  printf("Feature lines    : %lf\n", duration);

  // Builds the view map structure from the flagged WSEdge structure:
  //----------------------------------------------------------
  ViewMapBuilder vmBuilder;
  vmBuilder.SetEnableQI(_EnableQI);
  vmBuilder.SetViewpoint(Vec3r(vp));
  
  vmBuilder.SetTransform(mv, proj, viewport, focalLength, _pView->GetAspect(), _pView->GetFovyRadian());
  vmBuilder.SetFrustum(_pView->znear(), _pView->zfar());
  
  vmBuilder.SetGrid(&_Grid);
  
  // Builds a tesselated form of the silhouette for display purpose:
  //---------------------------------------------------------------
  ViewMapTesselator3D sTesselator3d;
  //ViewMapTesselator2D sTesselator2d;
  //sTesselator2d.SetNature(_edgeTesselationNature);
  sTesselator3d.SetNature(_edgeTesselationNature);
    
  _Chrono.start();
  // Build View Map
  _ViewMap = vmBuilder.BuildViewMap(*_winged_edge, _VisibilityAlgo, _EPSILON);
  _ViewMap->setScene3dBBox(_RootNode->bbox());
  
  //Tesselate the 3D edges:
  _SilhouetteNode = sTesselator3d.Tesselate(_ViewMap);
  _SilhouetteNode->addRef();
  
  // Tesselate 2D edges
  //  _ProjectedSilhouette = sTesselator2d.Tesselate(_ViewMap);
  //  _ProjectedSilhouette->addRef();
  
  duration = _Chrono.stop();
  printf("ViewMap building : %lf\n", duration);

  
  _pView->AddSilhouette(_SilhouetteNode);
  //_pView->AddSilhouette(_WRoot);
  //_pView->Add2DSilhouette(_ProjectedSilhouette);
  //_pView->Add2DVisibleSilhouette(_VisibleProjectedSilhouette);  
  _pView->AddDebug(_DebugNode);

  // Draw the steerable density map:
  //--------------------------------
  if(_ComputeSteerableViewMap){
    ComputeSteerableViewMap();
  }
  // Reset Style modules modification flags
  resetModified(true);
}

void Controller::ComputeSteerableViewMap(){
//soc
  // if((!_Canvas) || (!_ViewMap))
  //   return;
  //   
  // // Build 4 nodes containing the edges in the 4 directions
  // NodeGroup *ng[Canvas::NB_STEERABLE_VIEWMAP];
  // unsigned i;
  // real c = 32.f/255.f; // see SteerableViewMap::readSteerableViewMapPixel() for information about this 32.
  // for(i=0; i<Canvas::NB_STEERABLE_VIEWMAP; ++i){
  //   ng[i] = new NodeGroup;
  // }
  // NodeShape *completeNS = new NodeShape;
  // completeNS->material().SetDiffuse(c,c,c,1);
  // ng[Canvas::NB_STEERABLE_VIEWMAP-1]->AddChild(completeNS);
  // SteerableViewMap * svm = _Canvas->getSteerableViewMap();
  // svm->Reset();
  // 
  // ViewMap::fedges_container& fedges = _ViewMap->FEdges();
  // LineRep * fRep;
  // NodeShape *ns;
  // for(ViewMap::fedges_container::iterator f=fedges.begin(), fend=fedges.end();
  // f!=fend;
  // ++f){
  //   if((*f)->viewedge()->qi() != 0)
  //     continue;
  //   fRep = new LineRep((*f)->vertexA()->point2d(),(*f)->vertexB()->point2d()) ;
  //   completeNS->AddRep(fRep); // add to the complete map anyway
  //   double *oweights = svm->AddFEdge(*f);
  //   for(i=0; i<Canvas::NB_STEERABLE_VIEWMAP-1; ++i){
  //     ns = new NodeShape;
  //     double wc = oweights[i]*c;
  //     if(oweights[i] == 0)
  //       continue;
  //     ns->material().SetDiffuse(wc, wc, wc, 1);
  //     ns->AddRep(fRep);
  //     ng[i]->AddChild(ns);
  //   }
  // }
  // 
  // GrayImage *img[Canvas::NB_STEERABLE_VIEWMAP];
  // //#ifdef WIN32
  // QGLBasicWidget offscreenBuffer(_pView, "SteerableViewMap", _pView->width(), _pView->height()); 
  // QPixmap pm;
  // QImage qimg;
  // for(i=0; i<Canvas::NB_STEERABLE_VIEWMAP; ++i){
  //   offscreenBuffer.AddNode(ng[i]);
  //   //img[i] = new GrayImage(_pView->width(), _pView->height());
  //   //offscreenBuffer.readPixels(0,0,_pView->width(), _pView->height(), img[i]->getArray());
  //   pm = offscreenBuffer.renderPixmap(_pView->width(), _pView->height());
  // 
  //   if(pm.isNull())
  //     cout << "BuildViewMap Warning: couldn't render the steerable ViewMap" << endl;
  //   //pm.save(QString("steerable")+QString::number(i)+QString(".bmp"), "BMP");
  //   // FIXME!! Lost of time !
  //   qimg = pm.toImage();
  //   // FIXME !! again!
  //   img[i] = new GrayImage(_pView->width(), _pView->height());
  //   for(unsigned y=0;y<img[i]->height();++y){
  //     for(unsigned x=0;x<img[i]->width();++x){
  //       //img[i]->setPixel(x,y,(float)qGray(qimg.pixel(x,y))/255.f);
  //       img[i]->setPixel(x,y,(float)qGray(qimg.pixel(x,y)));
  //       //        float c = qGray(qimg.pixel(x,y));
  //       //        img[i]->setPixel(x,y,qGray(qimg.pixel(x,y)));
  //     }
  //   }
  //   offscreenBuffer.DetachNode(ng[i]);
  //   ng[i]->destroy();
  //   delete ng[i];
  //   // check
  //   //    qimg = QImage(_pView->width(), _pView->height(), 32);
  //   //    for(y=0;y<img[i]->height();++y){
  //   //      for(unsigned x=0;x<img[i]->width();++x){
  //   //        float v = img[i]->pixel(x,y);
  //   //        qimg.setPixel(x,y,qRgb(v,v,v));
  //   //      }
  //   //    }
  //   //    qimg.save(QString("newsteerable")+QString::number(i)+QString(".bmp"), "BMP");
  // }
  // 
  // 
  // svm->buildImagesPyramids(img,false,0,1.f);
}

void Controller::saveSteerableViewMapImages(){
  SteerableViewMap * svm = _Canvas->getSteerableViewMap();
  if(!svm){
    cerr << "the Steerable ViewMap has not been computed yet" << endl;
    return;
  }
  svm->saveSteerableViewMap();
}

void Controller::toggleVisibilityAlgo() 
{
  if(_VisibilityAlgo == ViewMapBuilder::ray_casting) {
    _VisibilityAlgo = ViewMapBuilder::ray_casting_fast;
  }
  else if (_VisibilityAlgo == ViewMapBuilder::ray_casting_fast) {
    _VisibilityAlgo = ViewMapBuilder::ray_casting_very_fast;
  }
  else {
    _VisibilityAlgo = ViewMapBuilder::ray_casting;
  }
}

void Controller::setQuantitativeInvisibility(bool iBool)
{
  _EnableQI = iBool;
}

bool Controller::getQuantitativeInvisibility() const
{
  return _EnableQI;
}

void Controller::setComputeRidgesAndValleysFlag(bool iBool){
  _ComputeRidges = iBool;
}

bool Controller::getComputeRidgesAndValleysFlag() const {
  return _ComputeRidges;
}
void Controller::setComputeSuggestiveContoursFlag(bool b){
  _ComputeSuggestive = b;
}
  
bool Controller::getComputeSuggestiveContoursFlag() const {
  return _ComputeSuggestive;
}
void Controller::setComputeSteerableViewMapFlag(bool iBool){
  _ComputeSteerableViewMap = iBool;
}

bool Controller::getComputeSteerableViewMapFlag() const {
  return _ComputeSteerableViewMap;
}
void Controller::setFrontBufferFlag(bool iBool)
{
  AppGLWidget::setFrontBufferFlag(iBool);
}

bool Controller::getFrontBufferFlag() const
{
  return AppGLWidget::getFrontBufferFlag();
}

void Controller::setBackBufferFlag(bool iBool)
{
  AppGLWidget::setBackBufferFlag(iBool);
}

bool Controller::getBackBufferFlag() const
{
  return AppGLWidget::getBackBufferFlag();
}

void Controller::DrawStrokes()
{
  if(_ViewMap == 0)
    return;

  _Chrono.start();
  _Canvas->Draw();
  real d = _Chrono.stop();
  cout << "Strokes drawing  : " << d << endl;
  resetModified();
}

void Controller::InsertStyleModule(unsigned index, const char *iFileName)
{
  // QFileInfo fi(iFileName);
  // string ext = fi.suffix();
  // if (ext != "py") {
  //   cerr << "Error: Cannot load \"" << fi.fileName().toAscii().data()
  // 	 << "\", unknown extension" << endl;
  //   return;
  // }

	if( !BLI_testextensie(iFileName, ".py") ) {
		cerr << "Error: Cannot load \"" << StringUtils::toAscii( string(iFileName) )
		  << "\", unknown extension" << endl;
		  return;
	}

  StyleModule* sm = new StyleModule(iFileName, _inter);
  _Canvas->InsertStyleModule(index, sm);
  
}

void Controller::AddStyleModule(const char *iFileName)
{
  //_pStyleWindow->Add(iFileName);
}

void Controller::RemoveStyleModule(unsigned index)
{
  _Canvas->RemoveStyleModule(index);
}

void Controller::Clear()
{
  _Canvas->Clear();
}

void Controller::ReloadStyleModule(unsigned index, const char * iFileName)
{
  StyleModule* sm = new StyleModule(iFileName, _inter);
  _Canvas->ReplaceStyleModule(index, sm);
}

void Controller::SwapStyleModules(unsigned i1, unsigned i2)
{
  _Canvas->SwapStyleModules(i1, i2);
}


void Controller::toggleLayer(unsigned index, bool iDisplay)
{
  _Canvas->SetVisible(index, iDisplay);
  _pView->updateGL();
}

void Controller::setModified(unsigned index, bool iMod)
{
  //_pStyleWindow->setModified(index, iMod);
  _Canvas->setModified(index, iMod);
  updateCausalStyleModules(index + 1);
}

void Controller::updateCausalStyleModules(unsigned index) {
  vector<unsigned> vec;
  _Canvas->causalStyleModules(vec, index);
  for (vector<unsigned>::const_iterator it = vec.begin(); it != vec.end(); it++) {
    //_pStyleWindow->setModified(*it, true);
    _Canvas->setModified(*it, true);
  }
}

void Controller::saveSnapshot(bool b) {
 _pView->saveSnapshot(b);
}

void Controller::resetModified(bool iMod)
{
  //_pStyleWindow->resetModified(iMod);
  _Canvas->resetModified(iMod);
}

FEdge* Controller::SelectFEdge(real x, real y)
{
  if (!_ViewMap)
    return NULL;

  FEdge *fedge = (FEdge*)_ViewMap->GetClosestFEdge(x,y);
  //ViewEdge *selection = fedge->viewedge();
  _pView->SetSelectedFEdge(fedge);
  _Canvas->SetSelectedFEdge(fedge);
  return fedge;
}

ViewEdge* Controller::SelectViewEdge(real x, real y)
{
  if (!_ViewMap)
    return NULL;

  FEdge *fedge = (FEdge*)_ViewMap->GetClosestFEdge(x,y);
  ViewEdge *selection = fedge->viewedge();
  _pView->SetSelectedFEdge(fedge);
  _Canvas->SetSelectedFEdge(fedge);
  return selection;
}

NodeGroup * Controller::BuildRep(vector<ViewEdge*>::iterator vedges_begin, 
                                       vector<ViewEdge*>::iterator vedges_end)
{
  ViewMapTesselator2D tesselator2D;
  Material mat;
  mat.SetDiffuse(1,1,0.3,1);
  tesselator2D.SetMaterial(mat);

  return (tesselator2D.Tesselate(vedges_begin, vedges_end));
}

void Controller::toggleEdgeTesselationNature(Nature::EdgeNature iNature)
{
  _edgeTesselationNature ^= (iNature);
  ComputeViewMap();
}

void		Controller::setModelsDir(const string& dir) {
  //_current_dirs->setValue("models/dir", dir);
}

string		Controller::getModelsDir() const {
  string dir = ".";
  //_current_dirs->getValue("models/dir", dir);
  return dir;
}

void		Controller::setModulesDir(const string& dir) {
  //_current_dirs->setValue("modules/dir", dir);
}

string		Controller::getModulesDir() const {
  string dir = ".";
  //_current_dirs->getValue("modules/dir", dir);
  return dir;
}

void		Controller::setPapersDir(const string& dir) {
  //_current_dirs->setValue("papers/dir", dir);
}

string		Controller::getPapersDir() const {
  string dir = Config::Path::getInstance()->getPapersDir();
  //_current_dirs->getValue("papers/dir", dir);
  return dir;
}

void		Controller::setHelpIndex(const string& index) {
  _help_index = index;
}

string		Controller::getHelpIndex() const {
  return _help_index;
}

void		Controller::setBrowserCmd(const string& cmd) {
  _browser_cmd = cmd;
}

string		Controller::getBrowserCmd() const {
  return _browser_cmd;
}

void		Controller::resetInterpreter() {
  if (_inter)
    _inter->reset();
}


void Controller::displayDensityCurves(int x, int y){
  SteerableViewMap * svm = _Canvas->getSteerableViewMap();
  if(!svm)
    return;

  unsigned i,j;
  typedef vector<Vec3r> densityCurve;
  vector<densityCurve> curves(svm->getNumberOfOrientations()+1);
  vector<densityCurve> curvesDirection(svm->getNumberOfPyramidLevels());

  // collect the curves values
  unsigned nbCurves = svm->getNumberOfOrientations()+1;
  unsigned nbPoints = svm->getNumberOfPyramidLevels();
  if(!nbPoints)
    return;

  // build the density/nbLevels curves for each orientation
  for(i=0;i<nbCurves; ++i){
    for(j=0; j<nbPoints; ++j){
      curves[i].push_back(Vec3r(j, svm->readSteerableViewMapPixel(i, j, x, y), 0));
    }
  }
  // build the density/nbOrientations curves for each level
  for(i=0;i<nbPoints; ++i){
    for(j=0; j<nbCurves; ++j){
      curvesDirection[i].push_back(Vec3r(j, svm->readSteerableViewMapPixel(j, i, x, y), 0));
    }
  }

  // display the curves
  // for(i=0; i<nbCurves; ++i)
  //     _pDensityCurvesWindow->SetOrientationCurve(i, Vec2d(0,0), Vec2d(nbPoints, 1), curves[i], "scale", "density");
  //   for(i=1; i<=8; ++i)
  //     _pDensityCurvesWindow->SetLevelCurve(i, Vec2d(0,0), Vec2d(nbCurves, 1), curvesDirection[i], "orientation", "density");
  //   _pDensityCurvesWindow->show();
}

void Controller::init_options(){
// 	//from AppOptionsWindow.cpp
// 	
// 	// Directories
//   ViewMapIO::Options::setModelsPath((const char*)modelsPathLineEdit->text().toAscii().data());
//   PythonInterpreter::Options::setPythonPath((const char*)pythonPathLineEdit->text().toAscii().data());
//   TextureManager::Options::setPatternsPath((const char*)patternsPathLineEdit->text().toAscii().data());
//   TextureManager::Options::setBrushesPath((const char*)brushesPathLineEdit->text().toAscii().data());
//   //g_pController->setBrowserCmd(browserCmdLineEdit->text());
//   //g_pController->setHelpIndex(helpIndexPathLineEdit->text());
// 
//   // ViewMap Format
//   if (asFloatCheckBox->isChecked())
//     ViewMapIO::Options::addFlags(ViewMapIO::Options::FLOAT_VECTORS);
//   else
//     ViewMapIO::Options::rmFlags(ViewMapIO::Options::FLOAT_VECTORS);
//   if (noOccluderListCheckBox->isChecked())
//     ViewMapIO::Options::addFlags(ViewMapIO::Options::NO_OCCLUDERS);
//   else
//     ViewMapIO::Options::rmFlags(ViewMapIO::Options::NO_OCCLUDERS);
//   g_pController->setComputeSteerableViewMapFlag(steerableViewMapCheckBox->isChecked());
// 
//   // Visibility
//   if (qiCheckBox->isChecked())
//     g_pController->setQuantitativeInvisibility(true);
//   else
//     g_pController->setQuantitativeInvisibility(false);
// 
//   // Papers Textures
//   vector<string> sl;
//   for (unsigned i = 0; i < paperTexturesList->count(); i++) {
//     sl.push_back(paperTexturesList->item(i)->text().toAscii().constData());
//   }
//   TextureManager::Options::setPaperTextures(sl);
// 
//    // Drawing Buffers
//   if (frontBufferCheckBox->isChecked())
//     g_pController->setFrontBufferFlag(true);
//   else
//     g_pController->setFrontBufferFlag(false);
//   if (backBufferCheckBox->isChecked())
//     g_pController->setBackBufferFlag(true);
//   else
//     g_pController->setBackBufferFlag(false);
// 
//   // Ridges and Valleys
//   g_pController->setComputeRidgesAndValleysFlag(ridgeValleyCheckBox->isChecked());
//   // Suggestive Contours
//   g_pController->setComputeSuggestiveContoursFlag(suggestiveContoursCheckBox->isChecked());
//   bool ok;
//   real r = sphereRadiusLineEdit->text().toFloat(&ok);
//   if(ok)
//     g_pController->setSphereRadius(r);
//   else
//     sphereRadiusLineEdit->setText(QString(QString::number(g_pController->getSphereRadius())));
//   r = krEpsilonLineEdit->text().toFloat(&ok);
//   if(ok)
//     g_pController->setSuggestiveContourKrDerivativeEpsilon(r);
//   else
//     krEpsilonLineEdit->setText(QString(QString::number(g_pController->getSuggestiveContourKrDerivativeEpsilon())));
// }
}
