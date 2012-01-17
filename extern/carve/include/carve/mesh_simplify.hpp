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

#include <carve/carve.hpp>
#include <carve/mesh.hpp>
#include <carve/mesh_ops.hpp>
#include <carve/geom2d.hpp>
#include <carve/heap.hpp>
#include <carve/rtree.hpp>
#include <carve/triangle_intersection.hpp>

#include <fstream>
#include <string>
#include <utility>
#include <set>
#include <algorithm>
#include <vector>

#include "write_ply.hpp"


namespace carve {
  namespace mesh {


    class MeshSimplifier {
      typedef carve::mesh::MeshSet<3> meshset_t;
      typedef carve::mesh::Mesh<3> mesh_t;
      typedef mesh_t::vertex_t vertex_t;
      typedef vertex_t::vector_t vector_t;
      typedef mesh_t::edge_t edge_t;
      typedef mesh_t::face_t face_t;
      typedef face_t::aabb_t aabb_t;

      typedef carve::geom::RTreeNode<3, carve::mesh::Face<3> *> face_rtree_t;


      struct EdgeInfo {
        edge_t *edge;
        double delta_v;

        double c[4];
        double l[2], t1[2], t2[2];
        size_t heap_idx;

        void update() {
          const vertex_t *v1 = edge->vert;
          const vertex_t *v2 = edge->next->vert;
          const vertex_t *v3 = edge->next->next->vert;
          const vertex_t *v4 = edge->rev ? edge->rev->next->next->vert : NULL;

          l[0] = (v1->v - v2->v).length();

          t1[0] = (v3->v - v1->v).length();
          t1[1] = (v3->v - v2->v).length();

          c[0] = std::max((t1[0] + t1[1]) / l[0] - 1.0, 0.0);

          if (v4) {
            l[1] = (v3->v - v4->v).length();
            t2[0] = (v4->v - v1->v).length();
            t2[1] = (v4->v - v2->v).length();
            c[1] = std::max((t2[0] + t2[1]) / l[0] - 1.0, 0.0);
            c[2] = std::max((t1[0] + t2[0]) / l[1] - 1.0, 0.0);
            c[3] = std::max((t1[1] + t2[1]) / l[1] - 1.0, 0.0);
            delta_v = carve::geom3d::tetrahedronVolume(v1->v, v2->v, v3->v, v4->v);
          } else {
            l[1] = 0.0;
            t2[0] = t2[1] = 0.0;
            c[1] = c[2] = c[3] = 0.0;
            delta_v = 0.0;
          }
        }

        EdgeInfo(edge_t *e) : edge(e) {
          update();
        }

        EdgeInfo() : edge(NULL) {
          delta_v = 0.0;
          c[0] = c[1] = c[2] = c[3] = 0.0;
          l[0] = l[1] = 0.0;
          t1[0] = t1[1] = 0.0;
          t2[0] = t2[1] = 0.0;
        }

        struct NotifyPos {
          void operator()(EdgeInfo *edge, size_t pos) const { edge->heap_idx = pos; }
          void operator()(EdgeInfo &edge, size_t pos) const { edge.heap_idx = pos; }
        };
      };



      struct FlippableBase {
        double min_dp;

        FlippableBase(double _min_dp = 0.0) : min_dp(_min_dp) {
        }

        bool open(const EdgeInfo *e) const {
          return e->edge->rev == NULL;
        }

        bool wouldCreateDegenerateEdge(const EdgeInfo *e) const {
          return e->edge->prev->vert == e->edge->rev->prev->vert;
        }

        bool flippable_DotProd(const EdgeInfo *e) const { 
          using carve::geom::dot;
          using carve::geom::cross;

          if (open(e)) return false;

          edge_t *edge = e->edge;

          const vertex_t *v1 = edge->vert;
          const vertex_t *v2 = edge->next->vert;
          const vertex_t *v3 = edge->next->next->vert;
          const vertex_t *v4 = edge->rev->next->next->vert;

          if (dot(cross(v3->v - v2->v, v1->v - v2->v).normalized(),
                  cross(v4->v - v1->v, v2->v - v1->v).normalized()) < min_dp) return false;

          if (dot(cross(v3->v - v4->v, v1->v - v4->v).normalized(),
                  cross(v4->v - v3->v, v2->v - v3->v).normalized()) < min_dp) return false;

          return true;
        }

        virtual bool canFlip(const EdgeInfo *e) const {
          return !open(e) && !wouldCreateDegenerateEdge(e) && score(e) > 0.0;
        }

        virtual double score(const EdgeInfo *e) const {
          return std::min(e->c[2], e->c[3]) - std::min(e->c[0], e->c[1]);
        }

        class Priority {
          Priority &operator=(const Priority &);
          const FlippableBase &flip;

        public:
          Priority(const FlippableBase &_flip) : flip(_flip) {}
          bool operator()(const EdgeInfo *a, const EdgeInfo *b) const { return flip.score(a) > flip.score(b); }
        };

        Priority priority() const {
          return Priority(*this);
        }
      };



      struct FlippableConservative : public FlippableBase {
        FlippableConservative() : FlippableBase(0.0) {
        }

        bool connectsAlmostCoplanarFaces(const EdgeInfo *e) const {
          // XXX: remove hard coded constants.
          if (e->c[0] < 1e-10 || e->c[1] < 1e-10) return true;
          return fabs(carve::geom::dot(e->edge->face->plane.N, e->edge->rev->face->plane.N) - 1.0) < 1e-10;
        }

        bool connectsExactlyCoplanarFaces(const EdgeInfo *e) const {
          edge_t *edge = e->edge;
          return
            carve::geom3d::orient3d(edge->vert->v,
                                    edge->next->vert->v,
                                    edge->next->next->vert->v,
                                    edge->rev->next->next->vert->v) == 0.0 &&
            carve::geom3d::orient3d(edge->rev->vert->v,
                                    edge->rev->next->vert->v,
                                    edge->rev->next->next->vert->v,
                                    edge->next->next->vert->v) == 0.0;
        }

        virtual bool canFlip(const EdgeInfo *e) const {
          return FlippableBase::canFlip(e) && connectsExactlyCoplanarFaces(e) && flippable_DotProd(e);
        }
      };



      struct FlippableColinearPair : public FlippableBase {

        FlippableColinearPair() {
        }


        virtual double score(const EdgeInfo *e) const {
          return e->l[0] - e->l[1];
        }

        virtual bool canFlip(const EdgeInfo *e) const {
          if (!FlippableBase::canFlip(e)) return false;

          if (e->c[0] > 1e-3 || e->c[1] > 1e-3) return false;

          return true;
        }
      };



      struct Flippable : public FlippableBase {
        double min_colinearity;
        double min_delta_v;

        Flippable(double _min_colinearity,
                  double _min_delta_v,
                  double _min_normal_angle) :
            FlippableBase(cos(_min_normal_angle)),
            min_colinearity(_min_colinearity),
            min_delta_v(_min_delta_v) {
        }


        virtual bool canFlip(const EdgeInfo *e) const {
          if (!FlippableBase::canFlip(e)) return false;

          if (fabs(e->delta_v) > min_delta_v) return false;

          // if (std::min(e->c[0], e->c[1]) > min_colinearity) return false;

          return flippable_DotProd(e);
        }
      };



      struct EdgeMerger {
        double min_edgelen;

        virtual bool canMerge(const EdgeInfo *e) const {
          return e->l[0] <= min_edgelen;
        }

        EdgeMerger(double _min_edgelen) : min_edgelen(_min_edgelen) {
        }

        double score(const EdgeInfo *e) const {
          return min_edgelen - e->l[0];
        }

        class Priority {
          Priority &operator=(const Priority &);

        public:
          const EdgeMerger &merger;
          Priority(const EdgeMerger &_merger) : merger(_merger) {
          }
          bool operator()(const EdgeInfo *a, const EdgeInfo *b) const {
            // collapse edges in order from shortest to longest.
            return merger.score(a) < merger.score(b);
          }
        };

        Priority priority() const {
          return Priority(*this);
        }
      };



      typedef std::unordered_map<edge_t *, EdgeInfo *> edge_info_map_t;
      std::unordered_map<edge_t *, EdgeInfo *> edge_info;



      void initEdgeInfo(mesh_t *mesh) {
        for (size_t i = 0; i < mesh->faces.size(); ++i) {
          edge_t *e = mesh->faces[i]->edge;
          do {
            edge_info[e] = new EdgeInfo(e);
            e = e->next;
          } while (e != mesh->faces[i]->edge);
        }
      }



      void initEdgeInfo(meshset_t *meshset) {
        for (size_t m = 0; m < meshset->meshes.size(); ++m) {
          mesh_t *mesh = meshset->meshes[m];
          initEdgeInfo(mesh);
        }
      }



      void clearEdgeInfo() {
        for (edge_info_map_t::iterator i = edge_info.begin(); i != edge_info.end(); ++i) {
          delete (*i).second;
        }
      }



      void updateEdgeFlipHeap(std::vector<EdgeInfo *> &edge_heap,
                              edge_t *edge,
                              const FlippableBase &flipper) {
        std::unordered_map<edge_t *, EdgeInfo *>::const_iterator i = edge_info.find(edge);
        CARVE_ASSERT(i != edge_info.end());
        EdgeInfo *e = (*i).second;

        bool heap_pre = e->heap_idx != ~0U;
        (*i).second->update();
        bool heap_post = edge->v1() < edge->v2() && flipper.canFlip(e);

        if (!heap_pre && heap_post) {
          edge_heap.push_back(e);
          carve::heap::push_heap(edge_heap.begin(),
                                 edge_heap.end(),
                                 flipper.priority(),
                                 EdgeInfo::NotifyPos());
        } else if (heap_pre && !heap_post) {
          CARVE_ASSERT(edge_heap[e->heap_idx] == e);
          carve::heap::remove_heap(edge_heap.begin(),
                                   edge_heap.end(),
                                   edge_heap.begin() + e->heap_idx,
                                   flipper.priority(),
                                   EdgeInfo::NotifyPos());
          CARVE_ASSERT(edge_heap.back() == e);
          edge_heap.pop_back();
          e->heap_idx = ~0U;
        } else if (heap_pre && heap_post) {
          CARVE_ASSERT(edge_heap[e->heap_idx] == e);
          carve::heap::adjust_heap(edge_heap.begin(),
                                   edge_heap.end(),
                                   edge_heap.begin() + e->heap_idx,
                                   flipper.priority(),
                                   EdgeInfo::NotifyPos());
          CARVE_ASSERT(edge_heap[e->heap_idx] == e);
        }
      }


      std::string vk(const vertex_t *v1,
                     const vertex_t *v2,
                     const vertex_t *v3) {
        const vertex_t *v[3];
        v[0] = v1; v[1] = v2; v[2] = v3;
        std::sort(v, v+3);
        std::ostringstream s;
        s << v[0] << ";" << v[1] << ";" << v[2];
        return s.str();
      }

      std::string vk(const face_t *f) { return vk(f->edge->vert, f->edge->next->vert, f->edge->next->next->vert); }

      int mapTriangle(const face_t *face,
                      const vertex_t *remap1, const vertex_t *remap2,
                      const vector_t &tgt,
                      vector_t tri[3]) {
        edge_t *edge = face->edge;
        int n_remaps = 0;
        for (size_t i = 0; i < 3; edge = edge->next, ++i) {
          if      (edge->vert == remap1) { tri[i] = tgt; ++n_remaps; }
          else if (edge->vert == remap2) { tri[i] = tgt; ++n_remaps; }
          else                           { tri[i] = edge->vert->v;   }
        }
        return n_remaps;
      }

      template<typename iter1_t, typename iter2_t>
      int countIntersectionPairs(iter1_t fabegin, iter1_t faend,
                                 iter2_t fbbegin, iter2_t fbend,
                                 const vertex_t *remap1, const vertex_t *remap2,
                                 const vector_t &tgt) {
        vector_t tri_a[3], tri_b[3];
        int remap_a, remap_b;
        std::set<std::pair<const face_t *, const face_t *> > ints;

        for (iter1_t i = fabegin; i != faend; ++i) {
          remap_a = mapTriangle(*i, remap1, remap2, tgt, tri_a);
          if (remap_a >= 2) continue;
          for (iter2_t j = fbbegin; j != fbend; ++j) {
            remap_b = mapTriangle(*j, remap1, remap2, tgt, tri_b);
            if (remap_b >= 2) continue;
            if (carve::geom::triangle_intersection_exact(tri_a, tri_b) == carve::geom::TR_TYPE_INT) {
              ints.insert(std::make_pair(std::min(*i, *j), std::max(*i, *j)));
            }
          }
        }

        return ints.size();
      }

      int countIntersections(const vertex_t *v1,
                             const vertex_t *v2,
                             const vertex_t *v3,
                             const std::vector<face_t *> &faces) {
        int n_int = 0;
        vector_t tri_a[3], tri_b[3];
        tri_a[0] = v1->v;
        tri_a[1] = v2->v;
        tri_a[2] = v3->v;

        for (std::vector<face_t *>::const_iterator i = faces.begin(); i != faces.end(); ++i) {
          face_t *fb = *i;
          if (fb->nEdges() != 3) continue;
          tri_b[0] = fb->edge->vert->v;
          tri_b[1] = fb->edge->next->vert->v;
          tri_b[2] = fb->edge->next->next->vert->v;

          if (carve::geom::triangle_intersection_exact(tri_a, tri_b) == carve::geom::TR_TYPE_INT) {
            n_int++;
          }
        }
        return n_int;
      }



      int _findSelfIntersections(const face_rtree_t *a_node,
                                 const face_rtree_t *b_node,
                                 bool descend_a = true) {
        int r = 0;

        if (!a_node->bbox.intersects(b_node->bbox)) {
          return 0;
        }

        if (a_node->child && (descend_a || !b_node->child)) {
          for (face_rtree_t *node = a_node->child; node; node = node->sibling) {
            r += _findSelfIntersections(node, b_node, false);
          }
        } else if (b_node->child) {
          for (face_rtree_t *node = b_node->child; node; node = node->sibling) {
            r += _findSelfIntersections(a_node, node, true);
          }
        } else {
          for (size_t i = 0; i < a_node->data.size(); ++i) {
            face_t *fa = a_node->data[i];
            if (fa->nVertices() != 3) continue;

            aabb_t aabb_a = fa->getAABB();

            vector_t tri_a[3];
            tri_a[0] = fa->edge->vert->v;
            tri_a[1] = fa->edge->next->vert->v;
            tri_a[2] = fa->edge->next->next->vert->v;

            if (!aabb_a.intersects(b_node->bbox)) continue;

            for (size_t j = 0; j < b_node->data.size(); ++j) {
              face_t *fb = b_node->data[j];
              if (fb->nVertices() != 3) continue;

              vector_t tri_b[3];
              tri_b[0] = fb->edge->vert->v;
              tri_b[1] = fb->edge->next->vert->v;
              tri_b[2] = fb->edge->next->next->vert->v;
              
              if (carve::geom::triangle_intersection_exact(tri_a, tri_b) == carve::geom::TR_TYPE_INT) {
                ++r;
              }
            }
          }
        }

        return r;
      }



      int countSelfIntersections(meshset_t *meshset) {
        int n_ints = 0;
        face_rtree_t *tree = face_rtree_t::construct_STR(meshset->faceBegin(), meshset->faceEnd(), 4, 4);

        for (meshset_t::face_iter f = meshset->faceBegin(); f != meshset->faceEnd(); ++f) {
          face_t *fa = *f;
          if (fa->nVertices() != 3) continue;

          vector_t tri_a[3];
          tri_a[0] = fa->edge->vert->v;
          tri_a[1] = fa->edge->next->vert->v;
          tri_a[2] = fa->edge->next->next->vert->v;

          std::vector<face_t *> near_faces;
          tree->search(fa->getAABB(), std::back_inserter(near_faces));

          for (size_t f2 = 0; f2 < near_faces.size(); ++f2) {
            const face_t *fb = near_faces[f2];
            if (fb->nVertices() != 3) continue;

            if (fa >= fb) continue;

            vector_t tri_b[3];
            tri_b[0] = fb->edge->vert->v;
            tri_b[1] = fb->edge->next->vert->v;
            tri_b[2] = fb->edge->next->next->vert->v;

            if (carve::geom::triangle_intersection_exact(tri_a, tri_b) == carve::geom::TR_TYPE_INT) {
              ++n_ints;
            }
          }
        }

        delete tree;

        return n_ints;
      }

      size_t flipEdges(meshset_t *mesh,
                       const FlippableBase &flipper) {
        face_rtree_t *tree = face_rtree_t::construct_STR(mesh->faceBegin(), mesh->faceEnd(), 4, 4);

        size_t n_mods = 0;

        std::vector<EdgeInfo *> edge_heap;

        edge_heap.reserve(edge_info.size());

        for (edge_info_map_t::iterator i = edge_info.begin();
             i != edge_info.end();
             ++i) {
          EdgeInfo *e = (*i).second;
          e->update();
          if (e->edge->v1() < e->edge->v2() && flipper.canFlip(e)) {
            edge_heap.push_back(e);
          } else {
            e->heap_idx = ~0U;
          }
        }

        carve::heap::make_heap(edge_heap.begin(),
                               edge_heap.end(),
                               flipper.priority(),
                               EdgeInfo::NotifyPos());

        while (edge_heap.size()) {
//           std::cerr << "test" << std::endl;
//           for (size_t m = 0; m < mesh->meshes.size(); ++m) {
//             for (size_t f = 0; f < mesh->meshes[m]->faces.size(); ++f) {
//               if (mesh->meshes[m]->faces[f]->edge) mesh->meshes[m]->faces[f]->edge->validateLoop();
//             }
//           }

          carve::heap::pop_heap(edge_heap.begin(),
                                edge_heap.end(),
                                flipper.priority(),
                                EdgeInfo::NotifyPos());
          EdgeInfo *e = edge_heap.back();
//           std::cerr << "flip " << e << std::endl;
          edge_heap.pop_back();
          e->heap_idx = ~0U;

          aabb_t aabb;
          aabb = e->edge->face->getAABB();
          aabb.unionAABB(e->edge->rev->face->getAABB());

          std::vector<face_t *> overlapping;
          tree->search(aabb, std::back_inserter(overlapping));

          // overlapping.erase(e->edge->face);
          // overlapping.erase(e->edge->rev->face);

          const vertex_t *v1 = e->edge->vert;
          const vertex_t *v2 = e->edge->next->vert;
          const vertex_t *v3 = e->edge->next->next->vert;
          const vertex_t *v4 = e->edge->rev->next->next->vert;

          int n_int1 = countIntersections(v1, v2, v3, overlapping);
          int n_int2 = countIntersections(v2, v1, v4, overlapping);
          int n_int3 = countIntersections(v3, v4, v2, overlapping);
          int n_int4 = countIntersections(v4, v3, v1, overlapping);

          if ((n_int3 + n_int4) - (n_int1 + n_int2) > 0) {
            std::cerr << "delta[ints] = " << (n_int3 + n_int4) - (n_int1 + n_int2) << std::endl;
            // avoid creating a self intersection.
            continue;
          }

          n_mods++;
          CARVE_ASSERT(flipper.canFlip(e));
          edge_info[e->edge]->update();
          edge_info[e->edge->rev]->update();

          carve::mesh::flipTriEdge(e->edge);

          tree->updateExtents(aabb);

          updateEdgeFlipHeap(edge_heap, e->edge, flipper);
          updateEdgeFlipHeap(edge_heap, e->edge->rev, flipper);

          CARVE_ASSERT(!flipper.canFlip(e));

          updateEdgeFlipHeap(edge_heap, e->edge->next, flipper);
          updateEdgeFlipHeap(edge_heap, e->edge->next->next, flipper);
          updateEdgeFlipHeap(edge_heap, e->edge->rev->next, flipper);
          updateEdgeFlipHeap(edge_heap, e->edge->rev->next->next, flipper);
          updateEdgeFlipHeap(edge_heap, e->edge->next->rev, flipper);
          updateEdgeFlipHeap(edge_heap, e->edge->next->next->rev, flipper);
          updateEdgeFlipHeap(edge_heap, e->edge->rev->next->rev, flipper);
          updateEdgeFlipHeap(edge_heap, e->edge->rev->next->next->rev, flipper);
        }

        delete tree;

        return n_mods;
      }



      void removeFromEdgeMergeHeap(std::vector<EdgeInfo *> &edge_heap,
                                   EdgeInfo *edge,
                                   const EdgeMerger &merger) {
        if (edge->heap_idx != ~0U) {
          CARVE_ASSERT(edge_heap[edge->heap_idx] == edge);
          carve::heap::remove_heap(edge_heap.begin(),
                                   edge_heap.end(),
                                   edge_heap.begin() + edge->heap_idx,
                                   merger.priority(),
                                   EdgeInfo::NotifyPos());
          CARVE_ASSERT(edge_heap.back() == edge);
          edge_heap.pop_back();
          edge->heap_idx = ~0U;
        }
      }

      void updateEdgeMergeHeap(std::vector<EdgeInfo *> &edge_heap,
                               EdgeInfo *edge,
                               const EdgeMerger &merger) {
        bool heap_pre = edge->heap_idx != ~0U;
        edge->update();
        bool heap_post = merger.canMerge(edge);

        if (!heap_pre && heap_post) {
          edge_heap.push_back(edge);
          carve::heap::push_heap(edge_heap.begin(),
                                 edge_heap.end(),
                                 merger.priority(),
                                 EdgeInfo::NotifyPos());
        } else if (heap_pre && !heap_post) {
          CARVE_ASSERT(edge_heap[edge->heap_idx] == edge);
          carve::heap::remove_heap(edge_heap.begin(),
                                   edge_heap.end(),
                                   edge_heap.begin() + edge->heap_idx,
                                   merger.priority(),
                                   EdgeInfo::NotifyPos());
          CARVE_ASSERT(edge_heap.back() == edge);
          edge_heap.pop_back();
          edge->heap_idx = ~0U;
        } else if (heap_pre && heap_post) {
          CARVE_ASSERT(edge_heap[edge->heap_idx] == edge);
          carve::heap::adjust_heap(edge_heap.begin(),
                                   edge_heap.end(),
                                   edge_heap.begin() + edge->heap_idx,
                                   merger.priority(),
                                   EdgeInfo::NotifyPos());
          CARVE_ASSERT(edge_heap[edge->heap_idx] == edge);
        }
      }



      // collapse edges edges based upon the predicate implemented by EdgeMerger.
      size_t collapseEdges(meshset_t *mesh,
                           const EdgeMerger &merger) {
        face_rtree_t *tree = face_rtree_t::construct_STR(mesh->faceBegin(), mesh->faceEnd(), 4, 4);

        size_t n_mods = 0;

        std::vector<EdgeInfo *> edge_heap;
        std::unordered_map<vertex_t *, std::set<EdgeInfo *> > vert_to_edges;

        edge_heap.reserve(edge_info.size());

        for (edge_info_map_t::iterator i = edge_info.begin();
             i != edge_info.end();
             ++i) {
          EdgeInfo *e = (*i).second;

          vert_to_edges[e->edge->v1()].insert(e);
          vert_to_edges[e->edge->v2()].insert(e);

          if (merger.canMerge(e)) {
            edge_heap.push_back(e);
          } else {
            e->heap_idx = ~0U;
          }
        }

        carve::heap::make_heap(edge_heap.begin(),
                               edge_heap.end(),
                               merger.priority(),
                               EdgeInfo::NotifyPos());

        while (edge_heap.size()) {
//           std::cerr << "test" << std::endl;
//           for (size_t m = 0; m < mesh->meshes.size(); ++m) {
//             for (size_t f = 0; f < mesh->meshes[m]->faces.size(); ++f) {
//               if (mesh->meshes[m]->faces[f]->edge) mesh->meshes[m]->faces[f]->edge->validateLoop();
//             }
//           }
          carve::heap::pop_heap(edge_heap.begin(),
                                edge_heap.end(),
                                merger.priority(),
                                EdgeInfo::NotifyPos());
          EdgeInfo *e = edge_heap.back();
          edge_heap.pop_back();
          e->heap_idx = ~0U;

          edge_t *edge = e->edge;
          vertex_t *v1 = edge->v1();
          vertex_t *v2 = edge->v2();

          std::set<face_t *> affected_faces;
          for (std::set<EdgeInfo *>::iterator f = vert_to_edges[v1].begin();
               f != vert_to_edges[v1].end();
               ++f) {
            affected_faces.insert((*f)->edge->face);
            affected_faces.insert((*f)->edge->rev->face);
          }
          for (std::set<EdgeInfo *>::iterator f = vert_to_edges[v2].begin();
               f != vert_to_edges[v2].end();
               ++f) {
            affected_faces.insert((*f)->edge->face);
            affected_faces.insert((*f)->edge->rev->face);
          }

          std::vector<EdgeInfo *> edges_to_merge;
          std::vector<EdgeInfo *> v1_incident;
          std::vector<EdgeInfo *> v2_incident;

          std::set_intersection(vert_to_edges[v1].begin(), vert_to_edges[v1].end(),
                                vert_to_edges[v2].begin(), vert_to_edges[v2].end(),
                                std::back_inserter(edges_to_merge));

          CARVE_ASSERT(edges_to_merge.size() > 0);

          std::set_difference(vert_to_edges[v1].begin(), vert_to_edges[v1].end(),
                              edges_to_merge.begin(), edges_to_merge.end(),
                              std::back_inserter(v1_incident));
          std::set_difference(vert_to_edges[v2].begin(), vert_to_edges[v2].end(),
                              edges_to_merge.begin(), edges_to_merge.end(),
                              std::back_inserter(v2_incident));

          vector_t aabb_min, aabb_max;
          assign_op(aabb_min, v1->v, v2->v, carve::util::min_functor());
          assign_op(aabb_max, v1->v, v2->v, carve::util::max_functor());
          
          for (size_t i = 0; i < v1_incident.size(); ++i) {
            assign_op(aabb_min, aabb_min, v1_incident[i]->edge->v1()->v, carve::util::min_functor());
            assign_op(aabb_max, aabb_max, v1_incident[i]->edge->v1()->v, carve::util::max_functor());
            assign_op(aabb_min, aabb_min, v1_incident[i]->edge->v2()->v, carve::util::min_functor());
            assign_op(aabb_max, aabb_max, v1_incident[i]->edge->v2()->v, carve::util::max_functor());
          }

          for (size_t i = 0; i < v2_incident.size(); ++i) {
            assign_op(aabb_min, aabb_min, v2_incident[i]->edge->v1()->v, carve::util::min_functor());
            assign_op(aabb_max, aabb_max, v2_incident[i]->edge->v1()->v, carve::util::max_functor());
            assign_op(aabb_min, aabb_min, v2_incident[i]->edge->v2()->v, carve::util::min_functor());
            assign_op(aabb_max, aabb_max, v2_incident[i]->edge->v2()->v, carve::util::max_functor());
          }

          aabb_t aabb;
          aabb.fit(aabb_min, aabb_max);

          std::vector<face_t *> near_faces;
          tree->search(aabb, std::back_inserter(near_faces));

          double frac = 0.5; // compute this based upon v1_incident and v2_incident?
          vector_t merge = frac * v1->v + (1 - frac) * v2->v;

          int i1 = countIntersectionPairs(affected_faces.begin(), affected_faces.end(),
                                          near_faces.begin(), near_faces.end(),
                                          NULL, NULL, merge);
          int i2 = countIntersectionPairs(affected_faces.begin(), affected_faces.end(),
                                          near_faces.begin(), near_faces.end(),
                                          v1, v2, merge);
          if (i2 != i1) {
            std::cerr << "near faces: " << near_faces.size() << " affected faces: " << affected_faces.size() << std::endl;
            std::cerr << "merge delta[ints] = " << i2 - i1 << " pre: " << i1 << " post: " << i2 << std::endl;
            if (i2 > i1) continue;
          }

          std::cerr << "collapse " << e << std::endl;

          v2->v = merge;
          ++n_mods;

          for (size_t i = 0; i < v1_incident.size(); ++i) {
            if (v1_incident[i]->edge->vert == v1) {
              v1_incident[i]->edge->vert = v2;
            }
          }

          for (size_t i = 0; i < v1_incident.size(); ++i) {
            updateEdgeMergeHeap(edge_heap, v1_incident[i], merger);
          }

          for (size_t i = 0; i < v2_incident.size(); ++i) {
            updateEdgeMergeHeap(edge_heap, v2_incident[i], merger);
          }

          vert_to_edges[v2].insert(vert_to_edges[v1].begin(), vert_to_edges[v1].end());
          vert_to_edges.erase(v1);

          for (size_t i = 0; i < edges_to_merge.size(); ++i) {
            EdgeInfo *e = edges_to_merge[i];

            removeFromEdgeMergeHeap(edge_heap, e, merger);
            edge_info.erase(e->edge);

            vert_to_edges[v1].erase(e);
            vert_to_edges[v2].erase(e);

            face_t *f1 = e->edge->face;

            e->edge->removeHalfEdge();

            if (f1->n_edges == 2) {
              edge_t *e1 = f1->edge;
              edge_t *e2 = f1->edge->next;
              if (e1->rev) e1->rev->rev = e2->rev;
              if (e2->rev) e2->rev->rev = e1->rev;
              EdgeInfo *e1i = edge_info[e1];
              EdgeInfo *e2i = edge_info[e2];
              CARVE_ASSERT(e1i != NULL);
              CARVE_ASSERT(e2i != NULL);
              vert_to_edges[e1->v1()].erase(e1i);
              vert_to_edges[e1->v2()].erase(e1i);
              vert_to_edges[e2->v1()].erase(e2i);
              vert_to_edges[e2->v2()].erase(e2i);
              removeFromEdgeMergeHeap(edge_heap, e1i, merger);
              removeFromEdgeMergeHeap(edge_heap, e2i, merger);
              edge_info.erase(e1);
              edge_info.erase(e2);
              f1->clearEdges();
              tree->remove(f1, aabb);

              delete e1i;
              delete e2i;
            }
            delete e;
          }

          tree->updateExtents(aabb);
        }

        delete tree;

        return n_mods;
      }



      size_t mergeCoplanarFaces(mesh_t *mesh, double min_normal_angle) {
        std::unordered_set<edge_t *> coplanar_face_edges;
        double min_dp = cos(min_normal_angle);
        size_t n_merge = 0;

        for (size_t i = 0; i < mesh->closed_edges.size(); ++i) {
          edge_t *e = mesh->closed_edges[i];
          face_t *f1 = e->face;
          face_t *f2 = e->rev->face;

          if (carve::geom::dot(f1->plane.N, f2->plane.N) < min_dp) {
            continue;
          }

          coplanar_face_edges.insert(std::min(e, e->rev));
        }

        while (coplanar_face_edges.size()) {
          edge_t *edge = *coplanar_face_edges.begin();
          if (edge->face == edge->rev->face) {
            coplanar_face_edges.erase(edge);
            continue;
          }

          edge_t *removed = edge->mergeFaces();
          if (removed == NULL) {
            coplanar_face_edges.erase(edge);
            ++n_merge;
          } else {
            edge_t *e = removed;
            do {
              edge_t *n = e->next;
              coplanar_face_edges.erase(std::min(e, e->rev));
              delete e->rev;
              delete e;
              e = n;
            } while (e != removed);
          }
        }
        return n_merge;
      }



      uint8_t affected_axes(const face_t *face) {
        uint8_t r = 0;
        if (fabs(carve::geom::dot(face->plane.N, carve::geom::VECTOR(1,0,0))) > 0.001) r |= 1;
        if (fabs(carve::geom::dot(face->plane.N, carve::geom::VECTOR(0,1,0))) > 0.001) r |= 2;
        if (fabs(carve::geom::dot(face->plane.N, carve::geom::VECTOR(0,0,1))) > 0.001) r |= 4;
        return r;
      }



      double median(std::vector<double> &v) {
        if (v.size() & 1) {
          size_t N = v.size() / 2 + 1;
          std::nth_element(v.begin(), v.begin() + N, v.end());
          return v[N];
        } else {
          size_t N = v.size() / 2;
          std::nth_element(v.begin(), v.begin() + N, v.end());
          return (v[N] + *std::min_element(v.begin() + N + 1, v.end())) / 2.0;
        }
      }



      double harmonicmean(const std::vector<double> &v) {
        double m = 0.0;
        for (size_t i = 0; i < v.size(); ++i) {
          m *= v[i];
        }
        return pow(m, 1.0 / v.size());
      }



      double mean(const std::vector<double> &v) {
        double m = 0.0;
        for (size_t i = 0; i < v.size(); ++i) {
          m += v[i];
        }
        return m / v.size();
      }



      template<typename iter_t>
      void snapFaces(iter_t begin, iter_t end, double grid, int axis) {
        std::set<vertex_t *> vertices;
        for (iter_t i = begin; i != end; ++i) {
          face_t *face = *i;
          edge_t *edge = face->edge;
          do {
            vertices.insert(edge->vert);
            edge = edge->next;
          } while (edge != face->edge);
        }

        std::vector<double> pos;
        pos.reserve(vertices.size());
        for (std::set<vertex_t *>::iterator i = vertices.begin(); i != vertices.end(); ++i) {
          pos.push_back((*i)->v.v[axis]);
        }

        double med = median(pos);

        double snap_pos = med;
        if (grid) snap_pos = round(snap_pos / grid) * grid;

        for (std::set<vertex_t *>::iterator i = vertices.begin(); i != vertices.end(); ++i) {
          (*i)->v.v[axis] = snap_pos;
        }

        for (iter_t i = begin; i != end; ++i) {
          face_t *face = *i;
          face->recalc();
          edge_t *edge = face->edge;
          do {
            if (edge->rev && edge->rev->face) edge->rev->face->recalc();
            edge = edge->next;
          } while (edge != face->edge);
        }
      }

      carve::geom::plane<3> quantizePlane(const face_t *face,
                                          int angle_xy_quantization,
                                          int angle_z_quantization) {
        if (!angle_xy_quantization && !angle_z_quantization) {
          return face->plane;
        }
        carve::geom::vector<3> normal = face->plane.N;

        if (angle_z_quantization) {
          if (normal.x || normal.y) {
            double a = asin(std::min(std::max(normal.z, 0.0), 1.0));
            a = round(a * angle_z_quantization / (M_PI * 2)) * (M_PI * 2) / angle_z_quantization;
            normal.z = sin(a);
            double s = sqrt((1 - normal.z * normal.z) / (normal.x * normal.x + normal.y * normal.y));
            normal.x = normal.x * s;
            normal.y = normal.y * s;
          }
        }
        if (angle_xy_quantization) {
          if (normal.x || normal.y) {
            double a = atan2(normal.y, normal.x);
            a = round(a * angle_xy_quantization / (M_PI * 2)) * (M_PI * 2) / angle_xy_quantization;
            double s = sqrt(1 - normal.z * normal.z);
            s = std::min(std::max(s, 0.0), 1.0);
            normal.x = cos(a) * s;
            normal.y = sin(a) * s;
          }
        }

        std::cerr << "normal = " << normal << std::endl;

        std::vector<double> d_vec;
        d_vec.reserve(face->nVertices());
        edge_t *e = face->edge;
        do {
          d_vec.push_back(-carve::geom::dot(normal, e->vert->v));
          e = e->next;
        } while (e != face->edge);

        return carve::geom::plane<3>(normal, mean(d_vec));
      }



      double summedError(const carve::geom::vector<3> &vert, const std::list<carve::geom::plane<3> > &planes) {
        double d = 0;
        for (std::list<carve::geom::plane<3> >::const_iterator i = planes.begin(); i != planes.end(); ++i) {
          d += fabs(carve::geom::distance2(*i, vert));
        }
        return d;
      }



      double minimize(carve::geom::vector<3> &vert, const std::list<carve::geom::plane<3> > &planes, int axis) {
        double num = 0.0;
        double den = 0.0;
        int a1 = (axis + 1) % 3;
        int a2 = (axis + 2) % 3;
        for (std::list<carve::geom::plane<3> >::const_iterator i = planes.begin(); i != planes.end(); ++i) {
          const carve::geom::vector<3> &N = (*i).N;
          const double d = (*i).d;
          den += N.v[axis] * N.v[axis];
          num -= N.v[axis] * (N.v[a1] * vert.v[a1] + N.v[a2] * vert.v[a2] + d);
        }
        if (fabs(den) < 1e-5) return vert.v[axis];
        return num / den;
      }



      size_t cleanFaceEdges(mesh_t *mesh) {
        size_t n_removed = 0;
        for (size_t i = 0; i < mesh->faces.size(); ++i) {
          face_t *face = mesh->faces[i];
          edge_t *start = face->edge;
          edge_t *edge = start;
          do {
            if (edge->next == edge->rev || edge->prev == edge->rev) {
              edge = edge->removeEdge();
              ++n_removed;
              start = edge->prev;
            } else {
              edge = edge->next;
            }
          } while (edge != start);
        }
        return n_removed;
      }



      size_t cleanFaceEdges(meshset_t *mesh) {
        size_t n_removed = 0;
        for (size_t i = 0; i < mesh->meshes.size(); ++i) {
          n_removed += cleanFaceEdges(mesh->meshes[i]);
        }
        return n_removed;
      }



      void removeRemnantFaces(mesh_t *mesh) {
        size_t n = 0;
        for (size_t i = 0; i < mesh->faces.size(); ++i) {
          if (mesh->faces[i]->nEdges() == 0) {
            delete mesh->faces[i];
          } else {
            mesh->faces[n++] = mesh->faces[i];
          }
        }
        mesh->faces.resize(n);
      }



      void removeRemnantFaces(meshset_t *mesh) {
        for (size_t i = 0; i < mesh->meshes.size(); ++i) {
          removeRemnantFaces(mesh->meshes[i]);
        }
      }



      edge_t *removeFin(edge_t *e) {
        // e and e->next are shared with the same reverse triangle.
        edge_t *e1 = e->prev;
        edge_t *e2 = e->rev->next;
        CARVE_ASSERT(e1->v2() == e2->v1());
        CARVE_ASSERT(e2->v2() == e1->v1());

        CARVE_ASSERT(e1->rev != e2 && e2->rev != e1);

        edge_t *e1r = e1->rev;
        edge_t *e2r = e2->rev;
        if (e1r) e1r->rev = e2r;
        if (e2r) e2r->rev = e1r;

        face_t *f1 = e1->face;
        face_t *f2 = e2->face;
        f1->clearEdges();
        f2->clearEdges();

        return e1r;
      }

      size_t removeFin(face_t *face) {
        if (face->edge == NULL || face->nEdges() != 3) return 0;
        edge_t *e = face->edge;
        do {
          if (e->rev != NULL) {
            face_t *revface = e->rev->face;
            if (revface->nEdges() == 3) {
              if (e->next->rev && e->next->rev->face == revface) {
                if (e->next->next->rev && e->next->next->rev->face == revface) {
                  // isolated tripair
                  face->clearEdges();
                  revface->clearEdges();
                  return 1;
                }
                // fin
                edge_t *spliced_edge = removeFin(e);
                return 1 + removeFin(spliced_edge->face);
              }
            }
          }
          e = e->next;
        } while (e != face->edge);
        return 0;
      }



    public:
      // Merge adjacent coplanar faces (where coplanar is determined
      // by dot-product >= cos(min_normal_angle)).
      size_t mergeCoplanarFaces(meshset_t *meshset, double min_normal_angle) {
        size_t n_removed = 0;
        for (size_t i = 0; i < meshset->meshes.size(); ++i) {
          n_removed += mergeCoplanarFaces(meshset->meshes[i], min_normal_angle);
          removeRemnantFaces(meshset->meshes[i]);
          cleanFaceEdges(meshset->meshes[i]);
          meshset->meshes[i]->cacheEdges();
        }
        return n_removed;
      }

      size_t improveMesh_conservative(meshset_t *meshset) {
        initEdgeInfo(meshset);
        size_t modifications = flipEdges(meshset, FlippableConservative());
        clearEdgeInfo();
        return modifications;
      }



      size_t improveMesh(meshset_t *meshset,
                         double min_colinearity,
                         double min_delta_v,
                         double min_normal_angle) {
        initEdgeInfo(meshset);
        size_t modifications = flipEdges(meshset, Flippable(min_colinearity, min_delta_v, min_normal_angle));
        clearEdgeInfo();
        return modifications;
      }



      size_t eliminateShortEdges(meshset_t *meshset,
                                 double min_length) {
        initEdgeInfo(meshset);
        size_t modifications = collapseEdges(meshset, EdgeMerger(min_length));
        removeRemnantFaces(meshset);
        clearEdgeInfo();
        return modifications;
      }



      // Snap vertices to grid, aligning almost flat axis-aligned
      // faces to the axis, and flattening other faces as much as is
      // possible. Passing a number less than DBL_MIN_EXPONENT (-1021)
      // turns off snapping to grid (but face alignment is still
      // performed).
      void snap(meshset_t *meshset,
                int log2_grid,
                int angle_xy_quantization = 0,
                int angle_z_quantization = 0) {
        double grid = 0.0;
        if (log2_grid >= std::numeric_limits<double>::min_exponent) grid = pow(2.0, (double)log2_grid);

        typedef std::unordered_map<face_t *, uint8_t> axis_influence_map_t;
        axis_influence_map_t axis_influence;

        typedef std::unordered_map<face_t *, std::set<face_t *> > interaction_graph_t;
        interaction_graph_t interacting_faces;

        for (size_t m = 0; m < meshset->meshes.size(); ++m) {
          mesh_t *mesh = meshset->meshes[m];
          for (size_t f = 0; f < mesh->faces.size(); ++f) {
            face_t *face = mesh->faces[f];
            axis_influence[face] = affected_axes(face);
          }
        }

        std::map<vertex_t *, std::list<carve::geom::plane<3> > > non_axis_vertices;
        std::unordered_map<vertex_t *, uint8_t> vertex_constraints;

        for (axis_influence_map_t::iterator i = axis_influence.begin(); i != axis_influence.end(); ++i) {
          face_t *face = (*i).first;
          uint8_t face_axes = (*i).second;
          edge_t *edge = face->edge;
          if (face_axes != 1 && face_axes != 2 && face_axes != 4) {
            do {
              non_axis_vertices[edge->vert].push_back(quantizePlane(face,
                                                                    angle_xy_quantization,
                                                                    angle_z_quantization));
              edge = edge->next;
            } while (edge != face->edge);
          } else {
            interacting_faces[face].insert(face);
            do {
              vertex_constraints[edge->vert] |= face_axes;

              if (edge->rev && edge->rev->face) {
                face_t *face2 = edge->rev->face;
                uint8_t face2_axes = axis_influence[face2];
                if (face2_axes == face_axes) {
                  interacting_faces[face].insert(face2);
                }
              }
              edge = edge->next;
            } while (edge != face->edge);
          }
        }

        while (interacting_faces.size()) {
          std::set<face_t *> face_set;
          uint8_t axes = 0;

          std::set<face_t *> open;
          open.insert((*interacting_faces.begin()).first);
          while (open.size()) {
            face_t *curr = *open.begin();
            open.erase(open.begin());
            face_set.insert(curr);
            axes |= axis_influence[curr];
            for (interaction_graph_t::data_type::iterator i = interacting_faces[curr].begin(), e = interacting_faces[curr].end(); i != e; ++i) {
              face_t *f = *i;
              if (face_set.find(f) != face_set.end()) continue;
              open.insert(f);
            }
          }

          switch (axes) {
          case 1: snapFaces(face_set.begin(), face_set.end(), grid, 0); break;
          case 2: snapFaces(face_set.begin(), face_set.end(), grid, 1); break;
          case 4: snapFaces(face_set.begin(), face_set.end(), grid, 2); break;
          default: CARVE_FAIL("should not be reached");
          }

          for (std::set<face_t *>::iterator i = face_set.begin(); i != face_set.end(); ++i) {
            interacting_faces.erase((*i));
          }
        }

        for (std::map<vertex_t *, std::list<carve::geom::plane<3> > >::iterator i = non_axis_vertices.begin(); i != non_axis_vertices.end(); ++i) {
          vertex_t *vert = (*i).first;
          std::list<carve::geom::plane<3> > &planes = (*i).second;
          uint8_t constraint = vertex_constraints[vert];

          if (constraint == 7) continue;

          double d = summedError(vert->v, planes);
          for (size_t N = 0; ; N = (N+1) % 3) {
            if (constraint & (1 << N)) continue;
            vert->v[N] = minimize(vert->v, planes, N);
            double d_next = summedError(vert->v, planes);
            if (d - d_next < 1e-20) break;
            d = d_next;
          }

          if (grid) {
            carve::geom::vector<3> v_best = vert->v;
            double d_best = 0.0;
            
            for (size_t axes = 0; axes < 8; ++axes) {
              carve::geom::vector<3> v = vert->v;
              for (size_t N = 0; N < 3; ++N) {
                if (constraint & (1 << N)) continue;
                if (axes & (1<<N)) {
                  v.v[N] = ceil(v.v[N] / grid) * grid;
                } else {
                  v.v[N] = floor(v.v[N] / grid) * grid;
                }
              }
              double d = summedError(v, planes);
              if (axes == 0 || d < d_best) {
                v_best = v;
                d_best = d;
              }
            }

            vert->v = v_best;
          }
        }
      }



      size_t simplify(meshset_t *meshset,
                      double min_colinearity,
                      double min_delta_v,
                      double min_normal_angle,
                      double min_length) {
        size_t modifications = 0;
        size_t n, n_flip, n_merge;

        initEdgeInfo(meshset);

        std::cerr << "initial merge" << std::endl;
        modifications = collapseEdges(meshset, EdgeMerger(0.0));
        removeRemnantFaces(meshset);

        do {
          n_flip = n_merge = 0;
          // std::cerr << "flip colinear pairs";
          // n = flipEdges(meshset, FlippableColinearPair());
          // std::cerr << " " << n << std::endl;
          // n_flip = n;

          std::cerr << "flip conservative";
          n = flipEdges(meshset, FlippableConservative());
          std::cerr << " " << n << std::endl;
          n_flip += n;

          std::cerr << "flip";
          n = flipEdges(meshset, Flippable(min_colinearity, min_delta_v, min_normal_angle));
          std::cerr << " " << n << std::endl;
          n_flip += n;

          std::cerr << "merge";
          n = collapseEdges(meshset, EdgeMerger(min_length));
          removeRemnantFaces(meshset);
          std::cerr << " " << n << std::endl;
          n_merge = n;

          modifications += n_flip + n_merge;
          std::cerr << "stats:" << n_flip << " " << n_merge << std::endl;
        } while (n_flip || n_merge);

        clearEdgeInfo();

        for (size_t i = 0; i < meshset->meshes.size(); ++i) {
          meshset->meshes[i]->cacheEdges();
        }

        return modifications;
      }

      
      
      size_t removeFins(mesh_t *mesh) {
        size_t n_removed = 0;
        for (size_t i = 0; i < mesh->faces.size(); ++i) {
          n_removed += removeFin(mesh->faces[i]);
        }
        if (n_removed) removeRemnantFaces(mesh);
        return n_removed;
      }



      size_t removeFins(meshset_t *meshset) {
        size_t n_removed = 0;
        for (size_t i = 0; i < meshset->meshes.size(); ++i) {
          n_removed += removeFins(meshset->meshes[i]);
        }
        return n_removed;
      }



      size_t removeLowVolumeManifolds(meshset_t *meshset, double min_abs_volume) {
        size_t n_removed;
        for (size_t i = 0; i < meshset->meshes.size(); ++i) {
          if (fabs(meshset->meshes[i]->volume()) < min_abs_volume) {
            delete meshset->meshes[i];
            meshset->meshes[i] = NULL;
            ++n_removed;
          }
        }
        meshset->meshes.erase(std::remove_if(meshset->meshes.begin(),
                                             meshset->meshes.end(),
                                             std::bind2nd(std::equal_to<mesh_t *>(), (mesh_t *)NULL)),
                              meshset->meshes.end());
        return n_removed;
      }

      struct point_enumerator_t {
        struct heapval_t {
          double dist;
          vector_t pt;
          heapval_t(double _dist, vector_t _pt) : dist(_dist), pt(_pt) {
          }
          heapval_t() {}
          bool operator==(const heapval_t &other) const { return dist == other.dist && pt == other.pt; }
          bool operator<(const heapval_t &other) const { return dist > other.dist || (dist == other.dist && pt > other.pt); }
        };

        vector_t origin;
        double rounding_fac;
        heapval_t last;
        std::vector<heapval_t> heap;

        point_enumerator_t(vector_t _origin, int _base, int _n_dp) : origin(_origin), rounding_fac(pow(_base, _n_dp)), last(-1.0, _origin), heap() {
          for (size_t i = 0; i < (1 << 3); ++i) {
            vector_t t = origin;
            for (size_t j = 0; j < 3; ++j) {
              if (i & (1U << j)) {
                t[j] = ceil(t[j] * rounding_fac) / rounding_fac;
              } else {
                t[j] = floor(t[j] * rounding_fac) / rounding_fac;
              }
            }
            heap.push_back(heapval_t(carve::geom::distance2(origin, t), t));
          }
          std::make_heap(heap.begin(), heap.end());
        }

        vector_t next() {
          heapval_t curr;
          do {
            CARVE_ASSERT(heap.size());
            std::pop_heap(heap.begin(), heap.end());
            curr = heap.back();
            heap.pop_back();
          } while (curr == last);

          vector_t t;

          for (int dx = -1; dx <= +1; ++dx) {
            t.x = floor(curr.pt.x * rounding_fac + dx) / rounding_fac;
            for (int dy = -1; dy <= +1; ++dy) {
              t.y = floor(curr.pt.y * rounding_fac + dy) / rounding_fac;
              for (int dz = -1; dz <= +1; ++dz) {
                t.z = floor(curr.pt.z * rounding_fac + dz) / rounding_fac;
                heapval_t h2(carve::geom::distance2(origin, t), t);
                if (h2 < curr) {
                  heap.push_back(h2);
                  std::push_heap(heap.begin(), heap.end());
                }
              }
            }
          }
          last = curr;
          return curr.pt;
        }
      };

      struct quantization_info_t {
        point_enumerator_t *pt;
        std::set<face_t *> faces;

        quantization_info_t() : pt(NULL), faces() {
        }

        ~quantization_info_t() {
          if (pt) delete pt;
        }

        aabb_t getAABB() const {
          std::set<face_t *>::iterator i = faces.begin();
          aabb_t aabb = (*i)->getAABB();
          while (++i != faces.end()) {
            aabb.unionAABB((*i)->getAABB());
          }
          return aabb;
        }
      };

      void selfIntersectionAwareQuantize(meshset_t *meshset, int base, int n_dp) {
        typedef std::unordered_map<vertex_t *, quantization_info_t> vfsmap_t;

        vfsmap_t vertex_qinfo;

        for (size_t m = 0; m < meshset->meshes.size(); ++m) {
          mesh_t *mesh = meshset->meshes[m];
          for (size_t f = 0; f < mesh->faces.size(); ++f) {
            face_t *face = mesh->faces[f];
            edge_t *e = face->edge;
            do {
              vertex_qinfo[e->vert].faces.insert(face);
              e = e->next;
            } while (e != face->edge);
          }
        }

        face_rtree_t *tree = face_rtree_t::construct_STR(meshset->faceBegin(), meshset->faceEnd(), 4, 4);

        for (vfsmap_t::iterator i = vertex_qinfo.begin(); i != vertex_qinfo.end(); ++i) {
          (*i).second.pt = new point_enumerator_t((*i).first->v, base, n_dp);
        }

        while (vertex_qinfo.size()) {
          std::vector<vertex_t *> quantized;

          std::cerr << "vertex_qinfo.size() == " << vertex_qinfo.size() << std::endl;

          for (vfsmap_t::iterator i = vertex_qinfo.begin(); i != vertex_qinfo.end(); ++i) {
            vertex_t *vert = (*i).first;
            quantization_info_t &qi = (*i).second;
            vector_t q_pt = qi.pt->next();
            aabb_t aabb = qi.getAABB();
            aabb.unionAABB(aabb_t(q_pt));

            std::vector<face_t *> overlapping;
            tree->search(aabb, std::back_inserter(overlapping));


            int n_intersections = countIntersectionPairs(qi.faces.begin(), qi.faces.end(),
                                                         overlapping.begin(), overlapping.end(),
                                                         vert, NULL, q_pt);

            if (n_intersections == 0) {
              vert->v = q_pt;
              quantized.push_back((*i).first);
              tree->updateExtents(aabb);
            }
          }
          for (size_t i = 0; i < quantized.size(); ++i) {
            vertex_qinfo.erase(quantized[i]);
          }

          if (!quantized.size()) break;
        }
      }


    };
  }
}
