//
//  Filename         : Controller.h
//  Author           : Stephane Grabli
//  Purpose          : The spinal tap of the system
//  Date of creation : 01/07/2002
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

#ifndef  CONTROLLER_H
# define CONTROLLER_H

# include <string>
//# include "ConfigIO.h"
# include "../geometry/FastGrid.h"
# include "../geometry/HashGrid.h"
# include "../view_map/ViewMapBuilder.h"
# include "../system/TimeUtils.h"
# include "../system/ProgressBar.h"
# include "../system/Precision.h"
# include "../system/Interpreter.h"
# include "../view_map/FEdgeXDetector.h"

class AppGLWidget;
class NodeGroup;
class WShape;
class SShape;
class ViewMap;
class ViewEdge;
class AppCanvas;
class InteractiveShader;
class Shader;

class Controller
{
public:
  Controller() ;
  ~Controller() ;
  
  void SetView(AppGLWidget *iView);

  int  Load3DSFile(const char *iFileName);
  void CloseFile();
  void LoadViewMapFile(const char *iFileName, bool only_camera = false);
  void SaveViewMapFile(const char *iFileName);
  void ComputeViewMap();
  void ComputeSteerableViewMap();
  void saveSteerableViewMapImages();
  void toggleEdgeTesselationNature(Nature::EdgeNature iNature);
  void DrawStrokes();
  void SwapStyleModules(unsigned i1, unsigned i2);
  void InsertStyleModule(unsigned index, const char *iFileName);
  void AddStyleModule(const char *iFileName);
  void RemoveStyleModule(unsigned index);
  void ReloadStyleModule(unsigned index, const char * iFileName);
  void Clear();
  void toggleLayer(unsigned index, bool iDisplay);
  void setModified(unsigned index, bool iMod);
  void resetModified(bool iMod=false);
  void updateCausalStyleModules(unsigned index);
  void saveSnapshot(bool b = false);
  void displayDensityCurves(int x, int y);
 
  
  ViewEdge * SelectViewEdge(real x, real y);
  FEdge * SelectFEdge(real x, real y);
  NodeGroup* BuildRep(vector<ViewEdge*>::iterator vedges_begin, 
		      vector<ViewEdge*>::iterator vedges_end) ;
  
  NodeGroup* debugNode() {return _DebugNode;}
  AppGLWidget * view() {return _pView;}
  NodeGroup* debugScene() {return _DebugNode;}
  Grid& grid() {return _Grid;}
  
  void toggleVisibilityAlgo();

  void setQuantitativeInvisibility(bool iBool); // if true, we compute quantitativeInvisibility
  bool getQuantitativeInvisibility() const;

  void setFrontBufferFlag(bool b);
  bool getFrontBufferFlag() const;
  void setBackBufferFlag(bool b);
  bool getBackBufferFlag() const;

  void setComputeRidgesAndValleysFlag(bool b);
  bool getComputeRidgesAndValleysFlag() const ;
  void setComputeSuggestiveContoursFlag(bool b);
  bool getComputeSuggestiveContoursFlag() const ;

  void setComputeSteerableViewMapFlag(bool iBool);
  bool getComputeSteerableViewMapFlag() const;
  void setSphereRadius(real s){_sphereRadius=s;}
  real getSphereRadius() const {return _sphereRadius;}
  void setSuggestiveContourKrDerivativeEpsilon(real dkr){_suggestiveContourKrDerivativeEpsilon=dkr;}
  real getSuggestiveContourKrDerivativeEpsilon() const {return _suggestiveContourKrDerivativeEpsilon;}

  void		setModelsDir(const string& dir);
  string	getModelsDir() const;
  void		setModulesDir(const string& dir);
  string	getModulesDir() const;
  void		setPapersDir(const string& dir);
  string	getPapersDir() const;
  void		setHelpIndex(const string& dir);
  string	getHelpIndex() const;
  void		setBrowserCmd(const string& cmd);
  string	getBrowserCmd() const;

  void resetInterpreter();

private:

  // Main Window:
  //AppMainWindow *_pMainWindow;

  // List of models currently loaded
  vector<string> _ListOfModels;

  // Current directories
  //ConfigIO* _current_dirs;

  //View
  // 3D
  AppGLWidget *_pView;
  
  // 2D
  //Viewer2DWindow *_pView2DWindow;
  //Viewer2D *_pView2D;
  
  //Model
  // Drawing Structure
  NodeGroup *_RootNode;

  // Winged-Edge structure
  WingedEdge* _winged_edge;
  
  ViewMap * _ViewMap;

  // Silhouette structure:
  //std::vector<SShape*> _SShapes;
  //NodeGroup *_SRoot;
  
  // Silhouette
  NodeGroup *_SilhouetteNode;
  NodeGroup *_ProjectedSilhouette;
  NodeGroup *_VisibleProjectedSilhouette;
  
  // more Debug info
  NodeGroup *_DebugNode;

  // debug
  //  NodeUser<ViewMap> *_ViewMapNode; // FIXME

  // Chronometer:
  Chronometer _Chrono;

	// Progress Bar
  ProgressBar *_ProgressBar;

  // edges tesselation nature
  int _edgeTesselationNature;

  FastGrid _Grid;
  //HashGrid _Grid;
  
  unsigned int _SceneNumFaces;
  real _minEdgeSize;
  real _EPSILON;
  real _bboxDiag;

  AppCanvas *_Canvas;

  //AppStyleWindow *_pStyleWindow;
  //AppOptionsWindow *_pOptionsWindow;
  //AppDensityCurvesWindow *_pDensityCurvesWindow;

  ViewMapBuilder::visibility_algo	_VisibilityAlgo;
  
  // Script Interpreter
  Interpreter* _inter;

  string	_help_index;
  string	_browser_cmd;

  bool _EnableQI;
  bool _ComputeRidges;
  bool _ComputeSuggestive;
  real _sphereRadius;
  real _suggestiveContourKrDerivativeEpsilon;

  bool _ComputeSteerableViewMap;

  FEdgeXDetector edgeDetector;
};

extern Controller	*g_pController;

#endif // CONTROLLER_H
