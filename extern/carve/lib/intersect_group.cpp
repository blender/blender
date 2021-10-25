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

#include <carve/csg.hpp>
#include <carve/timing.hpp>

#include "csg_detail.hpp"
#include "intersect_common.hpp"

void carve::csg::CSG::makeEdgeMap(const carve::csg::FaceLoopList &loops,
                                  size_t edge_count,
                                  detail::LoopEdges &edge_map) {
#if defined(UNORDERED_COLLECTIONS_SUPPORT_RESIZE)
  edge_map.resize(edge_count);
#endif

  for (carve::csg::FaceLoop *i = loops.head; i; i = i->next) {
    edge_map.addFaceLoop(i);
    i->group = NULL;
  }
}

#include <carve/polyline.hpp>

#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
void writePLY(const std::string &out_file, const carve::mesh::MeshSet<3> *poly, bool ascii);
void writePLY(const std::string &out_file, const carve::line::PolylineSet *lines, bool ascii);
#endif

void carve::csg::CSG::findSharedEdges(const detail::LoopEdges &edge_map_a,
                                      const detail::LoopEdges &edge_map_b,
                                      V2Set &shared_edges) {
  for (detail::LoopEdges::const_iterator
         i = edge_map_a.begin(), e = edge_map_a.end();
       i != e;
       ++i) {
    detail::LoopEdges::const_iterator j = edge_map_b.find((*i).first);
    if (j != edge_map_b.end()) {
      shared_edges.insert((*i).first);
    }
  }

#if defined(CARVE_DEBUG)
  detail::VVSMap edge_graph;

  for (V2Set::const_iterator i = shared_edges.begin(); i != shared_edges.end(); ++i) {
    edge_graph[(*i).first].insert((*i).second);
    edge_graph[(*i).second].insert((*i).first);
  }

  std::cerr << "*** testing consistency of edge graph" << std::endl;
  for (detail::VVSMap::const_iterator i = edge_graph.begin(); i != edge_graph.end(); ++i) {
    if ((*i).second.size() > 2) {
      std::cerr << "branch at: " << (*i).first << std::endl;
    }
    if ((*i).second.size() == 1) {
      std::cerr << "endpoint at: " << (*i).first << std::endl;
      std::cerr << "coordinate: " << (*i).first->v << std::endl;
    }
  }

  {
    carve::line::PolylineSet intersection_graph;
    intersection_graph.vertices.resize(edge_graph.size());
    std::map<const carve::mesh::MeshSet<3>::vertex_t *, size_t> vmap;

    size_t j = 0;
    for (detail::VVSMap::const_iterator i = edge_graph.begin(); i != edge_graph.end(); ++i) {
      intersection_graph.vertices[j].v = (*i).first->v;
      vmap[(*i).first] = j++;
    }

    while (edge_graph.size()) {
      detail::VVSMap::iterator prior_i = edge_graph.begin();
      carve::mesh::MeshSet<3>::vertex_t *prior = (*prior_i).first;
      std::vector<size_t> connected;
      connected.push_back(vmap[prior]);
      while (prior_i != edge_graph.end() && (*prior_i).second.size()) {
        carve::mesh::MeshSet<3>::vertex_t *next = *(*prior_i).second.begin();
        detail::VVSMap::iterator next_i = edge_graph.find(next);
        CARVE_ASSERT(next_i != edge_graph.end());
        connected.push_back(vmap[next]);
        (*prior_i).second.erase(next);
        (*next_i).second.erase(prior);
        if (!(*prior_i).second.size()) { edge_graph.erase(prior_i); prior_i = edge_graph.end(); }
        if (!(*next_i).second.size()) { edge_graph.erase(next_i); next_i = edge_graph.end(); }
        prior_i = next_i;
        prior = next;
      }
      bool closed = connected.front() == connected.back();
      for (size_t k = 0; k < connected.size(); ++k) {
        std::cerr << " " << connected[k];
      }
      std::cerr << std::endl;
      intersection_graph.addPolyline(closed, connected.begin(), connected.end());
    }

#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
    std::string out("/tmp/intersection.ply");
    ::writePLY(out, &intersection_graph, true);
#endif
  }

  std::cerr << "*** edge graph consistency test done" << std::endl;
#endif
}



#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
static carve::mesh::MeshSet<3> *groupToPolyhedron(const carve::csg::FaceLoopGroup &grp) {
  const carve::csg::FaceLoopList &fl = grp.face_loops;
  std::vector<carve::mesh::MeshSet<3>::face_t *> faces;
  faces.reserve(fl.size());
  for (carve::csg::FaceLoop *f = fl.head; f; f = f->next) {
    faces.push_back(f->orig_face->create(f->vertices.begin(), f->vertices.end(), false));
  }
  carve::mesh::MeshSet<3> *poly = new carve::mesh::MeshSet<3>(faces);

  poly->canonicalize();
  return poly;
}
#endif



void carve::csg::CSG::groupFaceLoops(carve::mesh::MeshSet<3> *src,
                                     carve::csg::FaceLoopList &face_loops,
                                     const carve::csg::detail::LoopEdges &loop_edges,
                                     const carve::csg::V2Set &no_cross,
                                     carve::csg::FLGroupList &out_loops) {
  // Find all the groups of face loops that are connected by edges
  // that are not part of no_cross.
  // this could potentially be done with a disjoint set data-structure.
#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
  static int call_num = 0;
  call_num++;
#endif

  static carve::TimingName GROUP_FACE_LOOPS("groupFaceLoops()");

  carve::TimingBlock block(GROUP_FACE_LOOPS);

  int tag_num = 0;
  while (face_loops.size()) {
    out_loops.push_back(FaceLoopGroup(src));
    carve::csg::FaceLoopGroup &group = (out_loops.back());
    carve::csg::FaceLoopList &curr = (group.face_loops);
    carve::csg::V2Set &perim = (group.perimeter);

    carve::csg::FaceLoop *expand = face_loops.head;

    expand->group = &group;
    face_loops.remove(expand);
    curr.append(expand);

    while (expand) {
      std::vector<carve::mesh::MeshSet<3>::vertex_t *> &loop = (expand->vertices);
      carve::mesh::MeshSet<3>::vertex_t *v1, *v2;

      v1 = loop.back();
      for (size_t i = 0; i < loop.size(); ++i) {
        v2 = loop[i];

        carve::csg::V2Set::const_iterator nc = no_cross.find(std::make_pair(v1, v2));
        if (nc == no_cross.end()) {
          carve::csg::detail::LoopEdges::const_iterator j;

          j = loop_edges.find(std::make_pair(v1, v2));
          if (j != loop_edges.end()) {
            for (std::list<carve::csg::FaceLoop *>::const_iterator
                   k = (*j).second.begin(), ke = (*j).second.end();
                 k != ke; ++k) {
              if ((*k)->group != NULL ||
                  (*k)->orig_face->mesh != expand->orig_face->mesh) continue;
              face_loops.remove((*k));
              curr.append((*k));
              (*k)->group = &group;
            }
          }

          j = loop_edges.find(std::make_pair(v2, v1));
          if (j != loop_edges.end()) {
            for (std::list<carve::csg::FaceLoop *>::const_iterator
                   k = (*j).second.begin(), ke = (*j).second.end();
                 k != ke; ++k) {
              if ((*k)->group != NULL ||
                  (*k)->orig_face->mesh != expand->orig_face->mesh) continue;
              face_loops.remove((*k));
              curr.append((*k));
              (*k)->group = &group;
            }
          }
        } else {
          perim.insert(std::make_pair(v1, v2));
        }
        v1 = v2;
      }
      expand = expand->next;
    }
    tag_num++;

#if defined(CARVE_DEBUG_WRITE_PLY_DATA)
    {
      carve::mesh::MeshSet<3> *poly = groupToPolyhedron(group);
      char buf[128];
      sprintf(buf, "/tmp/group-%d-%p.ply", call_num, &curr);
      std::string out(buf);
      ::writePLY(out, poly, false);
      delete poly;
    }
#endif
  }
}
