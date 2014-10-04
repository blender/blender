/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/freestyle/intern/application/Controller.cpp
 *  \ingroup freestyle
 */

extern "C" {
#include <Python.h>
}

#include <string>
#include <fstream>
#include <float.h>

#include "AppView.h"
#include "AppCanvas.h"
#include "AppConfig.h"
#include "Controller.h"

#include "../image/Image.h"

#include "../scene_graph/NodeDrawingStyle.h"
#include "../scene_graph/NodeShape.h"
#include "../scene_graph/NodeTransform.h"
#include "../scene_graph/ScenePrettyPrinter.h"
#include "../scene_graph/VertexRep.h"

#include "../stroke/PSStrokeRenderer.h"
#include "../stroke/TextStrokeRenderer.h"
#include "../stroke/StrokeTesselator.h"
#include "../stroke/StyleModule.h"

#include "../system/StringUtils.h"
#include "../system/PythonInterpreter.h"

#include "../view_map/SteerableViewMap.h"
#include "../view_map/ViewMap.h"
#include "../view_map/ViewMapIO.h"
#include "../view_map/ViewMapTesselator.h"

#include "../winged_edge/Curvature.h"
#include "../winged_edge/WEdge.h"
#include "../winged_edge/WingedEdgeBuilder.h"
#include "../winged_edge/WXEdgeBuilder.h"

#include "../blender_interface/BlenderFileLoader.h"
#include "../blender_interface/BlenderStrokeRenderer.h"
#include "../blender_interface/BlenderStyleModule.h"

#include "BKE_global.h"
#include "BLI_utildefines.h"
#include "BLI_path_util.h"

#include "DNA_freestyle_types.h"

#include "FRS_freestyle.h"

namespace Freestyle {

Controller::Controller()
{
	const string sep(Config::DIR_SEP.c_str());
#if 0
	const string filename = Config::Path::getInstance()->getHomeDir() + sep + Config::OPTIONS_DIR + sep +
	                        Config::OPTIONS_CURRENT_DIRS_FILE;
	_current_dirs = new ConfigIO(filename, Config::APPLICATION_NAME + "CurrentDirs", true);
#endif

	_RootNode = new NodeGroup;
	_RootNode->addRef();

	_SilhouetteNode = NULL;
#if 0
	_ProjectedSilhouette = NULL;
	_VisibleProjectedSilhouette = NULL;
#endif

	_DebugNode = new NodeGroup;
	_DebugNode->addRef();

	_winged_edge = NULL;

	_pView = NULL;
	_pRenderMonitor = NULL;

	_edgeTesselationNature = (Nature::SILHOUETTE | Nature::BORDER | Nature::CREASE);

	_ProgressBar = new ProgressBar;
	_SceneNumFaces = 0;
	_minEdgeSize = DBL_MAX;
	_EPSILON = 1.0e-6;
	_bboxDiag = 0;

	_ViewMap = 0;

	_Canvas = 0;

	_VisibilityAlgo = ViewMapBuilder::ray_casting_adaptive_traditional;
	//_VisibilityAlgo = ViewMapBuilder::ray_casting;

	_Canvas = new AppCanvas;

	_inter = new PythonInterpreter();
	_EnableViewMapCache = false;
	_EnableQI = true;
	_EnableFaceSmoothness = false;
	_ComputeRidges = true;
	_ComputeSteerableViewMap = false;
	_ComputeSuggestive = true;
	_ComputeMaterialBoundaries = true;
	_sphereRadius = 1.0;
	_creaseAngle = 134.43;
	prevSceneHash = -1.0;

	init_options();
}

Controller::~Controller()
{
	if (NULL != _RootNode) {
		int ref = _RootNode->destroy();
		if (0 == ref)
			delete _RootNode;
	}

	if (NULL != _SilhouetteNode) {
		int ref = _SilhouetteNode->destroy();
		if (0 == ref)
			delete _SilhouetteNode;
	}

	if (NULL != _DebugNode) {
		int ref = _DebugNode->destroy();
		if (0 == ref)
			delete _DebugNode;
	}

	if (_winged_edge) {
		delete _winged_edge;
		_winged_edge = NULL;
	}

	if (0 != _ViewMap) {
		delete _ViewMap;
		_ViewMap = 0;
	}

	if (0 != _Canvas) {
		delete _Canvas;
		_Canvas = 0;
	}

	if (_inter) {
		delete _inter;
		_inter = NULL;
	}

	if (_ProgressBar) {
		delete _ProgressBar;
		_ProgressBar = NULL;
	}

	//delete _current_dirs;
}

void Controller::setView(AppView *iView)
{
	if (NULL == iView)
		return;

	_pView = iView;
	_Canvas->setViewer(_pView);
}

void Controller::setRenderMonitor(RenderMonitor *iRenderMonitor)
{
	_pRenderMonitor = iRenderMonitor;
}

void Controller::setPassDiffuse(float *buf, int width, int height)
{
	AppCanvas *app_canvas = dynamic_cast<AppCanvas *>(_Canvas);
	BLI_assert(app_canvas != 0);
	app_canvas->setPassDiffuse(buf, width, height);
}

void Controller::setPassZ(float *buf, int width, int height)
{
	AppCanvas *app_canvas = dynamic_cast<AppCanvas *>(_Canvas);
	BLI_assert(app_canvas != 0);
	app_canvas->setPassZ(buf, width, height);
}

void Controller::setContext(bContext *C)
{
	PythonInterpreter *py_inter = dynamic_cast<PythonInterpreter*>(_inter);
	py_inter->setContext(C);
}

bool Controller::hitViewMapCache()
{
	if (!_EnableViewMapCache) {
		return false;
	}
	if (sceneHashFunc.match()) {
		return (NULL != _ViewMap);
	}
	sceneHashFunc.store();
	return false;
}

int Controller::LoadMesh(Render *re, SceneRenderLayer *srl)
{
	BlenderFileLoader loader(re, srl);

	loader.setRenderMonitor(_pRenderMonitor);

	_Chrono.start();

	NodeGroup *blenderScene = loader.Load();

	if (blenderScene == NULL) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "Cannot load scene" << endl;
		}
		return 1;
	}

	if (blenderScene->numberOfChildren() < 1) {
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "Empty scene" << endl;
		}
		blenderScene->destroy();
		delete blenderScene;
		return 1;
	}

	real duration = _Chrono.stop();
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "Scene loaded" << endl;
		printf("Mesh cleaning    : %lf\n", duration);
		printf("View map cache   : %s\n", _EnableViewMapCache ? "enabled" : "disabled");
	}
	_SceneNumFaces += loader.numFacesRead();

	if (loader.minEdgeSize() < _minEdgeSize) {
		_minEdgeSize = loader.minEdgeSize();
	}

#if 0  // DEBUG
	ScenePrettyPrinter spp;
	blenderScene->accept(spp);
#endif

	_RootNode->AddChild(blenderScene);
	_RootNode->UpdateBBox(); // FIXME: Correct that by making a Renderer to compute the bbox

	_pView->setModel(_RootNode);
	//_pView->FitBBox();

	if (_pRenderMonitor->testBreak())
		return 0;

	if (_EnableViewMapCache) {

		NodeCamera *cam;
		if (freestyle_proj[3][3] != 0.0)
			cam = new NodeOrthographicCamera;
		else
			cam = new NodePerspectiveCamera;
		double proj[16];
		for (int i = 0; i < 4; i++) {
			for (int j = 0; j < 4; j++) {
				proj[i * 4 + j] = freestyle_proj[i][j];
			}
		}
		cam->setProjectionMatrix(proj);
		_RootNode->AddChild(cam);

		sceneHashFunc.reset();
		//blenderScene->accept(sceneHashFunc);
		_RootNode->accept(sceneHashFunc);
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "Scene hash       : " << sceneHashFunc.toString() << endl;
		}
		if (hitViewMapCache()) {
			ClearRootNode();
			return 0;
		}
		else {
			delete _ViewMap;
			_ViewMap = NULL;
		}
	}

	_Chrono.start();

	WXEdgeBuilder wx_builder;
	wx_builder.setRenderMonitor(_pRenderMonitor);
	blenderScene->accept(wx_builder);
	_winged_edge = wx_builder.getWingedEdge();

	duration = _Chrono.stop();
	if (G.debug & G_DEBUG_FREESTYLE) {
		printf("WEdge building   : %lf\n", duration);
	}

#if 0
	_pView->setDebug(_DebugNode);

	// delete stuff
	if (0 != ws_builder) {
		delete ws_builder;
		ws_builder = 0;
	}

	soc QFileInfo qfi(iFileName);
	soc string basename((const char*)qfi.fileName().toAscii().data());
	char cleaned[FILE_MAX];
	BLI_strncpy(cleaned, iFileName, FILE_MAX);
	BLI_cleanup_file(NULL, cleaned);
	string basename = string(cleaned);
#endif

	_ListOfModels.push_back("Blender_models");

	_Scene3dBBox = _RootNode->bbox();

	_bboxDiag = (_RootNode->bbox().getMax() - _RootNode->bbox().getMin()).norm();
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "Triangles nb     : " << _SceneNumFaces << " imported, " <<
		        _winged_edge->getNumFaces() << " retained" << endl;
		cout << "Bounding Box     : " << _bboxDiag << endl;
	}

	ClearRootNode();

	_SceneNumFaces = _winged_edge->getNumFaces();
	if (_SceneNumFaces == 0) {
		DeleteWingedEdge();
		return 1;
	}

	return 0;
}

void Controller::CloseFile()
{
	WShape::setCurrentId(0);
	_ListOfModels.clear();

	// We deallocate the memory:
	ClearRootNode();
	DeleteWingedEdge();
	DeleteViewMap();

	// clears the canvas
	_Canvas->Clear();

	// soc: reset passes
	setPassDiffuse(NULL, 0, 0);
	setPassZ(NULL, 0, 0);
}

void Controller::ClearRootNode()
{
	_pView->DetachModel();
	if (NULL != _RootNode) {
		int ref = _RootNode->destroy();
		if (0 == ref)
			_RootNode->addRef();
		_RootNode->clearBBox();
	}
}

void Controller::DeleteWingedEdge()
{
	if (_winged_edge) {
		delete _winged_edge;
		_winged_edge = NULL;
	}

	// clears the grid
	_Grid.clear();
	_Scene3dBBox.clear();
	_SceneNumFaces = 0;
	_minEdgeSize = DBL_MAX;
}

void Controller::DeleteViewMap(bool freeCache)
{
	_pView->DetachSilhouette();
	if (NULL != _SilhouetteNode) {
		int ref = _SilhouetteNode->destroy();
		if (0 == ref) {
			delete _SilhouetteNode;
			_SilhouetteNode = NULL;
		}
	}

#if 0
	if (NULL != _ProjectedSilhouette) {
		int ref = _ProjectedSilhouette->destroy();
		if (0 == ref) {
			delete _ProjectedSilhouette;
			_ProjectedSilhouette = NULL;
		}
	}
	if (NULL != _VisibleProjectedSilhouette) {
		int ref = _VisibleProjectedSilhouette->destroy();
		if (0 == ref) {
			delete _VisibleProjectedSilhouette;
			_VisibleProjectedSilhouette = NULL;
		}
	}
#endif

	_pView->DetachDebug();
	if (NULL != _DebugNode) {
		int ref = _DebugNode->destroy();
		if (0 == ref)
			_DebugNode->addRef();
	}

	if (NULL != _ViewMap) {
		if (freeCache || !_EnableViewMapCache) {
			delete _ViewMap;
			_ViewMap = NULL;
			prevSceneHash = -1.0;
		}
		else {
			_ViewMap->Clean();
		}
	}
}

void Controller::ComputeViewMap()
{
	if (!_ListOfModels.size())
		return;

	DeleteViewMap(true);

	// retrieve the 3D viewpoint and transformations information
	//----------------------------------------------------------
	// Save the viewpoint context at the view level in order 
	// to be able to restore it later:

	// Restore the context of view:
	// we need to perform all these operations while the 
	// 3D context is on.
	Vec3r vp(freestyle_viewpoint[0], freestyle_viewpoint[1], freestyle_viewpoint[2]);

#if 0
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "mv" << endl;
	}
#endif
	real mv[4][4];
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			mv[i][j] = freestyle_mv[i][j];
#if 0
			if (G.debug & G_DEBUG_FREESTYLE) {
				cout << mv[i][j] << " ";
			}
#endif
		}
#if 0
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << endl;
		}
#endif
	}

#if 0
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "\nproj" << endl;
	}
#endif
	real proj[4][4];
	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			proj[i][j] = freestyle_proj[i][j];
#if 0
			if (G.debug & G_DEBUG_FREESTYLE) {
				cout << proj[i][j] << " ";
			}
#endif
		}
#if 0
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << endl;
		}
#endif
	}

	int viewport[4];
	for (int i = 0; i < 4; i++)
		viewport[i] = freestyle_viewport[i];

#if 0
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "\nfocal:" << _pView->GetFocalLength() << endl << endl;
	}
#endif

	// Flag the WXEdge structure for silhouette edge detection:
	//----------------------------------------------------------

	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "\n===  Detecting silhouette edges  ===" << endl;
	}
	_Chrono.start();

	edgeDetector.setViewpoint(Vec3r(vp));
	edgeDetector.enableOrthographicProjection(proj[3][3] != 0.0);
	edgeDetector.enableRidgesAndValleysFlag(_ComputeRidges);
	edgeDetector.enableSuggestiveContours(_ComputeSuggestive);
	edgeDetector.enableMaterialBoundaries(_ComputeMaterialBoundaries);
	edgeDetector.enableFaceSmoothness(_EnableFaceSmoothness);
	edgeDetector.setCreaseAngle(_creaseAngle);
	edgeDetector.setSphereRadius(_sphereRadius);
	edgeDetector.setSuggestiveContourKrDerivativeEpsilon(_suggestiveContourKrDerivativeEpsilon);
	edgeDetector.setRenderMonitor(_pRenderMonitor);
	edgeDetector.processShapes(*_winged_edge);

	real duration = _Chrono.stop();
	if (G.debug & G_DEBUG_FREESTYLE) {
		printf("Feature lines    : %lf\n", duration);
	}

	if (_pRenderMonitor->testBreak())
		return;

	// Builds the view map structure from the flagged WSEdge structure:
	//----------------------------------------------------------
	ViewMapBuilder vmBuilder;
	vmBuilder.setEnableQI(_EnableQI);
	vmBuilder.setViewpoint(Vec3r(vp));
	vmBuilder.setTransform(mv, proj, viewport, _pView->GetFocalLength(), _pView->GetAspect(), _pView->GetFovyRadian());
	vmBuilder.setFrustum(_pView->znear(), _pView->zfar());
	vmBuilder.setGrid(&_Grid);
	vmBuilder.setRenderMonitor(_pRenderMonitor);

	// Builds a tesselated form of the silhouette for display purpose:
	//---------------------------------------------------------------
	ViewMapTesselator3D sTesselator3d;
#if 0
	ViewMapTesselator2D sTesselator2d;
	sTesselator2d.setNature(_edgeTesselationNature);
#endif
	sTesselator3d.setNature(_edgeTesselationNature);

	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "\n===  Building the view map  ===" << endl;
	}
	_Chrono.start();
	// Build View Map
	_ViewMap = vmBuilder.BuildViewMap(*_winged_edge, _VisibilityAlgo, _EPSILON, _Scene3dBBox, _SceneNumFaces);
	_ViewMap->setScene3dBBox(_Scene3dBBox);

	if (G.debug & G_DEBUG_FREESTYLE) {
		printf("ViewMap edge count : %i\n", _ViewMap->viewedges_size());
	}

	// Tesselate the 3D edges:
	_SilhouetteNode = sTesselator3d.Tesselate(_ViewMap);
	_SilhouetteNode->addRef();

	// Tesselate 2D edges
#if 0
	_ProjectedSilhouette = sTesselator2d.Tesselate(_ViewMap);
	_ProjectedSilhouette->addRef();
#endif

	duration = _Chrono.stop();
	if (G.debug & G_DEBUG_FREESTYLE) {
		printf("ViewMap building : %lf\n", duration);
	}

	_pView->AddSilhouette(_SilhouetteNode);
#if 0
	_pView->AddSilhouette(_WRoot);
	_pView->Add2DSilhouette(_ProjectedSilhouette);
	_pView->Add2DVisibleSilhouette(_VisibleProjectedSilhouette);
#endif
	_pView->AddDebug(_DebugNode);

	// Draw the steerable density map:
	//--------------------------------
	if (_ComputeSteerableViewMap) {
		ComputeSteerableViewMap();
	}
	// Reset Style modules modification flags
	resetModified(true);

	DeleteWingedEdge();
}

void Controller::ComputeSteerableViewMap()
{
#if 0  //soc
	if ((!_Canvas) || (!_ViewMap))
		return;

	// Build 4 nodes containing the edges in the 4 directions
	NodeGroup *ng[Canvas::NB_STEERABLE_VIEWMAP];
	unsigned i;
	real c = 32.0f/255.0f; // see SteerableViewMap::readSteerableViewMapPixel() for information about this 32.
	for (i = 0; i < Canvas::NB_STEERABLE_VIEWMAP; ++i) {
		ng[i] = new NodeGroup;
	}
	NodeShape *completeNS = new NodeShape;
	completeNS->material().setDiffuse(c,c,c,1);
	ng[Canvas::NB_STEERABLE_VIEWMAP-1]->AddChild(completeNS);
	SteerableViewMap * svm = _Canvas->getSteerableViewMap();
	svm->Reset();

	ViewMap::fedges_container& fedges = _ViewMap->FEdges();
	LineRep * fRep;
	NodeShape *ns;
	for (ViewMap::fedges_container::iterator f = fedges.begin(), fend = fedges.end();
	     f != fend;
	     ++f)
	{
		if ((*f)->viewedge()->qi() != 0)
			continue;
		fRep = new LineRep((*f)->vertexA()->point2d(), (*f)->vertexB()->point2d());
		completeNS->AddRep(fRep); // add to the complete map anyway
		double *oweights = svm->AddFEdge(*f);
		for (i = 0; i < (Canvas::NB_STEERABLE_VIEWMAP - 1); ++i) {
			ns = new NodeShape;
			double wc = oweights[i]*c;
			if (oweights[i] == 0)
				continue;
			ns->material().setDiffuse(wc, wc, wc, 1);
			ns->AddRep(fRep);
			ng[i]->AddChild(ns);
		}
	}

	GrayImage *img[Canvas::NB_STEERABLE_VIEWMAP];
	//#ifdef WIN32
	QGLBasicWidget offscreenBuffer(_pView, "SteerableViewMap", _pView->width(), _pView->height());
	QPixmap pm;
	QImage qimg;
	for (i = 0; i < Canvas::NB_STEERABLE_VIEWMAP; ++i) {
		offscreenBuffer.AddNode(ng[i]);
#if 0
		img[i] = new GrayImage(_pView->width(), _pView->height());
		offscreenBuffer.readPixels(0,0,_pView->width(), _pView->height(), img[i]->getArray());
#endif
		pm = offscreenBuffer.renderPixmap(_pView->width(), _pView->height());

		if (pm.isNull()) {
			if (G.debug & G_DEBUG_FREESTYLE) {
				cout << "BuildViewMap Warning: couldn't render the steerable ViewMap" << endl;
			}
		}
		//pm.save(QString("steerable") + QString::number(i) + QString(".bmp"), "BMP");
		// FIXME!! Lost of time !
		qimg = pm.toImage();
		// FIXME !! again!
		img[i] = new GrayImage(_pView->width(), _pView->height());
		for (unsigned int y = 0; y < img[i]->height(); ++y) {
			for (unsigned int x = 0; x < img[i]->width(); ++x) {
				//img[i]->setPixel(x, y, (float)qGray(qimg.pixel(x, y)) / 255.0f);
				img[i]->setPixel(x, y, (float)qGray(qimg.pixel(x, y)));
				//float c = qGray(qimg.pixel(x, y));
				//img[i]->setPixel(x, y, qGray(qimg.pixel(x, y)));
			}
		}
		offscreenBuffer.DetachNode(ng[i]);
		ng[i]->destroy();
		delete ng[i];
		// check
#if 0
		qimg = QImage(_pView->width(), _pView->height(), 32);
		for (unsigned int y = 0; y < img[i]->height(); ++y) {
			for (unsigned int x = 0; x < img[i]->width(); ++x) {
				float v = img[i]->pixel(x, y);
				qimg.setPixel(x, y, qRgb(v, v, v));
			}
		}
		qimg.save(QString("newsteerable") + QString::number(i) + QString(".bmp"), "BMP");
#endif
	}


	svm->buildImagesPyramids(img, false, 0, 1.0f);
#endif
}

void Controller::saveSteerableViewMapImages()
{
	SteerableViewMap * svm = _Canvas->getSteerableViewMap();
	if (!svm) {
		cerr << "the Steerable ViewMap has not been computed yet" << endl;
		return;
	}
	svm->saveSteerableViewMap();
}

void Controller::toggleVisibilityAlgo()
{
	if (_VisibilityAlgo == ViewMapBuilder::ray_casting) {
		_VisibilityAlgo = ViewMapBuilder::ray_casting_fast;
	}
	else if (_VisibilityAlgo == ViewMapBuilder::ray_casting_fast) {
		_VisibilityAlgo = ViewMapBuilder::ray_casting_very_fast;
	}
	else {
		_VisibilityAlgo = ViewMapBuilder::ray_casting;
	}
}

void Controller::setVisibilityAlgo(int algo)
{
	switch (algo) {
		case FREESTYLE_ALGO_REGULAR:
			_VisibilityAlgo = ViewMapBuilder::ray_casting;
			break;
		case FREESTYLE_ALGO_FAST:
			_VisibilityAlgo = ViewMapBuilder::ray_casting_fast;
			break;
		case FREESTYLE_ALGO_VERYFAST:
			_VisibilityAlgo = ViewMapBuilder::ray_casting_very_fast;
			break;
		case FREESTYLE_ALGO_CULLED_ADAPTIVE_TRADITIONAL:
			_VisibilityAlgo = ViewMapBuilder::ray_casting_culled_adaptive_traditional;
			break;
		case FREESTYLE_ALGO_ADAPTIVE_TRADITIONAL:
			_VisibilityAlgo = ViewMapBuilder::ray_casting_adaptive_traditional;
			break;
		case FREESTYLE_ALGO_CULLED_ADAPTIVE_CUMULATIVE:
			_VisibilityAlgo = ViewMapBuilder::ray_casting_culled_adaptive_cumulative;
			break;
		case FREESTYLE_ALGO_ADAPTIVE_CUMULATIVE:
			_VisibilityAlgo = ViewMapBuilder::ray_casting_adaptive_cumulative;
			break;
	}
}

int Controller::getVisibilityAlgo()
{
	switch (_VisibilityAlgo) {
		case ViewMapBuilder::ray_casting:
			return FREESTYLE_ALGO_REGULAR;
		case ViewMapBuilder::ray_casting_fast:
			return FREESTYLE_ALGO_FAST;
		case ViewMapBuilder::ray_casting_very_fast:
			return FREESTYLE_ALGO_VERYFAST;
		case ViewMapBuilder::ray_casting_culled_adaptive_traditional:
			return FREESTYLE_ALGO_CULLED_ADAPTIVE_TRADITIONAL;
		case ViewMapBuilder::ray_casting_adaptive_traditional:
			return FREESTYLE_ALGO_ADAPTIVE_TRADITIONAL;
		case ViewMapBuilder::ray_casting_culled_adaptive_cumulative:
			return FREESTYLE_ALGO_CULLED_ADAPTIVE_CUMULATIVE;
		case ViewMapBuilder::ray_casting_adaptive_cumulative:
			return FREESTYLE_ALGO_ADAPTIVE_CUMULATIVE;
	}

	// ray_casting_adaptive_traditional is the most exact replacement
	// for legacy code
	return FREESTYLE_ALGO_ADAPTIVE_TRADITIONAL;
}

void Controller::setViewMapCache(bool iBool)
{
	_EnableViewMapCache = iBool;
}

bool Controller::getViewMapCache() const
{
	return _EnableViewMapCache;
}

void Controller::setQuantitativeInvisibility(bool iBool)
{
	_EnableQI = iBool;
}

bool Controller::getQuantitativeInvisibility() const
{
	return _EnableQI;
}

void Controller::setFaceSmoothness(bool iBool)
{
	_EnableFaceSmoothness = iBool;
}

bool Controller::getFaceSmoothness() const
{
	return _EnableFaceSmoothness;
}

void Controller::setComputeRidgesAndValleysFlag(bool iBool)
{
	_ComputeRidges = iBool;
}

bool Controller::getComputeRidgesAndValleysFlag() const
{
	return _ComputeRidges;
}

void Controller::setComputeSuggestiveContoursFlag(bool b)
{
	_ComputeSuggestive = b;
}

bool Controller::getComputeSuggestiveContoursFlag() const
{
	return _ComputeSuggestive;
}

void Controller::setComputeMaterialBoundariesFlag(bool b)
{
	_ComputeMaterialBoundaries = b;
}

bool Controller::getComputeMaterialBoundariesFlag() const
{
	return _ComputeMaterialBoundaries;
}

void Controller::setComputeSteerableViewMapFlag(bool iBool)
{
	_ComputeSteerableViewMap = iBool;
}

bool Controller::getComputeSteerableViewMapFlag() const
{
	return _ComputeSteerableViewMap;
}

void Controller::DrawStrokes()
{
	if (_ViewMap == 0)
		return;

	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "\n===  Stroke drawing  ===" << endl;
	}
	_Chrono.start();
	_Canvas->Draw();
	real d = _Chrono.stop();
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "Strokes generation  : " << d << endl;
		cout << "Stroke count  : " << _Canvas->stroke_count << endl;
	}
	resetModified();
	DeleteViewMap();
}

void Controller::ResetRenderCount()
{
	_render_count = 0;
}

Render *Controller::RenderStrokes(Render *re, bool render)
{
	_Chrono.start();
	BlenderStrokeRenderer *blenderRenderer = new BlenderStrokeRenderer(re, ++_render_count);
	if (render)
		_Canvas->Render(blenderRenderer);
	real d = _Chrono.stop();
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "Temporary scene generation: " << d << endl;
	}
	_Chrono.start();
	Render *freestyle_render = blenderRenderer->RenderScene(re, render);
	d = _Chrono.stop();
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "Stroke rendering  : " << d << endl;

		uintptr_t mem_in_use = MEM_get_memory_in_use();
		uintptr_t mmap_in_use = MEM_get_mapped_memory_in_use();
		uintptr_t peak_memory = MEM_get_peak_memory();

		float megs_used_memory = (mem_in_use - mmap_in_use) / (1024.0 * 1024.0);
		float mmap_used_memory = (mmap_in_use) / (1024.0 * 1024.0);
		float megs_peak_memory = (peak_memory) / (1024.0 * 1024.0);

		printf("%d verts, %d faces, mem %.2fM (%.2fM, peak %.2fM)\n",
		       freestyle_render->i.totvert, freestyle_render->i.totface,
		       megs_used_memory, mmap_used_memory, megs_peak_memory);
	}
	delete blenderRenderer;

	return freestyle_render;
}

void Controller::InsertStyleModule(unsigned index, const char *iFileName)
{
	if (!BLI_testextensie(iFileName, ".py")) {
		cerr << "Error: Cannot load \"" << string(iFileName) << "\", unknown extension" << endl;
		return;
	}

	StyleModule *sm = new StyleModule(iFileName, _inter);
	_Canvas->InsertStyleModule(index, sm);
}

void Controller::InsertStyleModule(unsigned index, const char *iName, struct Text *iText)
{
	StyleModule *sm = new BlenderStyleModule(iText, iName, _inter);
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
	StyleModule *sm = new StyleModule(iFileName, _inter);
	_Canvas->ReplaceStyleModule(index, sm);
}

void Controller::SwapStyleModules(unsigned i1, unsigned i2)
{
	_Canvas->SwapStyleModules(i1, i2);
}

void Controller::toggleLayer(unsigned index, bool iDisplay)
{
	_Canvas->setVisible(index, iDisplay);
}

void Controller::setModified(unsigned index, bool iMod)
{
	//_pStyleWindow->setModified(index, iMod);
	_Canvas->setModified(index, iMod);
	updateCausalStyleModules(index + 1);
}

void Controller::updateCausalStyleModules(unsigned index)
{
	vector<unsigned> vec;
	_Canvas->causalStyleModules(vec, index);
	for (vector<unsigned>::const_iterator it = vec.begin(); it != vec.end(); it++) {
		//_pStyleWindow->setModified(*it, true);
		_Canvas->setModified(*it, true);
	}
}

void Controller::resetModified(bool iMod)
{
	//_pStyleWindow->resetModified(iMod);
	_Canvas->resetModified(iMod);
}

NodeGroup * Controller::BuildRep(vector<ViewEdge*>::iterator vedges_begin, vector<ViewEdge*>::iterator vedges_end)
{
	ViewMapTesselator2D tesselator2D;
	FrsMaterial mat;
	mat.setDiffuse(1, 1, 0.3, 1);
	tesselator2D.setFrsMaterial(mat);

	return (tesselator2D.Tesselate(vedges_begin, vedges_end));
}

void Controller::toggleEdgeTesselationNature(Nature::EdgeNature iNature)
{
	_edgeTesselationNature ^= (iNature);
	ComputeViewMap();
}

void Controller::setModelsDir(const string& dir)
{
	//_current_dirs->setValue("models/dir", dir);
}

string Controller::getModelsDir() const
{
	string dir = ".";
	//_current_dirs->getValue("models/dir", dir);
	return dir;
}

void Controller::setModulesDir(const string& dir)
{
	//_current_dirs->setValue("modules/dir", dir);
}

string Controller::getModulesDir() const
{
	string dir = ".";
	//_current_dirs->getValue("modules/dir", dir);
	return dir;
}

void Controller::resetInterpreter()
{
	if (_inter)
		_inter->reset();
}


void Controller::displayDensityCurves(int x, int y)
{
	SteerableViewMap * svm = _Canvas->getSteerableViewMap();
	if (!svm)
		return;

	unsigned int i, j;
	typedef vector<Vec3r> densityCurve;
	vector<densityCurve> curves(svm->getNumberOfOrientations() + 1);
	vector<densityCurve> curvesDirection(svm->getNumberOfPyramidLevels());

	// collect the curves values
	unsigned nbCurves = svm->getNumberOfOrientations() + 1;
	unsigned nbPoints = svm->getNumberOfPyramidLevels();
	if (!nbPoints)
		return;

	// build the density/nbLevels curves for each orientation
	for (i = 0; i < nbCurves; ++i) {
		for (j = 0; j < nbPoints; ++j) {
			curves[i].push_back(Vec3r(j, svm->readSteerableViewMapPixel(i, j, x, y), 0));
		}
	}
	// build the density/nbOrientations curves for each level
	for (i = 0; i < nbPoints; ++i) {
		for (j = 0; j < nbCurves; ++j) {
			curvesDirection[i].push_back(Vec3r(j, svm->readSteerableViewMapPixel(j, i, x, y), 0));
		}
	}

	// display the curves
#if 0
	for (i = 0; i < nbCurves; ++i)
		_pDensityCurvesWindow->setOrientationCurve(i, Vec2d(0, 0), Vec2d(nbPoints, 1), curves[i], "scale", "density");
	for (i = 1; i <= 8; ++i)
		_pDensityCurvesWindow->setLevelCurve(i, Vec2d(0, 0), Vec2d(nbCurves, 1), curvesDirection[i],
		                                     "orientation", "density");
	_pDensityCurvesWindow->show();
#endif
}

void Controller::init_options()
{
	// from AppOptionsWindow.cpp
	// Default init options

	Config::Path * cpath = Config::Path::getInstance();

	// Directories
	ViewMapIO::Options::setModelsPath(cpath->getModelsPath());
	TextureManager::Options::setPatternsPath(cpath->getPatternsPath());
	TextureManager::Options::setBrushesPath(cpath->getModelsPath());

	// ViewMap Format
	ViewMapIO::Options::rmFlags(ViewMapIO::Options::FLOAT_VECTORS);
	ViewMapIO::Options::rmFlags(ViewMapIO::Options::NO_OCCLUDERS);
	setComputeSteerableViewMapFlag(false);

	// Visibility
	setQuantitativeInvisibility(true);

	// soc: initialize canvas
	_Canvas->init();

	// soc: initialize passes
	setPassDiffuse(NULL, 0, 0);
	setPassZ(NULL, 0, 0);
}

} /* namespace Freestyle */
