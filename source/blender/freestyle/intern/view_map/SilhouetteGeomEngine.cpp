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

/** \file blender/freestyle/intern/view_map/SilhouetteGeomEngine.cpp
 *  \ingroup freestyle
 *  \brief Class to perform all geometric operations dedicated to silhouette. That, for example, implies that
 *         this geom engine has as member data the viewpoint, transformations, projections...
 *  \author Stephane Grabli
 *  \date 03/09/2002
 */

#include <cstring>
#include <cstdio>

#include "Silhouette.h"
#include "SilhouetteGeomEngine.h"

#include "../geometry/GeomUtils.h"

#include "BKE_global.h"

using namespace std;

namespace Freestyle {

Vec3r SilhouetteGeomEngine::_Viewpoint = Vec3r(0, 0, 0);
real SilhouetteGeomEngine::_translation[3] = {0, 0, 0};
real SilhouetteGeomEngine::_modelViewMatrix[4][4] = {
	{1, 0, 0, 0},
	{0, 1, 0, 0},
	{0, 0, 1, 0},
	{0, 0, 0, 1}
};
real SilhouetteGeomEngine::_projectionMatrix[4][4] = {
	{1, 0, 0, 0},
	{0, 1, 0, 0},
	{0, 0, 1, 0},
	{0, 0, 0, 1}
};
real SilhouetteGeomEngine::_transform[4][4] = {
	{1, 0, 0, 0},
	{0, 1, 0, 0},
	{0, 0, 1, 0},
	{0, 0, 0, 1}
};
int SilhouetteGeomEngine::_viewport[4] = {1, 1, 1, 1};
real SilhouetteGeomEngine::_Focal = 0.0;

real SilhouetteGeomEngine::_glProjectionMatrix[4][4] = {
	{1, 0, 0, 0},
	{0, 1, 0, 0},
	{0, 0, 1, 0},
	{0, 0, 0, 1}
};
real SilhouetteGeomEngine::_glModelViewMatrix[4][4] = {
	{1, 0, 0, 0},
	{0, 1, 0, 0},
	{0, 0, 1, 0},
	{0, 0, 0, 1}
};
real SilhouetteGeomEngine::_znear = 0.0;
real SilhouetteGeomEngine::_zfar = 100.0;
bool SilhouetteGeomEngine::_isOrthographicProjection = false;

SilhouetteGeomEngine *SilhouetteGeomEngine::_pInstance = NULL;

void SilhouetteGeomEngine::setTransform(const real iModelViewMatrix[4][4], const real iProjectionMatrix[4][4],
                                        const int iViewport[4], real iFocal)
{
	unsigned int i, j;
	_translation[0] = iModelViewMatrix[3][0];
	_translation[1] = iModelViewMatrix[3][1];
	_translation[2] = iModelViewMatrix[3][2];

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			_modelViewMatrix[i][j] = iModelViewMatrix[j][i];
			_glModelViewMatrix[i][j] = iModelViewMatrix[i][j];
		}
	}

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			_projectionMatrix[i][j] = iProjectionMatrix[j][i];
			_glProjectionMatrix[i][j] = iProjectionMatrix[i][j];
		}
	}

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++) {
			_transform[i][j] = 0;
			for (unsigned int k = 0; k < 4; k++)
				_transform[i][j] += _projectionMatrix[i][k] * _modelViewMatrix[k][j];
		}
	}

	for (i = 0; i < 4; i++) {
		_viewport[i] = iViewport[i];
	}
	_Focal = iFocal;

	_isOrthographicProjection = (iProjectionMatrix[3][3] != 0.0);
}

void SilhouetteGeomEngine::setFrustum(real iZNear, real iZFar)
{
	_znear = iZNear;
	_zfar = iZFar;
}

void SilhouetteGeomEngine::retrieveViewport(int viewport[4])
{
	memcpy(viewport, _viewport, 4 * sizeof(int));
}

void SilhouetteGeomEngine::ProjectSilhouette(vector<SVertex*>& ioVertices)
{
	Vec3r newPoint;
	vector<SVertex*>::iterator sv, svend;
	for (sv = ioVertices.begin(), svend = ioVertices.end(); sv != svend; sv++) {
		GeomUtils::fromWorldToImage((*sv)->point3D(), newPoint, _modelViewMatrix, _projectionMatrix, _viewport);
		(*sv)->setPoint2D(newPoint);
	}
}

void SilhouetteGeomEngine::ProjectSilhouette(SVertex *ioVertex)
{
	Vec3r newPoint;
	GeomUtils::fromWorldToImage(ioVertex->point3D(), newPoint, _modelViewMatrix, _projectionMatrix, _viewport);
	ioVertex->setPoint2D(newPoint);
}

real SilhouetteGeomEngine::ImageToWorldParameter(FEdge *fe, real t)
{
	if (_isOrthographicProjection)
		return t;

	// we need to compute for each parameter t the corresponding parameter T which gives the intersection in 3D.
	real T;

	// suffix w for world, c for camera, r for retina, i for image
	Vec3r Aw = (fe)->vertexA()->point3D();
	Vec3r Bw = (fe)->vertexB()->point3D();
	Vec3r Ac, Bc;
	GeomUtils::fromWorldToCamera(Aw, Ac, _modelViewMatrix);
	GeomUtils::fromWorldToCamera(Bw, Bc, _modelViewMatrix);
	Vec3r ABc = Bc - Ac;
#if 0
	if (G.debug & G_DEBUG_FREESTYLE) {
		cout << "Ac " << Ac << endl;
		cout << "Bc " << Bc << endl;
		cout << "ABc " << ABc << endl;
	}
#endif
	Vec3r Ai = (fe)->vertexA()->point2D();
	Vec3r Bi = (fe)->vertexB()->point2D();
	Vec3r Ii = Ai + t * (Bi - Ai); // the intersection point in the 2D image space
	Vec3r Ir, Ic;
	GeomUtils::fromImageToRetina(Ii, Ir, _viewport);

	real alpha, beta, denom;
	real m11 = _projectionMatrix[0][0];
	real m13 = _projectionMatrix[0][2];
	real m22 = _projectionMatrix[1][1];
	real m23 = _projectionMatrix[1][2];

	if (fabs(ABc[0]) > 1.0e-6) {
		alpha = ABc[2] / ABc[0];
		beta = Ac[2] - alpha * Ac[0];
		denom = alpha * (Ir[0] + m13) + m11;
		if (fabs(denom) < 1.0e-6)
			goto iter;
		Ic[0] = -beta * (Ir[0] + m13) / denom;
#if 0
		Ic[1] = -(Ir[1] + m23) * (alpha * Ic[0] + beta) / m22;
		Ic[2] = alpha * (Ic[0] - Ac[0]) + Ac[2];
#endif
		T = (Ic[0] - Ac[0]) / ABc[0];

	}
	else if (fabs(ABc[1]) > 1.0e-6) {
		alpha = ABc[2] / ABc[1];
		beta = Ac[2] - alpha * Ac[1];
		denom = alpha * (Ir[1] + m23) + m22;
		if (fabs(denom) < 1.0e-6)
			goto iter;
		Ic[1] = -beta * (Ir[1] + m23) / denom;
#if 0
		Ic[0] = -(Ir[0] + m13) * (alpha * Ic[1] + beta) / m11;
		Ic[2] = alpha * (Ic[1] - Ac[1]) + Ac[2];
#endif
		T = (Ic[1] - Ac[1]) / ABc[1];
	}
	else {
iter:
		bool x_coords, less_than;
		if (fabs(Bi[0] - Ai[0]) > 1.0e-6) {
			x_coords = true;
			less_than = Ai[0] < Bi[0];
		}
		else {
			x_coords = false;
			less_than = Ai[1] < Bi[1];
		}
		Vec3r Pc, Pr, Pi;
		real T_sta = 0.0;
		real T_end = 1.0;
		real delta_x, delta_y, dist, dist_threshold = 1.0e-6;
		int i, max_iters = 100;
		for (i = 0; i < max_iters; i++) {
			T = T_sta + 0.5 * (T_end - T_sta);
			Pc = Ac + T * ABc;
			GeomUtils::fromCameraToRetina(Pc, Pr, _projectionMatrix);
			GeomUtils::fromRetinaToImage(Pr, Pi, _viewport);
			delta_x = Ii[0] - Pi[0];
			delta_y = Ii[1] - Pi[1];
			dist = sqrt(delta_x * delta_x + delta_y * delta_y);
			if (dist < dist_threshold)
				break;
			if (x_coords) {
				if (less_than) {
					if (Pi[0] < Ii[0])
						T_sta = T;
					else
						T_end = T;
				}
				else {
					if (Pi[0] > Ii[0])
						T_sta = T;
					else
						T_end = T;
				}
			}
			else {
				if (less_than) {
					if (Pi[1] < Ii[1])
						T_sta = T;
					else
						T_end = T;
				}
				else {
					if (Pi[1] > Ii[1])
						T_sta = T;
					else
						T_end = T;
				}
			}
		}
#if 0
		if (G.debug & G_DEBUG_FREESTYLE) {
			cout << "SilhouetteGeomEngine::ImageToWorldParameter(): #iters = " << i << ", dist = " << dist << "\n";
		}
#endif
		if (i == max_iters && G.debug & G_DEBUG_FREESTYLE) {
			cout << "SilhouetteGeomEngine::ImageToWorldParameter(): reached to max_iters (dist = " << dist << ")\n";
		}
	}

	return T;
}

Vec3r SilhouetteGeomEngine::WorldToImage(const Vec3r& M)
{
	Vec3r newPoint;
	GeomUtils::fromWorldToImage(M, newPoint, _transform, _viewport);
	return newPoint;
}

Vec3r SilhouetteGeomEngine::CameraToImage(const Vec3r& M)
{
	Vec3r newPoint, p;
	GeomUtils::fromCameraToRetina(M, p, _projectionMatrix);
	GeomUtils::fromRetinaToImage(p, newPoint, _viewport);
	return newPoint;
}

} /* namespace Freestyle */
