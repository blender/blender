// Begin License:
// Copyright (C) 2006-2014 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of either the GNU General
// Public License version 2 or 3 (at your option) as published by the
// Free Software Foundation and appearing in the files LICENSE.GPL2
// and LICENSE.GPL3 included in the packaging of this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#if defined(HAVE_CONFIG_H)
#  include <carve_config.h>
#endif

#include <carve/poly.hpp>

namespace {

  double CALC_X(const carve::geom::plane<3> &p, double y, double z) { return -(p.d + p.N.y * y + p.N.z * z) / p.N.x; }
  double CALC_Y(const carve::geom::plane<3> &p, double x, double z) { return -(p.d + p.N.x * x + p.N.z * z) / p.N.y; }
  double CALC_Z(const carve::geom::plane<3> &p, double x, double y) { return -(p.d + p.N.x * x + p.N.y * y) / p.N.z; }

}  // namespace

namespace carve {
  namespace poly {

    namespace {

      carve::geom2d::P2 _project_1(const carve::geom3d::Vector &v) {
        return carve::geom::VECTOR(v.z, v.y);
      }

      carve::geom2d::P2 _project_2(const carve::geom3d::Vector &v) {
        return carve::geom::VECTOR(v.x, v.z);
      }

      carve::geom2d::P2 _project_3(const carve::geom3d::Vector &v) {
        return carve::geom::VECTOR(v.y, v.x);
      }

      carve::geom2d::P2 _project_4(const carve::geom3d::Vector &v) {
        return carve::geom::VECTOR(v.y, v.z);
      }

      carve::geom2d::P2 _project_5(const carve::geom3d::Vector &v) {
        return carve::geom::VECTOR(v.z, v.x);
      }

      carve::geom2d::P2 _project_6(const carve::geom3d::Vector &v) {
        return carve::geom::VECTOR(v.x, v.y);
      }


      carve::geom3d::Vector _unproject_1(const carve::geom2d::P2 &p, const carve::geom3d::Plane &plane_eqn) {
        return carve::geom::VECTOR(CALC_X(plane_eqn, p.y, p.x), p.y, p.x);
      }

      carve::geom3d::Vector _unproject_2(const carve::geom2d::P2 &p, const carve::geom3d::Plane &plane_eqn) {
        return carve::geom::VECTOR(p.x, CALC_Y(plane_eqn, p.x, p.y), p.y);
      }

      carve::geom3d::Vector _unproject_3(const carve::geom2d::P2 &p, const carve::geom3d::Plane &plane_eqn) {
        return carve::geom::VECTOR(p.y, p.x, CALC_Z(plane_eqn, p.y, p.x));
      }

      carve::geom3d::Vector _unproject_4(const carve::geom2d::P2 &p, const carve::geom3d::Plane &plane_eqn) {
        return carve::geom::VECTOR(CALC_X(plane_eqn, p.x, p.y), p.x, p.y);
      }

      carve::geom3d::Vector _unproject_5(const carve::geom2d::P2 &p, const carve::geom3d::Plane &plane_eqn) {
        return carve::geom::VECTOR(p.y, CALC_Y(plane_eqn, p.y, p.x), p.x);
      }

      carve::geom3d::Vector _unproject_6(const carve::geom2d::P2 &p, const carve::geom3d::Plane &plane_eqn) {
        return carve::geom::VECTOR(p.x, p.y, CALC_Z(plane_eqn, p.x, p.y));
      }

    }  // namespace

    static carve::geom2d::P2 (*project_tab[2][3])(const carve::geom3d::Vector &) = {
      { &_project_1, &_project_2, &_project_3 },
      { &_project_4, &_project_5, &_project_6 }
    };

    static carve::geom3d::Vector (*unproject_tab[2][3])(const carve::geom2d::P2 &, const carve::geom3d::Plane &) = {
      { &_unproject_1, &_unproject_2, &_unproject_3 },
      { &_unproject_4, &_unproject_5, &_unproject_6 }
    };

    // only implemented for 3d.
    template<unsigned ndim>
    typename Face<ndim>::project_t Face<ndim>::getProjector(bool positive_facing, int axis) {
      return NULL;
    }

    template<>
    Face<3>::project_t Face<3>::getProjector(bool positive_facing, int axis) {
      return project_tab[positive_facing ? 1 : 0][axis];
    }

    template<unsigned ndim>
    typename Face<ndim>::unproject_t Face<ndim>::getUnprojector(bool positive_facing, int axis) {
      return NULL;
    }

    template<>
    Face<3>::unproject_t Face<3>::getUnprojector(bool positive_facing, int axis) {
      return unproject_tab[positive_facing ? 1 : 0][axis];
    }



    template<unsigned ndim>
    Face<ndim>::Face(const std::vector<const vertex_t *> &_vertices,
                     bool delay_recalc) : tagable() {
      vertices = _vertices;
      edges.resize(nVertices(), NULL);
      if (!delay_recalc && !recalc()) { }
    }

    template<unsigned ndim>
    Face<ndim>::Face(const vertex_t *a,
                     const vertex_t *b,
                     const vertex_t *c,
                     bool delay_recalc) : tagable() {
      vertices.reserve(3);
      vertices.push_back(a);
      vertices.push_back(b);
      vertices.push_back(c);
      edges.resize(3, NULL);
      if (!delay_recalc && !recalc()) { }
    }

    template<unsigned ndim>
    Face<ndim>::Face(const vertex_t *a,
                     const vertex_t *b,
                     const vertex_t *c,
                     const vertex_t *d,
                     bool delay_recalc) : tagable() {
      vertices.reserve(4);
      vertices.push_back(a);
      vertices.push_back(b);
      vertices.push_back(c);
      vertices.push_back(d);
      edges.resize(4, NULL);
      if (!delay_recalc && !recalc()) { }
    }

    template<unsigned ndim>
    void Face<ndim>::invert() {
      size_t n_verts = vertices.size();
      std::reverse(vertices.begin(), vertices.end());

      if (project != NULL) {
        plane_eqn.negate();

        int da = carve::geom::largestAxis(plane_eqn.N);

        project = getProjector(plane_eqn.N.v[da] > 0, da);
        unproject = getUnprojector(plane_eqn.N.v[da] > 0, da);
      }

      std::reverse(edges.begin(), edges.end() - 1);
      for (size_t i = 0; i < n_verts; i++) {
        const vertex_t *v1 = vertices[i];
        const vertex_t *v2 = vertices[(i+1) % n_verts];
        CARVE_ASSERT((edges[i]->v1 == v1 && edges[i]->v2 == v2) || (edges[i]->v1 == v2 && edges[i]->v2 == v1));
      }
    }

    template<unsigned ndim>
    bool Face<ndim>::recalc() {
      aabb.fit(vertices.begin(), vertices.end(), vec_adapt_vertex_ptr());

      if (!carve::geom3d::fitPlane(vertices.begin(), vertices.end(), vec_adapt_vertex_ptr(), plane_eqn)) {
        return false;
      }

      int da = carve::geom::largestAxis(plane_eqn.N);
      project = getProjector(false, da);

      double A = carve::geom2d::signedArea(vertices, projector());
      if ((A < 0.0) ^ (plane_eqn.N.v[da] < 0.0)) {
        plane_eqn.negate();
      }

      project = getProjector(plane_eqn.N.v[da] > 0, da);
      unproject = getUnprojector(plane_eqn.N.v[da] > 0, da);

      return true;
    }

    template<unsigned ndim>
    Face<ndim> *Face<ndim>::init(const Face *base, const std::vector<const vertex_t *> &_vertices, bool flipped) {
      return init(base, _vertices.begin(), _vertices.end(), flipped);
    }

    template<unsigned ndim>
    bool Face<ndim>::containsPoint(const vector_t &p) const {
      if (!carve::math::ZERO(carve::geom::distance(plane_eqn, p))) return false;
      // return pointInPolySimple(vertices, projector(), (this->*project)(p));
      return carve::geom2d::pointInPoly(vertices, projector(), face::project(this, p)).iclass != POINT_OUT;
    }

    template<unsigned ndim>
    bool Face<ndim>::containsPointInProjection(const vector_t &p) const {
      return carve::geom2d::pointInPoly(vertices, projector(), face::project(this, p)).iclass != POINT_OUT;
    }

    template<unsigned ndim>
    bool Face<ndim>::simpleLineSegmentIntersection(const carve::geom::linesegment<ndim> &line,
                                                   vector_t &intersection) const {
      if (!line.OK()) return false;

      carve::geom3d::Vector p;
      IntersectionClass intersects = carve::geom3d::lineSegmentPlaneIntersection(plane_eqn,
                                                                  line,
                                                                  p);
      if (intersects == INTERSECT_NONE || intersects == INTERSECT_BAD) {
        return false;
      }

      carve::geom2d::P2 proj_p(face::project(this, p));
      if (carve::geom2d::pointInPolySimple(vertices, projector(), proj_p)) {
        intersection = p;
        return true;
      }
      return false;
    }

    // XXX: should try to return a pre-existing vertex in the case of a
    // line-vertex intersection.  as it stands, this code isn't used,
    // so... meh.
    template<unsigned ndim>
    IntersectionClass Face<ndim>::lineSegmentIntersection(const carve::geom::linesegment<ndim> &line,
                                                          vector_t &intersection) const {
      if (!line.OK()) return INTERSECT_NONE;

  
      carve::geom3d::Vector p;
      IntersectionClass intersects = carve::geom3d::lineSegmentPlaneIntersection(plane_eqn,
                                                                  line,
                                                                  p);
      if (intersects == INTERSECT_NONE || intersects == INTERSECT_BAD) {
        return intersects;
      }

      carve::geom2d::P2 proj_p(face::project(this, p));

      carve::geom2d::PolyInclusionInfo pi = carve::geom2d::pointInPoly(vertices, projector(), proj_p);
      switch (pi.iclass) {
      case POINT_VERTEX:
        intersection = p;
        return INTERSECT_VERTEX;

      case POINT_EDGE:
        intersection = p;
        return INTERSECT_EDGE;

      case POINT_IN:
        intersection = p;
        return INTERSECT_FACE;
      
      case POINT_OUT:
        return INTERSECT_NONE;

      default:
        break;
      }
      return INTERSECT_NONE;
    }


  }
}

// explicit instantiations.
template class carve::poly::Face<3>;
