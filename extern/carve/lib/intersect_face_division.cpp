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
#include <carve/polyline.hpp>
#include <carve/debug_hooks.hpp>
#include <carve/timing.hpp>
#include <carve/triangulator.hpp>

#include <list>
#include <set>
#include <iostream>

#include <algorithm>

#include "csg_detail.hpp"
#include "csg_data.hpp"

#include "intersect_common.hpp"



#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
void writePLY(const std::string &out_file, const carve::line::PolylineSet *lines, bool ascii);
#endif



namespace {



#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
  template<typename iter_t>
  void dumpFacesAndHoles(iter_t f_begin, iter_t f_end,
                         iter_t h_begin, iter_t h_end,
                         const std::string &fname) {
    std::cerr << "dumping " << std::distance(f_begin, f_end) << " faces, " << std::distance(h_begin, h_end) << " holes." << std::endl;
    std::map<carve::mesh::MeshSet<3>::vertex_t *, size_t> v_included;

    for (iter_t i = f_begin; i != f_end; ++i) {
      for (size_t j = 0; j < (*i).size(); ++j) {
        if (v_included.find((*i)[j]) == v_included.end()) {
          size_t &p = v_included[(*i)[j]];
          p = v_included.size() - 1;
        }
      }
    }

    for (iter_t i = h_begin; i != h_end; ++i) {
      for (size_t j = 0; j < (*i).size(); ++j) {
        if (v_included.find((*i)[j]) == v_included.end()) {
          size_t &p = v_included[(*i)[j]];
          p = v_included.size() - 1;
        }
      }
    }

    carve::line::PolylineSet fh;
    fh.vertices.resize(v_included.size());
    for (std::map<carve::mesh::MeshSet<3>::vertex_t *, size_t>::const_iterator
           i = v_included.begin(); i != v_included.end(); ++i) {
      fh.vertices[(*i).second].v = (*i).first->v;
    }

    {
      std::vector<size_t> connected;
      for (iter_t i = f_begin; i != f_end; ++i) {
        connected.clear();
        for (size_t j = 0; j < (*i).size(); ++j) {
          connected.push_back(v_included[(*i)[j]]);
        }
        fh.addPolyline(true, connected.begin(), connected.end());
      }
      for (iter_t i = h_begin; i != h_end; ++i) {
        connected.clear();
        for (size_t j = 0; j < (*i).size(); ++j) {
          connected.push_back(v_included[(*i)[j]]);
        }
        fh.addPolyline(true, connected.begin(), connected.end());
      }
    }

    ::writePLY(fname, &fh, true);
  }
#endif



  template<typename T>
  void populateVectorFromList(std::list<T> &l, std::vector<T> &v) {
    v.clear();
    v.reserve(l.size());
    for (typename std::list<T>::iterator i = l.begin(); i != l.end(); ++i) {
      v.push_back(T());
      std::swap(*i, v.back());
    }
    l.clear();
  }

  template<typename T>
  void populateListFromVector(std::vector<T> &v, std::list<T> &l) {
    l.clear();
    for (size_t i = 0; i < v.size(); ++i) {
      l.push_back(T());
      std::swap(v[i], l.back());
    }
    v.clear();
  }



  struct GraphEdge {
    GraphEdge *next;
    GraphEdge *prev;
    GraphEdge *loop_next;
    carve::mesh::MeshSet<3>::vertex_t *src;
    carve::mesh::MeshSet<3>::vertex_t *tgt;
    double ang;
    int visited;

    GraphEdge(carve::mesh::MeshSet<3>::vertex_t *_src, carve::mesh::MeshSet<3>::vertex_t *_tgt) :
      next(NULL), prev(NULL), loop_next(NULL),
      src(_src), tgt(_tgt),
      ang(0.0), visited(-1) {
    }
  };



  struct GraphEdges {
    GraphEdge *edges;
    carve::geom2d::P2 proj;

    GraphEdges() : edges(NULL), proj() {
    }
  };



  struct Graph {
    typedef std::unordered_map<carve::mesh::MeshSet<3>::vertex_t *, GraphEdges> graph_t;

    graph_t graph;

    Graph() : graph() {
    }

    ~Graph() {
      int c = 0;

      GraphEdge *edge;
      for (graph_t::iterator i = graph.begin(), e =  graph.end(); i != e; ++i) {
        edge = (*i).second.edges;
        while (edge) {
          GraphEdge *temp = edge;
          ++c;
          edge = edge->next;
          delete temp;
        }
      }

      if (c) {
        std::cerr << "warning: "
                  << c
                  << " edges should have already been removed at graph destruction time"
                  << std::endl;
      }
    }

    const carve::geom2d::P2 &projection(carve::mesh::MeshSet<3>::vertex_t *v) const {
      graph_t::const_iterator i = graph.find(v);
      CARVE_ASSERT(i != graph.end());
      return (*i).second.proj;
    }

    void computeProjection(carve::mesh::MeshSet<3>::face_t *face) {
      for (graph_t::iterator i = graph.begin(), e =  graph.end(); i != e; ++i) {
        (*i).second.proj = face->project((*i).first->v);
      }
      for (graph_t::iterator i = graph.begin(), e =  graph.end(); i != e; ++i) {
        for (GraphEdge *e = (*i).second.edges; e; e = e->next) {
          e->ang = carve::math::ANG(carve::geom2d::atan2(projection(e->tgt) - projection(e->src)));
        }
      }
    }

    void print(std::ostream &out, const carve::csg::VertexIntersections *vi) const {
      for (graph_t::const_iterator i = graph.begin(), e =  graph.end(); i != e; ++i) {
        out << (*i).first << (*i).first->v << '(' << projection((*i).first).x << ',' << projection((*i).first).y << ") :";
        for (const GraphEdge *e = (*i).second.edges; e; e = e->next) {
          out << ' ' << e->tgt << e->tgt->v << '(' << projection(e->tgt).x << ',' << projection(e->tgt).y << ')';
        }
        out << std::endl;
        if (vi) {
          carve::csg::VertexIntersections::const_iterator j = vi->find((*i).first);
          if (j != vi->end()) {
            out << "   (int) ";
            for (carve::csg::IObjPairSet::const_iterator
                   k = (*j).second.begin(), ke = (*j).second.end(); k != ke; ++k) {
              if ((*k).first < (*k).second) {
                out << (*k).first << ".." << (*k).second << "; ";
              }
            }
            out << std::endl;
          }
        }
      }
    }

    void addEdge(carve::mesh::MeshSet<3>::vertex_t *v1, carve::mesh::MeshSet<3>::vertex_t *v2) {
      GraphEdges &edges = graph[v1];
      GraphEdge *edge = new GraphEdge(v1, v2);
      if (edges.edges) edges.edges->prev = edge;
      edge->next = edges.edges;
      edges.edges = edge;
    }

    void removeEdge(GraphEdge *edge) {
      if (edge->prev != NULL) {
        edge->prev->next = edge->next;
      } else {
        if (edge->next != NULL) {
          GraphEdges &edges = (graph[edge->src]);
          edges.edges = edge->next;
        } else {
          graph.erase(edge->src);
        }
      }
      if (edge->next != NULL) {
        edge->next->prev = edge->prev;
      }
      delete edge;
    }

    bool empty() const {
      return graph.size() == 0;
    }

    GraphEdge *pickStartEdge() {
      // Try and find a vertex from which there is only one outbound edge. Won't always succeed.
      for (graph_t::iterator i = graph.begin(); i != graph.end(); ++i) {
        GraphEdges &ge = i->second;
        if (ge.edges->next == NULL) {
          return ge.edges;
        }
      }
      return (*graph.begin()).second.edges;
    }

    GraphEdge *outboundEdges(carve::mesh::MeshSet<3>::vertex_t *v) {
      return graph[v].edges;
    }
  };



  /** 
   * \brief Take a set of new edges and split a face based upon those edges.
   * 
   * @param[in] face The face to be split.
   * @param[in] edges 
   * @param[out] face_loops Output list of face loops
   * @param[out] hole_loops Output list of hole loops
   * @param vi 
   */
  static void splitFace(carve::mesh::MeshSet<3>::face_t *face,
                        const carve::csg::V2Set &edges,
                        std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > &face_loops,
                        std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > &hole_loops,
                        const carve::csg::VertexIntersections & /* vi */) {
    Graph graph;

    for (carve::csg::V2Set::const_iterator
           i = edges.begin(), e = edges.end();
         i != e;
         ++i) {
      carve::mesh::MeshSet<3>::vertex_t *v1 = ((*i).first), *v2 = ((*i).second);
      if (carve::geom::equal(v1->v, v2->v)) std::cerr << "WARNING! " << v1->v << "==" << v2->v << std::endl;
      graph.addEdge(v1, v2);
    }

    graph.computeProjection(face);

    while (!graph.empty()) {
      GraphEdge *edge;
      GraphEdge *start;
      start = edge = graph.pickStartEdge();

      edge->visited = 0;

      int len = 0;

      for (;;) {
        double in_ang = M_PI + edge->ang;
        if (in_ang > M_TWOPI) in_ang -= M_TWOPI;

        GraphEdge *opts;
        GraphEdge *out = NULL;
        double best = M_TWOPI + 1.0;

        for (opts = graph.outboundEdges(edge->tgt); opts; opts = opts->next) {
          if (opts->tgt == edge->src) {
            if (out == NULL && opts->next == NULL) out = opts;
          } else {
            double out_ang = carve::math::ANG(in_ang - opts->ang);

            if (out == NULL || out_ang < best) {
              out = opts;
              best = out_ang;
            }
          }
        }

        CARVE_ASSERT(out != NULL);

        edge->loop_next = out;

        if (out->visited >= 0) {
          while (start != out) {
            GraphEdge *e = start;
            start = start->loop_next;
            e->loop_next = NULL;
            e->visited = -1;
          }
          len = edge->visited - out->visited + 1;
          break;
        }

        out->visited = edge->visited + 1;
        edge = out;
      }

      std::vector<carve::mesh::MeshSet<3>::vertex_t *> loop(len);
      std::vector<carve::geom2d::P2> projected(len);

      edge = start;
      for (int i = 0; i < len; ++i) {
        GraphEdge *next = edge->loop_next;
        loop[i] = edge->src;
        projected[i] = graph.projection(edge->src);
        graph.removeEdge(edge);
        edge = next;
      }

      CARVE_ASSERT(edge == start);

      if (carve::geom2d::signedArea(projected) < 0) {
        face_loops.push_back(std::vector<carve::mesh::MeshSet<3>::vertex_t *>());
        face_loops.back().swap(loop);
      } else {
        hole_loops.push_back(std::vector<carve::mesh::MeshSet<3>::vertex_t *>());
        hole_loops.back().swap(loop);
      }
    }
  }



  /** 
   * \brief Determine the relationship between a face loop and a hole loop.
   * 
   * Determine whether a face and hole share an edge, or a vertex,
   * or do not touch. Find a hole vertex that is not part of the
   * face, and a hole,face vertex pair that are coincident, if such
   * a pair exists.
   *
   * @param[in] f A face loop.
   * @param[in] f_sort A vector indexing \a f in address order
   * @param[in] h A hole loop.
   * @param[in] h_sort A vector indexing \a h in address order
   * @param[out] f_idx Index of a face vertex that is shared with the hole.
   * @param[out] h_idx Index of the hole vertex corresponding to \a f_idx.
   * @param[out] unmatched_h_idx Index of a hole vertex that is not part of the face.
   * @param[out] shares_vertex Boolean indicating that the face and the hole share a vertex.
   * @param[out] shares_edge Boolean indicating that the face and the hole share an edge.
   */
  static void compareFaceLoopAndHoleLoop(const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &f,
                                         const std::vector<unsigned> &f_sort,
                                         const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &h,
                                         const std::vector<unsigned> &h_sort,
                                         unsigned &f_idx,
                                         unsigned &h_idx,
                                         int &unmatched_h_idx,
                                         bool &shares_vertex,
                                         bool &shares_edge) {
    const size_t F = f.size();
    const size_t H = h.size();

    shares_vertex = shares_edge = false;
    unmatched_h_idx = -1;

    unsigned I, J;
    for (I = J = 0; I < F && J < H;) {
      unsigned i = f_sort[I], j = h_sort[J];
      if (f[i] == h[j]) {
        shares_vertex = true;
        f_idx = i;
        h_idx = j;
        if (f[(i + F - 1) % F] == h[(j + 1) % H]) {
          shares_edge = true;
        }
        carve::mesh::MeshSet<3>::vertex_t *t = f[i];
        do { ++I; } while (I < F && f[f_sort[I]] == t);
        do { ++J; } while (J < H && h[h_sort[J]] == t);
      } else if (f[i] < h[j]) {
        ++I;
      } else {
        unmatched_h_idx = j;
        ++J;
      }
    }
    if (J < H) {
      unmatched_h_idx = h_sort[J];
    }
  }



  /** 
   * \brief Compute an embedding for a set of face loops and hole loops.
   *
   * Because face and hole loops may be contained within each other,
   * it must be determined which hole loops are directly contained
   * within a face loop.
   * 
   * @param[in] face The face from which these face and hole loops derive.
   * @param[in] face_loops 
   * @param[in] hole_loops 
   * @param[out] containing_faces     A vector which for each hole loop
   *                                  lists the indices of the face
   *                                  loops it is containined in.
   * @param[out] hole_shared_vertices A map from a face,hole pair to
   *                                  a shared vertex pair.
   */
  static void computeContainment(carve::mesh::MeshSet<3>::face_t *face,
                                 std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > &face_loops,
                                 std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > &hole_loops,
                                 std::vector<std::vector<int> > &containing_faces,
                                 std::map<int, std::map<int, std::pair<unsigned, unsigned> > > &hole_shared_vertices) {
#if defined(CARVE_DEBUG)
    std::cerr << "input: "
              << face_loops.size() << "faces, "
              << hole_loops.size() << "holes."
              << std::endl;
#endif

    std::vector<std::vector<carve::geom2d::P2> > face_loops_projected, hole_loops_projected;
    std::vector<carve::geom::aabb<2> > face_loop_aabb, hole_loop_aabb;
    std::vector<std::vector<unsigned> > face_loops_sorted, hole_loops_sorted;

    std::vector<double> face_loop_areas, hole_loop_areas;

    face_loops_projected.resize(face_loops.size());
    face_loops_sorted.resize(face_loops.size());
    face_loop_aabb.resize(face_loops.size());
    face_loop_areas.resize(face_loops.size());

    hole_loops_projected.resize(hole_loops.size());
    hole_loops_sorted.resize(hole_loops.size());
    hole_loop_aabb.resize(hole_loops.size());
    hole_loop_areas.resize(hole_loops.size());

    // produce a projection of each face loop onto a 2D plane, and an
    // index vector which sorts vertices by address.
    for (size_t m = 0; m < face_loops.size(); ++m) {
      const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &f_loop = (face_loops[m]);
      face_loops_projected[m].reserve(f_loop.size());
      face_loops_sorted[m].reserve(f_loop.size());
      for (size_t n = 0; n < f_loop.size(); ++n) {
        face_loops_projected[m].push_back(face->project(f_loop[n]->v));
        face_loops_sorted[m].push_back(n);
      }
      face_loop_areas.push_back(carve::geom2d::signedArea(face_loops_projected[m]));

      std::sort(face_loops_sorted[m].begin(), face_loops_sorted[m].end(), 
                carve::make_index_sort(face_loops[m].begin()));
      face_loop_aabb[m].fit(face_loops_projected[m].begin(), face_loops_projected[m].end());
    }

    // produce a projection of each hole loop onto a 2D plane, and an
    // index vector which sorts vertices by address.
    for (size_t m = 0; m < hole_loops.size(); ++m) {
      const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &h_loop = (hole_loops[m]);
      hole_loops_projected[m].reserve(h_loop.size());
      hole_loops_projected[m].reserve(h_loop.size());
      for (size_t n = 0; n < h_loop.size(); ++n) {
        hole_loops_projected[m].push_back(face->project(h_loop[n]->v));
        hole_loops_sorted[m].push_back(n);
      }
      hole_loop_areas.push_back(carve::geom2d::signedArea(hole_loops_projected[m]));

      std::sort(hole_loops_sorted[m].begin(), hole_loops_sorted[m].end(), 
                carve::make_index_sort(hole_loops[m].begin()));
      hole_loop_aabb[m].fit(hole_loops_projected[m].begin(), hole_loops_projected[m].end());
    }

    containing_faces.resize(hole_loops.size());

    for (unsigned i = 0; i < hole_loops.size(); ++i) {

      for (unsigned j = 0; j < face_loops.size(); ++j) {
        if (!face_loop_aabb[j].completelyContains(hole_loop_aabb[i])) {
#if defined(CARVE_DEBUG)
          std::cerr << "face: " << j
                    << " hole: " << i
                    << " skipped test (aabb fail)"
                    << std::endl;
#endif
          continue;
        }

        unsigned f_idx, h_idx;
        int unmatched_h_idx;
        bool shares_vertex, shares_edge;
        compareFaceLoopAndHoleLoop(face_loops[j],
                                   face_loops_sorted[j],
                                   hole_loops[i],
                                   hole_loops_sorted[i],
                                   f_idx, h_idx,
                                   unmatched_h_idx,
                                   shares_vertex,
                                   shares_edge);

#if defined(CARVE_DEBUG)
        std::cerr << "face: " << j
                  << " hole: " << i
                  << " shares_vertex: " << shares_vertex
                  << " shares_edge: " << shares_edge
                  << std::endl;
#endif

        carve::geom3d::Vector test = hole_loops[i][0]->v;
        carve::geom2d::P2 test_p = face->project(test);

        if (shares_vertex) {
          hole_shared_vertices[i][j] = std::make_pair(h_idx, f_idx);
          // Hole touches face. Should be able to connect it up
          // trivially. Still need to record its containment, so that
          // the assignment below works.
          if (unmatched_h_idx != -1) {
#if defined(CARVE_DEBUG)
            std::cerr << "using unmatched vertex: " << unmatched_h_idx << std::endl;
#endif
            test = hole_loops[i][unmatched_h_idx]->v;
            test_p = face->project(test);
          } else {
            // XXX: hole shares ALL vertices with face. Pick a point
            // internal to the projected poly.
            if (shares_edge) {
              // Hole shares edge with face => face can't contain hole.
              continue;
            }

            // XXX: how is this possible? Doesn't share an edge, but
            // also doesn't have any vertices that are not in
            // common. Degenerate hole?

            // XXX: come up with a test case for this.
            CARVE_FAIL("implement me");
          }
        }


        // XXX: use loop area to avoid some point-in-poly tests? Loop
        // area is faster, but not sure which is more robust.
        if (carve::geom2d::pointInPolySimple(face_loops_projected[j], test_p)) {
#if defined(CARVE_DEBUG)
          std::cerr << "contains: " << i << " - " << j << std::endl;
#endif
          containing_faces[i].push_back(j);
        } else {
#if defined(CARVE_DEBUG)
          std::cerr << "does not contain: " << i << " - " << j << std::endl;
#endif
        }
      }

#if defined(CARVE_DEBUG)
      if (containing_faces[i].size() == 0) {
        //HOOK(drawFaceLoopWireframe(hole_loops[i], face->normal, 1.0, 0.0, 0.0, 1.0););
        std::cerr << "hole loop: ";
        for (unsigned j = 0; j < hole_loops[i].size(); ++j) {
          std::cerr << " " << hole_loops[i][j] << ":" << hole_loops[i][j]->v;
        }
        std::cerr << std::endl;
        for (unsigned j = 0; j < face_loops.size(); ++j) {
          //HOOK(drawFaceLoopWireframe(face_loops[j], face->normal, 0.0, 1.0, 0.0, 1.0););
        }
      }
#endif

      // CARVE_ASSERT(containing_faces[i].size() >= 1);
    }
  }



  /** 
   * \brief Merge face loops and hole loops to produce a set of face loops without holes.
   * 
   * @param[in] face The face from which these face loops derive.
   * @param[in,out] f_loops A list of face loops.
   * @param[in] h_loops A list of hole loops to be incorporated into face loops.
   */
  static void mergeFacesAndHoles(carve::mesh::MeshSet<3>::face_t *face,
                                 std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > &f_loops,
                                 std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > &h_loops,
                                 carve::csg::CSG::Hooks & /* hooks */) {
    std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > face_loops;
    std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > hole_loops;

    std::vector<std::vector<int> > containing_faces;
    std::map<int, std::map<int, std::pair<unsigned, unsigned> > > hole_shared_vertices;

#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
    dumpFacesAndHoles(f_loops.begin(), f_loops.end(), h_loops.begin(), h_loops.end(), "/tmp/pre_merge.ply");
#endif

    {
      // move input face and hole loops to temp vectors.
      size_t m;
      face_loops.resize(f_loops.size());
      m = 0;
      for (std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> >::iterator
             i = f_loops.begin(), ie = f_loops.end();
           i != ie;
           ++i, ++m) {
        face_loops[m].swap((*i));
      }

      hole_loops.resize(h_loops.size());
      m = 0;
      for (std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> >::iterator
             i = h_loops.begin(), ie = h_loops.end();
           i != ie;
           ++i, ++m) {
        hole_loops[m].swap((*i));
      }
      f_loops.clear();
      h_loops.clear();
    }

    // work out the embedding of holes and faces.
    computeContainment(face, face_loops, hole_loops, containing_faces, hole_shared_vertices);

    int unassigned = (int)hole_loops.size();

    std::vector<std::vector<int> > face_holes;
    face_holes.resize(face_loops.size());

    for (unsigned i = 0; i < containing_faces.size(); ++i) {
      if (containing_faces[i].size() == 0) {
        std::map<int, std::map<int, std::pair<unsigned, unsigned> > >::iterator it = hole_shared_vertices.find(i);
        if (it != hole_shared_vertices.end()) {
          std::map<int, std::pair<unsigned, unsigned> >::iterator it2 = (*it).second.begin();
          int f = (*it2).first;
          unsigned h_idx = (*it2).second.first;
          unsigned f_idx = (*it2).second.second;

          // patch the hole into the face directly. because
          // f_loop[f_idx] == h_loop[h_idx], we don't need to
          // duplicate the f_loop vertex.

          std::vector<carve::mesh::MeshSet<3>::vertex_t *> &f_loop = face_loops[f];
          std::vector<carve::mesh::MeshSet<3>::vertex_t *> &h_loop = hole_loops[i];

          f_loop.insert(f_loop.begin() + f_idx + 1, h_loop.size(), NULL);

          unsigned p = f_idx + 1;
          for (unsigned a = h_idx + 1; a < h_loop.size(); ++a, ++p) {
            f_loop[p] = h_loop[a];
          }
          for (unsigned a = 0; a <= h_idx; ++a, ++p) {
            f_loop[p] = h_loop[a];
          }

#if defined(CARVE_DEBUG)
          std::cerr << "hook face " << f << " to hole " << i << "(vertex)" << std::endl;
#endif
        } else {
          std::cerr << "uncontained hole loop does not share vertices with any face loop!" << std::endl;
        }
        unassigned--;
      }
    }


    // work out which holes are directly contained within which faces.
    while (unassigned) {
      std::set<int> removed;

      for (unsigned i = 0; i < containing_faces.size(); ++i) {
        if (containing_faces[i].size() == 1) {
          int f = containing_faces[i][0];
          face_holes[f].push_back(i);
#if defined(CARVE_DEBUG)
          std::cerr << "hook face " << f << " to hole " << i << std::endl;
#endif
          removed.insert(f);
          unassigned--;
        }
      }
      for (std::set<int>::iterator f = removed.begin(); f != removed.end(); ++f) {
        for (unsigned i = 0; i < containing_faces.size(); ++i) {
          containing_faces[i].erase(std::remove(containing_faces[i].begin(),
                                                containing_faces[i].end(),
                                                *f),
                                    containing_faces[i].end());
        }
      }
    }

#if 0
    // use old templated projection code to patch holes into faces.
    for (unsigned i = 0; i < face_loops.size(); ++i) {
      std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > face_hole_loops;
      face_hole_loops.resize(face_holes[i].size());
      for (unsigned j = 0; j < face_holes[i].size(); ++j) {
        face_hole_loops[j].swap(hole_loops[face_holes[i][j]]);
      }
      if (face_hole_loops.size()) {

        f_loops.push_back(carve::triangulate::incorporateHolesIntoPolygon(
          carve::mesh::MeshSet<3>::face_t::projection_mapping(face->project),
          face_loops[i],
          face_hole_loops));
      } else {
        f_loops.push_back(face_loops[i]);
      }
    }

#else
    // use new 2d-only hole patching code.
    for (size_t i = 0; i < face_loops.size(); ++i) {
      if (!face_holes[i].size()) {
        f_loops.push_back(face_loops[i]);
        continue;
      }

      std::vector<std::vector<carve::geom2d::P2> > projected_poly;
      projected_poly.resize(face_holes[i].size() + 1);
      projected_poly[0].reserve(face_loops[i].size());
      for (size_t j = 0; j < face_loops[i].size(); ++j) {
        projected_poly[0].push_back(face->project(face_loops[i][j]->v));
      }
      for (size_t j = 0; j < face_holes[i].size(); ++j) {
        projected_poly[j+1].reserve(hole_loops[face_holes[i][j]].size());
        for (size_t k = 0; k < hole_loops[face_holes[i][j]].size(); ++k) {
          projected_poly[j+1].push_back(face->project(hole_loops[face_holes[i][j]][k]->v));
        }
      }

      std::vector<std::pair<size_t, size_t> > result = carve::triangulate::incorporateHolesIntoPolygon(projected_poly);

      f_loops.push_back(std::vector<carve::mesh::MeshSet<3>::vertex_t *>());
      std::vector<carve::mesh::MeshSet<3>::vertex_t *> &out = f_loops.back();
      out.reserve(result.size());
      for (size_t j = 0; j < result.size(); ++j) {
        if (result[j].first == 0) {
          out.push_back(face_loops[i][result[j].second]);
        } else {
          out.push_back(hole_loops[face_holes[i][result[j].first-1]][result[j].second]);
        }
      }
    }
#endif
#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
    dumpFacesAndHoles(f_loops.begin(), f_loops.end(), h_loops.begin(), h_loops.end(), "/tmp/post_merge.ply");
#endif

  }



  /** 
   * \brief Assemble the base loop for a face.
   *
   * The base loop is the original face loop, including vertices
   * created by intersections crossing any of its edges.
   * 
   * @param[in] face The face to process.
   * @param[in] vmap 
   * @param[in] face_split_edges 
   * @param[in] divided_edges A mapping from edge pointer to sets of
   *            ordered vertices corrsponding to the intersection points
   *            on that edge.
   * @param[out] base_loop A vector of the vertices of the base loop.
   */
  static bool assembleBaseLoop(carve::mesh::MeshSet<3>::face_t *face,
                               const carve::csg::detail::Data &data,
                               std::vector<carve::mesh::MeshSet<3>::vertex_t *> &base_loop,
                               carve::csg::CSG::Hooks &hooks) {
    base_loop.clear();

    // XXX: assumes that face->edges is in the same order as
    // face->vertices. (Which it is)
    carve::mesh::MeshSet<3>::edge_t *e = face->edge;
    size_t e_idx = 0;
    bool face_edge_intersected = false;
    do {
      base_loop.push_back(carve::csg::map_vertex(data.vmap, e->vert));

      carve::csg::detail::EVVMap::const_iterator ev = data.divided_edges.find(e);

      if (ev != data.divided_edges.end()) {
        const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &ev_vec = ((*ev).second);

        for (size_t k = 0, ke = ev_vec.size(); k < ke;) {
          base_loop.push_back(ev_vec[k++]);
        }

        if (ev_vec.size() && hooks.hasHook(carve::csg::CSG::Hooks::EDGE_DIVISION_HOOK)) {
          carve::mesh::MeshSet<3>::vertex_t *v1 = e->vert;
          carve::mesh::MeshSet<3>::vertex_t *v2;
          for (size_t k = 0, ke = ev_vec.size(); k < ke;) {
            v2 = ev_vec[k++];
            hooks.edgeDivision(e, e_idx, v1, v2);
            v1 = v2;
          }
          v2 = e->v2();
          hooks.edgeDivision(e, e_idx, v1, v2);
        }

        face_edge_intersected = true;
      }
      e = e->next;
      ++e_idx;
    } while (e != face->edge);

    return face_edge_intersected;
  }



  // the crossing_data structure holds temporary information regarding
  // paths, and their relationship to the loop of edges that forms the
  // face perimeter.
  struct crossing_data {
    std::vector<carve::mesh::MeshSet<3>::vertex_t *> *path;
    size_t edge_idx[2];

    crossing_data(std::vector<carve::mesh::MeshSet<3>::vertex_t *> *p, size_t e1, size_t e2) : path(p) {
      edge_idx[0] = e1; edge_idx[1] = e2;
    }

    bool operator<(const crossing_data &c) const {
      // the sort order for paths is in order of increasing initial
      // position on the edge loop, but decreasing final position.
      return edge_idx[0] < c.edge_idx[0] || (edge_idx[0] == c.edge_idx[0] && edge_idx[1] > c.edge_idx[1]);
    }
  };



  bool processCrossingEdges(carve::mesh::MeshSet<3>::face_t *face,
                            const carve::csg::VertexIntersections &vertex_intersections,
                            carve::csg::CSG::Hooks &hooks,
                            std::vector<carve::mesh::MeshSet<3>::vertex_t *> &base_loop,
                            std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > &paths,
                            std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > &face_loops_out) {
    const size_t N = base_loop.size();
    std::vector<crossing_data> endpoint_indices;

    endpoint_indices.reserve(paths.size());

    for (size_t i = 0; i < paths.size(); ++i) {
      endpoint_indices.push_back(crossing_data(&paths[i], N, N));
    }

    // Step 1:
    // locate endpoints of paths on the base loop.
    for (size_t i = 0; i < N; ++i) {
      for (size_t j = 0; j < paths.size(); ++j) {
        // test beginning of path.
        if (paths[j].front() == base_loop[i]) {
          if (endpoint_indices[j].edge_idx[0] == N) {
            endpoint_indices[j].edge_idx[0] = i;
          } else {
            // there is a duplicated vertex in the face perimeter. The
            // path might attach to either of the duplicate instances
            // so we have to work out which is the right one to attach
            // to. We assume it's the index currently being examined,
            // if the path heads in a direction that's internal to the
            // angle made by the prior and next edges of the face
            // perimeter. Otherwise, leave it as the currently
            // selected index (until another duplicate is found, if it
            // exists, and is tested).
            const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &p = *endpoint_indices[j].path;
            const size_t pN = p.size();

            carve::mesh::MeshSet<3>::vertex_t *a, *b, *c;
            a = base_loop[(i+N-1)%N];
            b = base_loop[i];
            c = base_loop[(i+1)%N];

            carve::mesh::MeshSet<3>::vertex_t *adj = (p[0] == base_loop[i]) ? p[1] : p[pN-2];

            if (carve::geom2d::internalToAngle(face->project(c->v),
                                               face->project(b->v),
                                               face->project(a->v),
                                               face->project(adj->v))) {
              endpoint_indices[j].edge_idx[0] = i;
            }
          }
        }

        // test end of path.
        if (paths[j].back() == base_loop[i]) {
          if (endpoint_indices[j].edge_idx[1] == N) {
            endpoint_indices[j].edge_idx[1] = i;
          } else {
            // Work out which of the duplicated vertices is the right
            // one to attach to, as above.
            const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &p = *endpoint_indices[j].path;
            const size_t pN = p.size();

            carve::mesh::MeshSet<3>::vertex_t *a, *b, *c;
            a = base_loop[(i+N-1)%N];
            b = base_loop[i];
            c = base_loop[(i+1)%N];

            carve::mesh::MeshSet<3>::vertex_t *adj = (p[0] == base_loop[i]) ? p[1] : p[pN-2];

            if (carve::geom2d::internalToAngle(face->project(c->v),
                                               face->project(b->v),
                                               face->project(a->v),
                                               face->project(adj->v))) {
              endpoint_indices[j].edge_idx[1] = i;
            }
          }
        }
      }
    }

#if defined(CARVE_DEBUG)
    std::cerr << "### N: " << N << std::endl;
    for (size_t i = 0; i < paths.size(); ++i) {
      std::cerr << "### path: " << i << " endpoints: " << endpoint_indices[i].edge_idx[0] << " - " << endpoint_indices[i].edge_idx[1] << std::endl;
    }
#endif


    // Step 2:
    // divide paths up into those that connect to the base loop in two
    // places (cross), and those that do not (noncross).
    std::vector<crossing_data> cross, noncross;
    cross.reserve(endpoint_indices.size() + 1);
    noncross.reserve(endpoint_indices.size());

    for (size_t i = 0; i < endpoint_indices.size(); ++i) {
#if defined(CARVE_DEBUG)
      std::cerr << "### orienting path: " << i << " endpoints: " << endpoint_indices[i].edge_idx[0] << " - " << endpoint_indices[i].edge_idx[1] << std::endl;
#endif
      if (endpoint_indices[i].edge_idx[0] != N && endpoint_indices[i].edge_idx[1] != N) {
        // Orient each path correctly. Paths should progress from
        // smaller perimeter index to larger, but if the path starts
        // and ends at the same perimeter index, then the decision
        // needs to be made based upon area.
        if (endpoint_indices[i].edge_idx[0] == endpoint_indices[i].edge_idx[1]) {
          // The path forms a loop that starts and ends at the same
          // vertex of the perimeter. In this case, we need to orient
          // the path so that the constructed loop has the right
          // signed area.
          double area = carve::geom2d::signedArea(endpoint_indices[i].path->begin() + 1,
                                                  endpoint_indices[i].path->end(),
                                                  carve::mesh::MeshSet<3>::face_t::projection_mapping(face->project));
          if (area < 0) {
            // XXX: Create test case to check that this is the correct sign for the area.
            std::reverse(endpoint_indices[i].path->begin(), endpoint_indices[i].path->end());
          }
        } else {
          if (endpoint_indices[i].edge_idx[0] > endpoint_indices[i].edge_idx[1]) {
            std::swap(endpoint_indices[i].edge_idx[0], endpoint_indices[i].edge_idx[1]);
            std::reverse(endpoint_indices[i].path->begin(), endpoint_indices[i].path->end());
          }
        }
      }

      if (endpoint_indices[i].edge_idx[0] != N &&
          endpoint_indices[i].edge_idx[1] != N &&
          endpoint_indices[i].edge_idx[0] != endpoint_indices[i].edge_idx[1]) {
        cross.push_back(endpoint_indices[i]);
      } else {
        noncross.push_back(endpoint_indices[i]);
      }
    }

    // Step 3:
    // add a temporary crossing path that connects the beginning and the
    // end of the base loop. this stops us from needing special case
    // code to handle the left over loop after all the other crossing
    // paths are considered.
    std::vector<carve::mesh::MeshSet<3>::vertex_t *> base_loop_temp_path;
    base_loop_temp_path.reserve(2);
    base_loop_temp_path.push_back(base_loop.front());
    base_loop_temp_path.push_back(base_loop.back());

    cross.push_back(crossing_data(&base_loop_temp_path, 0, base_loop.size() - 1));
#if defined(CARVE_DEBUG)
    std::cerr << "### crossing edge count (with sentinel): " << cross.size() << std::endl;
#endif

    // Step 4:
    // sort paths by increasing beginning point and decreasing ending point.
    std::sort(cross.begin(), cross.end());
    std::sort(noncross.begin(), noncross.end());

    // Step 5:
    // divide up the base loop based upon crossing paths.
    std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > divided_base_loop;
    divided_base_loop.reserve(cross.size());
    std::vector<carve::mesh::MeshSet<3>::vertex_t *> out;

    for (size_t i = 0; i < cross.size(); ++i) {
      size_t j;
      for (j = i + 1;
           j < cross.size() &&
             cross[i].edge_idx[0] == cross[j].edge_idx[0] && 
             cross[i].edge_idx[1] == cross[j].edge_idx[1];
           ++j) {}
      if (j - i >= 2) {
        // when there are multiple paths that begin and end at the
        // same point, they need to be ordered so that the constructed
        // loops have the right orientation. this means that the loop
        // made by taking path(i+1) forward, then path(i) backward
        // needs to have negative area. this combined area is equal to
        // the area of path(i+1) minus the area of path(i). in turn
        // this means that the loop made by path path(i+1) alone has
        // to have smaller signed area than loop made by path(i).
        // thus, we sort paths in order of decreasing area.

        std::vector<std::pair<double, std::vector<carve::mesh::MeshSet<3>::vertex_t *> *> > order;
        order.reserve(j - i);
        for (size_t k = i; k < j; ++k) {
          double area = carve::geom2d::signedArea(cross[k].path->begin(),
                                                  cross[k].path->end(),
                                                  carve::mesh::MeshSet<3>::face_t::projection_mapping(face->project));
#if defined(CARVE_DEBUG)
          std::cerr << "### k=" << k << " area=" << area << std::endl;
#endif
          order.push_back(std::make_pair(-area, cross[k].path));
        }
        std::sort(order.begin(), order.end());
        for (size_t k = i; k < j; ++k) {
          cross[k].path = order[k-i].second;
#if defined(CARVE_DEBUG)
          std::cerr << "### post-sort k=" << k << " cross[k].path->size()=" << cross[k].path->size() << std::endl;
#endif
        }
      }
    }

    // Step 6:
    for (size_t i = 0; i < cross.size(); ++i) {
#if defined(CARVE_DEBUG)
      std::cerr << "### i=" << i << " working on edge: " << cross[i].edge_idx[0] << " - " << cross[i].edge_idx[1] << std::endl;
#endif
      size_t e1_0 = cross[i].edge_idx[0];
      size_t e1_1 = cross[i].edge_idx[1];
      std::vector<carve::mesh::MeshSet<3>::vertex_t *> &p1 = *cross[i].path;
#if defined(CARVE_DEBUG)
      std::cerr << "###     path size = " << p1.size() << std::endl;
#endif

      out.clear();

      if (i < cross.size() - 1 &&
          cross[i+1].edge_idx[1] <= cross[i].edge_idx[1]) {
#if defined(CARVE_DEBUG)
        std::cerr << "###     complex case" << std::endl;
#endif
        // complex case. crossing path with other crossing paths embedded within.
        size_t pos = e1_0;

        size_t skip = i+1;

        while (pos != e1_1) {

          std::vector<carve::mesh::MeshSet<3>::vertex_t *> &p2 = *cross[skip].path;
          size_t e2_0 = cross[skip].edge_idx[0];
          size_t e2_1 = cross[skip].edge_idx[1];

          // copy up to the beginning of the next path.
          std::copy(base_loop.begin() + pos, base_loop.begin() + e2_0, std::back_inserter(out));

          CARVE_ASSERT(base_loop[e2_0] == p2[0]);
          // copy the next path in the right direction.
          std::copy(p2.begin(), p2.end() - 1, std::back_inserter(out));

          // move to the position of the end of the path.
          pos = e2_1;

          // advance to the next hit path.
          do {
            ++skip;
          } while(skip != cross.size() && cross[skip].edge_idx[0] < e2_1);

          if (skip == cross.size()) break;

          // if the next hit path is past the start point of the current path, we're done.
          if (cross[skip].edge_idx[0] >= e1_1) break;
        }

        // copy up to the end of the path.
        std::copy(base_loop.begin() + pos, base_loop.begin() + e1_1, std::back_inserter(out));

        CARVE_ASSERT(base_loop[e1_1] == p1.back());
        std::copy(p1.rbegin(), p1.rend() - 1, std::back_inserter(out));
      } else {
        size_t loop_size = (e1_1 - e1_0) + (p1.size() - 1);
        out.reserve(loop_size);

        std::copy(base_loop.begin() + e1_0, base_loop.begin() + e1_1, std::back_inserter(out));
        std::copy(p1.rbegin(), p1.rend() - 1, std::back_inserter(out));

        CARVE_ASSERT(out.size() == loop_size);
      }
      divided_base_loop.push_back(out);

#if defined(CARVE_DEBUG)
      {
        std::vector<carve::geom2d::P2> projected;
        projected.reserve(out.size());
        for (size_t n = 0; n < out.size(); ++n) {
          projected.push_back(face->project(out[n]->v));
        }

        double A = carve::geom2d::signedArea(projected);
        std::cerr << "### out area=" << A << std::endl;
        CARVE_ASSERT(A <= 0);
      }
#endif
    }

    if (!noncross.size()) {
      // If there are no non-crossing paths then we're done.
      populateListFromVector(divided_base_loop, face_loops_out);
      return true;
    }

    // for each divided base loop, work out which noncrossing paths and
    // loops are part of it. use the old algorithm to combine these into
    // the divided base loop. if none, the divided base loop is just
    // output.
    std::vector<std::vector<carve::geom2d::P2> > proj;
    std::vector<carve::geom::aabb<2> > proj_aabb;
    proj.resize(divided_base_loop.size());
    proj_aabb.resize(divided_base_loop.size());

    // calculate an aabb for each divided base loop, to avoid expensive
    // point-in-poly tests.
    for (size_t i = 0; i < divided_base_loop.size(); ++i) {
      proj[i].reserve(divided_base_loop[i].size());
      for (size_t j = 0; j < divided_base_loop[i].size(); ++j) {
        proj[i].push_back(face->project(divided_base_loop[i][j]->v));
      }
      proj_aabb[i].fit(proj[i].begin(), proj[i].end());
    }

    for (size_t i = 0; i < divided_base_loop.size(); ++i) {
      std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> *> inc;
      carve::geom2d::P2 test;

      // for each noncrossing path, choose an endpoint that isn't on the
      // base loop as a test point.
      for (size_t j = 0; j < noncross.size(); ++j) {
        if (noncross[j].edge_idx[0] < N) {
          if (noncross[j].path->front() == base_loop[noncross[j].edge_idx[0]]) {
            // noncrossing paths may be loops that run from the edge, back to the same vertex.
            if (noncross[j].path->front() == noncross[j].path->back()) {
              CARVE_ASSERT(noncross[j].path->size() > 2);
              test = face->project((*noncross[j].path)[1]->v);
            } else {
              test = face->project(noncross[j].path->back()->v);
            }
          } else {
            test = face->project(noncross[j].path->front()->v);
          }
        } else {
          test = face->project(noncross[j].path->front()->v);
        }

        if (proj_aabb[i].intersects(test) &&
            carve::geom2d::pointInPoly(proj[i], test).iclass != carve::POINT_OUT) {
          inc.push_back(noncross[j].path);
        }
      }

#if defined(CARVE_DEBUG)
      std::cerr << "### divided base loop:" << i << " inc.size()=" << inc.size() << std::endl;
      std::cerr << "### inc = [";
      for (size_t j = 0; j < inc.size(); ++j) {
        std::cerr << " " << inc[j];
      }
      std::cerr << " ]" << std::endl;
#endif

      if (inc.size()) {
        carve::csg::V2Set face_edges;

        for (size_t j = 0; j < divided_base_loop[i].size() - 1; ++j) {
          face_edges.insert(std::make_pair(divided_base_loop[i][j],
                                           divided_base_loop[i][j+1]));
        }

        face_edges.insert(std::make_pair(divided_base_loop[i].back(),
                                         divided_base_loop[i].front()));

        for (size_t j = 0; j < inc.size(); ++j) {
          std::vector<carve::mesh::MeshSet<3>::vertex_t *> &path = *inc[j];
          for (size_t k = 0; k < path.size() - 1; ++k) {
            face_edges.insert(std::make_pair(path[k], path[k+1]));
            face_edges.insert(std::make_pair(path[k+1], path[k]));
          }
        }

        std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > face_loops;
        std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > hole_loops;

        splitFace(face, face_edges, face_loops, hole_loops, vertex_intersections);

        if (hole_loops.size()) {
          mergeFacesAndHoles(face, face_loops, hole_loops, hooks);
        }
        std::copy(face_loops.begin(), face_loops.end(), std::back_inserter(face_loops_out));
      } else {
        face_loops_out.push_back(divided_base_loop[i]);
      }
    }
    return true;
  }



  void composeEdgesIntoPaths(const carve::csg::V2Set &edges,
                             const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &extra_endpoints,
                             std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > &paths,
                             std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > &cuts,
                             std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > &loops) {
    using namespace carve::csg;

    detail::VVSMap vertex_graph;
    detail::VSet endpoints;
    detail::VSet cut_endpoints;

    typedef std::vector<carve::mesh::MeshSet<3>::vertex_t *> vvec_t;
    vvec_t path;

    std::list<vvec_t> path_list, cut_list, loop_list;

    // build graph from edges.
    for (V2Set::const_iterator i = edges.begin(); i != edges.end(); ++i) {
#if defined(CARVE_DEBUG)
      std::cerr << "###    edge: " << (*i).first << " - " << (*i).second << std::endl;
#endif
      vertex_graph[(*i).first].insert((*i).second);
      vertex_graph[(*i).second].insert((*i).first);
    }

    // find the endpoints in the graph.
    // every vertex with number of incident edges != 2 is an endpoint.
    for (detail::VVSMap::const_iterator i = vertex_graph.begin(); i != vertex_graph.end(); ++i) {
      if ((*i).second.size() != 2) {
#if defined(CARVE_DEBUG)
        std::cerr << "###    endpoint: " << (*i).first << std::endl;
#endif
        endpoints.insert((*i).first);
        if ((*i).second.size() == 1) {
          cut_endpoints.insert((*i).first);
        }
      }
    }

    // every vertex on the perimeter of the face is also an endpoint.
    for (size_t i = 0; i < extra_endpoints.size(); ++i) {
      if (vertex_graph.find(extra_endpoints[i]) != vertex_graph.end()) {
#if defined(CARVE_DEBUG)
        std::cerr << "###    extra endpoint: " << extra_endpoints[i] << std::endl;
#endif
        endpoints.insert(extra_endpoints[i]);
        cut_endpoints.erase(extra_endpoints[i]);
      }
    }

    while (endpoints.size()) {
      carve::mesh::MeshSet<3>::vertex_t *v = *endpoints.begin();
      detail::VVSMap::iterator p = vertex_graph.find(v);
      if (p == vertex_graph.end()) {
        endpoints.erase(endpoints.begin());
        continue;
      }

      path.clear();
      path.push_back(v);

      for (;;) {
        CARVE_ASSERT(p != vertex_graph.end());

        // pick a connected vertex to move to.
        if ((*p).second.size() == 0) break;

        carve::mesh::MeshSet<3>::vertex_t *n = *((*p).second.begin());
        detail::VVSMap::iterator q = vertex_graph.find(n);

        // remove the link.
        (*p).second.erase(n);
        (*q).second.erase(v);

        // move on.
        v = n;
        path.push_back(v);

        if ((*p).second.size() == 0) vertex_graph.erase(p);
        if ((*q).second.size() == 0) {
          vertex_graph.erase(q);
          q = vertex_graph.end();
        }

        p = q;

        if (v == path[0] || p == vertex_graph.end() || endpoints.find(v) != endpoints.end()) break;
      }
      CARVE_ASSERT(endpoints.find(path.back()) != endpoints.end());

      bool is_cut =
        cut_endpoints.find(path.front()) != cut_endpoints.end() &&
        cut_endpoints.find(path.back()) != cut_endpoints.end();

      if (is_cut) {
        cut_list.push_back(vvec_t()); path.swap(cut_list.back());
      } else {
        path_list.push_back(vvec_t()); path.swap(path_list.back());
      }
    }

    populateVectorFromList(path_list, paths);
    populateVectorFromList(cut_list, cuts);

    // now only loops should remain in the graph.
    while (vertex_graph.size()) {
      detail::VVSMap::iterator p = vertex_graph.begin();
      carve::mesh::MeshSet<3>::vertex_t *v = (*p).first;
      CARVE_ASSERT((*p).second.size() == 2);

      std::vector<carve::mesh::MeshSet<3>::vertex_t *> path;
      path.clear();
      path.push_back(v);

      for (;;) {
        CARVE_ASSERT(p != vertex_graph.end());
        // pick a connected vertex to move to.

        carve::mesh::MeshSet<3>::vertex_t *n = *((*p).second.begin());
        detail::VVSMap::iterator q = vertex_graph.find(n);

        // remove the link.
        (*p).second.erase(n);
        (*q).second.erase(v);

        // move on.
        v = n;
        path.push_back(v);

        if ((*p).second.size() == 0) vertex_graph.erase(p);
        if ((*q).second.size() == 0) vertex_graph.erase(q);

        p = q;

        if (v == path[0]) break;
      }

      loop_list.push_back(vvec_t()); path.swap(loop_list.back());
    }
 
    populateVectorFromList(loop_list, loops);
  }



  template<typename T>
  std::string ptrstr(const T *ptr) {
    std::ostringstream s;
    s << ptr;
    return s.str().substr(1);
  }

  void dumpAsGraph(carve::mesh::MeshSet<3>::face_t *face,
                   const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &base_loop,
                   const carve::csg::V2Set &face_edges,
                   const carve::csg::V2Set &split_edges) {
    std::map<carve::mesh::MeshSet<3>::vertex_t *, carve::geom2d::P2> proj;

    for (size_t i = 0; i < base_loop.size(); ++i) {
      proj[base_loop[i]] = face->project(base_loop[i]->v);
    }
    for (carve::csg::V2Set::const_iterator i = split_edges.begin(); i != split_edges.end(); ++i) {
      proj[(*i).first] = face->project((*i).first->v);
      proj[(*i).second] = face->project((*i).second->v);
    }

    {
      carve::geom2d::P2 lo, hi;
      std::map<carve::mesh::MeshSet<3>::vertex_t *, carve::geom2d::P2>::iterator i;
      i = proj.begin();
      lo = hi = (*i).second;
      for (; i != proj.end(); ++i) {
        lo.x = std::min(lo.x, (*i).second.x); lo.y = std::min(lo.y, (*i).second.y);
        hi.x = std::max(hi.x, (*i).second.x); hi.y = std::max(hi.y, (*i).second.y);
      }
      for (i = proj.begin(); i != proj.end(); ++i) {
        (*i).second.x = ((*i).second.x - lo.x) / (hi.x - lo.x) * 10;
        (*i).second.y = ((*i).second.y - lo.y) / (hi.y - lo.y) * 10;
      }
    }

    std::cerr << "graph G {\nnode [shape=circle,style=filled,fixedsize=true,width=\".1\",height=\".1\"];\nedge [len=4]\n";
    for (std::map<carve::mesh::MeshSet<3>::vertex_t *, carve::geom2d::P2>::iterator i = proj.begin(); i != proj.end(); ++i) {
      std::cerr << "   " << ptrstr((*i).first) << " [pos=\"" << (*i).second.x << "," << (*i).second.y << "!\"];\n";
    }
    for (carve::csg::V2Set::const_iterator i = face_edges.begin(); i != face_edges.end(); ++i) {
      std::cerr << "   " << ptrstr((*i).first) << " -- " << ptrstr((*i).second) << ";\n";
    }
    for (carve::csg::V2Set::const_iterator i = split_edges.begin(); i != split_edges.end(); ++i) {
      std::cerr << "   " << ptrstr((*i).first) << " -- " << ptrstr((*i).second) << " [color=\"blue\"];\n";
    }
    std::cerr << "};\n";
  }

  void generateOneFaceLoop(carve::mesh::MeshSet<3>::face_t *face,
                           const carve::csg::detail::Data &data,
                           const carve::csg::VertexIntersections &vertex_intersections,
                           carve::csg::CSG::Hooks &hooks,
                           std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > &face_loops) {
    using namespace carve::csg;

    std::vector<carve::mesh::MeshSet<3>::vertex_t *> base_loop;
    std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > hole_loops;

    /*bool face_edge_intersected = */assembleBaseLoop(face, data, base_loop, hooks);

    detail::FV2SMap::const_iterator fse_iter = data.face_split_edges.find(face);

    face_loops.clear();

    if (fse_iter == data.face_split_edges.end()) {
      // simple case: input face is output face (possibly with the
      // addition of vertices at intersections).
      face_loops.push_back(base_loop);
      return;
    }

    // complex case: input face is split into multiple output faces.
    V2Set face_edges;

    for (size_t j = 0, je = base_loop.size() - 1; j < je; ++j) {
      face_edges.insert(std::make_pair(base_loop[j], base_loop[j + 1]));
    }
    face_edges.insert(std::make_pair(base_loop.back(), base_loop[0]));

    // collect the split edges (as long as they're not on the perimeter)
    const detail::FV2SMap::mapped_type &fse = ((*fse_iter).second);

    // split_edges contains all of the edges created by intersections
    // that aren't part of the perimeter of the face.
    V2Set split_edges;

    for (detail::FV2SMap::mapped_type::const_iterator
           j = fse.begin(), je =  fse.end();
         j != je;
         ++j) {
      carve::mesh::MeshSet<3>::vertex_t *v1 = ((*j).first), *v2 = ((*j).second);

      if (face_edges.find(std::make_pair(v1, v2)) == face_edges.end() &&
          face_edges.find(std::make_pair(v2, v1)) == face_edges.end()) {
        // If the edge isn't part of the face perimeter, add it to
        // split_edges.
        split_edges.insert(ordered_edge(v1, v2));
      }
    }

    // face is unsplit.
    if (!split_edges.size()) {
      face_loops.push_back(base_loop);
      return;
    }

#if defined(CARVE_DEBUG)
    dumpAsGraph(face, base_loop, face_edges, split_edges);
#endif

#if 0
    // old face splitting method.
    for (V2Set::const_iterator i = split_edges.begin(); i != split_edges.end(); ++i) {
      face_edges.insert(std::make_pair((*i).first, (*i).second));
      face_edges.insert(std::make_pair((*i).second, (*i).first));
    }
    splitFace(face, face_edges, face_loops, hole_loops, vertex_intersections);

    if (hole_loops.size()) {
      mergeFacesAndHoles(face, face_loops, hole_loops, hooks);
    }
    return;
#endif

#if defined(CARVE_DEBUG)
    std::cerr << "### split_edges.size(): " << split_edges.size() << std::endl;
#endif
    if (split_edges.size() == 1) {
      // handle the common case of a face that's split by a single edge.
      carve::mesh::MeshSet<3>::vertex_t *v1 = split_edges.begin()->first;
      carve::mesh::MeshSet<3>::vertex_t *v2 = split_edges.begin()->second;

      std::vector<carve::mesh::MeshSet<3>::vertex_t *>::iterator vi1 = std::find(base_loop.begin(), base_loop.end(), v1);
      std::vector<carve::mesh::MeshSet<3>::vertex_t *>::iterator vi2 = std::find(base_loop.begin(), base_loop.end(), v2);

      if (vi1 != base_loop.end() && vi2 != base_loop.end()) {
        // this is an inserted edge that connects two points on the base loop. nice and simple.
        if (vi2 < vi1) std::swap(vi1, vi2);

        size_t loop1_size = vi2 - vi1 + 1;
        size_t loop2_size = base_loop.size() + 2 - loop1_size;

        std::vector<carve::mesh::MeshSet<3>::vertex_t *> l1;
        std::vector<carve::mesh::MeshSet<3>::vertex_t *> l2;

        l1.reserve(loop1_size);
        l2.reserve(loop2_size);

        std::copy(vi1, vi2+1, std::back_inserter(l1));
        std::copy(vi2, base_loop.end(), std::back_inserter(l2));
        std::copy(base_loop.begin(), vi1+1, std::back_inserter(l2));

        CARVE_ASSERT(l1.size() == loop1_size);
        CARVE_ASSERT(l2.size() == loop2_size);

        face_loops.push_back(l1);
        face_loops.push_back(l2);

        return;
      }

      // Consider handling cases where one end of the edge touches the
      // perimeter, and where neither end does.
    }

    std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > paths;
    std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > cuts;
    std::vector<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > loops;

    // Take the split edges and compose them into a set of paths and
    // loops. Loops are edge paths that do not touch the boundary, or
    // any other path or loop - they are holes cut out of the centre
    // of the face. Paths are made up of all the other edge segments,
    // and start and end at the face perimeter, or where they meet
    // another path (sometimes both cases will be true).
    composeEdgesIntoPaths(split_edges, base_loop, paths, cuts, loops);

#if defined(CARVE_DEBUG)
    std::cerr << "###   paths.size(): " << paths.size() << std::endl;
    std::cerr << "###   cuts.size():  " << cuts.size() << std::endl;
    std::cerr << "###   loops.size(): " << loops.size() << std::endl;
#endif

    if (!paths.size()) {
      // No complex paths.
      face_loops.push_back(base_loop);
    } else {
      if (processCrossingEdges(face, vertex_intersections, hooks, base_loop, paths, face_loops)) {
        // Worked.
      } else {
        // complex case - fall back to old edge tracing code.
#if defined(CARVE_DEBUG)
        std::cerr << "### processCrossingEdges failed. Falling back to edge tracing code" << std::endl;
#endif
        for (size_t i = 0; i < paths.size(); ++i) {
          for (size_t j = 0; j < paths[i].size() - 1; ++j) {
            face_edges.insert(std::make_pair(paths[i][j], paths[i][j+1]));
            face_edges.insert(std::make_pair(paths[i][j+1], paths[i][j]));
          }
        }
        splitFace(face, face_edges, face_loops, hole_loops, vertex_intersections);
      }
    }

    // Now merge cuts and loops into face loops.

    // every cut creates a hole.
    for (size_t i = 0; i < cuts.size(); ++i) {
      hole_loops.push_back(std::vector<carve::mesh::MeshSet<3>::vertex_t *>());
      hole_loops.back().reserve(2 * cuts[i].size() - 2);
      std::copy(cuts[i].begin(), cuts[i].end(), std::back_inserter(hole_loops.back()));
      if (cuts[i].size() > 2) {
        std::copy(cuts[i].rbegin() + 1, cuts[i].rend() - 1, std::back_inserter(hole_loops.back()));
      }
    }

    // every loop creates a hole and a corresponding face.
    for (size_t i = 0; i < loops.size(); ++i) {
      hole_loops.push_back(std::vector<carve::mesh::MeshSet<3>::vertex_t *>());
      hole_loops.back().reserve(loops[i].size()-1);
      std::copy(loops[i].begin(), loops[i].end()-1, std::back_inserter(hole_loops.back()));

      face_loops.push_back(std::vector<carve::mesh::MeshSet<3>::vertex_t *>());
      face_loops.back().reserve(loops[i].size()-1);
      std::copy(loops[i].rbegin()+1, loops[i].rend(), std::back_inserter(face_loops.back()));

      std::vector<carve::geom2d::P2> projected;
      projected.reserve(face_loops.back().size());
      for (size_t i = 0; i < face_loops.back().size(); ++i) {
        projected.push_back(face->project(face_loops.back()[i]->v));
      }

      if (carve::geom2d::signedArea(projected) > 0.0) {
        std::swap(face_loops.back(), hole_loops.back());
      }
    }

    // if there are holes, then they need to be merged with faces.
    if (hole_loops.size()) {
      mergeFacesAndHoles(face, face_loops, hole_loops, hooks);
    }
  }
}



/** 
 * \brief Build a set of face loops for all (split) faces of a Polyhedron.
 * 
 * @param[in] poly The polyhedron to process
 * @param[in] data Internal intersection data
 * @param[out] face_loops_out The resulting face loops
 * 
 * @return The number of edges generated.
 */
size_t carve::csg::CSG::generateFaceLoops(carve::mesh::MeshSet<3> *poly,
                                          const detail::Data &data,
                                          FaceLoopList &face_loops_out) {
  static carve::TimingName FUNC_NAME("CSG::generateFaceLoops()");
  carve::TimingBlock block(FUNC_NAME);
  size_t generated_edges = 0;
  std::vector<carve::mesh::MeshSet<3>::vertex_t *> base_loop;
  std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> > face_loops;
  
  for (carve::mesh::MeshSet<3>::face_iter i = poly->faceBegin(); i != poly->faceEnd(); ++i) {
    carve::mesh::MeshSet<3>::face_t *face = (*i);

#if defined(CARVE_DEBUG)
    double in_area = 0.0, out_area = 0.0;

    {
      std::vector<carve::mesh::MeshSet<3>::vertex_t *> base_loop;
      assembleBaseLoop(face, data, base_loop);

      {
        std::vector<carve::geom2d::P2> projected;
        projected.reserve(base_loop.size());
        for (size_t n = 0; n < base_loop.size(); ++n) {
          projected.push_back(face->project(base_loop[n]->v));
        }

        in_area = carve::geom2d::signedArea(projected);
        std::cerr << "### in_area=" << in_area << std::endl;
      }
    }
#endif

    generateOneFaceLoop(face, data, vertex_intersections, hooks, face_loops);

#if defined(CARVE_DEBUG)
    {
      V2Set face_edges;

      std::vector<carve::mesh::MeshSet<3>::vertex_t *> base_loop;
      assembleBaseLoop(face, data, base_loop);

      for (size_t j = 0, je = base_loop.size() - 1; j < je; ++j) {
        face_edges.insert(std::make_pair(base_loop[j+1], base_loop[j]));
      }
      face_edges.insert(std::make_pair(base_loop[0], base_loop.back()));
      for (std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> >::const_iterator fli = face_loops.begin(); fli != face_loops.end(); ++ fli) {

        {
          std::vector<carve::geom2d::P2> projected;
          projected.reserve((*fli).size());
          for (size_t n = 0; n < (*fli).size(); ++n) {
            projected.push_back(face->project((*fli)[n]->v));
          }

          double area = carve::geom2d::signedArea(projected);
          std::cerr << "### loop_area[" << std::distance((std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> >::const_iterator)face_loops.begin(), fli) << "]=" << area << std::endl;
          out_area += area;
        }

        const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &fl = *fli;
        for (size_t j = 0, je = fl.size() - 1; j < je; ++j) {
          face_edges.insert(std::make_pair(fl[j], fl[j+1]));
        }
        face_edges.insert(std::make_pair(fl.back(), fl[0]));
      }
      for (V2Set::const_iterator j = face_edges.begin(); j != face_edges.end(); ++j) {
        if (face_edges.find(std::make_pair((*j).second, (*j).first)) == face_edges.end()) {
          std::cerr << "### error: unmatched edge [" << (*j).first << "-" << (*j).second << "]" << std::endl;
        }
      }
      std::cerr << "### out_area=" << out_area << std::endl;
      if (out_area != in_area) {
        std::cerr << "### error: area does not match. delta = " << (out_area - in_area) << std::endl;
        // CARVE_ASSERT(fabs(out_area - in_area) < 1e-5);
      }
    }
#endif

    // now record all the resulting face loops.
#if defined(CARVE_DEBUG)
    std::cerr << "### ======" << std::endl;
#endif
    for (std::list<std::vector<carve::mesh::MeshSet<3>::vertex_t *> >::const_iterator
           f = face_loops.begin(), fe = face_loops.end();
         f != fe;
         ++f) {
#if defined(CARVE_DEBUG)
      std::cerr << "### loop:";
      for (size_t i = 0; i < (*f).size(); ++i) {
        std::cerr << " " << (*f)[i];
      }
      std::cerr << std::endl;
#endif

      face_loops_out.append(new FaceLoop(face, *f));
      generated_edges += (*f).size();
    }
#if defined(CARVE_DEBUG)
    std::cerr << "### ======" << std::endl;
#endif
  }
  return generated_edges;
}
