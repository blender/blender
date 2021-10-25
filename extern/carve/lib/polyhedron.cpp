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

#if defined(CARVE_DEBUG)
#define DEBUG_CONTAINS_VERTEX
#endif

#include <carve/djset.hpp>

#include <carve/geom.hpp>
#include <carve/poly.hpp>

#include <carve/octree_impl.hpp>

#include <carve/timing.hpp>

#include <algorithm>

#include <carve/mesh.hpp>

#ifdef HAVE_BOOST_LIBRARY
#  include BOOST_INCLUDE(random.hpp)
#else
#  include <carve/random/random.h>
#endif

namespace {
  bool emb_test(carve::poly::Polyhedron *poly,
                std::map<int, std::set<int> > &embedding,
                carve::geom3d::Vector v,
                int m_id) {

    std::map<int, carve::PointClass> result;
#if defined(CARVE_DEBUG)
    std::cerr << "test " << v << " (m_id:" << m_id << ")" << std::endl;
#endif
    poly->testVertexAgainstClosedManifolds(v, result, true);
    std::set<int> inside;
    for (std::map<int, carve::PointClass>::iterator j = result.begin();
         j != result.end();
         ++j) {
      if ((*j).first == m_id) continue;
      if ((*j).second == carve::POINT_IN) inside.insert((*j).first);
      else if ((*j).second == carve::POINT_ON) {
#if defined(CARVE_DEBUG)
        std::cerr << " FAIL" << std::endl;
#endif
        return false;
      }
    }
#if defined(CARVE_DEBUG)
    std::cerr << " OK (inside.size()==" << inside.size() << ")" << std::endl;
#endif
    embedding[m_id] = inside;
    return true;
  }



  struct order_faces {
    bool operator()(const carve::poly::Polyhedron::face_t * const &a,
                    const carve::poly::Polyhedron::face_t * const &b) const {
      return std::lexicographical_compare(a->vbegin(), a->vend(), b->vbegin(), b->vend());
    }
  };



}



namespace carve {
  namespace poly {



    bool Polyhedron::initSpatialIndex() {
      static carve::TimingName FUNC_NAME("Polyhedron::initSpatialIndex()");
      carve::TimingBlock block(FUNC_NAME);

      octree.setBounds(aabb);
      octree.addFaces(faces);
      octree.addEdges(edges);
      octree.splitTree();

      return true;
    }



    void Polyhedron::invertAll() {
      for (size_t i = 0; i < faces.size(); ++i) {
        faces[i].invert();
      }

      for (size_t i = 0; i < edges.size(); ++i) {
        std::vector<const face_t *> &f = connectivity.edge_to_face[i];
        for (size_t j = 0; j < (f.size() & ~1U); j += 2) {
          std::swap(f[j], f[j+1]);
        }
      }

      for (size_t i = 0; i < manifold_is_negative.size(); ++i) {
        manifold_is_negative[i] = !manifold_is_negative[i];
      }
    }



    void Polyhedron::invert(const std::vector<bool> &selected_manifolds) {
      bool altered = false;
      for (size_t i = 0; i < faces.size(); ++i) {
        if (faces[i].manifold_id >= 0 &&
            (unsigned)faces[i].manifold_id < selected_manifolds.size() &&
            selected_manifolds[faces[i].manifold_id]) {
          altered = true;
          faces[i].invert();
        }
      }

      if (altered) {
        for (size_t i = 0; i < edges.size(); ++i) {
          std::vector<const face_t *> &f = connectivity.edge_to_face[i];
          for (size_t j = 0; j < (f.size() & ~1U); j += 2) {
            int m_id = -1;
            if (f[j]) m_id = f[j]->manifold_id;
            if (f[j+1]) m_id = f[j+1]->manifold_id;
            if (m_id >= 0 && (unsigned)m_id < selected_manifolds.size() && selected_manifolds[m_id]) {
              std::swap(f[j], f[j+1]);
            }
          }
        }

        for (size_t i = 0; i < std::min(selected_manifolds.size(), manifold_is_negative.size()); ++i) {
          manifold_is_negative[i] = !manifold_is_negative[i];
        }
      }
    }



    void Polyhedron::initVertexConnectivity() {
      static carve::TimingName FUNC_NAME("static Polyhedron initVertexConnectivity()");
      carve::TimingBlock block(FUNC_NAME);

      // allocate space for connectivity info.
      connectivity.vertex_to_edge.resize(vertices.size());
      connectivity.vertex_to_face.resize(vertices.size());

      std::vector<size_t> vertex_face_count;

      vertex_face_count.resize(vertices.size());

      // work out how many faces/edges each vertex is connected to, in
      // order to save on array reallocs.
      for (unsigned i = 0; i < faces.size(); ++i) {
        face_t &f = faces[i];
        for (unsigned j = 0; j < f.nVertices(); j++) {
          vertex_face_count[vertexToIndex_fast(f.vertex(j))]++;
        }
      }

      for (size_t i = 0; i < vertices.size(); ++i) {
        connectivity.vertex_to_edge[i].reserve(vertex_face_count[i]);
        connectivity.vertex_to_face[i].reserve(vertex_face_count[i]);
      }

      // record connectivity from vertex to edges.
      for (size_t i = 0; i < edges.size(); ++i) {
        size_t v1i = vertexToIndex_fast(edges[i].v1);
        size_t v2i = vertexToIndex_fast(edges[i].v2);

        connectivity.vertex_to_edge[v1i].push_back(&edges[i]);
        connectivity.vertex_to_edge[v2i].push_back(&edges[i]);
      }

      // record connectivity from vertex to faces.
      for (size_t i = 0; i < faces.size(); ++i) {
        face_t &f = faces[i];
        for (unsigned j = 0; j < f.nVertices(); j++) {
          size_t vi = vertexToIndex_fast(f.vertex(j));
          connectivity.vertex_to_face[vi].push_back(&f);
        }
      }
    }



    bool Polyhedron::initConnectivity() {
      static carve::TimingName FUNC_NAME("Polyhedron::initConnectivity()");
      carve::TimingBlock block(FUNC_NAME);

      // temporary measure: initialize connectivity by creating a
      // half-edge mesh, and then converting back.

      std::vector<mesh::Vertex<3> > vertex_storage;
      vertex_storage.reserve(vertices.size());
      for (size_t i = 0; i < vertices.size(); ++i) {
        vertex_storage.push_back(mesh::Vertex<3>(vertices[i].v));
      }

      std::vector<mesh::Face<3> *> mesh_faces;
      std::unordered_map<const mesh::Face<3> *, size_t> face_map;
      {
        std::vector<mesh::Vertex<3> *> vert_ptrs;
        for (size_t i = 0; i < faces.size(); ++i) {
          const face_t &src = faces[i];
          vert_ptrs.clear();
          vert_ptrs.reserve(src.nVertices());
          for (size_t j = 0; j < src.nVertices(); ++j) {
            size_t vi = vertexToIndex_fast(src.vertex(j));
            vert_ptrs.push_back(&vertex_storage[vi]);
          }
          mesh::Face<3> *face = new mesh::Face<3>(vert_ptrs.begin(), vert_ptrs.end());
          mesh_faces.push_back(face);
          face_map[face] = i;
        }
      }

      std::vector<mesh::Mesh<3> *> meshes;
      mesh::Mesh<3>::create(mesh_faces.begin(), mesh_faces.end(), meshes, mesh::MeshOptions());
      mesh::MeshSet<3> *meshset = new mesh::MeshSet<3>(vertex_storage, meshes);

      manifold_is_closed.resize(meshset->meshes.size());
      manifold_is_negative.resize(meshset->meshes.size());

      std::unordered_map<std::pair<size_t, size_t>, std::list<mesh::Edge<3> *> > edge_map;

      if (meshset->vertex_storage.size()) {
        mesh::Vertex<3> *Vbase = &meshset->vertex_storage[0];
        for (size_t m = 0; m < meshset->meshes.size(); ++m) {
          mesh::Mesh<3> *mesh = meshset->meshes[m];
          manifold_is_closed[m] = mesh->isClosed();
          for (size_t f = 0; f < mesh->faces.size(); ++f) {
            mesh::Face<3> *src = mesh->faces[f];
            mesh::Edge<3> *e = src->edge;
            faces[face_map[src]].manifold_id = m;
            do {
              edge_map[std::make_pair(e->v1() - Vbase, e->v2() - Vbase)].push_back(e);
              e = e->next;
            } while (e != src->edge);
          }
        }
      }

      size_t n_edges = 0;
      for (std::unordered_map<std::pair<size_t, size_t>, std::list<mesh::Edge<3> *> >::iterator i = edge_map.begin(); i != edge_map.end(); ++i) {
        if ((*i).first.first < (*i).first.second || edge_map.find(std::make_pair((*i).first.second, (*i).first.first)) == edge_map.end()) {
          n_edges++;
        }
      }

      edges.clear();
      edges.reserve(n_edges);
      for (std::unordered_map<std::pair<size_t, size_t>, std::list<mesh::Edge<3> *> >::iterator i = edge_map.begin(); i != edge_map.end(); ++i) {
        if ((*i).first.first < (*i).first.second || edge_map.find(std::make_pair((*i).first.second, (*i).first.first)) == edge_map.end()) {
          edges.push_back(edge_t(&vertices[(*i).first.first], &vertices[(*i).first.second], this));
        }
      }

      initVertexConnectivity();

      for (size_t f = 0; f < faces.size(); ++f) {
        face_t &face = faces[f];
        size_t N = face.nVertices();
        for (size_t v = 0; v < N; ++v) {
          size_t v1i = vertexToIndex_fast(face.vertex(v));
          size_t v2i = vertexToIndex_fast(face.vertex((v+1)%N));
          std::vector<const edge_t *> found_edge;

          CARVE_ASSERT(carve::is_sorted(connectivity.vertex_to_edge[v1i].begin(), connectivity.vertex_to_edge[v1i].end()));
          CARVE_ASSERT(carve::is_sorted(connectivity.vertex_to_edge[v2i].begin(), connectivity.vertex_to_edge[v2i].end()));

          std::set_intersection(connectivity.vertex_to_edge[v1i].begin(), connectivity.vertex_to_edge[v1i].end(),
                                connectivity.vertex_to_edge[v2i].begin(), connectivity.vertex_to_edge[v2i].end(),
                                std::back_inserter(found_edge));

          CARVE_ASSERT(found_edge.size() == 1);

          face.edge(v) = found_edge[0];
        }
      }

      connectivity.edge_to_face.resize(edges.size());

      for (size_t i = 0; i < edges.size(); ++i) {
        size_t v1i = vertexToIndex_fast(edges[i].v1);
        size_t v2i = vertexToIndex_fast(edges[i].v2);
        std::list<mesh::Edge<3> *> &efwd = edge_map[std::make_pair(v1i, v2i)];
        std::list<mesh::Edge<3> *> &erev = edge_map[std::make_pair(v1i, v2i)];

        for (std::list<mesh::Edge<3> *>::iterator j = efwd.begin(); j != efwd.end(); ++j) {
          mesh::Edge<3> *edge = *j;
          if (face_map.find(edge->face) != face_map.end()) {
            connectivity.edge_to_face[i].push_back(&faces[face_map[edge->face]]);
            if (edge->rev == NULL) {
              connectivity.edge_to_face[i].push_back(NULL);
            } else {
              connectivity.edge_to_face[i].push_back(&faces[face_map[edge->rev->face]]);
            }
          }
        }
        for (std::list<mesh::Edge<3> *>::iterator j = erev.begin(); j != erev.end(); ++j) {
          mesh::Edge<3> *edge = *j;
          if (face_map.find(edge->face) != face_map.end()) {
            if (edge->rev == NULL) {
              connectivity.edge_to_face[i].push_back(NULL);
              connectivity.edge_to_face[i].push_back(&faces[face_map[edge->face]]);
            }
          }
        }
      }

      delete meshset;

      return true;
    }



    bool Polyhedron::calcManifoldEmbedding() {
      // this could be significantly sped up using bounding box tests
      // to work out what pairs of manifolds are embedding candidates.
      // A per-manifold AABB could also be used to speed up
      // testVertexAgainstClosedManifolds().

      static carve::TimingName FUNC_NAME("Polyhedron::calcManifoldEmbedding()");
      static carve::TimingName CME_V("Polyhedron::calcManifoldEmbedding() (vertices)");
      static carve::TimingName CME_E("Polyhedron::calcManifoldEmbedding() (edges)");
      static carve::TimingName CME_F("Polyhedron::calcManifoldEmbedding() (faces)");

      carve::TimingBlock block(FUNC_NAME);

      const unsigned MCOUNT = manifoldCount();
      if (MCOUNT < 2) return true;

      std::set<int> vertex_manifolds;
      std::map<int, std::set<int> > embedding;

      carve::Timing::start(CME_V);
      for (size_t i = 0; i < vertices.size(); ++i) {
        vertex_manifolds.clear();
        if (vertexManifolds(&vertices[i], set_inserter(vertex_manifolds)) != 1) continue;
        int m_id = *vertex_manifolds.begin();
        if (embedding.find(m_id) == embedding.end()) {
          if (emb_test(this, embedding, vertices[i].v, m_id) && embedding.size() == MCOUNT) {
            carve::Timing::stop();
            goto done;
          }
        }
      }
      carve::Timing::stop();

      carve::Timing::start(CME_E);
      for (size_t i = 0; i < edges.size(); ++i) {
        if (connectivity.edge_to_face[i].size() == 2) {
          int m_id;
          const face_t *f1 = connectivity.edge_to_face[i][0];
          const face_t *f2 = connectivity.edge_to_face[i][1];
          if (f1) m_id = f1->manifold_id;
          if (f2) m_id = f2->manifold_id;
          if (embedding.find(m_id) == embedding.end()) {
            if (emb_test(this, embedding, (edges[i].v1->v + edges[i].v2->v) / 2, m_id) && embedding.size() == MCOUNT) {
              carve::Timing::stop();
              goto done;
            }
          }
        }
      }
      carve::Timing::stop();

      carve::Timing::start(CME_F);
      for (size_t i = 0; i < faces.size(); ++i) {
        int m_id = faces[i].manifold_id;
        if (embedding.find(m_id) == embedding.end()) {
          carve::geom2d::P2 pv;
          if (!carve::geom2d::pickContainedPoint(faces[i].projectedVertices(), pv)) continue;
          carve::geom3d::Vector v = carve::poly::face::unproject(faces[i], pv);
          if (emb_test(this, embedding, v, m_id) && embedding.size() == MCOUNT) {
            carve::Timing::stop();
            goto done;
          }
        }
      }
      carve::Timing::stop();

      CARVE_FAIL("could not find test points");

      // std::cerr << "could not find test points!!!" << std::endl;
      // return true;
    done:;
      for (std::map<int, std::set<int> >::iterator i = embedding.begin(); i != embedding.end(); ++i) {
#if defined(CARVE_DEBUG)
        std::cerr << (*i).first << " : ";
        std::copy((*i).second.begin(), (*i).second.end(), std::ostream_iterator<int>(std::cerr, ","));
        std::cerr << std::endl;
#endif
        (*i).second.insert(-1);
      }
      std::set<int> parents, new_parents;
      parents.insert(-1);

      while (embedding.size()) {
        new_parents.clear();
        for (std::map<int, std::set<int> >::iterator i = embedding.begin(); i != embedding.end(); ++i) {
          if ((*i).second.size() == 1) {
            if (parents.find(*(*i).second.begin()) != parents.end()) {
              new_parents.insert((*i).first);
#if defined(CARVE_DEBUG)
              std::cerr << "parent(" << (*i).first << "): " << *(*i).second.begin() << std::endl;
#endif
            } else {
#if defined(CARVE_DEBUG)
              std::cerr << "no parent: " << (*i).first << " (looking for: " << *(*i).second.begin() << ")" << std::endl;
#endif
            }
          }
        }
        for (std::set<int>::const_iterator i = new_parents.begin(); i != new_parents.end(); ++i) {
          embedding.erase(*i);
        }
        for (std::map<int, std::set<int> >::iterator i = embedding.begin(); i != embedding.end(); ++i) {
          size_t n = 0;
          for (std::set<int>::const_iterator j = parents.begin(); j != parents.end(); ++j) {
            n += (*i).second.erase((*j));
          }
          CARVE_ASSERT(n != 0);
        }
        parents.swap(new_parents);
      }

      return true;
    }



    bool Polyhedron::init() {
      static carve::TimingName FUNC_NAME("Polyhedron::init()");
      carve::TimingBlock block(FUNC_NAME);
  
      aabb.fit(vertices.begin(), vertices.end(), vec_adapt_vertex_ref());

      connectivity.vertex_to_edge.clear();
      connectivity.vertex_to_face.clear();
      connectivity.edge_to_face.clear();

      if (!initConnectivity()) return false;
      if (!initSpatialIndex()) return false;

      return true;
    }



    void Polyhedron::faceRecalc() {
      for (size_t i = 0; i < faces.size(); ++i) {
        if (!faces[i].recalc()) {
          std::ostringstream out;
          out << "face " << i << " recalc failed";
          throw carve::exception(out.str());
        }
      }
    }



    Polyhedron::Polyhedron(const Polyhedron &poly) {
      faces.reserve(poly.faces.size());

      for (size_t i = 0; i < poly.faces.size(); ++i) {
        const face_t &src = poly.faces[i];
        faces.push_back(src);
      }
      commonFaceInit(false); // calls setFaceAndVertexOwner() and init()
    }



    Polyhedron::Polyhedron(const Polyhedron &poly, const std::vector<bool> &selected_manifolds) {
      size_t n_faces = 0;

      for (size_t i = 0; i < poly.faces.size(); ++i) {
        const face_t &src = poly.faces[i];
        if (src.manifold_id >= 0 &&
            (unsigned)src.manifold_id < selected_manifolds.size() &&
            selected_manifolds[src.manifold_id]) {
          n_faces++;
        }
      }

      faces.reserve(n_faces);

      for (size_t i = 0; i < poly.faces.size(); ++i) {
        const face_t &src = poly.faces[i];
        if (src.manifold_id >= 0 &&
            (unsigned)src.manifold_id < selected_manifolds.size() &&
            selected_manifolds[src.manifold_id]) {
          faces.push_back(src);
        }
      }

      commonFaceInit(false); // calls setFaceAndVertexOwner() and init()
    }



    Polyhedron::Polyhedron(const Polyhedron &poly, int m_id) {
      size_t n_faces = 0;

      for (size_t i = 0; i < poly.faces.size(); ++i) {
        const face_t &src = poly.faces[i];
        if (src.manifold_id == m_id) n_faces++;
      }

      faces.reserve(n_faces);

      for (size_t i = 0; i < poly.faces.size(); ++i) {
        const face_t &src = poly.faces[i];
        if (src.manifold_id == m_id) faces.push_back(src);
      }

      commonFaceInit(false); // calls setFaceAndVertexOwner() and init()
    }



    Polyhedron::Polyhedron(const std::vector<carve::geom3d::Vector> &_vertices,
                           int n_faces,
                           const std::vector<int> &face_indices) {
      // The polyhedron is defined by a vector of vertices, which we
      // want to copy, and a face index list, from which we need to
      // generate a set of Faces.

      vertices.clear();
      vertices.resize(_vertices.size());
      for (size_t i = 0; i < _vertices.size(); ++i) {
        vertices[i].v = _vertices[i];
      }

      faces.reserve(n_faces);
  
      std::vector<int>::const_iterator iter = face_indices.begin();
      std::vector<const vertex_t *> v;
      for (int i = 0; i < n_faces; ++i) {
        int vertexCount = *iter++;
    
        v.clear();
    
        while (vertexCount--) {
          CARVE_ASSERT(*iter >= 0);
          CARVE_ASSERT((unsigned)*iter < vertices.size());
          v.push_back(&vertices[*iter++]);
        }
        faces.push_back(face_t(v));
      }

      setFaceAndVertexOwner();

      if (!init()) {
        throw carve::exception("polyhedron creation failed");
      }
    }



    Polyhedron::Polyhedron(std::vector<face_t> &_faces,
                           std::vector<vertex_t> &_vertices,
                           bool _recalc) {
      faces.swap(_faces);
      vertices.swap(_vertices);

      setFaceAndVertexOwner();

      if (_recalc) faceRecalc();

      if (!init()) {
        throw carve::exception("polyhedron creation failed");
      }
    }



    Polyhedron::Polyhedron(std::vector<face_t> &_faces,
                           bool _recalc) {
      faces.swap(_faces);
      commonFaceInit(_recalc); // calls setFaceAndVertexOwner() and init()
    }



    Polyhedron::Polyhedron(std::list<face_t> &_faces,
                           bool _recalc) {
      faces.reserve(_faces.size());
      std::copy(_faces.begin(), _faces.end(), std::back_inserter(faces));
      commonFaceInit(_recalc); // calls setFaceAndVertexOwner() and init()
    }



    void Polyhedron::collectFaceVertices(std::vector<face_t> &faces,
                                         std::vector<vertex_t> &vertices,
                                         std::unordered_map<const vertex_t *, const vertex_t *> &vmap) {
      // Given a set of faces, copy all referenced vertices into a
      // single vertex array and update the faces to point into that
      // array. On exit, vmap contains a mapping from old pointer to
      // new pointer.

      vertices.clear();
      vmap.clear();

      for (size_t i = 0, il = faces.size(); i != il; ++i) {
        face_t &f = faces[i];

        for (size_t j = 0, jl = f.nVertices(); j != jl; ++j) {
          vmap[f.vertex(j)] = NULL;
        }
      }

      vertices.reserve(vmap.size());

      for (std::unordered_map<const vertex_t *, const vertex_t *>::iterator i = vmap.begin(),
             e = vmap.end();
           i != e;
           ++i) {
        vertices.push_back(*(*i).first);
        (*i).second = &vertices.back();
      }

      for (size_t i = 0, il = faces.size(); i != il; ++i) {
        face_t &f = faces[i];

        for (size_t j = 0, jl = f.nVertices(); j != jl; ++j) {
          f.vertex(j) = vmap[f.vertex(j)];
        }
      }
    }



    void Polyhedron::collectFaceVertices(std::vector<face_t> &faces,
                                         std::vector<vertex_t> &vertices) {
      std::unordered_map<const vertex_t *, const vertex_t *> vmap;
      collectFaceVertices(faces, vertices, vmap);
    }



    void Polyhedron::setFaceAndVertexOwner() {
      for (size_t i = 0; i < vertices.size(); ++i) vertices[i].owner = this;
      for (size_t i = 0; i < faces.size(); ++i) faces[i].owner = this;
    }



    void Polyhedron::commonFaceInit(bool _recalc) {
      collectFaceVertices(faces, vertices);
      setFaceAndVertexOwner();
      if (_recalc) faceRecalc();

      if (!init()) {
        throw carve::exception("polyhedron creation failed");
      }
    }



    Polyhedron::~Polyhedron() {
    }



    void Polyhedron::testVertexAgainstClosedManifolds(const carve::geom3d::Vector &v,
                                                      std::map<int, PointClass> &result,
                                                      bool ignore_orientation) const {

      for (size_t i = 0; i < faces.size(); i++) {
        if (!manifold_is_closed[faces[i].manifold_id]) continue; // skip open manifolds
        if (faces[i].containsPoint(v)) {
          result[faces[i].manifold_id] = POINT_ON;
        }
      }

      double ray_len = aabb.extent.length() * 2;

      std::vector<const face_t *> possible_faces;

      std::vector<std::pair<const face_t *, carve::geom3d::Vector> > manifold_intersections;

      boost::mt19937 rng;
      boost::uniform_on_sphere<double> distrib(3);
      boost::variate_generator<boost::mt19937 &, boost::uniform_on_sphere<double> > gen(rng, distrib);

      for (;;) {
        carve::geom3d::Vector ray_dir;
        ray_dir = gen();

        carve::geom3d::Vector v2 = v + ray_dir * ray_len;

        bool failed = false;
        carve::geom3d::LineSegment line(v, v2);
        carve::geom3d::Vector intersection;

        possible_faces.clear();
        manifold_intersections.clear();
        octree.findFacesNear(line, possible_faces);

        for (unsigned i = 0; !failed && i < possible_faces.size(); i++) {
          if (!manifold_is_closed[possible_faces[i]->manifold_id]) continue; // skip open manifolds
          if (result.find(possible_faces[i]->manifold_id) != result.end()) continue; // already ON

          switch (possible_faces[i]->lineSegmentIntersection(line, intersection)) {
          case INTERSECT_FACE: {
            manifold_intersections.push_back(std::make_pair(possible_faces[i], intersection));
            break;
          }
          case INTERSECT_NONE: {
            break;
          }
          default: {
            failed = true;
            break;
          }
          }
        }

        if (!failed) break;
      }

      std::vector<int> crossings(manifold_is_closed.size(), 0);

      for (size_t i = 0; i < manifold_intersections.size(); ++i) {
        const face_t *f = manifold_intersections[i].first;
        crossings[f->manifold_id]++;
      }

      for (size_t i = 0; i < crossings.size(); ++i) {
#if defined(CARVE_DEBUG)
        std::cerr << "crossing: " << i << " = " << crossings[i] << " is_negative = " << manifold_is_negative[i] << std::endl;
#endif
        if (!manifold_is_closed[i]) continue;
        if (result.find(i) != result.end()) continue;
        PointClass pc = (crossings[i] & 1) ? POINT_IN : POINT_OUT;
        if (!ignore_orientation && manifold_is_negative[i]) pc = (PointClass)-pc;
        result[i] = pc;
      }
    }



    PointClass Polyhedron::containsVertex(const carve::geom3d::Vector &v,
                                          const face_t **hit_face,
                                          bool even_odd,
                                          int manifold_id) const {
      if (hit_face) *hit_face = NULL;

#if defined(DEBUG_CONTAINS_VERTEX)
      std::cerr << "{containsVertex " << v << "}" << std::endl;
#endif

      if (!aabb.containsPoint(v)) {
#if defined(DEBUG_CONTAINS_VERTEX)
        std::cerr << "{final:OUT(aabb short circuit)}" << std::endl;
#endif
        // XXX: if the top level manifolds are negative, this should be POINT_IN.
        // for the moment, this only works for a single manifold.
        if (manifold_is_negative.size() == 1 && manifold_is_negative[0]) return POINT_IN;
        return POINT_OUT;
      }

      for (size_t i = 0; i < faces.size(); i++) {
        if (manifold_id != -1 && manifold_id != faces[i].manifold_id) continue;

        // XXX: Do allow the tested vertex to be ON an open
        // manifold. This was here originally because of the
        // possibility of an open manifold contained within a closed
        // manifold.

        // if (!manifold_is_closed[faces[i].manifold_id]) continue;

        if (faces[i].containsPoint(v)) {
#if defined(DEBUG_CONTAINS_VERTEX)
          std::cerr << "{final:ON(hits face " << &faces[i] << ")}" << std::endl;
#endif
          if (hit_face) *hit_face = &faces[i];
          return POINT_ON;
        }
      }

      double ray_len = aabb.extent.length() * 2;

      std::vector<const face_t *> possible_faces;

      std::vector<std::pair<const face_t *, carve::geom3d::Vector> > manifold_intersections;

      for (;;) {
        double a1 = random() / double(RAND_MAX) * M_TWOPI;
        double a2 = random() / double(RAND_MAX) * M_TWOPI;

        carve::geom3d::Vector ray_dir = carve::geom::VECTOR(sin(a1) * sin(a2), cos(a1) * sin(a2), cos(a2));

#if defined(DEBUG_CONTAINS_VERTEX)
        std::cerr << "{testing ray: " << ray_dir << "}" << std::endl;
#endif

        carve::geom3d::Vector v2 = v + ray_dir * ray_len;

        bool failed = false;
        carve::geom3d::LineSegment line(v, v2);
        carve::geom3d::Vector intersection;

        possible_faces.clear();
        manifold_intersections.clear();
        octree.findFacesNear(line, possible_faces);

        for (unsigned i = 0; !failed && i < possible_faces.size(); i++) {
          if (manifold_id != -1 && manifold_id != faces[i].manifold_id) continue;

          if (!manifold_is_closed[possible_faces[i]->manifold_id]) continue;

          switch (possible_faces[i]->lineSegmentIntersection(line, intersection)) {
          case INTERSECT_FACE: {

#if defined(DEBUG_CONTAINS_VERTEX)
            std::cerr << "{intersects face: " << possible_faces[i]
                      << " dp: " << dot(ray_dir, possible_faces[i]->plane_eqn.N) << "}" << std::endl;
#endif

            if (!even_odd && fabs(dot(ray_dir, possible_faces[i]->plane_eqn.N)) < EPSILON) {

#if defined(DEBUG_CONTAINS_VERTEX)
              std::cerr << "{failing(small dot product)}" << std::endl;
#endif

              failed = true;
              break;
            }
            manifold_intersections.push_back(std::make_pair(possible_faces[i], intersection));
            break;
          }
          case INTERSECT_NONE: {
            break;
          }
          default: {

#if defined(DEBUG_CONTAINS_VERTEX)
            std::cerr << "{failing(degenerate intersection)}" << std::endl;
#endif
            failed = true;
            break;
          }
          }
        }

        if (!failed) {
          if (even_odd) {
            return (manifold_intersections.size() & 1) ? POINT_IN : POINT_OUT;
          }

#if defined(DEBUG_CONTAINS_VERTEX)
          std::cerr << "{intersections ok [count:"
                    << manifold_intersections.size()
                    << "], sorting}"
                    << std::endl;
#endif

          carve::geom3d::sortInDirectionOfRay(ray_dir,
                                              manifold_intersections.begin(),
                                              manifold_intersections.end(),
                                              carve::geom3d::vec_adapt_pair_second());

          std::vector<int> crossings(manifold_is_closed.size(), 0);

          for (size_t i = 0; i < manifold_intersections.size(); ++i) {
            const face_t *f = manifold_intersections[i].first;
            if (dot(ray_dir, f->plane_eqn.N) < 0.0) {
              crossings[f->manifold_id]++;
            } else {
              crossings[f->manifold_id]--;
            }
          }

#if defined(DEBUG_CONTAINS_VERTEX)
          for (size_t i = 0; i < crossings.size(); ++i) {
            std::cerr << "{manifold " << i << " crossing count: " << crossings[i] << "}" << std::endl;
          }
#endif

          for (size_t i = 0; i < manifold_intersections.size(); ++i) {
            const face_t *f = manifold_intersections[i].first;

#if defined(DEBUG_CONTAINS_VERTEX)
            std::cerr << "{intersection at "
                      << manifold_intersections[i].second
                      << " id: "
                      << f->manifold_id
                      << " count: "
                      << crossings[f->manifold_id]
                      << "}"
                      << std::endl;
#endif

            if (crossings[f->manifold_id] < 0) {
              // inside this manifold.

#if defined(DEBUG_CONTAINS_VERTEX)
              std::cerr << "{final:IN}" << std::endl;
#endif

              return POINT_IN;
            } else if (crossings[f->manifold_id] > 0) {
              // outside this manifold, but it's an infinite manifold. (for instance, an inverted cube)

#if defined(DEBUG_CONTAINS_VERTEX)
              std::cerr << "{final:OUT}" << std::endl;
#endif

              return POINT_OUT;
            }
          }

#if defined(DEBUG_CONTAINS_VERTEX)
          std::cerr << "{final:OUT(default)}" << std::endl;
#endif

          return POINT_OUT;
        }
      }
    }



    void Polyhedron::findEdgesNear(const carve::geom::aabb<3> &aabb,
                                   std::vector<const edge_t *> &outEdges) const {
      outEdges.clear();
      octree.findEdgesNear(aabb, outEdges);
    }



    void Polyhedron::findEdgesNear(const carve::geom3d::LineSegment &line,
                                   std::vector<const edge_t *> &outEdges) const {
      outEdges.clear();
      octree.findEdgesNear(line, outEdges);
    }



    void Polyhedron::findEdgesNear(const carve::geom3d::Vector &v,
                                   std::vector<const edge_t *> &outEdges) const {
      outEdges.clear();
      octree.findEdgesNear(v, outEdges);
    }



    void Polyhedron::findEdgesNear(const face_t &face,
                                   std::vector<const edge_t *> &edges) const {
      edges.clear();
      octree.findEdgesNear(face, edges);
    }



    void Polyhedron::findEdgesNear(const edge_t &edge,
                                   std::vector<const edge_t *> &outEdges) const {
      outEdges.clear();
      octree.findEdgesNear(edge, outEdges);
    }



    void Polyhedron::findFacesNear(const carve::geom3d::LineSegment &line,
                                   std::vector<const face_t *> &outFaces) const {
      outFaces.clear();
      octree.findFacesNear(line, outFaces);
    }



    void Polyhedron::findFacesNear(const carve::geom::aabb<3> &aabb,
                                   std::vector<const face_t *> &outFaces) const {
      outFaces.clear();
      octree.findFacesNear(aabb, outFaces);
    }



    void Polyhedron::findFacesNear(const edge_t &edge,
                                   std::vector<const face_t *> &outFaces) const {
      outFaces.clear();
      octree.findFacesNear(edge, outFaces);
    }



    void Polyhedron::transform(const carve::math::Matrix &xform) {
      for (size_t i = 0; i < vertices.size(); i++) {
        vertices[i].v = xform * vertices[i].v;
      }
      for (size_t i = 0; i < faces.size(); i++) {
        faces[i].recalc();
      }
      init();
    }



    void Polyhedron::print(std::ostream &o) const {
      o << "Polyhedron@" << this << " {" << std::endl;
      for (std::vector<vertex_t >::const_iterator
             i = vertices.begin(), e = vertices.end(); i != e; ++i) {
        o << "  V@" << &(*i) << " " << (*i).v << std::endl;
      }
      for (std::vector<edge_t >::const_iterator
             i = edges.begin(), e = edges.end(); i != e; ++i) {
        o << "  E@" << &(*i) << " {" << std::endl;
        o << "    V@" << (*i).v1 << " - " << "V@" << (*i).v2 << std::endl;
        const std::vector<const face_t *> &faces = connectivity.edge_to_face[edgeToIndex_fast(&(*i))];
        for (size_t j = 0; j < (faces.size() & ~1U); j += 2) {
          o << "      fp: F@" << faces[j] << ", F@" << faces[j+1] << std::endl;
        }
        o << "  }" << std::endl;
      }
      for (std::vector<face_t >::const_iterator
             i = faces.begin(), e = faces.end(); i != e; ++i) {
        o << "  F@" << &(*i) << " {" << std::endl;
        o << "    vertices {" << std::endl;
        for (face_t::const_vertex_iter_t j = (*i).vbegin(), je = (*i).vend(); j != je; ++j) {
          o << "      V@" << (*j) << std::endl;
        }
        o << "    }" << std::endl;
        o << "    edges {" << std::endl;
        for (face_t::const_edge_iter_t j = (*i).ebegin(), je = (*i).eend(); j != je; ++j) {
          o << "      E@" << (*j) << std::endl;
        }
        carve::geom::plane<3> p = (*i).plane_eqn;
        o << "    }" << std::endl;
        o << "    normal " << (*i).plane_eqn.N << std::endl;
        o << "    aabb " << (*i).aabb << std::endl;
        o << "    plane_eqn ";
        carve::geom::operator<< <3>(o, p);
        o << std::endl;
        o << "  }" << std::endl;
      }

      o << "}" << std::endl;
    }



    void Polyhedron::canonicalize() {
      orderVertices();
      for (size_t i = 0; i < faces.size(); i++) {
        face_t &f = faces[i];
        size_t j = std::distance(f.vbegin(),
                                 std::min_element(f.vbegin(),
                                                  f.vend()));
        if (j) {
          {
            std::vector<const vertex_t *> temp;
            temp.reserve(f.nVertices());
            std::copy(f.vbegin() + j, f.vend(),       std::back_inserter(temp));
            std::copy(f.vbegin(),     f.vbegin() + j, std::back_inserter(temp));
            std::copy(temp.begin(),   temp.end(),     f.vbegin());
          }
          {
            std::vector<const edge_t *> temp;
            temp.reserve(f.nEdges());
            std::copy(f.ebegin() + j, f.eend(),       std::back_inserter(temp));
            std::copy(f.ebegin(),     f.ebegin() + j, std::back_inserter(temp));
            std::copy(temp.begin(),   temp.end(),     f.ebegin());
          }
        }
      }

      std::vector<face_t *> face_ptrs;
      face_ptrs.reserve(faces.size());
      for (size_t i = 0; i < faces.size(); ++i) face_ptrs.push_back(&faces[i]);
      std::sort(face_ptrs.begin(), face_ptrs.end(), order_faces());
      std::vector<face_t> sorted_faces;
      sorted_faces.reserve(faces.size());
      for (size_t i = 0; i < faces.size(); ++i) sorted_faces.push_back(*face_ptrs[i]);
      std::swap(faces, sorted_faces);
    }

  }
}

