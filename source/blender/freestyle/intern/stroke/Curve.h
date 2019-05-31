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

#ifndef __FREESTYLE_CURVE_H__
#define __FREESTYLE_CURVE_H__

/** \file
 * \ingroup freestyle
 * \brief Class to define a container for curves
 */

#include <deque>

#include "../geometry/Geom.h"

//#include "../scene_graph/FrsMaterial.h"

#include "../view_map/Interface0D.h"
#include "../view_map/Interface1D.h"
#include "../view_map/Silhouette.h"
#include "../view_map/SilhouetteGeomEngine.h"

#include "../system/BaseIterator.h"

#ifdef WITH_CXX_GUARDEDALLOC
#  include "MEM_guardedalloc.h"
#endif

using namespace std;

namespace Freestyle {

using namespace Geometry;

/**********************************/
/*                                */
/*                                */
/*             CurvePoint         */
/*                                */
/*                                */
/**********************************/

/*! Class to represent a point of a curve.
 *  A CurvePoint can be any point of a 1D curve (it doesn't have to be a vertex of the curve).
 *  Any Interface1D is built upon ViewEdges, themselves built upon FEdges. Therefore, a curve is
 * basically a polyline made of a list SVertex. Thus, a CurvePoint is built by lineraly
 * interpolating two SVertex. CurvePoint can be used as virtual points while querying 0D
 * information along a curve at a given resolution.
 */
class CurvePoint : public Interface0D {
 public:  // Implementation of Interface0D
  /*! Returns the string "CurvePoint"*/
  virtual string getExactTypeName() const
  {
    return "CurvePoint";
  }

  // Data access methods
  /*! Returns the 3D X coordinate of the point */
  virtual real getX() const
  {
    return _Point3d.x();
  }

  /*! Returns the 3D Y coordinate of the point */
  virtual real getY() const
  {
    return _Point3d.y();
  }

  /*! Returns the 3D Z coordinate of the point */
  virtual real getZ() const
  {
    return _Point3d.z();
  }

  /*!  Returns the 3D point. */
  virtual Vec3r getPoint3D() const
  {
    return _Point3d;
  }

  /*! Returns the projected 3D X coordinate of the point */
  virtual real getProjectedX() const
  {
    return _Point2d.x();
  }

  /*! Returns the projected 3D Y coordinate of the point */
  virtual real getProjectedY() const
  {
    return _Point2d.y();
  }

  /*! Returns the projected 3D Z coordinate of the point */
  virtual real getProjectedZ() const
  {
    return _Point2d.z();
  }

  /*!  Returns the 2D point. */
  virtual Vec2r getPoint2D() const
  {
    return Vec2r(_Point2d.x(), _Point2d.y());
  }

  virtual FEdge *getFEdge(Interface0D &inter);

  /*! Returns the CurvePoint's Id */
  virtual Id getId() const
  {
    Id id;
    if (_t2d == 0) {
      return __A->getId();
    }
    else if (_t2d == 1) {
      return __B->getId();
    }
    return id;
  }

  /*! Returns the CurvePoint's Nature */
  virtual Nature::VertexNature getNature() const
  {
    Nature::VertexNature nature = Nature::POINT;
    if (_t2d == 0) {
      nature |= __A->getNature();
    }
    else if (_t2d == 1) {
      nature |= __B->getNature();
    }
    return nature;
  }

  /*! Cast the Interface0D in SVertex if it can be. */
  virtual SVertex *castToSVertex()
  {
    if (_t2d == 0) {
      return __A;
    }
    else if (_t2d == 1) {
      return __B;
    }
    return Interface0D::castToSVertex();
  }

  /*! Cast the Interface0D in ViewVertex if it can be. */
  virtual ViewVertex *castToViewVertex()
  {
    if (_t2d == 0) {
      return __A->castToViewVertex();
    }
    else if (_t2d == 1) {
      return __B->castToViewVertex();
    }
    return Interface0D::castToViewVertex();
  }

  /*! Cast the Interface0D in NonTVertex if it can be. */
  virtual NonTVertex *castToNonTVertex()
  {
    if (_t2d == 0) {
      return __A->castToNonTVertex();
    }
    else if (_t2d == 1) {
      return __B->castToNonTVertex();
    }
    return Interface0D::castToNonTVertex();
  }

  /*! Cast the Interface0D in TVertex if it can be. */
  virtual TVertex *castToTVertex()
  {
    if (_t2d == 0) {
      return __A->castToTVertex();
    }
    else if (_t2d == 1) {
      return __B->castToTVertex();
    }
    return Interface0D::castToTVertex();
  }

 public:
  typedef SVertex vertex_type;

 protected:
  SVertex *__A;
  SVertex *__B;
  float _t2d;
  // float _t3d;
  Vec3r _Point2d;
  Vec3r _Point3d;

 public:
  /*! Defult Constructor. */
  CurvePoint();

  /*! Builds a CurvePoint from two SVertex and an interpolation parameter.
   *  \param iA:
   *    The first SVertex
   *  \param iB:
   *    The second SVertex
   *  \param t2d:
   *    A 2D interpolation parameter used to linearly interpolate \a iA and \a iB
   */
  CurvePoint(SVertex *iA, SVertex *iB, float t2d);

  /*! Builds a CurvePoint from two CurvePoint and an interpolation parameter.
   *  \param iA:
   *    The first CurvePoint
   *  \param iB:
   *    The second CurvePoint
   *  \param t2d:
   *    The 2D interpolation parameter used to linearly interpolate \a iA and \a iB.
   */
  CurvePoint(CurvePoint *iA, CurvePoint *iB, float t2d);

  // CurvePoint(SVertex *iA, SVertex *iB, float t2d, float t3d);

  /*! Copy Constructor. */
  CurvePoint(const CurvePoint &iBrother);

  /*! Operator = */
  CurvePoint &operator=(const CurvePoint &iBrother);

  /*! Destructor */
  virtual ~CurvePoint()
  {
  }

  /*! Operator == */
  bool operator==(const CurvePoint &b)
  {
    return ((__A == b.__A) && (__B == b.__B) && (_t2d == b._t2d));
  }

  /* accessors */
  /*! Returns the first SVertex upon which the CurvePoint is built. */
  inline SVertex *A()
  {
    return __A;
  }

  /*! Returns the second SVertex upon which the CurvePoint is built. */
  inline SVertex *B()
  {
    return __B;
  }

  /*! Returns the interpolation parameter. */
  inline float t2d() const
  {
    return _t2d;
  }

#if 0
  inline const float t3d() const
  {
    return _t3d;
  }
#endif

  /* modifiers */
  /*! Sets the first SVertex upon which to build the CurvePoint. */
  inline void setA(SVertex *iA)
  {
    __A = iA;
  }

  /*! Sets the second SVertex upon which to build the CurvePoint. */
  inline void setB(SVertex *iB)
  {
    __B = iB;
  }

  /*! Sets the 2D interpolation parameter to use. */
  inline void setT2d(float t)
  {
    _t2d = t;
  }

#if 0
  inline void SetT3d(float t)
  {
    _t3d = t;
  }
#endif

  /* Information access interface */

  FEdge *fedge();

  inline const Vec3r &point2d() const
  {
    return _Point2d;
  }

  inline const Vec3r &point3d() const
  {
    return _Point3d;
  }

  Vec3r normal() const;
  // FrsMaterial material() const;
  // Id shape_id() const;
  const SShape *shape() const;
  // float shape_importance() const;

  // const unsigned qi() const;
  occluder_container::const_iterator occluders_begin() const;
  occluder_container::const_iterator occluders_end() const;
  bool occluders_empty() const;
  int occluders_size() const;
  const Polygon3r &occludee() const;
  const SShape *occluded_shape() const;
  const bool occludee_empty() const;
  real z_discontinuity() const;
#if 0
  float local_average_depth() const;
  float local_depth_variance() const;
  real local_average_density(float sigma = 2.3f) const;
  Vec3r shaded_color() const;
  Vec3r orientation2d() const;
  Vec3r orientation3d() const;

  real curvature2d() const
  {
    return viewedge()->curvature2d((_VertexA->point2d() + _VertexB->point2d()) / 2.0);
  }

  Vec3r curvature2d_as_vector() const;
  /*! angle in radians */
  real curvature2d_as_angle() const;

  real curvatureFredo() const;
  Vec2d directionFredo() const;
#endif

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:CurvePoint")
#endif
};

/**********************************/
/*                                */
/*                                */
/*             Curve              */
/*                                */
/*                                */
/**********************************/

namespace CurveInternal {

class CurvePoint_const_traits;
class CurvePoint_nonconst_traits;
template<class Traits> class __point_iterator;
class CurvePointIterator;

}  // end of namespace CurveInternal

/*! Base class for curves made of CurvePoints.
 *  SVertex is the type of the initial curve vertices.
 *  A Chain is a specialization of a Curve.
 */
class Curve : public Interface1D {
 public:
  typedef CurvePoint Vertex;
  typedef CurvePoint Point;
  typedef Point point_type;
  typedef Vertex vertex_type;
  typedef deque<Vertex *> vertex_container;

  /* Iterator to iterate over a vertex edges */

  typedef CurveInternal::__point_iterator<CurveInternal::CurvePoint_nonconst_traits>
      point_iterator;
  typedef CurveInternal::__point_iterator<CurveInternal::CurvePoint_const_traits>
      const_point_iterator;
  typedef point_iterator vertex_iterator;
  typedef const_point_iterator const_vertex_iterator;

 protected:
  vertex_container _Vertices;
  double _Length;
  Id _Id;
  unsigned _nSegments;  // number of segments

 public:
  /*! Default Constructor. */
  Curve()
  {
    _Length = 0;
    _Id = 0;
    _nSegments = 0;
  }

  /*! Builds a Curve from its id */
  Curve(const Id &id)
  {
    _Length = 0;
    _Id = id;
    _nSegments = 0;
  }

  /*! Copy Constructor. */
  Curve(const Curve &iBrother)
  {
    _Length = iBrother._Length;
    _Vertices = iBrother._Vertices;
    _Id = iBrother._Id;
    _nSegments = 0;
  }

  /*! Destructor. */
  virtual ~Curve();

  /*! Returns the string "Curve" */
  virtual string getExactTypeName() const
  {
    return "Curve";
  }

#if 0
  /* fredo's curvature storage */
  void computeCurvatureAndOrientation();
#endif

  /*! Adds a single vertex (CurvePoint) at the end of the Curve */
  inline void push_vertex_back(Vertex *iVertex)
  {
    if (!_Vertices.empty()) {
      Vec3r vec_tmp(iVertex->point2d() - _Vertices.back()->point2d());
      _Length += vec_tmp.norm();
      ++_nSegments;
    }
    Vertex *new_vertex = new Vertex(*iVertex);
    _Vertices.push_back(new_vertex);
  }

  /*! Adds a single vertex (SVertex) at the end of the Curve */
  inline void push_vertex_back(SVertex *iVertex)
  {
    if (!_Vertices.empty()) {
      Vec3r vec_tmp(iVertex->point2d() - _Vertices.back()->point2d());
      _Length += vec_tmp.norm();
      ++_nSegments;
    }
    Vertex *new_vertex = new Vertex(iVertex, 0, 0);
    _Vertices.push_back(new_vertex);
  }

  /*! Adds a single vertex (CurvePoint) at the front of the Curve */
  inline void push_vertex_front(Vertex *iVertex)
  {
    if (!_Vertices.empty()) {
      Vec3r vec_tmp(iVertex->point2d() - _Vertices.front()->point2d());
      _Length += vec_tmp.norm();
      ++_nSegments;
    }
    Vertex *new_vertex = new Vertex(*iVertex);
    _Vertices.push_front(new_vertex);
  }

  /*! Adds a single vertex (SVertex) at the front of the Curve */
  inline void push_vertex_front(SVertex *iVertex)
  {
    if (!_Vertices.empty()) {
      Vec3r vec_tmp(iVertex->point2d() - _Vertices.front()->point2d());
      _Length += vec_tmp.norm();
      ++_nSegments;
    }
    Vertex *new_vertex = new Vertex(iVertex, 0, 0);
    _Vertices.push_front(new_vertex);
  }

  /*! Returns true is the Curve doesn't have any Vertex yet. */
  inline bool empty() const
  {
    return _Vertices.empty();
  }

  /*! Returns the 2D length of the Curve. */
  inline real getLength2D() const
  {
    return _Length;
  }

  /*! Returns the Id of the 1D element. */
  virtual Id getId() const
  {
    return _Id;
  }

  /*! Returns the number of segments in the polyline constituting the Curve. */
  inline unsigned int nSegments() const
  {
    return _nSegments;
  }

  inline void setId(const Id &id)
  {
    _Id = id;
  }

  /* Information access interface */

#if 0
  inline Vec3r shaded_color(int iCombination = 0) const;
  inline Vec3r orientation2d(point_iterator it) const;
  Vec3r orientation2d(int iCombination = 0) const;
  Vec3r orientation3d(point_iterator it) const;
  Vec3r orientation3d(int iCombination = 0) const;

  real curvature2d(point_iterator it) const
  {
    return (*it)->curvature2d();
  }

  real curvature2d(int iCombination = 0) const;
  FrsMaterial material() const;
  int qi() const;
  occluder_container::const_iterator occluders_begin() const;
  occluder_container::const_iterator occluders_end() const;
  int occluders_size() const;
  bool occluders_empty() const;

  const Polygon3r &occludee() const
  {
    return *(_FEdgeA->aFace());
  }

  const SShape *occluded_shape() const;
  const bool occludee_empty() const;
  real z_discontinuity(int iCombination = 0) const;
  int shape_id() const;
  const SShape *shape() const;
  float shape_importance(int iCombination = 0) const;
  float local_average_depth(int iCombination = 0) const;
  float local_depth_variance(int iCombination = 0) const;
  real local_average_density(float sigma = 2.3f, int iCombination = 0) const;
  Vec3r curvature2d_as_vector(int iCombination = 0) const;
  /*! angle in radians */
  real curvature2d_as_angle(int iCombination = 0) const;
#endif

  /* advanced iterators access */
  point_iterator points_begin(float step = 0);
  const_point_iterator points_begin(float step = 0) const;
  point_iterator points_end(float step = 0);
  const_point_iterator points_end(float step = 0) const;

  /* methods given for convenience */
  point_iterator vertices_begin();
  const_point_iterator vertices_begin() const;
  point_iterator vertices_end();
  const_point_iterator vertices_end() const;

  // specialized iterators access
  CurveInternal::CurvePointIterator curvePointsBegin(float t = 0.0f);
  CurveInternal::CurvePointIterator curvePointsEnd(float t = 0.0f);

  CurveInternal::CurvePointIterator curveVerticesBegin();
  CurveInternal::CurvePointIterator curveVerticesEnd();

  // Iterators access
  /*! Returns an Interface0DIterator pointing onto the first vertex of the Curve and that can
   * iterate over the \a vertices of the Curve.
   */
  virtual Interface0DIterator verticesBegin();

  /*! Returns an Interface0DIterator pointing after the last vertex of the Curve and that can
   * iterate over the \a vertices of the Curve.
   */
  virtual Interface0DIterator verticesEnd();

  /*! Returns an Interface0DIterator pointing onto the first point of the Curve and that can
   * iterate over the \a points of the Curve at any resolution. At each iteration a virtual
   * temporary CurvePoint is created.
   */
  virtual Interface0DIterator pointsBegin(float t = 0.0f);

  /*! Returns an Interface0DIterator pointing after the last point of the Curve and that can
   * iterate over the \a points of the Curve at any resolution. At each iteration a virtual
   * temporary CurvePoint is created.
   */
  virtual Interface0DIterator pointsEnd(float t = 0.0f);

#ifdef WITH_CXX_GUARDEDALLOC
  MEM_CXX_CLASS_ALLOC_FUNCS("Freestyle:Curve")
#endif
};

} /* namespace Freestyle */

#endif  // __FREESTYLE_CURVE_H__
