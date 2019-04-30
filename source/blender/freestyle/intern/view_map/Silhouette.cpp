/*
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
 */

/** \file
 * \ingroup freestyle
 * \brief Classes to define a silhouette structure
 */

#include "Silhouette.h"
#include "ViewMap.h"

namespace Freestyle {

/**********************************/
/*                                */
/*                                */
/*             SVertex            */
/*                                */
/*                                */
/**********************************/

Nature::VertexNature SVertex::getNature() const
{
  Nature::VertexNature nature = Nature::S_VERTEX;
  if (_pViewVertex)
    nature |= _pViewVertex->getNature();
  return nature;
}

SVertex *SVertex::castToSVertex()
{
  return this;
}

ViewVertex *SVertex::castToViewVertex()
{
  return _pViewVertex;
}

NonTVertex *SVertex::castToNonTVertex()
{
  return dynamic_cast<NonTVertex *>(_pViewVertex);
}

TVertex *SVertex::castToTVertex()
{
  return dynamic_cast<TVertex *>(_pViewVertex);
}

float SVertex::shape_importance() const
{
  return shape()->importance();
}

#if 0
Material SVertex::material() const
{
  return _Shape->material();
}
#endif

Id SVertex::shape_id() const
{
  return _Shape->getId();
}

const SShape *SVertex::shape() const
{
  return _Shape;
}

const int SVertex::qi() const
{
  if (getNature() & Nature::T_VERTEX)
    Exception::raiseException();
  return (_FEdges[0])->qi();
}

occluder_container::const_iterator SVertex::occluders_begin() const
{
  if (getNature() & Nature::T_VERTEX)
    Exception::raiseException();
  return (_FEdges[0])->occluders_begin();
}

occluder_container::const_iterator SVertex::occluders_end() const
{
  if (getNature() & Nature::T_VERTEX)
    Exception::raiseException();
  return (_FEdges[0])->occluders_end();
}

bool SVertex::occluders_empty() const
{
  if (getNature() & Nature::T_VERTEX)
    Exception::raiseException();
  return (_FEdges[0])->occluders_empty();
}

int SVertex::occluders_size() const
{
  if (getNature() & Nature::T_VERTEX)
    Exception::raiseException();
  return (_FEdges[0])->occluders_size();
}

const Polygon3r &SVertex::occludee() const
{
  if (getNature() & Nature::T_VERTEX)
    Exception::raiseException();
  return (_FEdges[0])->occludee();
}

const SShape *SVertex::occluded_shape() const
{
  if (getNature() & Nature::T_VERTEX)
    Exception::raiseException();
  return (_FEdges[0])->occluded_shape();
}

const bool SVertex::occludee_empty() const
{
  if (getNature() & Nature::T_VERTEX)
    Exception::raiseException();
  return (_FEdges[0])->occludee_empty();
}

real SVertex::z_discontinuity() const
{
  if (getNature() & Nature::T_VERTEX)
    Exception::raiseException();
  return (_FEdges[0])->z_discontinuity();
}

FEdge *SVertex::fedge()
{
  if (getNature() & Nature::T_VERTEX)
    return NULL;
  return _FEdges[0];
}

FEdge *SVertex::getFEdge(Interface0D &inter)
{
  FEdge *result = NULL;
  SVertex *iVertexB = dynamic_cast<SVertex *>(&inter);
  if (!iVertexB)
    return result;
  vector<FEdge *>::const_iterator fe = _FEdges.begin(), feend = _FEdges.end();
  for (; fe != feend; ++fe) {
    if ((((*fe)->vertexA() == this) && ((*fe)->vertexB() == iVertexB)) ||
        (((*fe)->vertexB() == this) && ((*fe)->vertexA() == iVertexB))) {
      result = (*fe);
    }
  }
  if ((result == 0) && (getNature() & Nature::T_VERTEX)) {
    SVertex *brother;
    ViewVertex *vvertex = viewvertex();
    TVertex *tvertex = dynamic_cast<TVertex *>(vvertex);
    if (tvertex) {
      brother = tvertex->frontSVertex();
      if (this == brother)
        brother = tvertex->backSVertex();
      const vector<FEdge *> &fedges = brother->fedges();
      for (fe = fedges.begin(), feend = fedges.end(); fe != feend; ++fe) {
        if ((((*fe)->vertexA() == brother) && ((*fe)->vertexB() == iVertexB)) ||
            (((*fe)->vertexB() == brother) && ((*fe)->vertexA() == iVertexB))) {
          result = (*fe);
        }
      }
    }
  }
  if ((result == 0) && (iVertexB->getNature() & Nature::T_VERTEX)) {
    SVertex *brother;
    ViewVertex *vvertex = iVertexB->viewvertex();
    TVertex *tvertex = dynamic_cast<TVertex *>(vvertex);
    if (tvertex) {
      brother = tvertex->frontSVertex();
      if (iVertexB == brother)
        brother = tvertex->backSVertex();
      for (fe = _FEdges.begin(), feend = _FEdges.end(); fe != feend; ++fe) {
        if ((((*fe)->vertexA() == this) && ((*fe)->vertexB() == brother)) ||
            (((*fe)->vertexB() == this) && ((*fe)->vertexA() == brother))) {
          result = (*fe);
        }
      }
    }
  }

  return result;
}

/**********************************/
/*                                */
/*                                */
/*             FEdge              */
/*                                */
/*                                */
/**********************************/

int FEdge::viewedge_nature() const
{
  return _ViewEdge->getNature();
}

#if 0
float FEdge::viewedge_length() const
{
  return _ViewEdge->viewedge_length();
}
#endif

const SShape *FEdge::occluded_shape() const
{
  ViewShape *aShape = _ViewEdge->aShape();
  if (aShape == 0)
    return 0;
  return aShape->sshape();
}

float FEdge::shape_importance() const
{
  return _VertexA->shape()->importance();
}

int FEdge::invisibility() const
{
  return _ViewEdge->qi();
}

occluder_container::const_iterator FEdge::occluders_begin() const
{
  return _ViewEdge->occluders_begin();
}

occluder_container::const_iterator FEdge::occluders_end() const
{
  return _ViewEdge->occluders_end();
}

bool FEdge::occluders_empty() const
{
  return _ViewEdge->occluders_empty();
}

int FEdge::occluders_size() const
{
  return _ViewEdge->occluders_size();
}

const bool FEdge::occludee_empty() const
{
  return _ViewEdge->occludee_empty();
}

Id FEdge::shape_id() const
{
  return _VertexA->shape()->getId();
}

const SShape *FEdge::shape() const
{
  return _VertexA->shape();
}

real FEdge::z_discontinuity() const
{
  if (!(getNature() & Nature::SILHOUETTE) && !(getNature() & Nature::BORDER)) {
    return 0;
  }

  BBox<Vec3r> box = ViewMap::getInstance()->getScene3dBBox();

  Vec3r bbox_size_vec(box.getMax() - box.getMin());
  real bboxsize = bbox_size_vec.norm();
  if (occludee_empty()) {
    // return FLT_MAX;
    return 1.0;
    // return bboxsize;
  }

#if 0
  real result;
  z_discontinuity_functor<SVertex> _functor;
  Evaluate<SVertex, z_discontinuity_functor<SVertex>>(&_functor, iCombination, result);
#endif
  Vec3r middle((_VertexB->point3d() - _VertexA->point3d()));
  middle /= 2;
  Vec3r disc_vec(middle - _occludeeIntersection);
  real res = disc_vec.norm() / bboxsize;

  return res;
  // return fabs((middle.z() - _occludeeIntersection.z()));
}

#if 0
float FEdge::local_average_depth(int iCombination) const
{
  float result;
  local_average_depth_functor<SVertex> functor;
  Evaluate(&functor, iCombination, result);

  return result;
}

float FEdge::local_depth_variance(int iCombination) const
{
  float result;

  local_depth_variance_functor<SVertex> functor;

  Evaluate(&functor, iCombination, result);

  return result;
}

real FEdge::local_average_density(float sigma, int iCombination) const
{
  float result;

  density_functor<SVertex> functor(sigma);

  Evaluate(&functor, iCombination, result);

  return result;
}

Vec3r FEdge::normal(int &oException /* = Exception::NO_EXCEPTION */)
{
  Vec3r Na = _VertexA->normal(oException);
  if (oException != Exception::NO_EXCEPTION)
    return Na;
  Vec3r Nb = _VertexB->normal(oException);
  if (oException != Exception::NO_EXCEPTION)
    return Nb;
  return (Na + Nb) / 2.0;
}

Vec3r FEdge::curvature2d_as_vector(int iCombination) const
{
  Vec3r result;
  curvature2d_as_vector_functor<SVertex> _functor;
  Evaluate<Vec3r, curvature2d_as_vector_functor<SVertex>>(&_functor, iCombination, result);
  return result;
}

real FEdge::curvature2d_as_angle(int iCombination) const
{
  real result;
  curvature2d_as_angle_functor<SVertex> _functor;
  Evaluate<real, curvature2d_as_angle_functor<SVertex>>(&_functor, iCombination, result);
  return result;
}
#endif

/**********************************/
/*                                */
/*                                */
/*            FEdgeSharp          */
/*                                */
/*                                */
/**********************************/

#if 0
Material FEdge::material() const
{
  return _VertexA->shape()->material();
}
#endif

const FrsMaterial &FEdgeSharp::aFrsMaterial() const
{
  return _VertexA->shape()->frs_material(_aFrsMaterialIndex);
}

const FrsMaterial &FEdgeSharp::bFrsMaterial() const
{
  return _VertexA->shape()->frs_material(_bFrsMaterialIndex);
}

/**********************************/
/*                                */
/*                                */
/*            FEdgeSmooth         */
/*                                */
/*                                */
/**********************************/

const FrsMaterial &FEdgeSmooth::frs_material() const
{
  return _VertexA->shape()->frs_material(_FrsMaterialIndex);
}

} /* namespace Freestyle */
