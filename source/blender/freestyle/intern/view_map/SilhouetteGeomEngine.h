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

#ifndef __FREESTYLE_SILHOUETTE_GEOM_ENGINE_H__
#define __FREESTYLE_SILHOUETTE_GEOM_ENGINE_H__

/** \file blender/freestyle/intern/view_map/SilhouetteGeomEngine.h
 *  \ingroup freestyle
 *  \brief Class to perform all geometric operations dedicated to silhouette. That, for example, implies that
 *         this geom engine has as member data the viewpoint, transformations, projections...
 *  \author Stephane Grabli
 *  \date 03/09/2002
 */

#include <vector>

#include "../geometry/Geom.h"

#include "../system/FreestyleConfig.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

using namespace Geometry;

class SVertex;
class FEdge;

class SilhouetteGeomEngine
{
private:
	// The viewpoint under which the silhouette has to be computed
	static Vec3r _Viewpoint;
	static real _translation[3];
	// the model view matrix (_modelViewMatrix[i][j] means element of line i and column j)
	static real _modelViewMatrix[4][4];
	// the projection matrix (_projectionMatrix[i][j] means element of line i and column j)
	static real _projectionMatrix[4][4];
	// the global transformation from world to screen (projection included)
	// (_transform[i][j] means element of line i and column j)
	static real _transform[4][4];
	// the viewport
	static int _viewport[4];
	static real _Focal;

	static real _znear;
	static real _zfar;

	// GL style (column major) projection matrix
	static real _glProjectionMatrix[4][4];
	// GL style (column major) model view matrix
	static real _glModelViewMatrix[4][4];

	static bool _isOrthographicProjection;

	static SilhouetteGeomEngine *_pInstance;

public:
	/*! retrieves an instance on the singleton */
	static SilhouetteGeomEngine *getInstance()
	{
		if (_pInstance == NULL) {
			_pInstance = new SilhouetteGeomEngine;
		}
		return _pInstance;
	}

	/*! Sets the current viewpoint */
	static inline void setViewpoint(const Vec3r& ivp)
	{
		_Viewpoint = ivp;
	}

	/*! Sets the current transformation
	 *    iModelViewMatrix
	 *      The 4x4 model view matrix, in column major order (openGL like).
	 *    iProjection matrix
	 *      The 4x4 projection matrix, in column major order (openGL like).
	 *    iViewport
	 *      The viewport. 4 real array: origin.x, origin.y, width, length
	 *    iFocal
	 *      The focal length
	 */
	static void setTransform(const real iModelViewMatrix[4][4], const real iProjectionMatrix[4][4],
	                         const int iViewport[4], real iFocal);

	/*! Sets the current znear and zfar */
	static void setFrustum(real iZNear, real iZFar);

	/* accessors */
	static void retrieveViewport(int viewport[4]);

	/*! Projects the silhouette in camera coordinates
	 *  This method modifies the ioEdges passed as argument.
	 *    ioVertices
	 *      The vertices to project. It is modified during the operation.
	 */
	static void ProjectSilhouette(std::vector<SVertex*>& ioVertices);
	static void ProjectSilhouette(SVertex *ioVertex);

	/*! transforms the parameter t defining a 2D intersection for edge fe in order to obtain
	 *  the parameter giving the corresponding 3D intersection.
	 *  Returns the 3D parameter
	 *    fe
	 *      The edge
	 *    t
	 *      The parameter for the 2D intersection.
	 */
	static real ImageToWorldParameter(FEdge *fe, real t);

	/*! From world to image */
	static Vec3r WorldToImage(const Vec3r& M);

	/*! From camera to image */
	static Vec3r CameraToImage(const Vec3r& M);

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:SilhouetteGeomEngine")
#endif
};

} /* namespace Freestyle */

#endif // __FREESTYLE_SILHOUETTE_GEOM_ENGINE_H__
