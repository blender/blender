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


#if defined(HAVE_CONFIG_H)
#  include <carve_config.h>
#endif

#include <carve/csg.hpp>
#include <carve/pointset.hpp>
#include <carve/polyline.hpp>

#include <list>
#include <set>
#include <iostream>

#include <algorithm>

#include "csg_detail.hpp"
#include "csg_data.hpp"

#include "intersect_debug.hpp"
#include "intersect_common.hpp"
#include "intersect_classify_common.hpp"

#include "csg_collector.hpp"

#include <carve/timing.hpp>
#include <carve/colour.hpp>

#include <memory>



carve::csg::VertexPool::VertexPool() {
}

carve::csg::VertexPool::~VertexPool() {
}

void carve::csg::VertexPool::reset() {
  pool.clear();
}

carve::csg::VertexPool::vertex_t *carve::csg::VertexPool::get(const vertex_t::vector_t &v) {
  if (!pool.size() || pool.back().size() == blocksize) {
    pool.push_back(std::vector<vertex_t>());
    pool.back().reserve(blocksize);
  }
  pool.back().push_back(vertex_t(v));
  return &pool.back().back();
}

bool carve::csg::VertexPool::inPool(vertex_t *v) const {
  for (pool_t::const_iterator i = pool.begin(); i != pool.end(); ++i) {
    if (v >= &(i->front()) && v <= &(i->back())) return true;
  }
  return false;
}



#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
void writePLY(const std::string &out_file, const carve::point::PointSet *points, bool ascii);
void writePLY(const std::string &out_file, const carve::line::PolylineSet *lines, bool ascii);
void writePLY(const std::string &out_file, const carve::mesh::MeshSet<3> *poly, bool ascii);

static carve::mesh::MeshSet<3> *faceLoopsToPolyhedron(const carve::csg::FaceLoopList &fl) {
  std::vector<carve::mesh::MeshSet<3>::face_t *> faces;
  faces.reserve(fl.size());
  for (carve::csg::FaceLoop *f = fl.head; f; f = f->next) {
    faces.push_back(f->orig_face->create(f->vertices.begin(), f->vertices.end(), false));
  }
  carve::mesh::MeshSet<3> *poly = new carve::mesh::MeshSet<3>(faces);

  return poly;
}
#endif

namespace {
  /** 
   * \brief Sort a range [\a beg, \a end) of vertices in order of increasing dot product of vertex - \a base on \dir.
   * 
   * @tparam[in] T a forward iterator type.
   * @param[in] dir The direction in which to sort vertices.
   * @param[in] base 
   * @param[in] beg The start of the vertex range to sort.
   * @param[in] end The end of the vertex range to sort.
   * @param[out] out The sorted vertex result.
   * @param[in] size_hint A hint regarding the size of the output
   *            vector (to avoid needing to be able to calculate \a
   *            end - \a beg).
   */
  template<typename iter_t>
  void orderVertices(iter_t beg, const iter_t end,
                     const carve::mesh::MeshSet<3>::vertex_t::vector_t &dir,
                     const carve::mesh::MeshSet<3>::vertex_t::vector_t &base,
                     std::vector<carve::mesh::MeshSet<3>::vertex_t *> &out) {
    typedef std::vector<std::pair<double, carve::mesh::MeshSet<3>::vertex_t *> > DVVector;
    std::vector<std::pair<double, carve::mesh::MeshSet<3>::vertex_t *> > ordered_vertices;

    ordered_vertices.reserve(std::distance(beg, end));
  
    for (; beg != end; ++beg) {
      carve::mesh::MeshSet<3>::vertex_t *v = *beg;
      ordered_vertices.push_back(std::make_pair(carve::geom::dot(v->v - base, dir), v));
    }
  
    std::sort(ordered_vertices.begin(), ordered_vertices.end());
  
    out.clear();
    out.reserve(ordered_vertices.size());
    for (DVVector::const_iterator
           i = ordered_vertices.begin(), e = ordered_vertices.end();
         i != e;
         ++i) {
      out.push_back((*i).second);
    }
  }

  template<typename iter_t>
  void orderEdgeIntersectionVertices(iter_t beg, const iter_t end,
                                     const carve::mesh::MeshSet<3>::vertex_t::vector_t &dir,
                                     const carve::mesh::MeshSet<3>::vertex_t::vector_t &base,
                                     std::vector<carve::mesh::MeshSet<3>::vertex_t *> &out) {
    typedef std::vector<std::pair<std::pair<double, double>, carve::mesh::MeshSet<3>::vertex_t *> > DVVector;
    DVVector ordered_vertices;

    ordered_vertices.reserve(std::distance(beg, end));
  
    for (; beg != end; ++beg) {
      carve::mesh::MeshSet<3>::vertex_t *v = (*beg).first;
      double ovec = 0.0;
      for (carve::csg::detail::EdgeIntInfo::mapped_type::const_iterator j = (*beg).second.begin(); j != (*beg).second.end(); ++j) {
        ovec += (*j).second;
      }
      ordered_vertices.push_back(std::make_pair(std::make_pair(carve::geom::dot(v->v - base, dir), -ovec), v));
    }

    std::sort(ordered_vertices.begin(), ordered_vertices.end());

    out.clear();
    out.reserve(ordered_vertices.size());
    for (DVVector::const_iterator
           i = ordered_vertices.begin(), e = ordered_vertices.end();
         i != e;
         ++i) {
      out.push_back((*i).second);
    }
  }



  /** 
   * 
   * 
   * @param dir 
   * @param base 
   * @param beg 
   * @param end 
   */
  template<typename iter_t>
  void selectOrderingProjection(iter_t beg, const iter_t end,
                                carve::mesh::MeshSet<3>::vertex_t::vector_t &dir,
                                carve::mesh::MeshSet<3>::vertex_t::vector_t &base) {
    double dx, dy, dz;
    carve::mesh::MeshSet<3>::vertex_t *min_x, *min_y, *min_z, *max_x, *max_y, *max_z;
    if (beg == end) return;
    min_x = max_x = min_y = max_y = min_z = max_z = *beg++;
    for (; beg != end; ++beg) {
      if (min_x->v.x > (*beg)->v.x) min_x = *beg;
      if (min_y->v.y > (*beg)->v.y) min_y = *beg;
      if (min_z->v.z > (*beg)->v.z) min_z = *beg;
      if (max_x->v.x < (*beg)->v.x) max_x = *beg;
      if (max_y->v.y < (*beg)->v.y) max_y = *beg;
      if (max_z->v.z < (*beg)->v.z) max_z = *beg;
    }

    dx = max_x->v.x - min_x->v.x;
    dy = max_y->v.y - min_y->v.y;
    dz = max_z->v.z - min_z->v.z;

    if (dx > dy) {
      if (dx > dz) {
        dir = max_x->v - min_x->v; base = min_x->v;
      } else {
        dir = max_z->v - min_z->v; base = min_z->v;
      }
    } else {
      if (dy > dz) {
        dir = max_y->v - min_y->v; base = min_y->v;
      } else {
        dir = max_z->v - min_z->v; base = min_z->v;
      }
    }
  }
}

namespace {
  struct dump_data {
    carve::mesh::MeshSet<3>::vertex_t *i_pt;
    carve::csg::IObj i_src;
    carve::csg::IObj i_tgt;
    dump_data(carve::mesh::MeshSet<3>::vertex_t *_i_pt,
              carve::csg::IObj _i_src,
              carve::csg::IObj _i_tgt) : i_pt(_i_pt), i_src(_i_src), i_tgt(_i_tgt) {
    }
  };



  struct dump_sort {
    bool operator()(const dump_data &a, const dump_data &b) const {
      if (a.i_pt->v.x < b.i_pt->v.x) return true;
      if (a.i_pt->v.x > b.i_pt->v.x) return false;
      if (a.i_pt->v.y < b.i_pt->v.y) return true;
      if (a.i_pt->v.y > b.i_pt->v.y) return false;
      if (a.i_pt->v.z < b.i_pt->v.z) return true;
      if (a.i_pt->v.z > b.i_pt->v.z) return false;
      return false;
    }
  };



  void dump_intersections(std::ostream &out, carve::csg::Intersections &csg_intersections) {
    std::vector<dump_data> temp;

    for (carve::csg::Intersections::const_iterator
        i = csg_intersections.begin(),
        ie = csg_intersections.end();
        i != ie;
        ++i) {
      const carve::csg::IObj &i_src = ((*i).first);

      for (carve::csg::Intersections::mapped_type::const_iterator
          j = (*i).second.begin(),
          je = (*i).second.end();
          j != je;
          ++j) {
        const carve::csg::IObj &i_tgt = ((*j).first);
        carve::mesh::MeshSet<3>::vertex_t *i_pt = ((*j).second);
        temp.push_back(dump_data(i_pt, i_src, i_tgt));
      }
    }

    std::sort(temp.begin(), temp.end(), dump_sort());

    for (size_t i = 0; i < temp.size(); ++i) {
      const carve::csg::IObj &i_src = temp[i].i_src;
      const carve::csg::IObj &i_tgt = temp[i].i_tgt;
      out
        << "INTERSECTION: " << temp[i].i_pt << " (" << temp[i].i_pt->v << ") "
        << "is " << i_src << ".." << i_tgt << std::endl;
    }

#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
    std::vector<carve::geom3d::Vector> vertices;

    for (carve::csg::Intersections::const_iterator
        i = csg_intersections.begin(),
        ie = csg_intersections.end();
        i != ie;
        ++i) {
      for (carve::csg::Intersections::mapped_type::const_iterator
          j = (*i).second.begin(),
          je = (*i).second.end();
          j != je;
          ++j) {
        carve::mesh::MeshSet<3>::vertex_t *i_pt = ((*j).second);
        vertices.push_back(i_pt->v);
      }
    }

    carve::point::PointSet points(vertices);

    std::string outf("/tmp/intersection-points.ply");
    ::writePLY(outf, &points, true);
#endif
  }



  /** 
   * \brief Populate a collection with the faces adjoining an edge.
   * 
   * @tparam face_set_t A collection type.
   * @param e The edge for which to collect adjoining faces.
   * @param faces 
   */
  template<typename face_set_t>
  inline void facesForVertex(carve::mesh::MeshSet<3>::vertex_t *v,
                             const carve::csg::detail::VEVecMap &ve,
                             face_set_t &faces) {
    carve::csg::detail::VEVecMap::const_iterator vi = ve.find(v);
    if (vi != ve.end()) {
      for (carve::csg::detail::VEVecMap::data_type::const_iterator i = (*vi).second.begin(); i != (*vi).second.end(); ++i) {
        faces.insert((*i)->face);
      }
    }
  }

  /** 
   * \brief Populate a collection with the faces adjoining an edge.
   * 
   * @tparam face_set_t A collection type.
   * @param e The edge for which to collect adjoining faces.
   * @param faces 
   */
  template<typename face_set_t>
  inline void facesForEdge(carve::mesh::MeshSet<3>::edge_t *e,
                           face_set_t &faces) {
    faces.insert(e->face);
  }

  /** 
   * \brief Populate a collection with the faces adjoining a face.
   * 
   * @tparam face_set_t A collection type.
   * @param f The face for which to collect adjoining faces.
   * @param faces 
   */
  template<typename face_set_t>
  inline void facesForFace(carve::mesh::MeshSet<3>::face_t *f,
                           face_set_t &faces) {
    faces.insert(f);
  }

  /** 
   * \brief Populate a collection with the faces adjoining an intersection object.
   * 
   * @tparam face_set_t A collection type holding const carve::poly::Polyhedron::face_t *.
   * @param obj The intersection object for which to collect adjoining faces.
   * @param faces 
   */
  template<typename face_set_t>
  void facesForObject(const carve::csg::IObj &obj,
                      const carve::csg::detail::VEVecMap &ve,
                      face_set_t &faces) {
    switch (obj.obtype) {
    case carve::csg::IObj::OBTYPE_VERTEX:
      facesForVertex(obj.vertex, ve, faces);
      break;

    case carve::csg::IObj::OBTYPE_EDGE:
      facesForEdge(obj.edge, faces);
      break;

    case  carve::csg::IObj::OBTYPE_FACE:
      facesForFace(obj.face, faces);
      break;

    default:
      break;
    }
  }



}



bool carve::csg::CSG::Hooks::hasHook(unsigned hook_num) {
  return hooks[hook_num].size() > 0;
}

void carve::csg::CSG::Hooks::intersectionVertex(const meshset_t::vertex_t *vertex,
                                                const IObjPairSet &intersections) {
  for (std::list<Hook *>::iterator j = hooks[INTERSECTION_VERTEX_HOOK].begin();
       j != hooks[INTERSECTION_VERTEX_HOOK].end();
       ++j) {
    (*j)->intersectionVertex(vertex, intersections);
  }
}

void carve::csg::CSG::Hooks::processOutputFace(std::vector<meshset_t::face_t *> &faces,
                                               const meshset_t::face_t *orig_face,
                                               bool flipped) {
  for (std::list<Hook *>::iterator j = hooks[PROCESS_OUTPUT_FACE_HOOK].begin();
       j != hooks[PROCESS_OUTPUT_FACE_HOOK].end();
       ++j) {
    (*j)->processOutputFace(faces, orig_face, flipped);
  }
}

void carve::csg::CSG::Hooks::resultFace(const meshset_t::face_t *new_face,
                                        const meshset_t::face_t *orig_face,
                                        bool flipped) {
  for (std::list<Hook *>::iterator j = hooks[RESULT_FACE_HOOK].begin();
       j != hooks[RESULT_FACE_HOOK].end();
       ++j) {
    (*j)->resultFace(new_face, orig_face, flipped);
  }
}

void carve::csg::CSG::Hooks::registerHook(Hook *hook, unsigned hook_bits) {
  for (unsigned i = 0; i < HOOK_MAX; ++i) {
    if (hook_bits & (1U << i)) {
      hooks[i].push_back(hook);
    }
  }
}

void carve::csg::CSG::Hooks::unregisterHook(Hook *hook) {
  for (unsigned i = 0; i < HOOK_MAX; ++i) {
    hooks[i].erase(std::remove(hooks[i].begin(), hooks[i].end(), hook), hooks[i].end());
  }
}

void carve::csg::CSG::Hooks::reset() {
  for (unsigned i = 0; i < HOOK_MAX; ++i) {
    for (std::list<Hook *>::iterator j = hooks[i].begin(); j != hooks[i].end(); ++j) {
      delete (*j);
    }
    hooks[i].clear();
  }
}

carve::csg::CSG::Hooks::Hooks() : hooks() {
  hooks.resize(HOOK_MAX);
}
 
carve::csg::CSG::Hooks::~Hooks() {
  reset();
}



void carve::csg::CSG::makeVertexIntersections() {
  static carve::TimingName FUNC_NAME("CSG::makeVertexIntersections()");
  carve::TimingBlock block(FUNC_NAME);
  vertex_intersections.clear();
  for (Intersections::const_iterator
         i = intersections.begin(),
         ie = intersections.end();
       i != ie;
       ++i) {
    const IObj &i_src = ((*i).first);

    for (Intersections::mapped_type::const_iterator
           j = (*i).second.begin(),
           je = (*i).second.end();
         j != je;
         ++j) {
      const IObj &i_tgt = ((*j).first);
      meshset_t::vertex_t *i_pt = ((*j).second);

      vertex_intersections[i_pt].insert(std::make_pair(i_src, i_tgt));
    }
  }
}



static carve::mesh::MeshSet<3>::vertex_t *chooseWeldPoint(
    const carve::csg::detail::VSet &equivalent,
    carve::csg::VertexPool &vertex_pool) {
  // XXX: choose a better weld point.
  if (!equivalent.size()) return NULL;

  for (carve::csg::detail::VSet::const_iterator
         i = equivalent.begin(), e = equivalent.end();
       i != e;
       ++i) {
    if (!vertex_pool.inPool((*i))) return (*i);
  }
  return *equivalent.begin();
}



static const carve::mesh::MeshSet<3>::vertex_t *weld(
    const carve::csg::detail::VSet &equivalent,
    carve::csg::VertexIntersections &vertex_intersections,
    carve::csg::VertexPool &vertex_pool) {
  carve::mesh::MeshSet<3>::vertex_t *weld_point = chooseWeldPoint(equivalent, vertex_pool);

#if defined(CARVE_DEBUG)
  std::cerr << "weld: " << equivalent.size() << " vertices ( ";
  for (carve::csg::detail::VSet::const_iterator
         i = equivalent.begin(), e = equivalent.end();
       i != e;
       ++i) {
    const carve::mesh::MeshSet<3>::vertex_t *v = (*i);
    std::cerr << " " << v;
  }
  std::cerr << ") to " << weld_point << std::endl;
#endif

  if (!weld_point) return NULL;

  carve::csg::VertexIntersections::mapped_type &weld_tgt = (vertex_intersections[weld_point]);

  for (carve::csg::detail::VSet::const_iterator
         i = equivalent.begin(), e = equivalent.end();
       i != e;
       ++i) {
    carve::mesh::MeshSet<3>::vertex_t *v = (*i);

    if (v != weld_point) {
      carve::csg::VertexIntersections::iterator j = vertex_intersections.find(v);

      if (j != vertex_intersections.end()) {
        weld_tgt.insert((*j).second.begin(), (*j).second.end());
        vertex_intersections.erase(j);
      }
    }
  }
  return weld_point;
}



void carve::csg::CSG::groupIntersections() {
#if 0 // old code, to be removed.
  static carve::TimingName GROUP_INTERSECTONS("groupIntersections()");

  carve::TimingBlock block(GROUP_INTERSECTONS);
  
  std::vector<meshset_t::vertex_t *> vertices;
  detail::VVSMap graph;
#if defined(CARVE_DEBUG)
  std::cerr << "groupIntersections()" << ": vertex_intersections.size()==" << vertex_intersections.size() << std::endl;
#endif

  vertices.reserve(vertex_intersections.size());
  for (carve::csg::VertexIntersections::const_iterator
         i = vertex_intersections.begin(),
         e = vertex_intersections.end();
       i != e;
       ++i) 
    {
      vertices.push_back((*i).first);
    }
  carve::geom3d::AABB aabb;
  aabb.fit(vertices.begin(), vertices.end(), carve::poly::vec_adapt_vertex_ptr());
  Octree vertex_intersections_octree;
  vertex_intersections_octree.setBounds(aabb);

  vertex_intersections_octree.addVertices(vertices);
      
  std::vector<meshset_t::vertex_t *> out;
  for (size_t i = 0, l = vertices.size(); i != l; ++i) {
    // let's find all the vertices near this one. 
    out.clear();
    vertex_intersections_octree.findVerticesNearAllowDupes(vertices[i]->v, out);

    for (size_t j = 0; j < out.size(); ++j) {
      if (vertices[i] != out[j] && carve::geom::equal(vertices[i]->v, out[j]->v)) {
#if defined(CARVE_DEBUG)
        std::cerr << "EQ: " << vertices[i] << "," << out[j] << " " << vertices[i]->v << "," << out[j]->v << std::endl;
#endif
        graph[vertices[i]].insert(out[j]);
        graph[out[j]].insert(vertices[i]);
      }
    }
  }

  detail::VSet visited, open;
  while (graph.size()) {
    visited.clear();
    open.clear();
    detail::VVSMap::iterator i = graph.begin();
    open.insert((*i).first);
    while (open.size()) {
      detail::VSet::iterator t = open.begin();
      const meshset_t::vertex_t *o = (*t);
      open.erase(t);
      i = graph.find(o);
      CARVE_ASSERT(i != graph.end());
      visited.insert(o);
      for (detail::VVSMap::mapped_type::const_iterator
             j = (*i).second.begin(),
             je = (*i).second.end();
           j != je;
           ++j) {
        if (visited.count((*j)) == 0) {
          open.insert((*j));
        }
      }
      graph.erase(i);
    }
    weld(visited, vertex_intersections, vertex_pool);
  }
#endif
}


static void recordEdgeIntersectionInfo(carve::mesh::MeshSet<3>::vertex_t *intersection,
                                       carve::mesh::MeshSet<3>::edge_t *edge,
                                       const carve::csg::detail::VFSMap::mapped_type &intersected_faces,
                                       carve::csg::detail::Data &data) {
  carve::mesh::MeshSet<3>::vertex_t::vector_t edge_dir = edge->v2()->v - edge->v1()->v;
  carve::csg::detail::EdgeIntInfo::mapped_type &eint_info = data.emap[edge][intersection];

  for (carve::csg::detail::VFSMap::mapped_type::const_iterator i = intersected_faces.begin(); i != intersected_faces.end(); ++i) {
    carve::mesh::MeshSet<3>::vertex_t::vector_t normal = (*i)->plane.N;
    eint_info.insert(std::make_pair((*i), carve::geom::dot(edge_dir, normal)));
  }
}


void carve::csg::CSG::intersectingFacePairs(detail::Data &data) {
  static carve::TimingName FUNC_NAME("CSG::intersectingFacePairs()");
  carve::TimingBlock block(FUNC_NAME);

  // iterate over all intersection points.
  for (VertexIntersections::const_iterator i = vertex_intersections.begin(), ie = vertex_intersections.end(); i != ie; ++i) {
    meshset_t::vertex_t *i_pt = ((*i).first);
    detail::VFSMap::mapped_type &face_set = (data.fmap_rev[i_pt]);
    detail::VFSMap::mapped_type src_face_set;
    detail::VFSMap::mapped_type tgt_face_set;
    // for all pairs of intersecting objects at this point
    for (VertexIntersections::data_type::const_iterator j = (*i).second.begin(), je = (*i).second.end(); j != je; ++j) {
      const IObj &i_src = ((*j).first);
      const IObj &i_tgt = ((*j).second);

      src_face_set.clear();
      tgt_face_set.clear();
      // work out the faces involved.
      facesForObject(i_src, data.vert_to_edges, src_face_set);
      facesForObject(i_tgt, data.vert_to_edges, tgt_face_set);
      // this updates fmap_rev.
      std::copy(src_face_set.begin(), src_face_set.end(), set_inserter(face_set));
      std::copy(tgt_face_set.begin(), tgt_face_set.end(), set_inserter(face_set));

      // record the intersection with respect to any involved vertex.
      if (i_src.obtype == IObj::OBTYPE_VERTEX) data.vmap[i_src.vertex] = i_pt;
      if (i_tgt.obtype == IObj::OBTYPE_VERTEX) data.vmap[i_tgt.vertex] = i_pt;

      // record the intersection with respect to any involved edge.
      if (i_src.obtype == IObj::OBTYPE_EDGE) recordEdgeIntersectionInfo(i_pt, i_src.edge, tgt_face_set, data);
      if (i_tgt.obtype == IObj::OBTYPE_EDGE) recordEdgeIntersectionInfo(i_pt, i_tgt.edge, src_face_set, data);
    }

    // record the intersection with respect to each face.
    for (carve::csg::detail::VFSMap::mapped_type::const_iterator k = face_set.begin(), ke = face_set.end(); k != ke; ++k) {
      meshset_t::face_t *f = (*k);
      data.fmap[f].insert(i_pt);
    }
  }
}



void carve::csg::CSG::_generateVertexVertexIntersections(meshset_t::vertex_t *va,
                                                         meshset_t::edge_t *eb) {
  if (intersections.intersects(va, eb->v1())) {
    return;
  }

  double d_v1 = carve::geom::distance2(va->v, eb->v1()->v);

  if  (d_v1 < carve::EPSILON2) {
    intersections.record(va, eb->v1(), va);
  }
}



void carve::csg::CSG::generateVertexVertexIntersections(meshset_t::face_t *a,
                                                        const std::vector<meshset_t::face_t *> &b) {
  meshset_t::edge_t *ea, *eb;

  ea = a->edge;
  do {
    for (size_t i = 0; i < b.size(); ++i) {
      meshset_t::face_t *t = b[i];
      eb = t->edge;
      do {
        _generateVertexVertexIntersections(ea->v1(), eb);
        eb = eb->next;
      } while (eb != t->edge);
    }
    ea = ea->next;
  } while (ea != a->edge);
}



void carve::csg::CSG::_generateVertexEdgeIntersections(meshset_t::vertex_t *va,
                                                       meshset_t::edge_t *eb) {
  if (intersections.intersects(va, eb)) {
    return;
  }

  carve::geom::aabb<3> eb_aabb;
  eb_aabb.fit(eb->v1()->v, eb->v2()->v);
  if (eb_aabb.maxAxisSeparation(va->v) > carve::EPSILON) {
    return;
  }

  double a = cross(eb->v2()->v - eb->v1()->v, va->v - eb->v1()->v).length2();
  double b = (eb->v2()->v - eb->v1()->v).length2();

  if (a < b * carve::EPSILON2) {
    // vertex-edge intersection
    intersections.record(eb, va, va);
    if (eb->rev) intersections.record(eb->rev, va, va);
  }
}



void carve::csg::CSG::generateVertexEdgeIntersections(meshset_t::face_t *a,
                                                      const std::vector<meshset_t::face_t *> &b) {
  meshset_t::edge_t *ea, *eb;

  ea = a->edge;
  do {
    for (size_t i = 0; i < b.size(); ++i) {
      meshset_t::face_t *t = b[i];
      eb = t->edge;
      do {
        _generateVertexEdgeIntersections(ea->v1(), eb);
        eb = eb->next;
      } while (eb != t->edge);
    }
    ea = ea->next;
  } while (ea != a->edge);
}



void carve::csg::CSG::_generateEdgeEdgeIntersections(meshset_t::edge_t *ea,
                                                     meshset_t::edge_t *eb) {
  if (intersections.intersects(ea, eb)) {
    return;
  }

  meshset_t::vertex_t *v1 = ea->v1(), *v2 = ea->v2();
  meshset_t::vertex_t *v3 = eb->v1(), *v4 = eb->v2();

  carve::geom::aabb<3> ea_aabb, eb_aabb;
  ea_aabb.fit(v1->v, v2->v);
  eb_aabb.fit(v3->v, v4->v);
  if (ea_aabb.maxAxisSeparation(eb_aabb) > EPSILON) return;

  meshset_t::vertex_t::vector_t p1, p2;
  double mu1, mu2;

  switch (carve::geom3d::rayRayIntersection(carve::geom3d::Ray(v2->v - v1->v, v1->v),
                                            carve::geom3d::Ray(v4->v - v3->v, v3->v),
                                            p1, p2, mu1, mu2)) {
  case carve::RR_INTERSECTION: {
    // edges intersect
    if (mu1 >= 0.0 && mu1 <= 1.0 && mu2 >= 0.0 && mu2 <= 1.0) {
      meshset_t::vertex_t *p = vertex_pool.get((p1 + p2) / 2.0);
      intersections.record(ea, eb, p);
      if (ea->rev) intersections.record(ea->rev, eb, p);
      if (eb->rev) intersections.record(ea, eb->rev, p);
      if (ea->rev && eb->rev) intersections.record(ea->rev, eb->rev, p);
    }
    break;
  }
  case carve::RR_PARALLEL: {
    // edges parallel. any intersection of this type should have
    // been handled by generateVertexEdgeIntersections().
    break;
  }
  case carve::RR_DEGENERATE: {
    throw carve::exception("degenerate edge");
    break;
  }
  case carve::RR_NO_INTERSECTION: {
    break;
  }
  }
}



void carve::csg::CSG::generateEdgeEdgeIntersections(meshset_t::face_t *a,
                                                    const std::vector<meshset_t::face_t *> &b) {
  meshset_t::edge_t *ea, *eb;

  ea = a->edge;
  do {
    for (size_t i = 0; i < b.size(); ++i) {
      meshset_t::face_t *t = b[i];
      eb = t->edge;
      do {
        _generateEdgeEdgeIntersections(ea, eb);
        eb = eb->next;
      } while (eb != t->edge);
    }
    ea = ea->next;
  } while (ea != a->edge);
}



void carve::csg::CSG::_generateVertexFaceIntersections(meshset_t::face_t *fa,
                                                       meshset_t::edge_t *eb) {
  if (intersections.intersects(eb->v1(), fa)) {
    return;
  }

  double d1 = carve::geom::distance(fa->plane, eb->v1()->v);

  if (fabs(d1) < carve::EPSILON &&
      fa->containsPoint(eb->v1()->v)) {
    intersections.record(eb->v1(), fa, eb->v1());
  }
}



void carve::csg::CSG::generateVertexFaceIntersections(meshset_t::face_t *a,
                                                      const std::vector<meshset_t::face_t *> &b) {
  meshset_t::edge_t *eb;

  for (size_t i = 0; i < b.size(); ++i) {
    meshset_t::face_t *t = b[i];
    eb = t->edge;
    do {
      _generateVertexFaceIntersections(a, eb);
      eb = eb->next;
    } while (eb != t->edge);
  }
}



void carve::csg::CSG::_generateEdgeFaceIntersections(meshset_t::face_t *fa,
                                                     meshset_t::edge_t *eb) {
  if (intersections.intersects(eb, fa)) {
    return;
  }

  meshset_t::vertex_t::vector_t _p;
  if (fa->simpleLineSegmentIntersection(carve::geom3d::LineSegment(eb->v1()->v, eb->v2()->v), _p)) {
    meshset_t::vertex_t *p = vertex_pool.get(_p);
    intersections.record(eb, fa, p);
    if (eb->rev) intersections.record(eb->rev, fa, p);
  }
}



void carve::csg::CSG::generateEdgeFaceIntersections(meshset_t::face_t *a,
                                                    const std::vector<meshset_t::face_t *> &b) {
  meshset_t::edge_t *eb;

  for (size_t i = 0; i < b.size(); ++i) {
    meshset_t::face_t *t = b[i];
    eb = t->edge;
    do {
      _generateEdgeFaceIntersections(a, eb);
      eb = eb->next;
    } while (eb != t->edge);
  }
}



void carve::csg::CSG::generateIntersectionCandidates(meshset_t *a,
                                                     const face_rtree_t *a_node,
                                                     meshset_t *b,
                                                     const face_rtree_t *b_node,
                                                     face_pairs_t &face_pairs,
                                                     bool descend_a) {
  if (!a_node->bbox.intersects(b_node->bbox)) {
    return;
  }

  if (a_node->child && (descend_a || !b_node->child)) {
    for (face_rtree_t *node = a_node->child; node; node = node->sibling) {
      generateIntersectionCandidates(a, node, b, b_node, face_pairs, false);
    }
  } else if (b_node->child) {
    for (face_rtree_t *node = b_node->child; node; node = node->sibling) {
      generateIntersectionCandidates(a, a_node, b, node, face_pairs, true);
    }
  } else {
    for (size_t i = 0; i < a_node->data.size(); ++i) {
      meshset_t::face_t *fa = a_node->data[i];
      carve::geom::aabb<3> aabb_a = fa->getAABB();
      if (aabb_a.maxAxisSeparation(b_node->bbox) > carve::EPSILON) continue;

      for (size_t j = 0; j < b_node->data.size(); ++j) {
        meshset_t::face_t *fb = b_node->data[j];
        carve::geom::aabb<3> aabb_b = fb->getAABB();
        if (aabb_b.maxAxisSeparation(aabb_a) > carve::EPSILON) continue;

        std::pair<double, double> a_ra = fa->rangeInDirection(fa->plane.N, fa->edge->vert->v);
        std::pair<double, double> b_ra = fb->rangeInDirection(fa->plane.N, fa->edge->vert->v);
        if (carve::rangeSeparation(a_ra, b_ra) > carve::EPSILON) continue;

        std::pair<double, double> a_rb = fa->rangeInDirection(fb->plane.N, fb->edge->vert->v);
        std::pair<double, double> b_rb = fb->rangeInDirection(fb->plane.N, fb->edge->vert->v);
        if (carve::rangeSeparation(a_rb, b_rb) > carve::EPSILON) continue;

        if (!facesAreCoplanar(fa, fb)) {
          face_pairs[fa].push_back(fb); 
          face_pairs[fb].push_back(fa);
        }
     }
    }
  }
}




void carve::csg::CSG::generateIntersections(meshset_t *a,
                                            const face_rtree_t *a_rtree,
                                            meshset_t *b,
                                            const face_rtree_t *b_rtree,
                                            detail::Data &data) {
  face_pairs_t face_pairs;
  generateIntersectionCandidates(a, a_rtree, b, b_rtree, face_pairs);

  for (face_pairs_t::const_iterator i = face_pairs.begin(); i != face_pairs.end(); ++i) {
    meshset_t::face_t *f = (*i).first;
    meshset_t::edge_t *e = f->edge;
    do {
      data.vert_to_edges[e->v1()].push_back(e);
      e = e->next;
    } while (e != f->edge);
  }

  for (face_pairs_t::const_iterator i = face_pairs.begin(); i != face_pairs.end(); ++i) {
    generateVertexVertexIntersections((*i).first, (*i).second);
  }

  for (face_pairs_t::const_iterator i = face_pairs.begin(); i != face_pairs.end(); ++i) {
    generateVertexEdgeIntersections((*i).first, (*i).second);
  }

  for (face_pairs_t::const_iterator i = face_pairs.begin(); i != face_pairs.end(); ++i) {
    generateEdgeEdgeIntersections((*i).first, (*i).second);
  }

  for (face_pairs_t::const_iterator i = face_pairs.begin(); i != face_pairs.end(); ++i) {
    generateVertexFaceIntersections((*i).first, (*i).second);
  }

  for (face_pairs_t::const_iterator i = face_pairs.begin(); i != face_pairs.end(); ++i) {
    generateEdgeFaceIntersections((*i).first, (*i).second);
  }


#if defined(CARVE_DEBUG)
  std::cerr << "makeVertexIntersections" << std::endl;
#endif
  makeVertexIntersections();

#if defined(CARVE_DEBUG)
  std::cerr << "  intersections.size() " << intersections.size() << std::endl;
  map_histogram(std::cerr, intersections);
  std::cerr << "  vertex_intersections.size() " << vertex_intersections.size() << std::endl;
  map_histogram(std::cerr, vertex_intersections);
#endif

#if defined(CARVE_DEBUG) && defined(DEBUG_DRAW_INTERSECTIONS)
  HOOK(drawIntersections(vertex_intersections););
#endif

#if defined(CARVE_DEBUG)
  std::cerr << "  intersections.size() " << intersections.size() << std::endl;
  std::cerr << "  vertex_intersections.size() " << vertex_intersections.size() << std::endl;
#endif

  // notify about intersections.
  if (hooks.hasHook(Hooks::INTERSECTION_VERTEX_HOOK)) {
    for (VertexIntersections::const_iterator i = vertex_intersections.begin();
         i != vertex_intersections.end();
         ++i) {
      hooks.intersectionVertex((*i).first, (*i).second);
    }
  }

  // from here on, only vertex_intersections is used for intersection
  // information.

  // intersections still contains the vertex_to_face map. maybe that
  // should be moved out into another class.
  static_cast<Intersections::super>(intersections).clear();
}



carve::csg::CSG::CSG() {
}



/** 
 * \brief For each intersected edge, decompose into a set of vertex pairs representing an ordered set of edge fragments.
 * 
 * @tparam[in,out] data Internal intersection data. data.emap is used to produce data.divided_edges.
 */
void carve::csg::CSG::divideIntersectedEdges(detail::Data &data) {
  static carve::TimingName FUNC_NAME("CSG::divideIntersectedEdges()");
  carve::TimingBlock block(FUNC_NAME);

  for (detail::EIntMap::const_iterator i = data.emap.begin(), ei = data.emap.end(); i != ei; ++i) {
    meshset_t::edge_t *edge = (*i).first;
    const detail::EIntMap::mapped_type &int_info = (*i).second;
    std::vector<meshset_t::vertex_t *> &verts = data.divided_edges[edge];
    orderEdgeIntersectionVertices(int_info.begin(), int_info.end(),
                                  edge->v2()->v - edge->v1()->v, edge->v1()->v,
                                  verts);
  }
}



carve::csg::CSG::~CSG() {
}



void carve::csg::CSG::makeFaceEdges(carve::csg::EdgeClassification &eclass,
                                    detail::Data &data) {
  detail::FSet face_b_set;
  for (detail::FVSMap::const_iterator
         i = data.fmap.begin(), ie = data.fmap.end();
       i != ie;
       ++i) {
    meshset_t::face_t *face_a = (*i).first;
    const detail::FVSMap::mapped_type &face_a_intersections = ((*i).second);
    face_b_set.clear();

    // work out the set of faces from the opposing polyhedron that intersect face_a.
    for (detail::FVSMap::mapped_type::const_iterator
           j = face_a_intersections.begin(), je = face_a_intersections.end();
         j != je;
         ++j) {
      for (detail::VFSMap::mapped_type::const_iterator
             k = data.fmap_rev[*j].begin(), ke = data.fmap_rev[*j].end();
           k != ke;
           ++k) {
        meshset_t::face_t *face_b = (*k);
        if (face_a != face_b && face_b->mesh->meshset != face_a->mesh->meshset) {
          face_b_set.insert(face_b);
        }
      }
    }

    // run through each intersecting face.
    for (detail::FSet::const_iterator
           j = face_b_set.begin(), je = face_b_set.end();
         j != je;
         ++j) {
      meshset_t::face_t *face_b = (*j);
      const detail::FVSMap::mapped_type &face_b_intersections = (data.fmap[face_b]);

      std::vector<meshset_t::vertex_t *> vertices;
      vertices.reserve(std::min(face_a_intersections.size(), face_b_intersections.size()));

      // record the points of intersection between face_a and face_b
      std::set_intersection(face_a_intersections.begin(),
                            face_a_intersections.end(),
                            face_b_intersections.begin(),
                            face_b_intersections.end(),
                            std::back_inserter(vertices));

#if defined(CARVE_DEBUG)
      std::cerr << "face pair: "
                << face_a << ":" << face_b
                << " N(verts) " << vertices.size() << std::endl;
      for (std::vector<meshset_t::vertex_t *>::const_iterator i = vertices.begin(), e = vertices.end(); i != e; ++i) {
        std::cerr << (*i) << " " << (*i)->v << " ("
                  << carve::geom::distance(face_a->plane, (*i)->v) << ","
                  << carve::geom::distance(face_b->plane, (*i)->v) << ")"
                  << std::endl;
        //CARVE_ASSERT(carve::geom3d::distance(face_a->plane_eqn, *(*i)) < EPSILON);
        //CARVE_ASSERT(carve::geom3d::distance(face_b->plane_eqn, *(*i)) < EPSILON);
      }
#endif

      // if there are two points of intersection, then the added edge is simple to determine.
      if (vertices.size() == 2) {
        meshset_t::vertex_t *v1 = vertices[0];
        meshset_t::vertex_t *v2 = vertices[1];
        carve::geom3d::Vector c = (v1->v + v2->v) / 2;

        // determine whether the midpoint of the implied edge is contained in face_a and face_b

#if defined(CARVE_DEBUG)
        std::cerr << "face_a->nVertices() = " << face_a->nVertices() << " face_a->containsPointInProjection(c) = " << face_a->containsPointInProjection(c) << std::endl;
        std::cerr << "face_b->nVertices() = " << face_b->nVertices() << " face_b->containsPointInProjection(c) = " << face_b->containsPointInProjection(c) << std::endl;
#endif

        if (face_a->containsPointInProjection(c) && face_b->containsPointInProjection(c)) {
#if defined(CARVE_DEBUG)
          std::cerr << "adding edge: " << v1 << "-" << v2 << std::endl;
#if defined(DEBUG_DRAW_FACE_EDGES)
          HOOK(drawEdge(v1, v2, 1, 1, 1, 1, 1, 1, 1, 1, 2.0););
#endif
#endif
          // record the edge, with class information.
          if (v1 > v2) std::swap(v1, v2);
          eclass[ordered_edge(v1, v2)] = carve::csg::EC2(carve::csg::EDGE_ON, carve::csg::EDGE_ON);
          data.face_split_edges[face_a].insert(std::make_pair(v1, v2));
          data.face_split_edges[face_b].insert(std::make_pair(v1, v2));
        }
        continue;
      }

      // otherwise, it's more complex.
      carve::geom3d::Vector base, dir;
      std::vector<meshset_t::vertex_t *> ordered;

      // skip coplanar edges. this simplifies the resulting
      // mesh. eventually all coplanar face regions of two polyhedra
      // must reach a point where they are no longer coplanar (or the
      // polyhedra are identical).
      if (!facesAreCoplanar(face_a, face_b)) {
        // order the intersection vertices (they must lie along a
        // vector, as the faces aren't coplanar).
        selectOrderingProjection(vertices.begin(), vertices.end(), dir, base);
        orderVertices(vertices.begin(), vertices.end(), dir, base, ordered);

        // for each possible edge in the ordering, test the midpoint,
        // and record if it's contained in face_a and face_b.
        for (int k = 0, ke = (int)ordered.size() - 1; k < ke; ++k) {
          meshset_t::vertex_t *v1 = ordered[k];
          meshset_t::vertex_t *v2 = ordered[k + 1];
          carve::geom3d::Vector c = (v1->v + v2->v) / 2;

#if defined(CARVE_DEBUG)
          std::cerr << "testing edge: " << v1 << "-" << v2 << " at " << c << std::endl;
          std::cerr << "a: " << face_a->containsPointInProjection(c) << " b: " << face_b->containsPointInProjection(c) << std::endl;
          std::cerr << "face_a->containsPointInProjection(c): " << face_a->containsPointInProjection(c) << std::endl;
          std::cerr << "face_b->containsPointInProjection(c): " << face_b->containsPointInProjection(c) << std::endl;
#endif

          if (face_a->containsPointInProjection(c) && face_b->containsPointInProjection(c)) {
#if defined(CARVE_DEBUG)
            std::cerr << "adding edge: " << v1 << "-" << v2 << std::endl;
#if defined(DEBUG_DRAW_FACE_EDGES)
            HOOK(drawEdge(v1, v2, .5, .5, .5, 1, .5, .5, .5, 1, 2.0););
#endif
#endif
            // record the edge, with class information.
            if (v1 > v2) std::swap(v1, v2);
            eclass[ordered_edge(v1, v2)] = carve::csg::EC2(carve::csg::EDGE_ON, carve::csg::EDGE_ON);
            data.face_split_edges[face_a].insert(std::make_pair(v1, v2));
            data.face_split_edges[face_b].insert(std::make_pair(v1, v2));
          }
        }
      }
    }
  }


#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
  {
    V2Set edges;
    for (detail::FV2SMap::const_iterator i = data.face_split_edges.begin(); i != data.face_split_edges.end(); ++i) {
      edges.insert((*i).second.begin(), (*i).second.end());
    }

    detail::VSet vertices;
    for (V2Set::const_iterator i = edges.begin(); i != edges.end(); ++i) {
      vertices.insert((*i).first);
      vertices.insert((*i).second);
    }

    carve::line::PolylineSet intersection_graph;
    intersection_graph.vertices.resize(vertices.size());
    std::map<const meshset_t::vertex_t *, size_t> vmap;

    size_t j = 0;
    for (detail::VSet::const_iterator i = vertices.begin(); i != vertices.end(); ++i) {
      intersection_graph.vertices[j].v = (*i)->v;
      vmap[(*i)] = j++;
    }

    for (V2Set::const_iterator i = edges.begin(); i != edges.end(); ++i) {
      size_t line[2];
      line[0] = vmap[(*i).first];
      line[1] = vmap[(*i).second];
      intersection_graph.addPolyline(false, line, line + 2);
    }

    std::string out("/tmp/intersection-edges.ply");
    ::writePLY(out, &intersection_graph, true);
  }
#endif
}



/** 
 * 
 * 
 * @param fll 
 */
static void checkFaceLoopIntegrity(carve::csg::FaceLoopList &fll) {
  static carve::TimingName FUNC_NAME("CSG::checkFaceLoopIntegrity()");
  carve::TimingBlock block(FUNC_NAME);

  std::unordered_map<carve::csg::V2, int> counts;
  for (carve::csg::FaceLoop *fl = fll.head; fl; fl = fl->next) {
    std::vector<carve::mesh::MeshSet<3>::vertex_t *> &loop = (fl->vertices);
    carve::mesh::MeshSet<3>::vertex_t *v1, *v2;
    v1 = loop[loop.size() - 1];
    for (unsigned i = 0; i < loop.size(); ++i) {
      v2 = loop[i];
      if (v1 < v2) {
        counts[std::make_pair(v1, v2)]++;
      } else {
        counts[std::make_pair(v2, v1)]--;
      }
      v1 = v2;
    }
  }
  for (std::unordered_map<carve::csg::V2, int>::const_iterator
         x = counts.begin(), xe = counts.end(); x != xe; ++x) {
    if ((*x).second) {
      std::cerr << "FACE LOOP ERROR: " << (*x).first.first << "-" << (*x).first.second << " : " << (*x).second << std::endl;
    }
  }
}



/** 
 * 
 * 
 * @param a 
 * @param b 
 * @param vclass 
 * @param eclass 
 * @param a_face_loops 
 * @param b_face_loops 
 * @param a_edge_count 
 * @param b_edge_count 
 * @param hooks 
 */
void carve::csg::CSG::calc(meshset_t *a,
                           const face_rtree_t *a_rtree,
                           meshset_t *b,
                           const face_rtree_t *b_rtree,
                           carve::csg::VertexClassification &vclass,
                           carve::csg::EdgeClassification &eclass,
                           carve::csg::FaceLoopList &a_face_loops,
                           carve::csg::FaceLoopList &b_face_loops,
                           size_t &a_edge_count,
                           size_t &b_edge_count) {
  detail::Data data;

#if defined(CARVE_DEBUG)
  std::cerr << "init" << std::endl;
#endif
  init();

  generateIntersections(a, a_rtree, b, b_rtree, data);

#if defined(CARVE_DEBUG)
  std::cerr << "intersectingFacePairs" << std::endl;
#endif
  intersectingFacePairs(data);

#if defined(CARVE_DEBUG)
  std::cerr << "emap:" << std::endl;
  map_histogram(std::cerr, data.emap);
  std::cerr << "fmap:" << std::endl;
  map_histogram(std::cerr, data.fmap);
  std::cerr << "fmap_rev:" << std::endl;
  map_histogram(std::cerr, data.fmap_rev);
#endif

  // std::cerr << "removeCoplanarFaces" << std::endl;
  // fp_intersections.removeCoplanarFaces();

#if defined(CARVE_DEBUG) && defined(DEBUG_DRAW_OCTREE)
  HOOK(drawOctree(a->octree););
  HOOK(drawOctree(b->octree););
#endif

#if defined(CARVE_DEBUG)
  std::cerr << "divideIntersectedEdges" << std::endl;
#endif
  divideIntersectedEdges(data);

#if defined(CARVE_DEBUG)
  std::cerr << "makeFaceEdges" << std::endl;
#endif
  // makeFaceEdges(data.face_split_edges, eclass, data.fmap, data.fmap_rev);
  makeFaceEdges(eclass, data);

#if defined(CARVE_DEBUG)
  std::cerr << "generateFaceLoops" << std::endl;
#endif
  a_edge_count = generateFaceLoops(a, data, a_face_loops);
  b_edge_count = generateFaceLoops(b, data, b_face_loops);

#if defined(CARVE_DEBUG)
  std::cerr << "generated " << a_edge_count << " edges for poly a" << std::endl;
  std::cerr << "generated " << b_edge_count << " edges for poly b" << std::endl;
#endif

#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
  {
    std::auto_ptr<carve::mesh::MeshSet<3> > poly(faceLoopsToPolyhedron(a_face_loops));
    writePLY("/tmp/a_split.ply", poly.get(), false);
  }
  {
    std::auto_ptr<carve::mesh::MeshSet<3> > poly(faceLoopsToPolyhedron(b_face_loops));
    writePLY("/tmp/b_split.ply", poly.get(), false);
  }
#endif

  checkFaceLoopIntegrity(a_face_loops);
  checkFaceLoopIntegrity(b_face_loops);

#if defined(CARVE_DEBUG)
  std::cerr << "classify" << std::endl;
#endif
  // initialize some classification information.
  for (std::vector<meshset_t::vertex_t>::iterator
         i = a->vertex_storage.begin(), e = a->vertex_storage.end(); i != e; ++i) {
    vclass[map_vertex(data.vmap, &(*i))].cls[0] = POINT_ON;
  }
  for (std::vector<meshset_t::vertex_t>::iterator
         i = b->vertex_storage.begin(), e = b->vertex_storage.end(); i != e; ++i) {
    vclass[map_vertex(data.vmap, &(*i))].cls[1] = POINT_ON;
  }
  for (VertexIntersections::const_iterator
         i = vertex_intersections.begin(), e = vertex_intersections.end(); i != e; ++i) {
    vclass[(*i).first] = PC2(POINT_ON, POINT_ON);
  }

#if defined(CARVE_DEBUG)
  std::cerr << data.divided_edges.size() << " edges are split" << std::endl;
  std::cerr << data.face_split_edges.size() << " faces are split" << std::endl;

  std::cerr << "poly a: " << a_face_loops.size() << " face loops" << std::endl;
  std::cerr << "poly b: " << b_face_loops.size() << " face loops" << std::endl;
#endif

  // std::cerr << "OCTREE A:" << std::endl;
  // dump_octree_stats(a->octree.root, 0);
  // std::cerr << "OCTREE B:" << std::endl;
  // dump_octree_stats(b->octree.root, 0);
}



/** 
 * 
 * 
 * @param shared_edges 
 * @param result_list 
 * @param shared_edge_ptr 
 */
void returnSharedEdges(carve::csg::V2Set &shared_edges, 
                       std::list<carve::mesh::MeshSet<3> *> &result_list,
                       carve::csg::V2Set *shared_edge_ptr) {
  // need to convert shared edges to point into result
  typedef std::map<carve::geom3d::Vector, carve::mesh::MeshSet<3>::vertex_t *> remap_type;
  remap_type remap;
  for (std::list<carve::mesh::MeshSet<3> *>::iterator list_it =
         result_list.begin(); list_it != result_list.end(); list_it++) {
    carve::mesh::MeshSet<3> *result = *list_it;
    if (result) {
      for (std::vector<carve::mesh::MeshSet<3>::vertex_t>::iterator it =
             result->vertex_storage.begin(); it != result->vertex_storage.end(); it++) {
        remap.insert(std::make_pair((*it).v, &(*it)));
      }
    }
  }
  for (carve::csg::V2Set::iterator it = shared_edges.begin(); 
       it != shared_edges.end(); it++) {
    remap_type::iterator first_it = remap.find(((*it).first)->v);
    remap_type::iterator second_it = remap.find(((*it).second)->v);
    CARVE_ASSERT(first_it != remap.end() && second_it != remap.end());
    shared_edge_ptr->insert(std::make_pair(first_it->second, second_it->second));
  }
}



/** 
 * 
 * 
 * @param a 
 * @param b 
 * @param collector 
 * @param hooks 
 * @param shared_edges_ptr 
 * @param classify_type 
 * 
 * @return 
 */
carve::mesh::MeshSet<3> *carve::csg::CSG::compute(meshset_t *a,
                                                  meshset_t *b,
                                                  carve::csg::CSG::Collector &collector,
                                                  carve::csg::V2Set *shared_edges_ptr,
                                                  CLASSIFY_TYPE classify_type) {
  static carve::TimingName FUNC_NAME("CSG::compute");
  carve::TimingBlock block(FUNC_NAME);

  VertexClassification vclass;
  EdgeClassification eclass;

  FLGroupList a_loops_grouped;
  FLGroupList b_loops_grouped;

  FaceLoopList a_face_loops;
  FaceLoopList b_face_loops;

  size_t a_edge_count;
  size_t b_edge_count;

  std::auto_ptr<face_rtree_t> a_rtree(face_rtree_t::construct_STR(a->faceBegin(), a->faceEnd(), 4, 4));
  std::auto_ptr<face_rtree_t> b_rtree(face_rtree_t::construct_STR(b->faceBegin(), b->faceEnd(), 4, 4));

  {
    static carve::TimingName FUNC_NAME("CSG::compute - calc()");
    carve::TimingBlock block(FUNC_NAME);
    calc(a, a_rtree.get(), b, b_rtree.get(), vclass, eclass,a_face_loops, b_face_loops, a_edge_count, b_edge_count);
  }

  detail::LoopEdges a_edge_map;
  detail::LoopEdges b_edge_map;

  {
    static carve::TimingName FUNC_NAME("CSG::compute - makeEdgeMap()");
    carve::TimingBlock block(FUNC_NAME);
    makeEdgeMap(a_face_loops, a_edge_count, a_edge_map);
    makeEdgeMap(b_face_loops, b_edge_count, b_edge_map);

  }
  
  {
    static carve::TimingName FUNC_NAME("CSG::compute - sortFaceLoopLists()");
    carve::TimingBlock block(FUNC_NAME);
    a_edge_map.sortFaceLoopLists();
    b_edge_map.sortFaceLoopLists();
  }

  V2Set shared_edges;
  
  {
    static carve::TimingName FUNC_NAME("CSG::compute - findSharedEdges()");
    carve::TimingBlock block(FUNC_NAME);
    findSharedEdges(a_edge_map, b_edge_map, shared_edges);
  }

  {
    static carve::TimingName FUNC_NAME("CSG::compute - groupFaceLoops()");
    carve::TimingBlock block(FUNC_NAME);
    groupFaceLoops(a, a_face_loops, a_edge_map, shared_edges, a_loops_grouped);
    groupFaceLoops(b, b_face_loops, b_edge_map, shared_edges, b_loops_grouped);
#if defined(CARVE_DEBUG)
    std::cerr << "*** a_loops_grouped.size(): " << a_loops_grouped.size() << std::endl;
    std::cerr << "*** b_loops_grouped.size(): " << b_loops_grouped.size() << std::endl;
#endif
  }

#if defined(CARVE_DEBUG) && defined(DEBUG_DRAW_GROUPS)
  {
    float n = 1.0 / (a_loops_grouped.size() + b_loops_grouped.size() + 1);
    float H = 0.0, S = 1.0, V = 1.0;
    float r, g, b;
    for (FLGroupList::const_iterator i = a_loops_grouped.begin(); i != a_loops_grouped.end(); ++i) {
      carve::colour::HSV2RGB(H, S, V, r, g, b); H += n;
      drawFaceLoopList((*i).face_loops, r, g, b, 1.0, r * .5, g * .5, b * .5, 1.0, true);
    }
    for (FLGroupList::const_iterator i = b_loops_grouped.begin(); i != b_loops_grouped.end(); ++i) {
      carve::colour::HSV2RGB(H, S, V, r, g, b); H += n;
      drawFaceLoopList((*i).face_loops, r, g, b, 1.0, r * .5, g * .5, b * .5, 1.0, true);
    }

    for (FLGroupList::const_iterator i = a_loops_grouped.begin(); i != a_loops_grouped.end(); ++i) {
      drawFaceLoopListWireframe((*i).face_loops);
    }
    for (FLGroupList::const_iterator i = b_loops_grouped.begin(); i != b_loops_grouped.end(); ++i) {
      drawFaceLoopListWireframe((*i).face_loops);
    }
  }
#endif

  switch (classify_type) {
  case CLASSIFY_EDGE:
    classifyFaceGroupsEdge(shared_edges,
                           vclass,
                           a,
                           a_rtree.get(),
                           a_loops_grouped,
                           a_edge_map,
                           b,
                           b_rtree.get(),
                           b_loops_grouped,
                           b_edge_map,
                           collector);
    break;
  case CLASSIFY_NORMAL:
    classifyFaceGroups(shared_edges,
                       vclass,
                       a,
                       a_rtree.get(),
                       a_loops_grouped,
                       a_edge_map,
                       b,
                       b_rtree.get(),
                       b_loops_grouped,
                       b_edge_map,
                       collector);
    break;
  }

  meshset_t *result = collector.done(hooks);
  if (result != NULL && shared_edges_ptr != NULL) {
    std::list<meshset_t *> result_list;
    result_list.push_back(result);
    returnSharedEdges(shared_edges, result_list, shared_edges_ptr);
  }
  return result;
}



/** 
 * 
 * 
 * @param a 
 * @param b 
 * @param op 
 * @param hooks 
 * @param shared_edges 
 * @param classify_type 
 * 
 * @return 
 */
carve::mesh::MeshSet<3> *carve::csg::CSG::compute(meshset_t *a,
                                                  meshset_t *b,
                                                  carve::csg::CSG::OP op,
                                                  carve::csg::V2Set *shared_edges,
                                                  CLASSIFY_TYPE classify_type) {
  Collector *coll = makeCollector(op, a, b);
  if (!coll) return NULL;

  meshset_t *result = compute(a, b, *coll, shared_edges, classify_type);
     
  delete coll;

  return result;
}



/** 
 * 
 * 
 * @param closed 
 * @param open 
 * @param FaceClass 
 * @param result 
 * @param hooks 
 * @param shared_edges_ptr 
 * 
 * @return 
 */
bool carve::csg::CSG::sliceAndClassify(meshset_t *closed,
                                       meshset_t *open,
                                       std::list<std::pair<FaceClass, meshset_t *> > &result,
                                       carve::csg::V2Set *shared_edges_ptr) {
  if (!closed->isClosed()) return false;
  carve::csg::VertexClassification vclass;
  carve::csg::EdgeClassification eclass;

  carve::csg::FLGroupList a_loops_grouped;
  carve::csg::FLGroupList b_loops_grouped;

  carve::csg::FaceLoopList a_face_loops;
  carve::csg::FaceLoopList b_face_loops;

  size_t a_edge_count;
  size_t b_edge_count;

  std::auto_ptr<face_rtree_t> closed_rtree(face_rtree_t::construct_STR(closed->faceBegin(), closed->faceEnd(), 4, 4));
  std::auto_ptr<face_rtree_t> open_rtree(face_rtree_t::construct_STR(open->faceBegin(), open->faceEnd(), 4, 4));

  calc(closed, closed_rtree.get(), open, open_rtree.get(), vclass, eclass,a_face_loops, b_face_loops, a_edge_count, b_edge_count);

  detail::LoopEdges a_edge_map;
  detail::LoopEdges b_edge_map;

  makeEdgeMap(a_face_loops, a_edge_count, a_edge_map);
  makeEdgeMap(b_face_loops, b_edge_count, b_edge_map);
  
  carve::csg::V2Set shared_edges;
  
  findSharedEdges(a_edge_map, b_edge_map, shared_edges);
  
  groupFaceLoops(closed, a_face_loops, a_edge_map, shared_edges, a_loops_grouped);
  groupFaceLoops(open,   b_face_loops, b_edge_map, shared_edges, b_loops_grouped);

  halfClassifyFaceGroups(shared_edges,
                         vclass,
                         closed,
                         closed_rtree.get(),
                         a_loops_grouped,
                         a_edge_map,
                         open,
                         open_rtree.get(),
                         b_loops_grouped,
                         b_edge_map,
                         result);

  if (shared_edges_ptr != NULL) {
    std::list<meshset_t *> result_list;
    for (std::list<std::pair<FaceClass, meshset_t *> >::iterator it = result.begin(); it != result.end(); it++) {
      result_list.push_back(it->second);
    }
    returnSharedEdges(shared_edges, result_list, shared_edges_ptr);
  }
  return true;
}



/** 
 * 
 * 
 * @param a 
 * @param b 
 * @param a_sliced 
 * @param b_sliced 
 * @param hooks 
 * @param shared_edges_ptr 
 */
void carve::csg::CSG::slice(meshset_t *a,
                            meshset_t *b,
                            std::list<meshset_t *> &a_sliced,
                            std::list<meshset_t *> &b_sliced,
                            carve::csg::V2Set *shared_edges_ptr) {
  carve::csg::VertexClassification vclass;
  carve::csg::EdgeClassification eclass;

  carve::csg::FLGroupList a_loops_grouped;
  carve::csg::FLGroupList b_loops_grouped;

  carve::csg::FaceLoopList a_face_loops;
  carve::csg::FaceLoopList b_face_loops;

  size_t a_edge_count;
  size_t b_edge_count;

  std::auto_ptr<face_rtree_t> a_rtree(face_rtree_t::construct_STR(a->faceBegin(), a->faceEnd(), 4, 4));
  std::auto_ptr<face_rtree_t> b_rtree(face_rtree_t::construct_STR(b->faceBegin(), b->faceEnd(), 4, 4));

  calc(a, a_rtree.get(), b, b_rtree.get(), vclass, eclass,a_face_loops, b_face_loops, a_edge_count, b_edge_count);

  detail::LoopEdges a_edge_map;
  detail::LoopEdges b_edge_map;
      
  makeEdgeMap(a_face_loops, a_edge_count, a_edge_map);
  makeEdgeMap(b_face_loops, b_edge_count, b_edge_map);
  
  carve::csg::V2Set shared_edges;
  
  findSharedEdges(a_edge_map, b_edge_map, shared_edges);
  
  groupFaceLoops(a, a_face_loops, a_edge_map, shared_edges, a_loops_grouped);
  groupFaceLoops(b, b_face_loops, b_edge_map, shared_edges, b_loops_grouped);

  for (carve::csg::FLGroupList::iterator
         i = a_loops_grouped.begin(), e = a_loops_grouped.end();
       i != e; ++i) {
    Collector *all = makeCollector(ALL, a, b);
    all->collect(&*i, hooks);
    a_sliced.push_back(all->done(hooks));

    delete all;
  }

  for (carve::csg::FLGroupList::iterator
         i = b_loops_grouped.begin(), e = b_loops_grouped.end();
       i != e; ++i) {
    Collector *all = makeCollector(ALL, a, b);
    all->collect(&*i, hooks);
    b_sliced.push_back(all->done(hooks));

    delete all;
  }
  if (shared_edges_ptr != NULL) {
    std::list<meshset_t *> result_list;
    result_list.insert(result_list.end(), a_sliced.begin(), a_sliced.end());
    result_list.insert(result_list.end(), b_sliced.begin(), b_sliced.end());
    returnSharedEdges(shared_edges, result_list, shared_edges_ptr);
  }
}



/** 
 * 
 * 
 */
void carve::csg::CSG::init() {
  intersections.clear();
  vertex_intersections.clear();
  vertex_pool.reset();
}
