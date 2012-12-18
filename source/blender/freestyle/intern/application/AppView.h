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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __APPVIEW_H__
#define __APPVIEW_H__

/** \file blender/freestyle/intern/application/AppView.h
 *  \ingroup freestyle
 */

#if !defined(WIN32) || defined(__GNUC__)
#  include <algorithm>
#  define __min(x,y) (min(x,y))
#  define __max(x,y) (max(x,y))
   using namespace std;
#endif // WIN32

#include "AppConfig.h"
#include "../geometry/Geom.h"
#include "../geometry/BBox.h"
#include "../scene_graph/NodeDrawingStyle.h"
#include "../system/Precision.h"

using namespace Geometry;

class AppView
{
public:
	AppView(const char *iName = 0);
	virtual ~AppView();

public:
	//inherited
	inline unsigned int width() {return _width;}
	inline unsigned int height() {return _height;}
	inline BBox<Vec2i> border() {return _border;}
	inline float thickness() {return _thickness;}
	inline void setWidth(unsigned int width) {_width = width;}
	inline void setHeight(unsigned int height) {_height = height;}
	inline void setBorder(int xmin, int ymin, int xmax, int ymax) {
		_border = BBox<Vec2i>(Vec2i(xmin, ymin), Vec2i(xmax, ymax));
	}
	inline void setThickness(float thickness) {_thickness = thickness;}

protected:
	unsigned int _width, _height;
	BBox<Vec2i> _border;
	float _thickness;

public:
	/*! Sets the model to draw in the viewer
	 *  iModel
	 *    The Root Node of the model
	 */
	inline void setModel(NodeGroup *iModel)
	{
		if (0 != _ModelRootNode->numberOfChildren()) {
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

		_minBBox = __min(__min(_ModelRootNode->bbox().getMin()[0], _ModelRootNode->bbox().getMin()[1]),
		                 _ModelRootNode->bbox().getMin()[2]);
		_maxBBox = __max(__max(_ModelRootNode->bbox().getMax()[0], _ModelRootNode->bbox().getMax()[1]),
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
		if (0 != _DebugRootNode->numberOfChildren()) {
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

		_minBBox = __min(__min(_ModelRootNode->bbox().getMin()[0], _ModelRootNode->bbox().getMin()[1]),
		                 _ModelRootNode->bbox().getMin()[2]);
		_maxBBox = __max(__max(_ModelRootNode->bbox().getMax()[0], _ModelRootNode->bbox().getMax()[1]),
		                 _ModelRootNode->bbox().getMax()[2]);

		_maxAbs = __max(rabs(_minBBox), rabs(_maxBBox));
		_minAbs = __min(rabs(_minBBox), rabs(_maxBBox));
	}

	inline void DetachModel()
	{
		_ModelRootNode->DetachChildren();
		_ModelRootNode->clearBBox();

#if 0
		// 2D Scene
		_p2DNode.DetachChildren();
		_pFENode->DetachChildren();
		_pVisibleSilhouetteNode->DetachChildren();
#endif
	}

	inline void DetachSilhouette()
	{
		_SilhouetteRootNode->DetachChildren();
#if 0
		_pFENode->DetachChildren();
		_pVisibleSilhouetteNode->DetachChildren();
#endif
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
		return ((real)_width / (real)_height);
	}

	void setHorizontalFov(float hfov)
	{
		_Fovy = 2.0 * atan (tan(hfov / 2.0) / GetAspect());
	}

	inline real GetFovyRadian() const
	{
		return _Fovy;
	}

	inline real GetFovyDegrees() const
	{
		return _Fovy * 180.0 / M_PI;  // TODO Use RAD2DEG here too?
	}

	BBox<Vec3r> scene3DBBox() const {return _ModelRootNode->bbox();}

	real znear();
	real zfar();

public:
	/*! Core scene drawing */
	void DrawScene(SceneVisitor *iRenderer);

	/*! 2D Scene Drawing */
	void Draw2DScene(SceneVisitor *iRenderer);

protected:
	/*! fabs or abs */
	inline int rabs(int x) {return abs(x);}
	inline real rabs(real x) {return fabs(x);}

protected:
	float _Fovy;

	//The root node container
	NodeGroup         _RootNode;
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
	bool _Draw3DScene;
	NodeGroup _p2DNode;
	NodeDrawingStyle *_p2DSelectionNode;
};

#endif // __APPVIEW_H__
