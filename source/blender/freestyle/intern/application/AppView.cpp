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

/** \file blender/freestyle/intern/application/AppView.cpp
 *  \ingroup freestyle
 */

/* This header file needs to be included first, in order to avoid a
   compilation with MinGW (see the commit log of revision 28253) */
extern "C" {
#include "BLI_jitter.h"
}

#include <iostream>

#include "Controller.h"
#include "AppConfig.h"
#include "AppView.h"
#include "../view_map/Silhouette.h"
#include "../view_map/ViewMap.h"
#include "../scene_graph/LineRep.h"
#include "../scene_graph/NodeLight.h"
#include "../scene_graph/NodeShape.h"
#include "../scene_graph/VertexRep.h"
#include "../stroke/Canvas.h"
#include "../system/StringUtils.h"

extern "C" {
#include "BLI_blenlib.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

#if 1 // FRS_antialiasing
#  include "BKE_global.h"
#  include "DNA_scene_types.h"
#endif

#include "FRS_freestyle.h"
}

namespace Freestyle {

AppView::AppView(const char *iName)
{
	_Fovy = DEG2RADF(30.0f);
	_ModelRootNode = new NodeDrawingStyle;
	_SilhouetteRootNode = new NodeDrawingStyle;
	_DebugRootNode = new NodeDrawingStyle;

	_RootNode.AddChild(_ModelRootNode);
	_SilhouetteRootNode->setStyle(DrawingStyle::LINES);
	_SilhouetteRootNode->setLightingEnabled(false);
	_SilhouetteRootNode->setLineWidth(2.0f);
	_SilhouetteRootNode->setPointSize(3.0f);

	_RootNode.AddChild(_SilhouetteRootNode);

	_DebugRootNode->setStyle(DrawingStyle::LINES);
	_DebugRootNode->setLightingEnabled(false);
	_DebugRootNode->setLineWidth(1.0f);

	_RootNode.AddChild(_DebugRootNode);

	_minBBox = std::min(std::min(_ModelRootNode->bbox().getMin()[0], _ModelRootNode->bbox().getMin()[1]),
	                    _ModelRootNode->bbox().getMin()[2]);
	_maxBBox = std::max(std::max(_ModelRootNode->bbox().getMax()[0], _ModelRootNode->bbox().getMax()[1]),
	                    _ModelRootNode->bbox().getMax()[2]);

	_maxAbs = std::max(rabs(_minBBox), rabs(_maxBBox));
	_minAbs = std::min(rabs(_minBBox), rabs(_maxBBox));

	_p2DSelectionNode = new NodeDrawingStyle;
	_p2DSelectionNode->setLightingEnabled(false);
	_p2DSelectionNode->setStyle(DrawingStyle::LINES);
	_p2DSelectionNode->setLineWidth(5.0f);

	_p2DNode.AddChild(_p2DSelectionNode);

	NodeLight *light = new NodeLight;
	_Light.AddChild(light);
}

AppView::~AppView()
{
	/*int ref =*/ /* UNUSED */ _RootNode.destroy();

	_Light.destroy();
	/*ref =*/ /* UNUSED */ _p2DNode.destroy();
}

real AppView::distanceToSceneCenter()
{
	BBox<Vec3r> bbox = _ModelRootNode->bbox();

	Vec3r v(freestyle_viewpoint[0], freestyle_viewpoint[1], freestyle_viewpoint[2]);
	v -= 0.5 * (bbox.getMin() + bbox.getMax());

	return v.norm();
}

real AppView::znear()
{
	BBox<Vec3r> bbox = _ModelRootNode->bbox();
	Vec3r u = bbox.getMin();
	Vec3r v = bbox.getMax();
	Vec3r cameraCenter(freestyle_viewpoint[0], freestyle_viewpoint[1], freestyle_viewpoint[2]);

	Vec3r w1(u[0], u[1], u[2]);
	Vec3r w2(v[0], u[1], u[2]);
	Vec3r w3(u[0], v[1], u[2]);
	Vec3r w4(v[0], v[1], u[2]);
	Vec3r w5(u[0], u[1], v[2]);
	Vec3r w6(v[0], u[1], v[2]);
	Vec3r w7(u[0], v[1], v[2]);
	Vec3r w8(v[0], v[1], v[2]);

	real _znear = std::min((w1 - cameraCenter).norm(),
	                       std::min((w2 - cameraCenter).norm(),
	                                std::min((w3 - cameraCenter).norm(),
	                                         std::min((w4 - cameraCenter).norm(),
	                                                  std::min((w5 - cameraCenter).norm(),
	                                                           std::min((w6 - cameraCenter).norm(),
	                                                                    std::min((w7 - cameraCenter).norm(),
	                                                                             (w8 - cameraCenter).norm()
	                                                                            )
	                                                                   )
	                                                          )
	                                                 )
	                                        )
	                               )
	                      );

	return std::max(_znear, 0.001);
}

real AppView::zfar()
{
	BBox<Vec3r> bbox = _ModelRootNode->bbox();
	Vec3r u = bbox.getMin();
	Vec3r v = bbox.getMax();
	Vec3r cameraCenter(freestyle_viewpoint[0], freestyle_viewpoint[1], freestyle_viewpoint[2]);

	Vec3r w1(u[0], u[1], u[2]);
	Vec3r w2(v[0], u[1], u[2]);
	Vec3r w3(u[0], v[1], u[2]);
	Vec3r w4(v[0], v[1], u[2]);
	Vec3r w5(u[0], u[1], v[2]);
	Vec3r w6(v[0], u[1], v[2]);
	Vec3r w7(u[0], v[1], v[2]);
	Vec3r w8(v[0], v[1], v[2]);

	real _zfar = std::max((w1 - cameraCenter).norm(),
	                      std::max((w2 - cameraCenter).norm(),
	                               std::max((w3 - cameraCenter).norm(),
	                                        std::max((w4 - cameraCenter).norm(),
	                                                 std::max((w5 - cameraCenter).norm(),
	                                                          std::max((w6 - cameraCenter).norm(),
	                                                                   std::max((w7 - cameraCenter).norm(),
	                                                                            (w8 - cameraCenter).norm()
	                                                                           )
	                                                                  )
	                                                         )
	                                                )
	                                       )
	                              )
	                     );

	return _zfar;
}

real AppView::GetFocalLength()
{
	real Near = std::max(0.1, (real)(-2.0f * _maxAbs + distanceToSceneCenter()));
	return Near;
}

} /* namespace Freestyle */
