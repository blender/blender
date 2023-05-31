/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 * The Original Code is:
 *     GTS - Library for the manipulation of triangulated surfaces
 *     Copyright 1999 Stephane Popinet
 * and:
 *     OGF/Graphite: Geometry and Graphics Programming Library + Utilities
 *     Copyright 2000-2003 Bruno Levy <levy@loria.fr> */

#pragma once

/** \file
 * \ingroup freestyle
 * \brief GTS - Library for the manipulation of triangulated surfaces
 * \brief OGF/Graphite: Geometry and Graphics Programming Library + Utilities
 */

#include "../geometry/Geom.h"

#include "../system/FreestyleConfig.h"
#include "../system/Precision.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

namespace Freestyle {

using namespace Geometry;

class WVertex;

class CurvatureInfo {
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

  CurvatureInfo(const CurvatureInfo &iBrother)
  {
    K1 = iBrother.K1;
    K2 = iBrother.K2;
    e1 = iBrother.e1;
    e2 = iBrother.e2;
    Kr = iBrother.Kr;
    dKr = iBrother.dKr;
    er = iBrother.er;
  }

  CurvatureInfo(const CurvatureInfo &ca, const CurvatureInfo &cb, real t)
  {
    K1 = ca.K1 + t * (cb.K1 - ca.K1);
    K2 = ca.K2 + t * (cb.K2 - ca.K2);
    e1 = ca.e1 + t * (cb.e1 - ca.e1);
    e2 = ca.e2 + t * (cb.e2 - ca.e2);
    Kr = ca.Kr + t * (cb.Kr - ca.Kr);
    dKr = ca.dKr + t * (cb.dKr - ca.dKr);
    er = ca.er + t * (cb.er - ca.er);
  }

  real K1;   // maximum curvature
  real K2;   // minimum curvature
  Vec3r e1;  // maximum curvature direction
  Vec3r e2;  // minimum curvature direction
  real Kr;   // radial curvature
  real dKr;  // radial curvature
  Vec3r er;  // radial curvature direction

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:CurvatureInfo")
#endif
};

class Face_Curvature_Info {
 public:
  Face_Curvature_Info() {}

  ~Face_Curvature_Info()
  {
    for (vector<CurvatureInfo *>::iterator ci = vec_curvature_info.begin(),
                                           ciend = vec_curvature_info.end();
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

/**
 * \param v: a #WVertex.
 * \param Kh: the Mean Curvature Normal at \a v.
 *
 * Computes the Discrete Mean Curvature Normal approximation at \a v.
 * The mean curvature at \a v is half the magnitude of the vector \a Kh.
 *
 * \note the normal computed is not unit length, and may point either into or out of the surface,
 * depending on the curvature at \a v. It is the responsibility of the caller of the function to
 * use the mean curvature normal appropriately.
 *
 * This approximation is from the paper:
 *     Discrete Differential-Geometry Operators for Triangulated 2-Manifolds
 *     Mark Meyer, Mathieu Desbrun, Peter Schroder, Alan H. Barr
 *     VisMath '02, Berlin (Germany)
 *     http://www-grail.usc.edu/pubs.html
 *
 *  Returns: %true if the operator could be evaluated, %false if the evaluation failed for some
 * reason (`v` is boundary or is the endpoint of a non-manifold edge.)
 */
bool gts_vertex_mean_curvature_normal(WVertex *v, Vec3r &Kh);

/**
 * \param v: a #WVertex.
 * \param Kg: the Discrete Gaussian Curvature approximation at \a v.
 *
 * Computes the Discrete Gaussian Curvature approximation at \a v.
 *
 * This approximation is from the paper:
 *     Discrete Differential-Geometry Operators for Triangulated 2-Manifolds
 *     Mark Meyer, Mathieu Desbrun, Peter Schroder, Alan H. Barr
 *     VisMath '02, Berlin (Germany)
 *     http://www-grail.usc.edu/pubs.html
 *
 * Returns: %true if the operator could be evaluated, %false if the evaluation failed for some
 * reason (`v` is boundary or is the endpoint of a non-manifold edge.)
 */
bool gts_vertex_gaussian_curvature(WVertex *v, real *Kg);

/**
 * \param Kh: mean curvature.
 * \param Kg: Gaussian curvature.
 * \param K1: first principal curvature.
 * \param K2: second principal curvature.
 *
 * Computes the principal curvatures at a point given the mean and Gaussian curvatures at that
 * point.
 *
 * The mean curvature can be computed as one-half the magnitude of the vector computed by
 * #gts_vertex_mean_curvature_normal().
 *
 * The Gaussian curvature can be computed with gts_vertex_gaussian_curvature().
 */
void gts_vertex_principal_curvatures(real Kh, real Kg, real *K1, real *K2);

/**
 * \param v: a #WVertex.
 * \param Kh: mean curvature normal (a #Vec3r).
 * \param Kg: Gaussian curvature (a real).
 * \param e1: first principal curvature direction (direction of largest curvature).
 * \param e2: second principal curvature direction.
 *
 * Computes the principal curvature directions at a point given \a Kh and \a Kg,
 * the mean curvature normal and Gaussian curvatures at that point, computed with
 * #gts_vertex_mean_curvature_normal() and #gts_vertex_gaussian_curvature(), respectively.
 *
 * Note that this computation is very approximate and tends to be unstable. Smoothing of the
 * surface or the principal directions may be necessary to achieve reasonable results.
 */
void gts_vertex_principal_directions(WVertex *v, Vec3r Kh, real Kg, Vec3r &e1, Vec3r &e2);

namespace OGF {

class NormalCycle;

void compute_curvature_tensor(WVertex *start, double radius, NormalCycle &nc);

void compute_curvature_tensor_one_ring(WVertex *start, NormalCycle &nc);

}  // namespace OGF

} /* namespace Freestyle */
