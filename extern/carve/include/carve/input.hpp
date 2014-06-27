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

#include <map>
#include <string>

#include <carve/carve.hpp>
#include <carve/poly.hpp>
#include <carve/mesh.hpp>
#include <carve/polyline.hpp>
#include <carve/pointset.hpp>



namespace carve {
  namespace input {

    typedef std::map<std::string, std::string> Options;

    static inline Options opts() {
      return Options();
    }

    static inline Options opts(const char **kv) {
      Options r;
      for (size_t i = 0; kv[i] != NULL; i += 2) {
        r[kv[i]] = kv[i+1];
      }
      return r;
    }

    static inline Options opts(const std::string &k1, const std::string &v1) {
      Options r;
      r[k1] = v1;
      return r;
    }

    static inline Options opts(const std::string &k1, const std::string &v1,
                               const std::string &k2, const std::string &v2) {
      Options r;
      r[k1] = v1;
      r[k2] = v2;
      return r;
    }

    static inline Options opts(const std::string &k1, const std::string &v1,
                               const std::string &k2, const std::string &v2,
                               const std::string &k3, const std::string &v3) {
      Options r;
      r[k1] = v1;
      r[k2] = v2;
      r[k3] = v3;
      return r;
    }

    static inline bool _bool(const std::string &str, bool _default = false) {
      if (str == "true") return true;
      if (str == "false") return false;
      return _default;
    }

    struct Data {
      Data() {
      }

      virtual ~Data() {
      }

      virtual void transform(const carve::math::Matrix & /* transform */) {
      }
    };



    struct VertexData : public Data {
      std::vector<carve::geom3d::Vector> points;

      VertexData() : Data() {
      }

      virtual ~VertexData() {
      }

      virtual void transform(const carve::math::Matrix &transform) {
        for (size_t i = 0; i < points.size(); ++i) {
          points[i] *= transform;
        }
      }

      size_t addVertex(carve::geom3d::Vector point) {
        size_t index = points.size();
        points.push_back(point);
        return index;
      }
  
      inline void reserveVertices(int count) {
        points.reserve(count);
      }

      size_t getVertexCount() const {
        return points.size();
      }

      const carve::geom3d::Vector &getVertex(int index) const {
        return points[index];
      }
    };



    struct PolyhedronData : public VertexData {
      std::vector<int> faceIndices;
      int faceCount;

      PolyhedronData() : VertexData(), faceIndices(), faceCount(0) {
      }

      virtual ~PolyhedronData() {
      }

      void reserveFaces(int count, int avgFaceSize) {
        faceIndices.reserve(faceIndices.size() + count * (1 + avgFaceSize));
      }
  
      int getFaceCount() const {
        return faceCount;
      }
  
      template <typename Iter>
      void addFace(Iter begin, Iter end) {
        size_t n = std::distance(begin, end);
        faceIndices.reserve(faceIndices.size() + n + 1);
        faceIndices.push_back(n);
        std::copy(begin, end, std::back_inserter(faceIndices));
        ++faceCount;
      }
  
      void addFace(int a, int b, int c) {
        faceIndices.push_back(3);
        faceIndices.push_back(a);
        faceIndices.push_back(b);
        faceIndices.push_back(c);
        ++faceCount;
      }

      void addFace(int a, int b, int c, int d) {
        faceIndices.push_back(4);
        faceIndices.push_back(a);
        faceIndices.push_back(b);
        faceIndices.push_back(c);
        faceIndices.push_back(d);
        ++faceCount;
      }

      void clearFaces() {
        faceIndices.clear();
        faceCount = 0;
      }

      carve::poly::Polyhedron *create(const Options &options) const {
        return new carve::poly::Polyhedron(points, faceCount, faceIndices);
      }

      carve::mesh::MeshSet<3> *createMesh(const Options &options) const {
        Options::const_iterator i;
        carve::mesh::MeshOptions opts;
        i = options.find("avoid_cavities");
        if (i != options.end()) {
          opts.avoid_cavities(_bool((*i).second));
        }
        return new carve::mesh::MeshSet<3>(points, faceCount, faceIndices, opts);
      }
    };



    struct PolylineSetData : public VertexData {
      typedef std::pair<bool, std::vector<int> > polyline_data_t;
      std::list<polyline_data_t> polylines;

      PolylineSetData() : VertexData(), polylines() {
      }

      virtual ~PolylineSetData() {
      }

      void beginPolyline(bool closed = false) {
        polylines.push_back(std::make_pair(closed, std::vector<int>()));
      }

      void reservePolyline(size_t len) {
        polylines.back().second.reserve(len);
      }

      void addPolylineIndex(int idx) {
        polylines.back().second.push_back(idx);
      }

      carve::line::PolylineSet *create(const Options &options) const {
        carve::line::PolylineSet *p = new carve::line::PolylineSet(points);

        for (std::list<polyline_data_t>::const_iterator i = polylines.begin();
             i != polylines.end();
             ++i) {
          p->addPolyline((*i).first, (*i).second.begin(), (*i).second.end());
        }
        return p;
      }
    };



    struct PointSetData : public VertexData {

      PointSetData() : VertexData() {
      }

      virtual ~PointSetData() {
      }

      carve::point::PointSet *create(const Options &options) const {
        carve::point::PointSet *p = new carve::point::PointSet(points);
        return p;
      }
    };



    class Input {
    public:
      std::list<Data *> input;

      Input() {
      }

      ~Input() {
        for (std::list<Data *>::iterator i = input.begin(); i != input.end(); ++i) {
          delete (*i);
        }
      }

      void addDataBlock(Data *data) {
        input.push_back(data);
      }

      void transform(const carve::math::Matrix &transform) {
        if (transform == carve::math::Matrix::IDENT()) return;
        for (std::list<Data *>::iterator i = input.begin(); i != input.end(); ++i) {
          (*i)->transform(transform);
        }
      }

      template<typename T>
      static inline T *create(Data *d, const Options &options = Options()) {
        return NULL;
      }
    };

    template<>
    inline carve::mesh::MeshSet<3> *Input::create(Data *d, const Options &options) {
      PolyhedronData *p = dynamic_cast<PolyhedronData *>(d);
      if (p == NULL) return NULL;
      return p->createMesh(options);
    }

    template<>
    inline carve::poly::Polyhedron *Input::create(Data *d, const Options &options) {
      PolyhedronData *p = dynamic_cast<PolyhedronData *>(d);
      if (p == NULL) return NULL;
      return p->create(options);
    }

    template<>
    inline carve::line::PolylineSet *Input::create(Data *d, const Options &options) {
      PolylineSetData *p = dynamic_cast<PolylineSetData *>(d);
      if (p == NULL) return NULL;
      return p->create(options);
    }

    template<>
    inline carve::point::PointSet *Input::create(Data *d, const Options &options) {
      PointSetData *p = dynamic_cast<PointSetData *>(d);
      if (p == NULL) return NULL;
      return p->create(options);
    }

  }
}
