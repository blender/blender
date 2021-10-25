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

#ifndef __CONTROLLER_H__
#define __CONTROLLER_H__

/** \file blender/freestyle/intern/application/Controller.h
 *  \ingroup freestyle
 *  \brief The spinal tap of the system.
 *  \author Stephane Grabli
 *  \date 01/07/2002
 */

#include <string>

#include "../geometry/FastGrid.h"
#include "../scene_graph/SceneHash.h"
#include "../system/Precision.h"
#include "../system/TimeUtils.h"
#include "../view_map/FEdgeXDetector.h"
#include "../view_map/ViewMapBuilder.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

class AppCanvas;
class AppView;
class Interpreter;
class NodeGroup;
class ProgressBar;
class RenderMonitor;
class SShape;
class ViewEdge;
class ViewMap;

class Controller
{
public:
	Controller();
	~Controller();

	void setView(AppView *iView);
	void setRenderMonitor(RenderMonitor *iRenderMonitor);
	void setPassDiffuse(float *buf, int width, int height);
	void setPassZ(float *buf, int width, int height);
	void setContext(bContext *C);

	//soc
	void init_options();

	int  LoadMesh(Render *re, SceneRenderLayer *srl);
	int  Load3DSFile(const char *iFileName);
	void CloseFile();
	void ComputeViewMap();
	void ComputeSteerableViewMap();
	void saveSteerableViewMapImages();
	void toggleEdgeTesselationNature(Nature::EdgeNature iNature);
	int DrawStrokes();
	void ResetRenderCount();
	Render *RenderStrokes(Render *re, bool render);
	void SwapStyleModules(unsigned i1, unsigned i2);
	void InsertStyleModule(unsigned index, const char *iFileName);
	void InsertStyleModule(unsigned index, const char *iName, const char *iBuffer);
	void InsertStyleModule(unsigned index, const char *iName, struct Text *iText);
	void AddStyleModule(const char *iFileName);
	void RemoveStyleModule(unsigned index);
	void ReloadStyleModule(unsigned index, const char * iFileName);
	void Clear();
	void ClearRootNode();
	void DeleteWingedEdge();
	void DeleteViewMap(bool freeCache = false);
	void toggleLayer(unsigned index, bool iDisplay);
	void setModified(unsigned index, bool iMod);
	void resetModified(bool iMod=false);
	void updateCausalStyleModules(unsigned index);
	void displayDensityCurves(int x, int y);

	ViewEdge *SelectViewEdge(real x, real y);
	FEdge *SelectFEdge(real x, real y);
	NodeGroup *BuildRep(vector<ViewEdge*>::iterator vedges_begin, vector<ViewEdge*>::iterator vedges_end) ;

#if 0
	NodeGroup *debugNode() {return _DebugNode;}
	AppView *view() {return _pView;}
	NodeGroup *debugScene() {return _DebugNode;}
	Grid& grid() {return _Grid;}
#endif

	void toggleVisibilityAlgo();
	void setVisibilityAlgo(int algo);
	int getVisibilityAlgo();

	void setViewMapCache(bool iBool);
	bool getViewMapCache() const;
	void setQuantitativeInvisibility(bool iBool); // if true, we compute quantitativeInvisibility
	bool getQuantitativeInvisibility() const;
	void setFaceSmoothness(bool iBool);
	bool getFaceSmoothness() const;

	void setComputeRidgesAndValleysFlag(bool b);
	bool getComputeRidgesAndValleysFlag() const;
	void setComputeSuggestiveContoursFlag(bool b);
	bool getComputeSuggestiveContoursFlag() const;
	void setComputeMaterialBoundariesFlag(bool b);
	bool getComputeMaterialBoundariesFlag() const;

	void setComputeSteerableViewMapFlag(bool iBool);
	bool getComputeSteerableViewMapFlag() const;
	void setCreaseAngle(float angle) {_creaseAngle = angle;}
	float getCreaseAngle() const {return _creaseAngle;}
	void setSphereRadius(float s) {_sphereRadius = s;}
	float getSphereRadius() const {return _sphereRadius;}
	void setSuggestiveContourKrDerivativeEpsilon(float dkr) {_suggestiveContourKrDerivativeEpsilon = dkr;}
	float getSuggestiveContourKrDerivativeEpsilon() const {return _suggestiveContourKrDerivativeEpsilon;}

	void setModelsDir(const string& dir);
	string getModelsDir() const;
	void setModulesDir(const string& dir);
	string getModulesDir() const;

	bool hitViewMapCache();

	void resetInterpreter();

public:
	// Viewmap data structure
	ViewMap *_ViewMap;

	// Canvas
	AppCanvas *_Canvas;

private:
	// Main Window:
	//AppMainWindow *_pMainWindow;

	// List of models currently loaded
	vector<string> _ListOfModels;

	// Current directories
	//ConfigIO* _current_dirs;

	//View
	// 3D
	AppView *_pView;

	// 2D
#if 0
	Viewer2DWindow *_pView2DWindow;
	Viewer2D *_pView2D;
#endif

	RenderMonitor *_pRenderMonitor;

	//Model
	// Drawing Structure
	NodeGroup *_RootNode;

	// Winged-Edge structure
	WingedEdge *_winged_edge;

#if 0
	// Silhouette structure:
	std::vector<SShape*> _SShapes;
	NodeGroup *_SRoot;

	// Silhouette
	NodeGroup *_SilhouetteNode;
	NodeGroup *_ProjectedSilhouette;
	NodeGroup *_VisibleProjectedSilhouette;

	// more Debug info
	NodeGroup *_DebugNode;
#endif

	// debug
	//NodeUser<ViewMap> *_ViewMapNode; // FIXME

	// Chronometer:
	Chronometer _Chrono;

	// Progress Bar
	ProgressBar *_ProgressBar;

	// edges tesselation nature
	int _edgeTesselationNature;

	FastGrid _Grid;
	//HashGrid _Grid;

	BBox<Vec3r> _Scene3dBBox;
	unsigned int _SceneNumFaces;
#if 0
	real _minEdgeSize;
#endif
	real _EPSILON;
	real _bboxDiag;

	int _render_count;

	//AppStyleWindow *_pStyleWindow;
	//AppOptionsWindow *_pOptionsWindow;
	//AppDensityCurvesWindow *_pDensityCurvesWindow;

	ViewMapBuilder::visibility_algo _VisibilityAlgo;

	// Script Interpreter
	Interpreter *_inter;

	string _help_index;
	string _browser_cmd;

	bool _EnableViewMapCache;
	bool _EnableQI;
	bool _EnableFaceSmoothness;
	bool _ComputeRidges;
	bool _ComputeSuggestive;
	bool _ComputeMaterialBoundaries;
	float _creaseAngle;
	float _sphereRadius;
	float _suggestiveContourKrDerivativeEpsilon;

	bool _ComputeSteerableViewMap;

	FEdgeXDetector edgeDetector;

	SceneHash sceneHashFunc;
	real prevSceneHash;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Controller")
#endif
};

extern Controller *g_pController;

} /* namespace Freestyle */

#endif // __CONTROLLER_H__
