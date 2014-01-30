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
#include <carve/geom2d.hpp>
#include <carve/poly.hpp>
#include <carve/mesh.hpp>
#include <carve/csg.hpp>

namespace carve {
  namespace interpolate {

    static inline std::vector<double> polyInterpolate(const std::vector<carve::geom2d::P2> &s,
                                                      const carve::geom2d::P2 &v) {
      // see hormann et al. 2006
      const size_t SZ = s.size();
      std::vector<double> r;
      std::vector<double> A;
      std::vector<double> D;

      std::vector<double> result;

      r.resize(SZ);
      A.resize(SZ);
      D.resize(SZ);

      result.resize(SZ, 0.0);

      for (size_t i = 0; i < SZ; ++i) {
        size_t i2 = (i + 1) % SZ;
        carve::geom2d::P2 si = s[i] - v;
        carve::geom2d::P2 si2 = s[i2] - v;

        r[i] = sqrt(dot(si, si));
        A[i] = cross(si, si2) / 2.0;
        D[i] = dot(si, si2);
        if (fabs(r[i]) < 1e-16) {
          result[i] = 1.0;
          return result;
        } else if (fabs(A[i]) < 1e-16 && D[i] < 0.0) {
          double r2 = sqrt(dot(si2, si2));
          result[i2] = r[i] / (r[i] + r2);
          result[i] = r2 / (r[i] + r2);
          return result;
        }
      }

      double w_sum = 0.0;

      for (size_t i = 0; i < SZ; ++i) {
        size_t i_m = (i + SZ - 1) % SZ;
        size_t i_p = (i + 1) % SZ;

        double w = 0.0;
        if (fabs(A[i_m]) > 1e-16)
          w += (r[i_m] - D[i_m] / r[i]) / A[i_m];
        if (fabs(A[i]) > 1e-16)
          w += (r[i_p] - D[i] / r[i]) / A[i];

        result[i] = w;
        w_sum += w;
      }
  
      for (size_t i = 0; i < SZ; ++i) {
        result[i] /= w_sum;
      }

//       carve::geom2d::P2 test;
//       for (size_t i = 0; i < SZ; ++i) {
//         test = test + result[i] * s[i];
//       }

      return result;
    }



    template<typename iter_t,
             typename adapt_t,
             typename val_t,
             typename mod_t>
    val_t interp(iter_t begin,
                 iter_t end,
                 adapt_t adapt,
                 const std::vector<val_t> &vals,
                 double x,
                 double y,
                 mod_t mod = mod_t()) {
      std::vector<carve::geom2d::P2> s;
      s.reserve(std::distance(begin, end));
      std::transform(begin, end, std::back_inserter(s), adapt);
      std::vector<double> weight = polyInterpolate(s, carve::geom::VECTOR(x, y));

      val_t v;
      for (size_t z = 0; z < weight.size(); z++) {
        v += weight[z] * vals[z];
      }

      return mod(v);
    }



    template<typename iter_t,
             typename adapt_t,
             typename val_t>
    val_t interp(iter_t begin,
                 iter_t end,
                 adapt_t adapt,
                 const std::vector<val_t> &vals,
                 double x,
                 double y) {
      return interp(begin, end, adapt, vals, x, y, identity_t<val_t>());
    }


    
    template<typename vertex_t,
             typename adapt_t,
             typename val_t,
             typename mod_t>
    val_t interp(const std::vector<vertex_t> &poly,
                 adapt_t adapt,
                 const std::vector<val_t> &vals,
                 double x,
                 double y,
                 mod_t mod = mod_t()) {
      return interp(poly.begin(), poly.end(), adapt, vals, x, y, mod);
    }



    template<typename vertex_t,
             typename adapt_t,
             typename val_t>
    val_t interp(const std::vector<vertex_t> &poly,
                 adapt_t adapt,
                 const std::vector<val_t> &vals,
                 double x,
                 double y) {
      return interp(poly.begin(), poly.end(), adapt, vals, x, y, identity_t<val_t>());
    }
    


    template<typename val_t,
             typename mod_t>
    val_t interp(const std::vector<carve::geom2d::P2> &poly,
                 const std::vector<val_t> &vals,
                 double x,
                 double y,
                 mod_t mod = mod_t()) {
      std::vector<double> weight = polyInterpolate(poly, carve::geom::VECTOR(x, y));

      val_t v;
      for (size_t z = 0; z < weight.size(); z++) {
        v += weight[z] * vals[z];
      }

      return mod(v);
    }



    template<typename val_t>
    val_t interp(const std::vector<carve::geom2d::P2> &poly,
                 const std::vector<val_t> &vals,
                 double x,
                 double y) {
      return interp(poly, vals, x, y, identity_t<val_t>());
    }



    class Interpolator {
    public:
      typedef carve::mesh::MeshSet<3> meshset_t;

    protected:
      friend struct Hook;

      struct Hook : public carve::csg::CSG::Hook {
        const carve::csg::CSG &csg;
        Interpolator *interpolator;

        virtual unsigned hookBits() const {
          return carve::csg::CSG::Hooks::RESULT_FACE_BIT;
        }
        virtual void resultFace(const meshset_t::face_t *new_face,
                                const meshset_t::face_t *orig_face,
                                bool flipped) {
          interpolator->resultFace(csg, new_face, orig_face, flipped);
        }
        virtual void processOutputFace(std::vector<carve::mesh::MeshSet<3>::face_t *> &new_faces,
                                       const meshset_t::face_t *orig_face,
                                       bool flipped) {
          interpolator->processOutputFace(csg, new_faces, orig_face, flipped);
        }
        virtual void edgeDivision(const meshset_t::edge_t *orig_edge,
                                  size_t orig_edge_idx,
                                  const meshset_t::vertex_t *v1,
                                  const meshset_t::vertex_t *v2) {
          interpolator->edgeDivision(csg, orig_edge, orig_edge_idx, v1, v2);
        }

        Hook(Interpolator *_interpolator, const carve::csg::CSG &_csg) : csg(_csg), interpolator(_interpolator) {
        }

        virtual ~Hook() {
        }
      };

      virtual Hook *makeHook(carve::csg::CSG &csg) {
        return new Hook(this, csg);
      }

      virtual void resultFace(const carve::csg::CSG &csg,
                              const meshset_t::face_t *new_face,
                              const meshset_t::face_t *orig_face,
                              bool flipped) {
      }

      virtual void processOutputFace(const carve::csg::CSG &csg,
                                     std::vector<carve::mesh::MeshSet<3>::face_t *> &new_faces,
                                     const meshset_t::face_t *orig_face,
                                     bool flipped) {
      }

      virtual void edgeDivision(const carve::csg::CSG &csg,
                                const meshset_t::edge_t *orig_edge,
                                size_t orig_edge_idx,
                                const meshset_t::vertex_t *v1,
                                const meshset_t::vertex_t *v2) {
      }

    public:

      Interpolator() {
      }

      virtual ~Interpolator() {
      }

      void installHooks(carve::csg::CSG &csg) {
        Hook *hook = makeHook(csg);
        csg.hooks.registerHook(hook, hook->hookBits());
      }
    };



    template<typename attr_t>
    class FaceVertexAttr : public Interpolator {
    public:
      typedef std::pair<const meshset_t::face_t *, unsigned> key_t;

    protected:
      struct key_hash {
        size_t operator()(const key_t &v) const {
          return size_t(v.first) ^ size_t(v.second);
        }
      };

      typedef std::unordered_map<const meshset_t::vertex_t *, attr_t> attrvmap_t;
      typedef std::unordered_map<key_t, attr_t, key_hash> attrmap_t;

      attrmap_t attrs;

      virtual void resultFace(const carve::csg::CSG &csg,
                              const meshset_t::face_t *new_face,
                              const meshset_t::face_t *orig_face,
                              bool flipped) {
        std::vector<attr_t> vertex_attrs;
        attrvmap_t base_attrs;
        vertex_attrs.reserve(orig_face->nVertices());

        for (meshset_t::face_t::const_edge_iter_t e = orig_face->begin(); e != orig_face->end(); ++e) {
          typename attrmap_t::const_iterator a = attrs.find(key_t(orig_face, e.idx()));
          if (a == attrs.end()) return;
          vertex_attrs.push_back((*a).second);
          base_attrs[e->vert] = vertex_attrs.back();
        }

        for (meshset_t::face_t::const_edge_iter_t e = new_face->begin(); e != new_face->end(); ++e) {
          const meshset_t::vertex_t *vertex = e->vert;
          typename attrvmap_t::const_iterator b = base_attrs.find(vertex);
          if (b != base_attrs.end()) {
            attrs[key_t(new_face, e.idx())] = (*b).second;
          } else {
            carve::geom2d::P2 p = orig_face->project(e->vert->v);
            attr_t attr = interp(orig_face->begin(),
                                 orig_face->end(),
                                 orig_face->projector(),
                                 vertex_attrs,
                                 p.x,
                                 p.y);
            attrs[key_t(new_face, e.idx())] = attr;
          }
        }
      }

    public:
      bool hasAttribute(const meshset_t::face_t *f, unsigned v) {
        return attrs.find(key_t(f, v)) != attrs.end();
      }

      attr_t getAttribute(const meshset_t::face_t *f, unsigned v, const attr_t &def = attr_t()) {
        typename attrmap_t::const_iterator fv = attrs.find(key_t(f, v));
        if (fv != attrs.end()) {
          return (*fv).second;
        }
        return def;
      }

      void setAttribute(const meshset_t::face_t *f, unsigned v, const attr_t &attr) {
        attrs[key_t(f, v)] = attr;
      }

      FaceVertexAttr() : Interpolator() {
      }

      virtual ~FaceVertexAttr() {
      }

    };



    template<typename attr_t>
    class FaceEdgeAttr : public Interpolator {
    public:
      typedef std::pair<const meshset_t::face_t *, unsigned> key_t;

    protected:
      typedef std::pair<const meshset_t::vertex_t *, const meshset_t::vertex_t *> vpair_t;

      struct key_hash {
        size_t operator()(const key_t &v) const {
          return size_t(v.first) ^ size_t(v.second);
        }
      };
      struct vpair_hash {
        size_t operator()(const vpair_t &v) const {
          return size_t(v.first) ^ size_t(v.second);
        }
      };

      typedef std::unordered_map<key_t, attr_t, key_hash> attrmap_t;
      typedef std::unordered_map<vpair_t, key_t, vpair_hash> edgedivmap_t;

      attrmap_t attrs;
      edgedivmap_t edgediv;

      struct Hook : public Interpolator::Hook {
      public:
        virtual unsigned hookBits() const {
          return
            carve::csg::CSG::Hooks::PROCESS_OUTPUT_FACE_BIT |
            carve::csg::CSG::Hooks::EDGE_DIVISION_BIT;
        }
        Hook(Interpolator *_interpolator, const carve::csg::CSG &_csg) : Interpolator::Hook(_interpolator, _csg) {
        }
        virtual ~Hook() {
        }
      };

      virtual Interpolator::Hook *makeHook(carve::csg::CSG &csg) {
        return new Hook(this, csg);
      }

      virtual void edgeDivision(const carve::csg::CSG &csg,
                                const meshset_t::edge_t *orig_edge,
                                size_t orig_edge_idx,
                                const meshset_t::vertex_t *v1,
                                const meshset_t::vertex_t *v2) {
        key_t k(orig_edge->face, orig_edge_idx);
        typename attrmap_t::const_iterator attr_i = attrs.find(k);
        if (attr_i == attrs.end()) return;
        edgediv[vpair_t(v1, v2)] = k;
      }

      virtual void processOutputFace(const carve::csg::CSG &csg,
                                     std::vector<carve::mesh::MeshSet<3>::face_t *> &new_faces,
                                     const meshset_t::face_t *orig_face,
                                     bool flipped) {
        edgedivmap_t undiv;

        for (meshset_t::face_t::const_edge_iter_t e = orig_face->begin(); e != orig_face->end(); ++e) {
          key_t k(orig_face, e.idx());
          typename attrmap_t::const_iterator attr_i = attrs.find(k);
          if (attr_i == attrs.end()) {
            continue;
          } else {
            undiv[vpair_t(e->v1(), e->v2())] = k;
          }
        }

        for (size_t fnum = 0; fnum < new_faces.size(); ++fnum) {
          const carve::mesh::MeshSet<3>::face_t *new_face = new_faces[fnum];
          for (meshset_t::face_t::const_edge_iter_t e = new_face->begin(); e != new_face->end(); ++e) {
            key_t k(new_face, e.idx());

            vpair_t vp;
            if (!flipped) {
              vp = vpair_t(e->v1(), e->v2());
            } else {
              vp = vpair_t(e->v2(), e->v1());
            }
            typename edgedivmap_t::const_iterator vp_i;
            if ((vp_i = undiv.find(vp)) != undiv.end()) {
              attrs[k] = attrs[vp_i->second];
            } else if ((vp_i = edgediv.find(vp)) != edgediv.end()) {
              attrs[k] = attrs[vp_i->second];
            }
          }
        }
      }

    public:

      bool hasAttribute(const meshset_t::face_t *f, unsigned e) {
        return attrs.find(std::make_pair(f, e)) != attrs.end();
      }

      attr_t getAttribute(const meshset_t::face_t *f, unsigned e, const attr_t &def = attr_t()) {
        typename attrmap_t::const_iterator fv = attrs.find(std::make_pair(f, e));
        if (fv != attrs.end()) {
          return (*fv).second;
        }
        return def;
      }

      void setAttribute(const meshset_t::face_t *f, unsigned e, const attr_t &attr) {
        attrs[std::make_pair(f, e)] = attr;
      }

      FaceEdgeAttr() : Interpolator() {
      }

      virtual ~FaceEdgeAttr() {
      }
    };



    template<typename attr_t>
    class FaceAttr : public Interpolator {
    public:
      typedef const meshset_t::face_t *key_t;

    protected:
      struct key_hash {
        size_t operator()(const key_t &f) const {
          return size_t(f);
        }
      };

      typedef std::unordered_map<key_t, attr_t, key_hash> attrmap_t;

      attrmap_t attrs;
      
      virtual void resultFace(const carve::csg::CSG &csg,
                              const meshset_t::face_t *new_face,
                              const meshset_t::face_t *orig_face,
                              bool flipped) {
        typename attrmap_t::const_iterator i = attrs.find(key_t(orig_face));
        if (i != attrs.end()) {
          attrs[key_t(new_face)] = (*i).second;
        }
      }

    public:
      bool hasAttribute(const meshset_t::face_t *f) {
        return attrs.find(key_t(f)) != attrs.end();
      }

      attr_t getAttribute(const meshset_t::face_t *f, const attr_t &def = attr_t()) {
        typename attrmap_t::const_iterator i = attrs.find(key_t(f));
        if (i != attrs.end()) {
          return (*i).second;
        }
        return def;
      }

      void setAttribute(const meshset_t::face_t *f, const attr_t &attr) {
        attrs[key_t(f)] = attr;
      }

      FaceAttr() : Interpolator() {
      }

      virtual ~FaceAttr() {
      }
    };

  }
}
