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

#include <carve/csg.hpp>
#include <carve/tag.hpp>
#include <carve/poly.hpp>
#include <carve/triangulator.hpp>
#include <deque>

namespace carve {
  namespace csg {

    namespace detail {
      template<bool with_improvement>
      class CarveTriangulator : public csg::CSG::Hook {

      public:
        CarveTriangulator() {
        }

        virtual ~CarveTriangulator() {
        }

        virtual void processOutputFace(std::vector<carve::mesh::MeshSet<3>::face_t *> &faces,
                                       const carve::mesh::MeshSet<3>::face_t *orig,
                                       bool flipped) {
          std::vector<carve::mesh::MeshSet<3>::face_t *> out_faces;

          size_t n_tris = 0;
          for (size_t f = 0; f < faces.size(); ++f) {
            CARVE_ASSERT(faces[f]->nVertices() >= 3);
            n_tris += faces[f]->nVertices() - 2;
          }

          out_faces.reserve(n_tris);

          for (size_t f = 0; f < faces.size(); ++f) {
            carve::mesh::MeshSet<3>::face_t *face = faces[f];

            if (face->nVertices() == 3) {
              out_faces.push_back(face);
              continue;
            }

            std::vector<triangulate::tri_idx> result;

            std::vector<carve::mesh::MeshSet<3>::vertex_t *> vloop;
            face->getVertices(vloop);

            triangulate::triangulate(
                carve::mesh::MeshSet<3>::face_t::projection_mapping(face->project),
                vloop,
                result);

            if (with_improvement) {
              triangulate::improve(
                  carve::mesh::MeshSet<3>::face_t::projection_mapping(face->project),
                  vloop,
                  carve::mesh::vertex_distance(),
                  result);
            }

            std::vector<carve::mesh::MeshSet<3>::vertex_t *> fv;
            fv.resize(3);
            for (size_t i = 0; i < result.size(); ++i) {
              fv[0] = vloop[result[i].a];
              fv[1] = vloop[result[i].b];
              fv[2] = vloop[result[i].c];
              out_faces.push_back(face->create(fv.begin(), fv.end(), false));
            }
            delete face;
          }
          std::swap(faces, out_faces);
        }
      };
    }

    typedef detail::CarveTriangulator<false> CarveTriangulator;
    typedef detail::CarveTriangulator<true> CarveTriangulatorWithImprovement;

    class CarveTriangulationImprover : public csg::CSG::Hook {
    public:
      CarveTriangulationImprover() {
      }

      virtual ~CarveTriangulationImprover() {
      }

      virtual void processOutputFace(std::vector<carve::mesh::MeshSet<3>::face_t *> &faces,
                                     const carve::mesh::MeshSet<3>::face_t *orig,
                                     bool flipped) {
        if (faces.size() == 1) return;

        // doing improvement as a separate hook is much messier than
        // just incorporating it into the triangulation hook.

        typedef std::map<carve::mesh::MeshSet<3>::vertex_t *, size_t> vert_map_t;
        std::vector<carve::mesh::MeshSet<3>::face_t *> out_faces;
        vert_map_t vert_map;

        out_faces.reserve(faces.size());


        carve::mesh::MeshSet<3>::face_t::projection_mapping projector(faces[0]->project);

        std::vector<triangulate::tri_idx> result;

        for (size_t f = 0; f < faces.size(); ++f) {
          carve::mesh::MeshSet<3>::face_t *face = faces[f];
          if (face->nVertices() != 3) {
            out_faces.push_back(face);
          } else {
            triangulate::tri_idx tri;
            for (carve::mesh::MeshSet<3>::face_t::edge_iter_t i = face->begin(); i != face->end(); ++i) {
              size_t v = 0;
              vert_map_t::iterator j = vert_map.find(i->vert);
              if (j == vert_map.end()) {
                v = vert_map.size();
                vert_map[i->vert] = v;
              } else {
                v = (*j).second;
              }
              tri.v[i.idx()] = v;
            }
            result.push_back(tri);
            delete face;
          }
        }

        std::vector<carve::mesh::MeshSet<3>::vertex_t *> verts;
        verts.resize(vert_map.size());
        for (vert_map_t::iterator i = vert_map.begin(); i != vert_map.end(); ++i) {
          verts[(*i).second] = (*i).first;
        }
 
        triangulate::improve(projector, verts, carve::mesh::vertex_distance(), result);

        std::vector<carve::mesh::MeshSet<3>::vertex_t *> fv;
        fv.resize(3);
        for (size_t i = 0; i < result.size(); ++i) {
          fv[0] = verts[result[i].a];
          fv[1] = verts[result[i].b];
          fv[2] = verts[result[i].c];
          out_faces.push_back(orig->create(fv.begin(), fv.end(), false));
        }

        std::swap(faces, out_faces);
      }
    };

    class CarveTriangulationQuadMerger : public csg::CSG::Hook {
      // this code is incomplete.
      typedef std::map<V2, F2> edge_map_t;

    public:
      CarveTriangulationQuadMerger() {
      }

      virtual ~CarveTriangulationQuadMerger() {
      }

      double scoreQuad(edge_map_t::iterator i, edge_map_t &edge_map) {
        if (!(*i).second.first || !(*i).second.second) return -1;
        return -1;
      }

      carve::mesh::MeshSet<3>::face_t *mergeQuad(edge_map_t::iterator i, edge_map_t &edge_map) {
        return NULL;
      }

      void recordEdge(carve::mesh::MeshSet<3>::vertex_t *v1,
                      carve::mesh::MeshSet<3>::vertex_t *v2,
                      carve::mesh::MeshSet<3>::face_t *f,
                      edge_map_t &edge_map) {
        if (v1 < v2) {
          edge_map[V2(v1, v2)].first = f;
        } else {
          edge_map[V2(v2, v1)].second = f;
        }
      }

      virtual void processOutputFace(std::vector<carve::mesh::MeshSet<3>::face_t *> &faces,
                                     const carve::mesh::MeshSet<3>::face_t *orig,
                                     bool flipped) {
        if (faces.size() == 1) return;

        std::vector<carve::mesh::MeshSet<3>::face_t *> out_faces;
        edge_map_t edge_map;

        out_faces.reserve(faces.size());

        poly::p2_adapt_project<3> projector(faces[0]->project);

        for (size_t f = 0; f < faces.size(); ++f) {
          carve::mesh::MeshSet<3>::face_t *face = faces[f];
          if (face->nVertices() != 3) {
            out_faces.push_back(face);
          } else {
            carve::mesh::MeshSet<3>::face_t::vertex_t *v1, *v2, *v3;
            v1 = face->edge->vert;
            v2 = face->edge->next->vert;
            v3 = face->edge->next->next->vert;
            recordEdge(v1, v2, face, edge_map);
            recordEdge(v2, v3, face, edge_map);
            recordEdge(v3, v1, face, edge_map);
          }
        }

        for (edge_map_t::iterator i = edge_map.begin(); i != edge_map.end();) {
          if ((*i).second.first && (*i).second.second) {
            ++i;
          } else {
            edge_map.erase(i++);
          }
        }

        while (edge_map.size()) {
          edge_map_t::iterator i = edge_map.begin();
          edge_map_t::iterator best = i;
          double best_score = scoreQuad(i, edge_map);
          for (++i; i != edge_map.end(); ++i) {
            double score = scoreQuad(i, edge_map);
            if (score > best_score) best = i;
          }
          if (best_score < 0) break;
          out_faces.push_back(mergeQuad(best, edge_map));
        }

        if (edge_map.size()) {
          tagable::tag_begin();
          for (edge_map_t::iterator i = edge_map.begin(); i != edge_map.end(); ++i) {
            carve::mesh::MeshSet<3>::face_t *a = const_cast<carve::mesh::MeshSet<3>::face_t *>((*i).second.first);
            carve::mesh::MeshSet<3>::face_t *b = const_cast<carve::mesh::MeshSet<3>::face_t *>((*i).second.first);
            if (a && a->tag_once()) out_faces.push_back(a);
            if (b && b->tag_once()) out_faces.push_back(b);
          }
        }

        std::swap(faces, out_faces);
      }
    };

    class CarveHoleResolver : public csg::CSG::Hook {

    public:
      CarveHoleResolver() {
      }

      virtual ~CarveHoleResolver() {
      }

      bool findRepeatedEdges(const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &vertices,
                             std::list<std::pair<size_t, size_t> > &edge_pos) {
        std::map<V2, size_t> edges;
        for (size_t i = 0; i < vertices.size() - 1; ++i) {
          edges[std::make_pair(vertices[i], vertices[i+1])] = i;
        }
        edges[std::make_pair(vertices[vertices.size()-1], vertices[0])] = vertices.size() - 1;

        for (std::map<V2, size_t>::iterator i = edges.begin(); i != edges.end(); ++i) {
          V2 rev = V2((*i).first.second, (*i).first.first);
          std::map<V2, size_t>::iterator j = edges.find(rev);
          if (j != edges.end()) {
            edge_pos.push_back(std::make_pair((*i).second, (*j).second));
          }
        }
        return edge_pos.size() > 0;
      }

      void flood(size_t t1,
                 size_t t2,
                 size_t old_grp,
                 size_t new_grp_1,
                 size_t new_grp_2,
                 std::vector<size_t> &grp,
                 const std::vector<triangulate::tri_idx> &tris,
                 const std::map<std::pair<size_t, size_t>, size_t> &tri_edge) {
        grp[t1] = new_grp_1;
        grp[t2] = new_grp_2;

        std::deque<size_t> to_visit;
        to_visit.push_back(t1);
        to_visit.push_back(t2);
        std::vector<std::pair<size_t, size_t> > rev;
        rev.resize(3);
        while (to_visit.size()) {
          size_t curr = to_visit.front();
          to_visit.pop_front();
          triangulate::tri_idx ct = tris[curr];
          rev[0] = std::make_pair(ct.b, ct.a);
          rev[1] = std::make_pair(ct.c, ct.b);
          rev[2] = std::make_pair(ct.a, ct.c);

          for (size_t i = 0; i < 3; ++i) {
            std::map<std::pair<size_t, size_t>, size_t>::const_iterator adj = tri_edge.find(rev[i]);
            if (adj == tri_edge.end()) continue;
            size_t next = (*adj).second;
            if (grp[next] != old_grp) continue;
            grp[next] = grp[curr];
            to_visit.push_back(next);
          }
        }
      }

      void findPerimeter(const std::vector<triangulate::tri_idx> &tris,
                         const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &verts,
                         std::vector<carve::mesh::MeshSet<3>::vertex_t *> &out) {
        std::map<std::pair<size_t, size_t>, size_t> edges;
        for (size_t i = 0; i < tris.size(); ++i) {
          edges[std::make_pair(tris[i].a, tris[i].b)] = i;
          edges[std::make_pair(tris[i].b, tris[i].c)] = i;
          edges[std::make_pair(tris[i].c, tris[i].a)] = i;
        }
        std::map<size_t, size_t> unpaired;
        for (std::map<std::pair<size_t, size_t>, size_t>::iterator i = edges.begin(); i != edges.end(); ++i) {
          if (edges.find(std::make_pair((*i).first.second, (*i).first.first)) == edges.end()) {
            CARVE_ASSERT(unpaired.find((*i).first.first) == unpaired.end());
            unpaired[(*i).first.first] = (*i).first.second;
          }
        }
        out.clear();
        out.reserve(unpaired.size());
        size_t start = (*unpaired.begin()).first;
        size_t vert = start;
        do {
          out.push_back(verts[vert]);
          CARVE_ASSERT(unpaired.find(vert) != unpaired.end());
          vert = unpaired[vert];
        } while (vert != start);
      }

      virtual void processOutputFace(std::vector<carve::mesh::MeshSet<3>::face_t *> &faces,
                                     const carve::mesh::MeshSet<3>::face_t *orig,
                                     bool flipped) {
        std::vector<carve::mesh::MeshSet<3>::face_t *> out_faces;

        for (size_t f = 0; f < faces.size(); ++f) {
          carve::mesh::MeshSet<3>::face_t *face = faces[f];

          if (face->nVertices() == 3) {
            out_faces.push_back(face);
            continue;
          }

          std::vector<carve::mesh::MeshSet<3>::vertex_t *> vloop;
          face->getVertices(vloop);

          std::list<std::pair<size_t, size_t> > rep_edges;
          if (!findRepeatedEdges(vloop, rep_edges)) {
            out_faces.push_back(face);
            continue;
          }

          std::vector<triangulate::tri_idx> result;
          triangulate::triangulate(
              carve::mesh::MeshSet<3>::face_t::projection_mapping(face->project),
              vloop,
              result);

          std::map<std::pair<size_t, size_t>, size_t> tri_edge;
          for (size_t i = 0; i < result.size(); ++i) {
            tri_edge[std::make_pair(result[i].a, result[i].b)] = i;
            tri_edge[std::make_pair(result[i].b, result[i].c)] = i;
            tri_edge[std::make_pair(result[i].c, result[i].a)] = i;
          }

          std::vector<size_t> grp;
          grp.resize(result.size(), 0);

          size_t grp_max = 0;

          while (rep_edges.size()) {
            std::pair<size_t, size_t> e1, e2;

            e1.first = rep_edges.front().first;
            e1.second = (e1.first + 1) % vloop.size();

            e2.first = rep_edges.front().second;
            e2.second = (e2.first + 1) % vloop.size();

            rep_edges.pop_front();

            CARVE_ASSERT(tri_edge.find(e1) != tri_edge.end());
            size_t t1 = tri_edge[e1];
            CARVE_ASSERT(tri_edge.find(e2) != tri_edge.end());
            size_t t2 = tri_edge[e2];

            if (grp[t1] != grp[t2]) {
              continue;
            }

            size_t t1g = ++grp_max;
            size_t t2g = ++grp_max;

            flood(t1, t2, grp[t1], t1g, t2g, grp, result, tri_edge);
          }

          std::set<size_t> groups;
          std::copy(grp.begin(), grp.end(), std::inserter(groups, groups.begin()));

          // now construct perimeters for each group.
          std::vector<triangulate::tri_idx> grp_tris;
          grp_tris.reserve(result.size());
          for (std::set<size_t>::iterator i = groups.begin(); i != groups.end(); ++i) {
            size_t grp_id = *i;
            grp_tris.clear();
            for (size_t j = 0; j < grp.size(); ++j) {
              if (grp[j] == grp_id) {
                grp_tris.push_back(result[j]);
              }
            }
            std::vector<carve::mesh::MeshSet<3>::vertex_t *> grp_perim;
            findPerimeter(grp_tris, vloop, grp_perim);
            out_faces.push_back(face->create(grp_perim.begin(), grp_perim.end(), false));
          }
          delete face;
        }
        std::swap(faces, out_faces);
      }
    };
  }
}
