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
#include <carve/debug_hooks.hpp>

#include <list>
#include <set>
#include <iostream>

#include <algorithm>

#include "intersect_common.hpp"
#include "intersect_classify_common.hpp"
#include "intersect_classify_common_impl.hpp"


namespace carve {
  namespace csg {

    namespace {

#if defined(_MSC_VER) && _MSC_VER < 1300
      // VC++ 6.0 gets an internal compiler when compiling
      // the FaceMaker template. Not sure why but for now we just bypass
      // the template
      class FaceMaker0 {
      public:
        CSG::Collector &collector;
        CSG::Hooks &hooks;

        FaceMaker0(CSG::Collector &c, CSG::Hooks &h) : collector(c), hooks(h) {
        }
        bool pointOn(VertexClassification &vclass, FaceLoop *f, size_t index) const {
          return vclass[f->vertices[index]].cls[1] == POINT_ON;
        }
        void explain(FaceLoop *f, size_t index, PointClass pc) const {
#if defined(CARVE_DEBUG)
          std::cerr << "face loop " << f << " from poly " << "ab"[0] << " is easy because vertex " << index << " (" << *f->vertices[index] << ") is " << ENUM(pc) << std::endl;
#endif
        }
      };
      class FaceMaker1 {
      public:
        CSG::Collector &collector;
        CSG::Hooks &hooks;

        FaceMaker1(CSG::Collector &c, CSG::Hooks &h) : collector(c), hooks(h) {
        }
        bool pointOn(VertexClassification &vclass, FaceLoop *f, size_t index) const {
          return vclass[f->vertices[index]].cls[0] == POINT_ON;
        }
        void explain(FaceLoop *f, size_t index, PointClass pc) const {
#if defined(CARVE_DEBUG)
          std::cerr << "face loop " << f << " from poly " << "ab"[1] << " is easy because vertex " << index << " (" << *f->vertices[index] << ") is " << ENUM(pc) << std::endl;
#endif
        }
      };
#else
      template <int poly_num>
      class FaceMaker {
        FaceMaker &operator=(const FaceMaker &);

      public:
        CSG::Collector &collector;
        CSG::Hooks &hooks;

        FaceMaker(CSG::Collector &c, CSG::Hooks &h) : collector(c), hooks(h) {
        }

        bool pointOn(VertexClassification &vclass, FaceLoop *f, size_t index) const {
          return vclass[f->vertices[index]].cls[1 - poly_num] == POINT_ON;
        }

        void explain(FaceLoop *f, size_t index, PointClass pc) const {
#if defined(CARVE_DEBUG)
          std::cerr << "face loop " << f << " from poly " << "ab"[poly_num] << " is easy because vertex " << index << " (" << f->vertices[index]->v << ") is " << ENUM(pc) << std::endl;
#endif
        }
      };
      typedef FaceMaker<0> FaceMaker0;
      typedef FaceMaker<1> FaceMaker1;
#endif
      class ClassifyFaceGroups {
        ClassifyFaceGroups &operator=(const ClassifyFaceGroups &);

      public:
        CSG::Collector &collector;
        CSG::Hooks &hooks;

        ClassifyFaceGroups(CSG::Collector &c, CSG::Hooks &h) : collector(c), hooks(h) {
        }
    
        void classifySimple(FLGroupList &a_loops_grouped,
                            FLGroupList &b_loops_grouped,
                            VertexClassification & /* vclass */,
                            carve::mesh::MeshSet<3> *poly_a,
                            carve::mesh::MeshSet<3> *poly_b) const {
          if (a_loops_grouped.size() < b_loops_grouped.size()) {
            performClassifySimpleOnFaceGroups(a_loops_grouped, b_loops_grouped, poly_a, poly_b, collector, hooks);
          } else {
            performClassifySimpleOnFaceGroups(b_loops_grouped, a_loops_grouped, poly_b, poly_a, collector, hooks);
          }
#if defined(CARVE_DEBUG)
          std::cerr << "after removal of simple on groups: " << a_loops_grouped.size() << " a groups" << std::endl;
          std::cerr << "after removal of simple on groups: " << b_loops_grouped.size() << " b groups" << std::endl;
#endif
        }
    
        void classifyEasy(FLGroupList &a_loops_grouped,
                          FLGroupList &b_loops_grouped,
                          VertexClassification &vclass,
                          carve::mesh::MeshSet<3> *poly_a,
                          const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_a_rtree,
                          carve::mesh::MeshSet<3> *poly_b,
                          const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_b_rtree) const {
          performClassifyEasyFaceGroups(a_loops_grouped, poly_b, poly_b_rtree, vclass, FaceMaker0(collector, hooks), collector, hooks);
          performClassifyEasyFaceGroups(b_loops_grouped, poly_a, poly_a_rtree, vclass, FaceMaker1(collector, hooks), collector, hooks);
#if defined(CARVE_DEBUG)
          std::cerr << "after removal of easy groups: " << a_loops_grouped.size() << " a groups" << std::endl;
          std::cerr << "after removal of easy groups: " << b_loops_grouped.size() << " b groups" << std::endl;
#endif
        }
    
        void classifyHard(FLGroupList &a_loops_grouped,
                          FLGroupList &b_loops_grouped,
                          VertexClassification & /* vclass */,
                          carve::mesh::MeshSet<3> *poly_a,
                          const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_a_rtree,
                          carve::mesh::MeshSet<3> *poly_b,
                          const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_b_rtree) const {
          performClassifyHardFaceGroups(a_loops_grouped, poly_b, poly_b_rtree, FaceMaker0(collector, hooks), collector, hooks);
          performClassifyHardFaceGroups(b_loops_grouped, poly_a, poly_a_rtree, FaceMaker1(collector, hooks), collector, hooks);
#if defined(CARVE_DEBUG)
          std::cerr << "after removal of hard groups: " << a_loops_grouped.size() << " a groups" << std::endl;
          std::cerr << "after removal of hard groups: " << b_loops_grouped.size() << " b groups" << std::endl;
#endif
        }
    
        void faceLoopWork(FLGroupList &a_loops_grouped,
                          FLGroupList &b_loops_grouped,
                          VertexClassification & /* vclass */,
                          carve::mesh::MeshSet<3> *poly_a,
                          const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_a_rtree,
                          carve::mesh::MeshSet<3> *poly_b,
                          const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_b_rtree) const {
          performFaceLoopWork(poly_b, poly_b_rtree, a_loops_grouped, *this, collector, hooks);
          performFaceLoopWork(poly_a, poly_a_rtree, b_loops_grouped, *this, collector, hooks);
        }
    
        void postRemovalCheck(FLGroupList &a_loops_grouped,
                              FLGroupList &b_loops_grouped) const {
#if defined(CARVE_DEBUG)
          std::cerr << "after removal of on groups: " << a_loops_grouped.size() << " a groups" << std::endl;
          std::cerr << "after removal of on groups: " << b_loops_grouped.size() << " b groups" << std::endl;
#endif
        }

        bool faceLoopSanityChecker(FaceLoopGroup &i) const {
          return i.face_loops.size() != 1;
        }

        void finish(FLGroupList &a_loops_grouped,FLGroupList &b_loops_grouped) const {
#if defined(CARVE_DEBUG)
          if (a_loops_grouped.size() || b_loops_grouped.size())
            std::cerr << "UNCLASSIFIED! a=" << a_loops_grouped.size() << ", b=" << b_loops_grouped.size() << std::endl;
#endif   
        }
      };
    }

    void CSG::classifyFaceGroups(const V2Set & /* shared_edges */,
                                 VertexClassification &vclass,
                                 carve::mesh::MeshSet<3> *poly_a,                           
                                 const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_a_rtree,
                                 FLGroupList &a_loops_grouped,
                                 const detail::LoopEdges & /* a_edge_map */,
                                 carve::mesh::MeshSet<3> *poly_b,
                                 const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_b_rtree,
                                 FLGroupList &b_loops_grouped,
                                 const detail::LoopEdges & /* b_edge_map */,
                                 CSG::Collector &collector) {
      ClassifyFaceGroups classifier(collector, hooks);
#if defined(CARVE_DEBUG)
      std::cerr << "initial groups: " << a_loops_grouped.size() << " a groups" << std::endl;
      std::cerr << "initial groups: " << b_loops_grouped.size() << " b groups" << std::endl;
#endif
      performClassifyFaceGroups(
          a_loops_grouped,
          b_loops_grouped,
          vclass,
          poly_a,
          poly_a_rtree,
          poly_b,
          poly_b_rtree,
          classifier,
          collector,
          hooks);
    }

  }
}
