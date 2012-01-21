// Begin License:
// Copyright (C) 2006-2011 Tobias Sargeant (tobias.sargeant@gmail.com).
// All rights reserved.
//
// This file is part of the Carve CSG Library (http://carve-csg.com/)
//
// This file may be used under the terms of the GNU General Public
// License version 2.0 as published by the Free Software Foundation
// and appearing in the file LICENSE.GPL2 included in the packaging of
// this file.
//
// This file is provided "AS IS" with NO WARRANTY OF ANY KIND,
// INCLUDING THE WARRANTIES OF DESIGN, MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE.
// End:


#pragma once

#include <list>
#include <vector>
#include <algorithm>

#include <carve/carve.hpp>

#include <carve/geom2d.hpp>

namespace carve {
  namespace triangulate {

    /** 
     * \brief Merge a set of holes into a polygon. (templated)
     *
     * Take a polygon loop and a collection of hole loops, and patch
     * the hole loops into the polygon loop, returning a vector of
     * vertices from the polygon and holes, which describes a new
     * polygon boundary with no holes. The new polygon boundary is
     * constructed via the addition of edges * joining the polygon
     * loop to the holes.
     * 
     * This may be applied to arbitrary vertex data (generally
     * carve::geom3d::Vertex pointers), but a projection function must
     * be supplied to convert vertices to coordinates in 2-space, in
     * which the work is performed.
     *
     * @tparam project_t A functor which converts vertices to a 2d
     *                   projection.
     * @tparam vert_t    The vertex type.
     * @param project The projection functor.
     * @param f_loop The polygon loop into which holes are to be
     *               incorporated.
     * @param h_loops The set of hole loops to be incorporated.
     * 
     * @return A vector of vertex pointers.
     */
    template<typename project_t, typename vert_t>
    static std::vector<vert_t>
    incorporateHolesIntoPolygon(const project_t &project,
                                const std::vector<vert_t> &f_loop,
                                const std::vector<std::vector<vert_t> > &h_loops);

    void
    incorporateHolesIntoPolygon(const std::vector<std::vector<carve::geom2d::P2> > &poly,
                                std::vector<std::pair<size_t, size_t> > &result,
                                size_t poly_loop,
                                const std::vector<size_t> &hole_loops);

    /** 
     * \brief Merge a set of holes into a polygon. (2d)
     *
     * Take a polygon loop and a collection of hole loops, and patch
     * the hole loops into the polygon loop, returning a vector of
     * containing the vertices from the polygon and holes which
     * describes a new polygon boundary with no holes, through the
     * addition of edges joining the polygon loop to the holes.
     * 
     * @param poly A vector containing the face loop (the first
     *             element of poly) and the hole loops (second and
     *             subsequent elements of poly).
     * 
     * @return A vector of pairs of <loop_number, index> that
     *         reference poly and define the result polygon loop.
     */
    std::vector<std::pair<size_t, size_t> > incorporateHolesIntoPolygon(const std::vector<std::vector<carve::geom2d::P2> > &poly);

    std::vector<std::vector<std::pair<size_t, size_t> > > mergePolygonsAndHoles(const std::vector<std::vector<carve::geom2d::P2> > &poly);


    struct tri_idx {
      union {
        unsigned v[3];
        struct { unsigned a, b, c; };
      };

      tri_idx() : a(0), b(0), c(0) {
      }
      tri_idx(unsigned _a, unsigned _b, unsigned _c) : a(_a), b(_b), c(_c) {
      }
    };

    /** 
     * \brief Triangulate a 2-dimensional polygon.
     * 
     * Given a 2-dimensional polygon described as a vector of 2-d
     * points, with no holes and no self-crossings, produce a
     * triangulation using an ear-clipping algorithm.
     *
     * @param [in] poly A vector containing the input polygon.
     * @param [out] result A vector of triangles, represented as
     *                     indicies into poly.
     */

    
    void triangulate(const std::vector<carve::geom2d::P2> &poly, std::vector<tri_idx> &result);

    /** 
     * \brief Triangulate a polygon (templated).
     *
     * @tparam project_t A functor which converts vertices to a 2d
     *                   projection.
     * @tparam vert_t    The vertex type.
     * @param [in] project The projection functor.
     * @param [in] poly A vector containing the input polygon,
     *                  represented as vert_t pointers.
     * @param [out] result A vector of triangles, represented as
     *                     indicies into poly.
     */
    template<typename project_t, typename vert_t>
    void triangulate(const project_t &project,
                     const std::vector<vert_t> &poly,
                     std::vector<tri_idx> &result);

    /** 
     * \brief Improve a candidate triangulation of poly by minimising
     * the length of internal edges. (templated)
     *
     * @tparam project_t A functor which converts vertices to a 2d
     *                   projection.
     * @tparam vert_t    The vertex type.
     * @param [in] project The projection functor.
     * @param [in] poly A vector containing the input polygon,
     *                  represented as vert_t pointers.
     * @param [inout] result A vector of triangles, represented as
     *                       indicies into poly. On input, this vector
     *                       must contain a candidate triangulation of
     *                       poly. Calling improve() modifies the
     *                       contents of the vector, returning an
     *                       improved triangulation.
     */
    template<typename project_t, typename vert_t>
    void improve(const project_t &project,
                 const std::vector<vert_t> &poly,
                 std::vector<tri_idx> &result);

    /** 
     * \brief Improve a candidate triangulation of poly by minimising
     * the length of internal edges.
     *
     * @param [in] poly A vector containing the input polygon.

     * @param [inout] result A vector of triangles, represented as
     *                       indicies into poly. On input, this vector
     *                       must contain a candidate triangulation of
     *                       poly. Calling improve() modifies the
     *                       contents of the vector, returning an
     *                       improved triangulation.
     */
    static inline void improve(const std::vector<carve::geom2d::P2> &poly, std::vector<tri_idx> &result) {
      improve(carve::geom2d::p2_adapt_ident(), poly, result);
    }

  }
}

#include <carve/triangulator_impl.hpp>
