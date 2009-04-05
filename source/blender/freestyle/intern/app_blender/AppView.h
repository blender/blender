#ifndef  APPVIEW_H
# define APPVIEW_H

# if !defined(WIN32) || defined(__GNUC__)
#  include <algorithm>
using namespace std;
#  define __min(x,y) (min(x,y))
#  define __max(x,y) (max(x,y))
# endif // WIN32

# include "../geometry/Geom.h"
# include "../geometry/BBox.h"
# include "../scene_graph/NodeDrawingStyle.h"
# include "../system/Precision.h"
# include "AppConfig.h"

using namespace Geometry;
 
class AppView
{

public:

  AppView(const char *iName = 0);
  virtual ~AppView();
  
public:

	//inherited
		inline unsigned int width() { return _width; }
		inline unsigned int height() { return _height; }
		inline void setWidth( unsigned int width ) { _width = width; }
		inline void setHeight( unsigned int height ) { _height = height; }
		
protected:
	unsigned int _width, _height;

public:

  /*! Sets the model to draw in the viewer 
   *  iModel
   *    The Root Node of the model 
   */
  inline void setModel(NodeGroup *iModel)
  {
    if(0 != _ModelRootNode->numberOfChildren())
    {
      _ModelRootNode->DetachChildren();
      _ModelRootNode->clearBBox();
    }
      
    AddModel(iModel);
  }

  /*! Adds a model for displaying in the viewer */
  inline void AddModel(NodeGroup *iModel) 
  {
    _ModelRootNode->AddChild(iModel);
    
    _ModelRootNode->UpdateBBox();

    _minBBox = __min(__min(_ModelRootNode->bbox().getMin()[0], 
                            _ModelRootNode->bbox().getMin()[1]),
                      _ModelRootNode->bbox().getMin()[2]);
    _maxBBox = __max(__max(_ModelRootNode->bbox().getMax()[0], 
                            _ModelRootNode->bbox().getMax()[1]),
                      _ModelRootNode->bbox().getMax()[2]);
    
     _maxAbs = __max(rabs(_minBBox), rabs(_maxBBox));

     _minAbs = __min(rabs(_minBBox), rabs(_maxBBox));
 
  }

  inline void AddSilhouette(NodeGroup* iSilhouette)
  {
    _SilhouetteRootNode->AddChild(iSilhouette);
  }

  inline void Add2DSilhouette(NodeGroup *iSilhouette)
  {
    //_pFENode->AddChild(iSilhouette);
  }

  inline void Add2DVisibleSilhouette(NodeGroup *iVSilhouette)
  {
    //_pVisibleSilhouetteNode->AddChild(iVSilhouette);
  }

  inline void setDebug(NodeGroup* iDebug)
  {
    if(0 != _DebugRootNode->numberOfChildren())
    {
      _DebugRootNode->DetachChildren();
      _DebugRootNode->clearBBox();
    }
      
    AddDebug(iDebug);
  }

  inline void AddDebug(NodeGroup* iDebug)
  {
    _DebugRootNode->AddChild(iDebug);
  }

  inline void DetachModel(Node *iModel)
  {
    _ModelRootNode->DetachChild(iModel);
    _ModelRootNode->UpdateBBox();
    
    _minBBox = __min(__min(_ModelRootNode->bbox().getMin()[0], 
                            _ModelRootNode->bbox().getMin()[1]),
                      _ModelRootNode->bbox().getMin()[2]);
    _maxBBox = __max(__max(_ModelRootNode->bbox().getMax()[0], 
                            _ModelRootNode->bbox().getMax()[1]),
                      _ModelRootNode->bbox().getMax()[2]);
    
     _maxAbs = __max(rabs(_minBBox), rabs(_maxBBox));
     _minAbs = __min(rabs(_minBBox), rabs(_maxBBox));
  } 

  inline void DetachModel() 
  {
    _ModelRootNode->DetachChildren();
    _ModelRootNode->clearBBox();
    
    // 2D Scene
    //_p2DNode.DetachChildren();
    //_pFENode->DetachChildren();
    //_pVisibleSilhouetteNode->DetachChildren();
    
  }

  inline void DetachSilhouette()
  {
    _SilhouetteRootNode->DetachChildren();
    //_pFENode->DetachChildren();
    //_pVisibleSilhouetteNode->DetachChildren();
    _p2DSelectionNode->destroy();
  }

  inline void DetachVisibleSilhouette()
  {
    //_pVisibleSilhouetteNode->DetachChildren();
    _p2DSelectionNode->destroy();
  }

  inline void DetachDebug()
  {
    _DebugRootNode->DetachChildren();
  }

	

	real distanceToSceneCenter();
	real GetFocalLength();

  inline real GetAspect() const
  {
    return ((real) _width/(real) _height);
  }

  void setHorizontalFov( float hfov ) 
{
	_Fovy = 2.0 * atan (tan(hfov / 2.0) / GetAspect());
}
  inline real GetFovyRadian() const
  {
    return _Fovy;
  }

  inline real GetFovyDegrees() const
  {
    return _Fovy * 180.0 / M_PI;
  }

  BBox<Vec3r> scene3DBBox() const { return _ModelRootNode->bbox(); }

	real znear();
	real zfar();


public:
  /*! Core scene drawing */
  void            DrawScene(SceneVisitor *iRenderer);

  /*! 2D Scene Drawing */
  void            Draw2DScene(SceneVisitor *iRenderer);

  
protected:

  /*! fabs or abs */
  inline int rabs(int x) {return abs(x);}
  inline real rabs(real x) {return fabs(x);}

  
protected:
  float _Fovy;

  //The root node container
  NodeGroup        _RootNode;
  NodeDrawingStyle *_ModelRootNode;
  NodeDrawingStyle *_SilhouetteRootNode;
  NodeDrawingStyle *_DebugRootNode;
 
  NodeGroup _Light;

  real _minBBox;
  real _maxBBox;
  real _maxAbs;
  real _minAbs;

  // 2D Scene
  bool _Draw2DScene;
  bool _Draw3DScene;  NodeGroup _p2DNode;
  NodeDrawingStyle *_p2DSelectionNode;

};

#endif // APPVIEW_H
