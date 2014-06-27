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

#if defined(HAVE_STDINT_H)
#include <stdint.h>
#endif

#include <carve/csg.hpp>
#include <carve/debug_hooks.hpp>
#include <carve/colour.hpp>

#include <list>
#include <set>
#include <iostream>

#include <algorithm>

#include "csg_detail.hpp"

#include "intersect_common.hpp"
#include "intersect_classify_common.hpp"

#define ANGLE_EPSILON 1e-6

namespace carve {
  namespace csg {

    namespace {

      inline bool single_bit_set(uint32_t v) {
        v &= v - 1;
        return v == 0;
      }

      struct EdgeSurface {
        FaceLoop *fwd;
        double fwd_ang;
        FaceLoop *rev;
        double rev_ang;

        EdgeSurface() : fwd(NULL), fwd_ang(0.0), rev(NULL), rev_ang(0.0) { }
      };


      typedef std::map<const carve::mesh::MeshSet<3>::mesh_t *, EdgeSurface> GrpEdgeSurfMap;

      typedef std::pair<FaceLoopGroup *, const carve::mesh::MeshSet<3>::mesh_t *> ClassificationKey;

      struct ClassificationData {
        uint32_t class_bits : 5;
        uint32_t class_decided : 1;

        int c[5];

        ClassificationData() {
          class_bits = FACE_ANY_BIT;
          class_decided = 0;
          memset(c, 0, sizeof(c));
        }
      };

      struct hash_classification {
        size_t operator()(const ClassificationKey &f) const {
          return (size_t)f.first ^ (size_t)f.second;
        }
      };

      typedef std::unordered_map<ClassificationKey, ClassificationData, hash_classification> Classification;


      struct hash_group_ptr {
        size_t operator()(const FaceLoopGroup * const &f) const {
          return (size_t)f;
        }
      };


      typedef std::pair<size_t, const carve::mesh::MeshSet<3>::vertex_t *> PerimKey;

      struct hash_perim_key {
        size_t operator()(const PerimKey &v) const {
          return (size_t)v.first ^ (size_t)v.second;
        }
      };

      typedef std::unordered_map<std::pair<size_t, const carve::mesh::MeshSet<3>::vertex_t *>,
                                 std::unordered_set<FaceLoopGroup *, hash_group_ptr>,
                                 hash_perim_key> PerimMap;



      struct hash_group_pair {
        size_t operator()(const std::pair<int, const FaceLoopGroup *> &v) const {
          return (size_t)v.first ^ (size_t)v.second;
        }
      };

      typedef std::unordered_map<const FaceLoopGroup *,
                                 std::unordered_set<std::pair<int, const FaceLoopGroup *>, hash_group_pair>,
                                 hash_group_ptr> CandidateOnMap;



      static inline void remove(carve::mesh::MeshSet<3>::vertex_t *a,
                                carve::mesh::MeshSet<3>::vertex_t *b,
                                carve::csg::detail::VVSMap &shared_edge_graph) {
        carve::csg::detail::VVSMap::iterator i = shared_edge_graph.find(a);
        CARVE_ASSERT(i != shared_edge_graph.end());
        size_t n = (*i).second.erase(b);
        CARVE_ASSERT(n == 1);
        if ((*i).second.size() == 0) shared_edge_graph.erase(i);
      }



      static inline void remove(V2 edge,
                                carve::csg::detail::VVSMap &shared_edge_graph) {
        remove(edge.first, edge.second, shared_edge_graph);
        remove(edge.second, edge.first, shared_edge_graph);
      }



      static void walkGraphSegment(carve::csg::detail::VVSMap &shared_edge_graph,
                                   const carve::csg::detail::VSet &branch_points,
                                   V2 initial,
                                   const carve::csg::detail::LoopEdges & /* a_edge_map */,
                                   const carve::csg::detail::LoopEdges & /* b_edge_map */,
                                   std::list<V2> &out) {
        V2 curr;
        curr = initial;
        bool closed = false;

        out.clear();
        for (;;) {
          // walk forward.
          out.push_back(curr);
          remove(curr, shared_edge_graph);

          if (curr.second == initial.first) { closed = true; break; }
          if (branch_points.find(curr.second) != branch_points.end()) break;
          carve::csg::detail::VVSMap::const_iterator o = shared_edge_graph.find(curr.second);
          if (o == shared_edge_graph.end()) break;
          CARVE_ASSERT((*o).second.size() == 1);
          curr.first = curr.second;
          curr.second = *((*o).second.begin());
          // test here that the set of incident groups hasn't changed.
        }

        if (!closed) {
          // walk backward.
          curr = initial;
          for (;;) {
            if (branch_points.find(curr.first) != branch_points.end()) break;
            carve::csg::detail::VVSMap::const_iterator o = shared_edge_graph.find(curr.first);
            if (o == shared_edge_graph.end()) break;
            curr.second = curr.first;
            curr.first = *((*o).second.begin());
            // test here that the set of incident groups hasn't changed.

            out.push_front(curr);
            remove(curr, shared_edge_graph);
          }
        }

#if defined(CARVE_DEBUG)
        std::cerr << "intersection segment: " << out.size() << " edges." << std::endl;
#if defined(DEBUG_DRAW_INTERSECTION_LINE)
        {
          static float H = 0.0, S = 1.0, V = 1.0;
          float r, g, b;

          H = fmod((H + .37), 1.0);
          S = 0.5 + fmod((S - 0.37), 0.5);
          carve::colour::HSV2RGB(H, S, V, r, g, b);

          if (out.size() > 1) {
            drawEdges(out.begin(), ++out.begin(),
                      0.0, 0.0, 0.0, 1.0,
                      r, g, b, 1.0,
                      3.0);
            drawEdges(++out.begin(), --out.end(),
                      r, g, b, 1.0,
                      r, g, b, 1.0,
                      3.0);
            drawEdges(--out.end(), out.end(),
                      r, g, b, 1.0,
                      1.0, 1.0, 1.0, 1.0,
                      3.0);
          } else {
            drawEdges(out.begin(), out.end(),
                      r, g, b, 1.0,
                      r, g, b, 1.0,
                      3.0);
          }
        }
#endif
#endif
      }



      static carve::geom3d::Vector perpendicular(const carve::geom3d::Vector &v) {
        if (fabs(v.x) < fabs(v.y)) {
          if (fabs(v.x) < fabs(v.z)) {
            return cross(v, carve::geom::VECTOR(1.0, 0.0, 0.0)).normalized();
          } else {
            return cross(v, carve::geom::VECTOR(0.0, 0.0, 1.0)).normalized();
          }
        } else {
          if (fabs(v.y) < fabs(v.z)) {
            return cross(v, carve::geom::VECTOR(0.0, 1.0, 0.0)).normalized();
          } else {
            return cross(v, carve::geom::VECTOR(1.0, 0.0, 1.0)).normalized();
          }
        }
      }



      static void classifyAB(const GrpEdgeSurfMap &a_edge_surfaces,
                             const GrpEdgeSurfMap &b_edge_surfaces,
                             Classification &classifications) {
        // two faces in the a surface
        for (GrpEdgeSurfMap::const_iterator ib = b_edge_surfaces.begin(), eb = b_edge_surfaces.end(); ib != eb; ++ib) {

          if ((*ib).second.fwd) {
            FaceLoopGroup *b_grp = ((*ib).second.fwd->group);

            for (GrpEdgeSurfMap::const_iterator ia = a_edge_surfaces.begin(), ea = a_edge_surfaces.end(); ia != ea; ++ia) {

              if ((*ia).second.fwd && (*ia).second.rev) {
                const carve::mesh::MeshSet<3>::mesh_t *a_gid = (*ia).first;

                ClassificationData &data = classifications[std::make_pair(b_grp, a_gid)];
                if (data.class_decided) continue;

                // an angle between (*ia).fwd_ang and (*ia).rev_ang is outside/above group a.
                FaceClass fc;

                if (fabs((*ib).second.fwd_ang - (*ia).second.fwd_ang) < ANGLE_EPSILON) {
                  fc = FACE_ON_ORIENT_OUT;
                } else if (fabs((*ib).second.fwd_ang - (*ia).second.rev_ang) < ANGLE_EPSILON) {
                  fc = FACE_ON_ORIENT_IN;
                } else {
                  double a1 = (*ia).second.fwd_ang;
                  double a2 = (*ia).second.rev_ang;
                  if (a1 < a2) {
                    if (a1 < (*ib).second.fwd_ang && (*ib).second.fwd_ang < a2) {
                      fc = FACE_IN;
                    } else {
                      fc = FACE_OUT;
                    }
                  } else {
                    if (a2 < (*ib).second.fwd_ang && (*ib).second.fwd_ang < a1) {
                      fc = FACE_OUT;
                    } else {
                      fc = FACE_IN;
                    }
                  }
                }
                data.c[fc + 2]++;
              }
            }
          }

          if ((*ib).second.rev) {
            FaceLoopGroup *b_grp = ((*ib).second.rev->group);

            for (GrpEdgeSurfMap::const_iterator ia = a_edge_surfaces.begin(), ea = a_edge_surfaces.end(); ia != ea; ++ia) {

              if ((*ia).second.fwd && (*ia).second.rev) {
                const carve::mesh::MeshSet<3>::mesh_t *a_gid = (*ia).first;

                ClassificationData &data = (classifications[std::make_pair(b_grp, a_gid)]);
                if (data.class_decided) continue;

                // an angle between (*ia).fwd_ang and (*ia).rev_ang is outside/above group a.
                FaceClass fc;

                if (fabs((*ib).second.rev_ang - (*ia).second.fwd_ang) < ANGLE_EPSILON) {
                  fc = FACE_ON_ORIENT_IN;
                } else if (fabs((*ib).second.rev_ang - (*ia).second.rev_ang) < ANGLE_EPSILON) {
                  fc = FACE_ON_ORIENT_OUT;
                } else {
                  double a1 = (*ia).second.fwd_ang;
                  double a2 = (*ia).second.rev_ang;
                  if (a1 < a2) {
                    if (a1 < (*ib).second.rev_ang && (*ib).second.rev_ang < a2) {
                      fc = FACE_IN;
                    } else {
                      fc = FACE_OUT;
                    }
                  } else {
                    if (a2 < (*ib).second.rev_ang && (*ib).second.rev_ang < a1) {
                      fc = FACE_OUT;
                    } else {
                      fc = FACE_IN;
                    }
                  }
                }
                data.c[fc + 2]++;
              }
            }
          }
        }
      }


      static bool processForwardEdgeSurfaces(GrpEdgeSurfMap &edge_surfaces,
                                             const std::list<FaceLoop *> &fwd,
                                             const carve::geom3d::Vector &edge_vector,
                                             const carve::geom3d::Vector &base_vector) {
        for (std::list<FaceLoop *>::const_iterator i = fwd.begin(), e = fwd.end(); i != e; ++i) {
          EdgeSurface &es = (edge_surfaces[(*i)->orig_face->mesh]);
          if (es.fwd != NULL) return false;
          es.fwd = (*i);
          es.fwd_ang = carve::geom3d::antiClockwiseAngle((*i)->orig_face->plane.N, base_vector, edge_vector);
        }
        return true;
      }

      static bool processReverseEdgeSurfaces(GrpEdgeSurfMap &edge_surfaces,
                                             const std::list<FaceLoop *> &rev,
                                             const carve::geom3d::Vector &edge_vector,
                                             const carve::geom3d::Vector &base_vector) {
        for (std::list<FaceLoop *>::const_iterator i = rev.begin(), e = rev.end(); i != e; ++i) {
          EdgeSurface &es = (edge_surfaces[(*i)->orig_face->mesh]);
          if (es.rev != NULL) return false;
          es.rev = (*i);
          es.rev_ang = carve::geom3d::antiClockwiseAngle(-(*i)->orig_face->plane.N, base_vector, edge_vector);
        }
        return true;
      }



      static void processOneEdge(const V2 &edge,
                                 const carve::csg::detail::LoopEdges &a_edge_map,
                                 const carve::csg::detail::LoopEdges &b_edge_map,
                                 Classification &a_classification,
                                 Classification &b_classification) {
        GrpEdgeSurfMap a_edge_surfaces;
        GrpEdgeSurfMap b_edge_surfaces;

        carve::geom3d::Vector edge_vector = (edge.second->v - edge.first->v).normalized();
        carve::geom3d::Vector base_vector = perpendicular(edge_vector);

        carve::csg::detail::LoopEdges::const_iterator ae_f = a_edge_map.find(edge);
        carve::csg::detail::LoopEdges::const_iterator ae_r = a_edge_map.find(flip(edge));
        CARVE_ASSERT(ae_f != a_edge_map.end() || ae_r != a_edge_map.end());

        carve::csg::detail::LoopEdges::const_iterator be_f = b_edge_map.find(edge);
        carve::csg::detail::LoopEdges::const_iterator be_r = b_edge_map.find(flip(edge));
        CARVE_ASSERT(be_f != b_edge_map.end() || be_r != b_edge_map.end());

        if (ae_f != a_edge_map.end() && !processForwardEdgeSurfaces(a_edge_surfaces, (*ae_f).second, edge_vector, base_vector)) return;
        if (ae_r != a_edge_map.end() && !processReverseEdgeSurfaces(a_edge_surfaces, (*ae_r).second, edge_vector, base_vector)) return;
        if (be_f != b_edge_map.end() && !processForwardEdgeSurfaces(b_edge_surfaces, (*be_f).second, edge_vector, base_vector)) return;
        if (be_r != b_edge_map.end() && !processReverseEdgeSurfaces(b_edge_surfaces, (*be_r).second, edge_vector, base_vector)) return;

        classifyAB(a_edge_surfaces, b_edge_surfaces, b_classification);
        classifyAB(b_edge_surfaces, a_edge_surfaces, a_classification);
      }



      static void traceIntersectionGraph(const V2Set &shared_edges,
                                         const FLGroupList & /* a_loops_grouped */,
                                         const FLGroupList & /* b_loops_grouped */,
                                         const carve::csg::detail::LoopEdges &a_edge_map,
                                         const carve::csg::detail::LoopEdges &b_edge_map) {

        carve::csg::detail::VVSMap shared_edge_graph;
        carve::csg::detail::VSet branch_points;

        // first, make the intersection graph.
        for (V2Set::const_iterator i = shared_edges.begin(); i != shared_edges.end(); ++i) {
          const V2Set::key_type &edge = (*i);
          carve::csg::detail::VVSMap::mapped_type &out = (shared_edge_graph[edge.first]);
          out.insert(edge.second);
          if (out.size() == 3) branch_points.insert(edge.first);

#if defined(CARVE_DEBUG) && defined(DEBUG_DRAW_INTERSECTION_LINE)
          HOOK(drawEdge(edge.first, edge.second, 1, 1, 1, 1, 1, 1, 1, 1, 1.0););
#endif
        }
#if defined(CARVE_DEBUG)
        std::cerr << "graph nodes: " << shared_edge_graph.size() << std::endl;
        std::cerr << "branch nodes: " << branch_points.size() << std::endl;
#endif

        std::list<V2> out;
        while (shared_edge_graph.size()) {
          carve::csg::detail::VVSMap::iterator i = shared_edge_graph.begin();
          carve::mesh::MeshSet<3>::vertex_t *v1 = (*i).first;
          carve::mesh::MeshSet<3>::vertex_t *v2 = *((*i).second.begin());
          walkGraphSegment(shared_edge_graph, branch_points, V2(v1, v2), a_edge_map, b_edge_map, out);
        }
      }

      void hashByPerimeter(FLGroupList &grp, PerimMap &perim_map) {
        for (FLGroupList::iterator i = grp.begin(); i != grp.end(); ++i) {
          size_t perim_size = (*i).perimeter.size();
          // can be the case for non intersecting groups. (and groups that intersect at a point?)
          if (!perim_size) continue;
          const carve::mesh::MeshSet<3>::vertex_t *perim_min = std::min_element((*i).perimeter.begin(), (*i).perimeter.end())->first;
          perim_map[std::make_pair(perim_size, perim_min)].insert(&(*i));
        }
      }



      bool same_edge_set_fwd(const V2Set &a, const V2Set &b) {
        if (a.size() != b.size()) return false;
        for (V2Set::const_iterator i = a.begin(), e = a.end(); i != e; ++i) {
          if (b.find(*i) == b.end()) return false;
        }
        return true;
      }



      bool same_edge_set_rev(const V2Set &a, const V2Set &b) {
        if (a.size() != b.size()) return false;
        for (V2Set::const_iterator i = a.begin(), e = a.end(); i != e; ++i) {
          if (b.find(std::make_pair((*i).second, (*i).first)) == b.end()) return false;
        }
        return true;
      }



      int same_edge_set(const V2Set &a, const V2Set &b) {
        if (same_edge_set_fwd(a, b)) return +1;
        if (same_edge_set_rev(a, b)) return -1;
        return 0;
      }



      void generateCandidateOnSets(FLGroupList &a_grp,
                                   FLGroupList &b_grp,
                                   CandidateOnMap &candidate_on_map,
                                   Classification &a_classification,
                                   Classification &b_classification) {
        PerimMap a_grp_by_perim, b_grp_by_perim;

        hashByPerimeter(a_grp, a_grp_by_perim);
        hashByPerimeter(b_grp, b_grp_by_perim);

        for (PerimMap::iterator i = a_grp_by_perim.begin(), ie = a_grp_by_perim.end(); i != ie; ++i) {
          PerimMap::iterator j = b_grp_by_perim.find((*i).first);
          if (j == b_grp_by_perim.end()) continue;

          for (PerimMap::mapped_type::iterator a = (*i).second.begin(), ae = (*i).second.end(); a != ae; ++a) {
            for (PerimMap::mapped_type::iterator b = (*j).second.begin(), be = (*j).second.end(); b != be; ++b) {
              int x = same_edge_set((*a)->perimeter, (*b)->perimeter);
              if (!x) continue;
              candidate_on_map[(*a)].insert(std::make_pair(x, (*b)));
              if ((*a)->face_loops.count == 1 && (*b)->face_loops.count == 1) {
                uint32_t fcb = x == +1 ? FACE_ON_ORIENT_OUT_BIT : FACE_ON_ORIENT_IN_BIT;

#if defined(CARVE_DEBUG)
                std::cerr << "paired groups: " << (*a) << ", " << (*b) << std::endl;
#endif

                ClassificationData &a_data = a_classification[std::make_pair((*a), (*b)->face_loops.head->orig_face->mesh)];
                a_data.class_bits = fcb; a_data.class_decided = 1;

                ClassificationData &b_data = b_classification[std::make_pair((*b), (*a)->face_loops.head->orig_face->mesh)];
                b_data.class_bits = fcb; b_data.class_decided = 1;
              }
            }
          }
        }
      }

    }


    static inline std::string CODE(const FaceLoopGroup *grp) {
      const std::list<ClassificationInfo> &cinfo = (grp->classification);
      if (cinfo.size() == 0) {
        return "?";
      }

      FaceClass fc = FACE_UNCLASSIFIED;

      for (std::list<ClassificationInfo>::const_iterator i = grp->classification.begin(), e = grp->classification.end(); i != e; ++i) {
        if ((*i).intersected_mesh == NULL) {
          // classifier only returns global info
          fc = (*i).classification;
          break;
        }

        if ((*i).intersectedMeshIsClosed()) {
          if ((*i).classification == FACE_UNCLASSIFIED) continue;
          if (fc == FACE_UNCLASSIFIED) {
            fc = (*i).classification;
          } else if (fc != (*i).classification) {
            return "X";
          }
        }
      }
      if (fc == FACE_IN) return "I";
      if (fc == FACE_ON_ORIENT_IN) return "<";
      if (fc == FACE_ON_ORIENT_OUT) return ">";
      if (fc == FACE_OUT) return "O";
      return "*";
    }

    void CSG::classifyFaceGroupsEdge(const V2Set &shared_edges,
                                     VertexClassification &vclass,
                                     carve::mesh::MeshSet<3> *poly_a,
                                     const face_rtree_t *poly_a_rtree,
                                     FLGroupList &a_loops_grouped,
                                     const detail::LoopEdges &a_edge_map,
                                     carve::mesh::MeshSet<3> *poly_b,
                                     const face_rtree_t *poly_b_rtree,
                                     FLGroupList &b_loops_grouped,
                                     const detail::LoopEdges &b_edge_map,
                                     CSG::Collector &collector) {
      Classification a_classification;
      Classification b_classification;

      CandidateOnMap candidate_on_map;

#if defined(CARVE_DEBUG)
      std::cerr << "a input loops (" << a_loops_grouped.size() << "): ";
      for (FLGroupList::iterator i = a_loops_grouped.begin(); i != a_loops_grouped.end(); ++i) {
        std::cerr << &*i << " ";
      }
      std::cerr << std::endl;
      std::cerr << "b input loops (" << b_loops_grouped.size() << "): ";
      for (FLGroupList::iterator i = b_loops_grouped.begin(); i != b_loops_grouped.end(); ++i) {
        std::cerr << &*i << " ";
      }
      std::cerr << std::endl;
#endif

#if defined(DISPLAY_GRP_GRAPH)
      // XXX: this is hopelessly inefficient.
      std::map<const FaceLoopGroup *, std::set<const FaceLoopGroup *> > grp_graph_fwd, grp_graph_rev;
      {
        for (FLGroupList::iterator i = a_loops_grouped.begin(); i != a_loops_grouped.end(); ++i) {
          FaceLoopGroup *src = &(*i);
          for (V2Set::const_iterator k = src->perimeter.begin(); k != src->perimeter.end(); ++k) {
            V2 fwd = *k;
            V2 rev = std::make_pair(fwd.second, fwd.first);
            for (FLGroupList::iterator j = a_loops_grouped.begin(); j != a_loops_grouped.end(); ++j) {
              FaceLoopGroup *tgt = &(*j);
              if (tgt->perimeter.find(fwd) != tgt->perimeter.end()) { grp_graph_fwd[src].insert(tgt); }
              if (tgt->perimeter.find(rev) != tgt->perimeter.end()) { grp_graph_rev[src].insert(tgt); }
            }
            for (FLGroupList::iterator j = b_loops_grouped.begin(); j != b_loops_grouped.end(); ++j) {
              FaceLoopGroup *tgt = &(*j);
              if (tgt->perimeter.find(fwd) != tgt->perimeter.end()) { grp_graph_fwd[src].insert(tgt); }
              if (tgt->perimeter.find(rev) != tgt->perimeter.end()) { grp_graph_rev[src].insert(tgt); }
            }
          }
        }
        for (FLGroupList::iterator i = b_loops_grouped.begin(); i != b_loops_grouped.end(); ++i) {
          FaceLoopGroup *src = &(*i);
          for (V2Set::const_iterator k = src->perimeter.begin(); k != src->perimeter.end(); ++k) {
            V2 fwd = *k;
            V2 rev = std::make_pair(fwd.second, fwd.first);
            for (FLGroupList::iterator j = a_loops_grouped.begin(); j != a_loops_grouped.end(); ++j) {
              FaceLoopGroup *tgt = &(*j);
              if (tgt->perimeter.find(fwd) != tgt->perimeter.end()) { grp_graph_fwd[src].insert(tgt); }
              if (tgt->perimeter.find(rev) != tgt->perimeter.end()) { grp_graph_rev[src].insert(tgt); }
            }
            for (FLGroupList::iterator j = b_loops_grouped.begin(); j != b_loops_grouped.end(); ++j) {
              FaceLoopGroup *tgt = &(*j);
              if (tgt->perimeter.find(fwd) != tgt->perimeter.end()) { grp_graph_fwd[src].insert(tgt); }
              if (tgt->perimeter.find(rev) != tgt->perimeter.end()) { grp_graph_rev[src].insert(tgt); }
            }
          }
        }
      }
#endif

      generateCandidateOnSets(a_loops_grouped, b_loops_grouped, candidate_on_map, a_classification, b_classification);


      for (V2Set::const_iterator i = shared_edges.begin(); i != shared_edges.end(); ++i) {
        const V2 &edge = (*i);
        processOneEdge(edge, a_edge_map, b_edge_map, a_classification, b_classification);
      }


      for (Classification::iterator i = a_classification.begin(), e = a_classification.end(); i != e; ++i) {
        if (!(*i).second.class_decided) {
          if ((*i).second.c[FACE_IN            + 2] == 0) (*i).second.class_bits &= ~ FACE_IN_BIT;
          if ((*i).second.c[FACE_ON_ORIENT_IN  + 2] == 0) (*i).second.class_bits &= ~ FACE_ON_ORIENT_IN_BIT;
          if ((*i).second.c[FACE_ON_ORIENT_OUT + 2] == 0) (*i).second.class_bits &= ~ FACE_ON_ORIENT_OUT_BIT;
          if ((*i).second.c[FACE_OUT           + 2] == 0) (*i).second.class_bits &= ~ FACE_OUT_BIT;

          // XXX: this is the wrong thing to do. It's intended just as a test.
          if ((*i).second.class_bits == (FACE_IN_BIT | FACE_OUT_BIT)) {
            if ((*i).second.c[FACE_OUT + 2] > (*i).second.c[FACE_IN + 2]) {
              (*i).second.class_bits = FACE_OUT_BIT;
            } else {
              (*i).second.class_bits = FACE_IN_BIT;
            }
          }

          if (single_bit_set((*i).second.class_bits)) (*i).second.class_decided = 1;
        }
      }

      for (Classification::iterator i = b_classification.begin(), e = b_classification.end(); i != e; ++i) {
        if (!(*i).second.class_decided) {
          if ((*i).second.c[FACE_IN            + 2] == 0) (*i).second.class_bits &= ~ FACE_IN_BIT;
          if ((*i).second.c[FACE_ON_ORIENT_IN  + 2] == 0) (*i).second.class_bits &= ~ FACE_ON_ORIENT_IN_BIT;
          if ((*i).second.c[FACE_ON_ORIENT_OUT + 2] == 0) (*i).second.class_bits &= ~ FACE_ON_ORIENT_OUT_BIT;
          if ((*i).second.c[FACE_OUT           + 2] == 0) (*i).second.class_bits &= ~ FACE_OUT_BIT;

          // XXX: this is the wrong thing to do. It's intended just as a test.
          if ((*i).second.class_bits == (FACE_IN_BIT | FACE_OUT_BIT)) {
            if ((*i).second.c[FACE_OUT + 2] > (*i).second.c[FACE_IN + 2]) {
              (*i).second.class_bits = FACE_OUT_BIT;
            } else {
              (*i).second.class_bits = FACE_IN_BIT;
            }
          }

          if (single_bit_set((*i).second.class_bits)) (*i).second.class_decided = 1;
        }
      }


#if defined(CARVE_DEBUG)
      std::cerr << "poly a:" << std::endl;
      for (Classification::iterator i = a_classification.begin(), e = a_classification.end(); i != e; ++i) {
        FaceLoopGroup *grp = ((*i).first.first);

        std::cerr << "  group: " << grp << " gid: " << (*i).first.second
                  << "  "
                  << ((*i).second.class_decided ? "+" : "-")
                  << "  "
                  << ((*i).second.class_bits & FACE_IN_BIT ? "I" : ".")
                  << ((*i).second.class_bits & FACE_ON_ORIENT_IN_BIT ? "<" : ".")
                  << ((*i).second.class_bits & FACE_ON_ORIENT_OUT_BIT ? ">" : ".")
                  << ((*i).second.class_bits & FACE_OUT_BIT ? "O" : ".")
                  << "  ["
                  << std::setw(4) << (*i).second.c[0] << " "
                  << std::setw(4) << (*i).second.c[1] << " "
                  << std::setw(4) << (*i).second.c[2] << " "
                  << std::setw(4) << (*i).second.c[3] << " "
                  << std::setw(4) << (*i).second.c[4] << "]" << std::endl;
      }

      std::cerr << "poly b:" << std::endl;
      for (Classification::iterator i = b_classification.begin(), e = b_classification.end(); i != e; ++i) {
        FaceLoopGroup *grp = ((*i).first.first);

        std::cerr << "  group: " << grp << " gid: " << (*i).first.second
                  << "  "
                  << ((*i).second.class_decided ? "+" : "-")
                  << "  "
                  << ((*i).second.class_bits & FACE_IN_BIT ? "I" : ".")
                  << ((*i).second.class_bits & FACE_ON_ORIENT_IN_BIT ? "<" : ".")
                  << ((*i).second.class_bits & FACE_ON_ORIENT_OUT_BIT ? ">" : ".")
                  << ((*i).second.class_bits & FACE_OUT_BIT ? "O" : ".")
                  << "  ["
                  << std::setw(4) << (*i).second.c[0] << " "
                  << std::setw(4) << (*i).second.c[1] << " "
                  << std::setw(4) << (*i).second.c[2] << " "
                  << std::setw(4) << (*i).second.c[3] << " "
                  << std::setw(4) << (*i).second.c[4] << "]" << std::endl;
      }
#endif

      for (Classification::iterator i = a_classification.begin(), e = a_classification.end(); i != e; ++i) {
        FaceLoopGroup *grp = ((*i).first.first);

        grp->classification.push_back(ClassificationInfo());
        ClassificationInfo &info = grp->classification.back();

        info.intersected_mesh = (*i).first.second;

        if ((*i).second.class_decided) {
          info.classification = class_bit_to_class((*i).second.class_bits);
        } else {
          info.classification = FACE_UNCLASSIFIED;
        }
      }

      for (Classification::iterator i = b_classification.begin(), e = b_classification.end(); i != e; ++i) {
        FaceLoopGroup *grp = ((*i).first.first);

        grp->classification.push_back(ClassificationInfo());
        ClassificationInfo &info = grp->classification.back();

        info.intersected_mesh = (*i).first.second;

        if ((*i).second.class_decided) {
          info.classification = class_bit_to_class((*i).second.class_bits);
        } else {
          info.classification = FACE_UNCLASSIFIED;
        }
      }

      for (FLGroupList::iterator i = a_loops_grouped.begin(); i != a_loops_grouped.end(); ++i) {
        if ((*i).classification.size() == 0) {
#if defined(CARVE_DEBUG)
          std::cerr << " non intersecting group (poly a): " << &(*i) << std::endl;
#endif
          bool classified = false;
          for (FaceLoop *fl = (*i).face_loops.head; !classified && fl != NULL; fl = fl->next) {
            for (size_t fli = 0; !classified && fli < fl->vertices.size(); ++fli) {
              if (vclass[fl->vertices[fli]].cls[1] == POINT_UNK) { 
                vclass[fl->vertices[fli]].cls[1] = carve::mesh::classifyPoint(poly_b, poly_b_rtree, fl->vertices[fli]->v);
              }
              switch (vclass[fl->vertices[fli]].cls[1]) {
                case POINT_IN:
                  (*i).classification.push_back(ClassificationInfo(NULL, FACE_IN));
                  classified = true;
                  break;
                case POINT_OUT:
                  (*i).classification.push_back(ClassificationInfo(NULL, FACE_OUT));
                  classified = true;
                  break;
                default:
                  break;
              }
            }
          }
          if (!classified) {
            throw carve::exception("non intersecting group is not IN or OUT! (poly_a)");
          }
        }
      }

      for (FLGroupList::iterator i = b_loops_grouped.begin(); i != b_loops_grouped.end(); ++i) {
        if ((*i).classification.size() == 0) {
#if defined(CARVE_DEBUG)
          std::cerr << " non intersecting group (poly b): " << &(*i) << std::endl;
#endif
          bool classified = false;
          for (FaceLoop *fl = (*i).face_loops.head; !classified && fl != NULL; fl = fl->next) {
            for (size_t fli = 0; !classified && fli < fl->vertices.size(); ++fli) {
              if (vclass[fl->vertices[fli]].cls[0] == POINT_UNK) { 
                vclass[fl->vertices[fli]].cls[0] = carve::mesh::classifyPoint(poly_a, poly_a_rtree, fl->vertices[fli]->v);
              }
              switch (vclass[fl->vertices[fli]].cls[0]) {
                case POINT_IN:
                  (*i).classification.push_back(ClassificationInfo(NULL, FACE_IN));
                  classified = true;
                  break;
                case POINT_OUT:
                  (*i).classification.push_back(ClassificationInfo(NULL, FACE_OUT));
                  classified = true;
                  break;
                default:
                  break;
              }
            }
          }
          if (!classified) {
            throw carve::exception("non intersecting group is not IN or OUT! (poly_b)");
          }
        }
      }

#if defined(DISPLAY_GRP_GRAPH)
#define POLY(grp) (std::string((grp)->face_loops.head->orig_face->polyhedron == poly_a ? "[A:" : "[B:") + CODE(grp) + "]")

      for (std::map<const FaceLoopGroup *, std::set<const FaceLoopGroup *> >::iterator i = grp_graph_fwd.begin(); i != grp_graph_fwd.end(); ++i) {
        const FaceLoopGroup *grp = (*i).first;

        std::cerr << "GRP: " << grp << POLY(grp) << std::endl;

        std::set<const FaceLoopGroup *> &fwd_set = grp_graph_fwd[grp];
        std::set<const FaceLoopGroup *> &rev_set = grp_graph_rev[grp];
        std::cerr << "  FWD: ";
        for (std::set<const FaceLoopGroup *>::const_iterator j = fwd_set.begin(); j != fwd_set.end(); ++j) {
          std::cerr << " " << (*j) << POLY(*j);
        }
        std::cerr << std::endl;
        std::cerr << "  REV: ";
        for (std::set<const FaceLoopGroup *>::const_iterator j = rev_set.begin(); j != rev_set.end(); ++j) {
          std::cerr << " " << (*j) << POLY(*j);
        }
        std::cerr << std::endl;
      }
#endif

      for (FLGroupList::iterator i = a_loops_grouped.begin(); i != a_loops_grouped.end(); ++i) {
        collector.collect(&*i, hooks);
      }

      for (FLGroupList::iterator i = b_loops_grouped.begin(); i != b_loops_grouped.end(); ++i) {
        collector.collect(&*i, hooks);
      }

      // traceIntersectionGraph(shared_edges, a_loops_grouped, b_loops_grouped, a_edge_map, b_edge_map);
    }

  }
}
