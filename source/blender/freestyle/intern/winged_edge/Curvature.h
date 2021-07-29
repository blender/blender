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
 * The Original Code is:
 *     GTS - Library for the manipulation of triangulated surfaces
 *     Copyright (C) 1999 Stephane Popinet
 * and:
 *     OGF/Graphite: Geometry and Graphics Programming Library + Utilities
 *     Copyright (C) 2000-2003 Bruno Levy
 *     Contact: Bruno Levy levy@loria.fr
 *         ISA Project
 *         LORIA, INRIA Lorraine,
 *         Campus Scientifique, BP 239
 *         54506 VANDOEUVRE LES NANCY CEDEX
 *         FRANCE
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __FREESTYLE_CURVATURE_H__
#define __FREESTYLE_CURVATURE_H__

/** \file blender/freestyle/intern/winged_edge/Curvature.h
 *  \ingroup freestyle
 *  \brief GTS - Library for the manipulation of triangulated surfaces
 *  \author Stephane Popinet
 *  \date 1999
 *  \brief OGF/Graphite: Geometry and Graphics Programming Library + Utilities
 *  \author Bruno Levy
 *  \date 2000-2003
 */

#include "../geometry/Geom.h"

#include "../system/FreestyleConfig.h"
#include "../system/Precision.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

using namespace Geometry;

class WVertex;

class CurvatureInfo
{
public:
	CurvatureInfo()
	{
		K1 = 0.0;
		K2 = 0.0;
		e1 = Vec3r(0.0, 0.0, 0.0);
		e2 = Vec3r(0.0, 0.0, 0.0);
		Kr = 0.0;
		dKr = 0.0;
		er = Vec3r(0.0, 0.0, 0.0);
	}

	CurvatureInfo(const CurvatureInfo& iBrother)
	{
		K1 = iBrother.K1;
		K2 = iBrother.K2;
		e1 = iBrother.e1;
		e2 = iBrother.e2;
		Kr = iBrother.Kr;
		dKr = iBrother.dKr;
		er = iBrother.er;
	}

	CurvatureInfo(const CurvatureInfo& ca, const CurvatureInfo& cb, real t)
	{
		K1 = ca.K1 + t * (cb.K1 - ca.K1);
		K2 = ca.K2 + t * (cb.K2 - ca.K2);
		e1 = ca.e1 + t * (cb.e1 - ca.e1);
		e2 = ca.e2 + t * (cb.e2 - ca.e2);
		Kr = ca.Kr + t * (cb.Kr - ca.Kr);
		dKr = ca.dKr + t * (cb.dKr - ca.dKr);
		er = ca.er + t * (cb.er - ca.er);
	}

	real K1;  // maximum curvature
	real K2;  // minimum curvature
	Vec3r e1; // maximum curvature direction
	Vec3r e2; // minimum curvature direction
	real Kr;  // radial curvature
	real dKr; // radial curvature
	Vec3r er; // radial curvature direction

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:CurvatureInfo")
#endif
};

class Face_Curvature_Info
{
public:
	Face_Curvature_Info() {}

	~Face_Curvature_Info()
	{
		for (vector<CurvatureInfo*>::iterator ci = vec_curvature_info.begin(), ciend = vec_curvature_info.end();
		     ci != ciend;
		     ++ci)
		{
			delete (*ci);
		}
		vec_curvature_info.clear();
	}

	vector<CurvatureInfo *> vec_curvature_info;

#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Face_Curvature_Info")
#endif
};

bool gts_vertex_mean_curvature_normal(WVertex *v, Vec3r &n);

bool gts_vertex_gaussian_curvature(WVertex *v, real *Kg);

void gts_vertex_principal_curvatures(real Kh, real Kg, real *K1, real *K2);

void gts_vertex_principal_directions(WVertex *v, Vec3r Kh, real Kg, Vec3r &e1, Vec3r &e2);

namespace OGF {

class NormalCycle ;

void compute_curvature_tensor( WVertex *start, double radius, NormalCycle& nc);

void compute_curvature_tensor_one_ring(WVertex *start, NormalCycle& nc);

}  // OGF namespace

} /* namespace Freestyle */

#endif /* __FREESTYLE_CURVATURE_H__ */
