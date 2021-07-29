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


#pragma once

namespace carve {
  namespace csg {
    typedef std::unordered_map<
      carve::mesh::MeshSet<3>::vertex_t *,
      std::list<FLGroupList::iterator> > GroupLookup;


    inline bool isSameFwd(const V2Set &a, const V2Set &b) {
      if (a.size() != b.size()) return false;
      for (V2Set::const_iterator i = a.begin(), e = a.end(); i != e; ++i) {
        if (b.find((*i)) == b.end()) return false;
      }
      return true;
    }

    inline bool isSameRev(const V2Set &a, const V2Set &b) {
      if (a.size() != b.size()) return false;
      for (V2Set::const_iterator i = a.begin(), e = a.end(); i != e; ++i) {
        if (b.find(std::make_pair((*i).second, (*i).first)) == b.end()) return false;
      }
      return true;
    }


    static void performClassifySimpleOnFaceGroups(FLGroupList &a_groups,
                                                  FLGroupList &b_groups,
                                                  carve::mesh::MeshSet<3> *poly_a,
                                                  carve::mesh::MeshSet<3> *poly_b,
                                                  CSG::Collector &collector,
                                                  CSG::Hooks &hooks) {
      // Simple ON faces groups are face groups that consist of a single
      // face, and which have copy in both inputs. These are trivially ON.
      // This has the side effect of short circuiting the case where the
      // two inputs share geometry.
      GroupLookup a_map, b_map;

      // First, hash FaceLoopGroups with one FaceLoop based upon their
      // minimum vertex pointer - this pointer must be shared between
      // FaceLoops that this test catches.
      for (FLGroupList::iterator i = a_groups.begin(); i != a_groups.end(); ++i) {
        if ((*i).face_loops.size() != 1) continue;
        FaceLoop *f = (*i).face_loops.head;
        carve::mesh::MeshSet<3>::vertex_t *v = *std::min_element(f->vertices.begin(), f->vertices.end());
        a_map[v].push_back(i);
      }

      for (FLGroupList::iterator i = b_groups.begin(); i != b_groups.end(); ++i) {
        if ((*i).face_loops.size() != 1) continue;
        FaceLoop *f = (*i).face_loops.head;
        carve::mesh::MeshSet<3>::vertex_t *v = *std::min_element(f->vertices.begin(), f->vertices.end());
        if (a_map.find(v) != a_map.end()) {
          b_map[v].push_back(i);
        }
      }

      // Then, iterate through the FaceLoops hashed in the first map, and
      // find candidate matches in the second map.
      for (GroupLookup::iterator j = b_map.begin(), je = b_map.end(); j != je; ++j) {
        carve::mesh::MeshSet<3>::vertex_t *v = (*j).first;
        GroupLookup::iterator i = a_map.find(v);

        for (std::list<FLGroupList::iterator>::iterator bi = (*j).second.begin(), be = (*j).second.end(); bi != be;) {
          FLGroupList::iterator b(*bi);
          FaceLoop *f_b = (*b).face_loops.head;

          // For each candidate match pair, see if their vertex pointers
          // are the same, allowing for rotation and inversion.
          for (std::list<FLGroupList::iterator>::iterator ai = (*i).second.begin(), ae = (*i).second.end(); ai != ae; ++ai) {
            FLGroupList::iterator a(*ai);
            FaceLoop *f_a = (*a).face_loops.head;

            int s = is_same(f_a->vertices, f_b->vertices);
            if (!s) continue;

            // if they are ordered in the same direction, then they are
            // oriented out, otherwise oriented in.
            FaceClass fc = s == +1 ? FACE_ON_ORIENT_OUT : FACE_ON_ORIENT_IN;

            (*a).classification.push_back(ClassificationInfo(NULL, fc));
            (*b).classification.push_back(ClassificationInfo(NULL, fc));

            collector.collect(&*a, hooks);
            collector.collect(&*b, hooks);

            a_groups.erase(a);
            b_groups.erase(b);

            (*i).second.erase(ai);
            bi = (*j).second.erase(bi);

            goto done;
          }
          ++bi;
        done:;
        }
      }
    }

    template <typename CLASSIFIER>
    static void performClassifyEasyFaceGroups(FLGroupList &group,
                                              carve::mesh::MeshSet<3> *poly_a,
                                              const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_a_rtree,
                                              VertexClassification &vclass,
                                              const CLASSIFIER &classifier,
                                              CSG::Collector &collector,
                                              CSG::Hooks &hooks) {
  
      for (FLGroupList::iterator i = group.begin(); i != group.end();) {
#if defined(CARVE_DEBUG)
        std::cerr << "............group " << &(*i) << std::endl;
#endif
        FaceLoopGroup &grp = (*i);
        FaceLoopList &curr = (grp.face_loops);
        FaceClass fc;

        for (FaceLoop *f = curr.head; f; f = f->next) {
          for (size_t j = 0; j < f->vertices.size(); ++j) {
            if (!classifier.pointOn(vclass, f, j)) {
              PointClass pc = carve::mesh::classifyPoint(poly_a, poly_a_rtree, f->vertices[j]->v);
              if (pc == POINT_IN || pc == POINT_OUT) {
                classifier.explain(f, j, pc);
              }
              if (pc == POINT_IN) { fc = FACE_IN; goto accept; }
              if (pc == POINT_OUT) { fc = FACE_OUT; goto accept; }
            }
          }
        }
        ++i;
        continue;
      accept: {
          grp.classification.push_back(ClassificationInfo(NULL, fc));
          collector.collect(&grp, hooks);
          i = group.erase(i);
        }
      }
    }


    template <typename CLASSIFIER> 
    static void performClassifyHardFaceGroups(FLGroupList &group,
                                              carve::mesh::MeshSet<3> *poly_a,
                                              const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_a_rtree,
                                              const CLASSIFIER & /* classifier */,
                                              CSG::Collector &collector,
                                              CSG::Hooks &hooks) {
      for (FLGroupList::iterator
             i = group.begin(); i != group.end();) {
        int n_in = 0, n_out = 0, n_on = 0;
        FaceLoopGroup &grp = (*i);
        FaceLoopList &curr = (grp.face_loops);
        V2Set &perim = ((*i).perimeter);
        FaceClass fc =FACE_UNCLASSIFIED;

        for (FaceLoop *f = curr.head; f; f = f->next) {
          carve::mesh::MeshSet<3>::vertex_t *v1, *v2;
          v1 = f->vertices.back();
          for (size_t j = 0; j < f->vertices.size(); ++j) {
            v2 = f->vertices[j];
            if (v1 < v2 && perim.find(std::make_pair(v1, v2)) == perim.end()) {
              carve::geom3d::Vector c = (v1->v + v2->v) / 2.0;

              PointClass pc = carve::mesh::classifyPoint(poly_a, poly_a_rtree, c);

              switch (pc) {
              case POINT_IN: n_in++; break;
              case POINT_OUT: n_out++; break;
              case POINT_ON: n_on++; break;
              default: break; // does not happen.
              }
            }
            v1 = v2;
          }
        }

#if defined(CARVE_DEBUG)
        std::cerr << ">>> n_in: " << n_in << " n_on: " << n_on << " n_out: " << n_out << std::endl;
#endif

        if (!n_in && !n_out) {
          ++i;
          continue;
        }

        if (n_in) fc = FACE_IN;
        if (n_out) fc = FACE_OUT;

        grp.classification.push_back(ClassificationInfo(NULL, fc));
        collector.collect(&grp, hooks);
        i = group.erase(i);
      }
    }

    template <typename CLASSIFIER>
    void performFaceLoopWork(carve::mesh::MeshSet<3> *poly_a,
                             const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_a_rtree,
                             FLGroupList &b_loops_grouped,
                             const CLASSIFIER &classifier,
                             CSG::Collector &collector,
                             CSG::Hooks &hooks) {
      for (FLGroupList::iterator i = b_loops_grouped.begin(), e = b_loops_grouped.end(); i != e;) {
        FaceClass fc;

        if (classifier.faceLoopSanityChecker(*i)) {
          std::cerr << "UNEXPECTED face loop with size != 1." << std::endl;
          ++i;
          continue;
        }
        CARVE_ASSERT((*i).face_loops.size() == 1);

        FaceLoop *fla = (*i).face_loops.head;

        const carve::mesh::MeshSet<3>::face_t *f = (fla->orig_face);
        std::vector<carve::mesh::MeshSet<3>::vertex_t *> &loop = (fla->vertices);
        std::vector<carve::geom2d::P2> proj;
        proj.reserve(loop.size());
        for (unsigned j = 0; j < loop.size(); ++j) {
          proj.push_back(f->project(loop[j]->v));
        }
        carve::geom2d::P2 pv;
        if (!carve::geom2d::pickContainedPoint(proj, pv)) {
          CARVE_FAIL("Failed");
        }
        carve::geom3d::Vector v = f->unproject(pv, f->plane);

        const carve::mesh::MeshSet<3>::face_t *hit_face;
        PointClass pc = carve::mesh::classifyPoint(poly_a, poly_a_rtree, v, false, NULL, &hit_face);
        switch (pc) {
        case POINT_IN: fc = FACE_IN; break;
        case POINT_OUT: fc = FACE_OUT; break;
        case POINT_ON: {
          double d = carve::geom::distance(hit_face->plane, v);
#if defined(CARVE_DEBUG)
          std::cerr << "d = " << d << std::endl;
#endif
          fc = d < 0 ? FACE_IN : FACE_OUT;
          break;
        }
	default:
          CARVE_FAIL("unhandled switch case -- should not happen");
        }
#if defined(CARVE_DEBUG)
        std::cerr << "CLASS: " << (fc == FACE_IN ? "FACE_IN" : "FACE_OUT" ) << std::endl;
#endif

        (*i).classification.push_back(ClassificationInfo(NULL, fc));
        collector.collect(&*i, hooks);
        i = b_loops_grouped.erase(i);
      }

    }

    template <typename CLASSIFIER>
    void performClassifyFaceGroups(FLGroupList &a_loops_grouped,
                                   FLGroupList &b_loops_grouped,
                                   VertexClassification &vclass,
                                   carve::mesh::MeshSet<3> *poly_a,
                                   const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_a_rtree,
                                   carve::mesh::MeshSet<3> *poly_b,
                                   const carve::geom::RTreeNode<3, carve::mesh::Face<3> *> *poly_b_rtree,
                                   const CLASSIFIER &classifier,
                                   CSG::Collector &collector,
                                   CSG::Hooks &hooks) {

      classifier.classifySimple(a_loops_grouped, b_loops_grouped, vclass, poly_a, poly_b);
      classifier.classifyEasy(a_loops_grouped, b_loops_grouped, vclass, poly_a, poly_a_rtree, poly_b, poly_b_rtree);
      classifier.classifyHard(a_loops_grouped, b_loops_grouped, vclass, poly_a, poly_a_rtree, poly_b, poly_b_rtree);

      {
        GroupLookup a_map;
        FLGroupList::iterator i, j;
        FaceClass fc;

        for (i = a_loops_grouped.begin(); i != a_loops_grouped.end(); ++i) {
          V2Set::iterator it_end = (*i).perimeter.end();
          V2Set::iterator it_begin = (*i).perimeter.begin();

          if(it_begin != it_end) {
            a_map[std::min_element(it_begin, it_end)->first].push_back(i);
          }
        }

        for (i = b_loops_grouped.begin(); i != b_loops_grouped.end();) {
          GroupLookup::iterator a = a_map.end();

          V2Set::iterator it_end = (*i).perimeter.end();
          V2Set::iterator it_begin = (*i).perimeter.begin();

          if(it_begin != it_end) {
            a = a_map.find(std::min_element(it_begin, it_end)->first);
          }

          if (a == a_map.end()) { ++i; continue; }

          for (std::list<FLGroupList::iterator>::iterator ji = (*a).second.begin(), je = (*a).second.end(); ji != je; ++ji) {
            j = (*ji);
            if (isSameFwd((*i).perimeter, (*j).perimeter)) {
#if defined(CARVE_DEBUG)
              std::cerr << "SAME FWD PAIR" << std::endl;
#endif
              fc = FACE_ON_ORIENT_OUT;
              goto face_pair;
            } else if (isSameRev((*i).perimeter, (*j).perimeter)) {
#if defined(CARVE_DEBUG)
              std::cerr << "SAME REV PAIR" << std::endl;
#endif
              fc = FACE_ON_ORIENT_IN;
              goto face_pair;
            }
          }
          ++i;
          continue;

        face_pair: {
            V2Set::iterator it_end = (*j).perimeter.end();
            V2Set::iterator it_begin = (*j).perimeter.begin();

            if(it_begin != it_end) {
              a_map[std::min_element(it_begin, it_end)->first].remove(j);
            }

            (*i).classification.push_back(ClassificationInfo(NULL, fc));
            (*j).classification.push_back(ClassificationInfo(NULL, fc));

            collector.collect(&*i, hooks);
            collector.collect(&*j, hooks);

            j = a_loops_grouped.erase(j);
            i = b_loops_grouped.erase(i);
          }
        }
      }

      // XXX: this may leave some face groups that are IN or OUT, and
      // consist of a single face loop.
      classifier.postRemovalCheck(a_loops_grouped, b_loops_grouped);

      classifier.faceLoopWork(a_loops_grouped, b_loops_grouped, vclass, poly_a, poly_a_rtree, poly_b, poly_b_rtree);

      classifier.finish(a_loops_grouped, b_loops_grouped);
    }

  }
}
