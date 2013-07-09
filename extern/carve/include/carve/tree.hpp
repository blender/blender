
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

#include <carve/matrix.hpp>
#include <carve/timing.hpp>
#include <carve/rescale.hpp>

namespace carve {
  namespace csg {

    class CSG_TreeNode {
      CSG_TreeNode(const CSG_TreeNode &);
      CSG_TreeNode &operator=(const CSG_TreeNode &);

    protected:
  
    public:
      CSG_TreeNode() {
      }

      virtual ~CSG_TreeNode() {
      }

      virtual carve::mesh::MeshSet<3> *eval(bool &is_temp, CSG &csg) =0;

      virtual carve::mesh::MeshSet<3> *eval(CSG &csg) {
        bool temp;
        carve::mesh::MeshSet<3> *r = eval(temp, csg);
        if (!temp) r = r->clone();
        return r;
      }
    };



    class CSG_TransformNode : public CSG_TreeNode {
      carve::math::Matrix transform;
      CSG_TreeNode *child;

    public:
      CSG_TransformNode(const carve::math::Matrix &_transform, CSG_TreeNode *_child) : transform(_transform), child(_child) {
      }
      virtual ~CSG_TransformNode() {
        delete child;
      }

      virtual carve::mesh::MeshSet<3> *eval(bool &is_temp, CSG &csg) {
        carve::mesh::MeshSet<3> *result = child->eval(is_temp, csg);
        if (!is_temp) {
          result = result->clone();
          is_temp = true;
        }
        result->transform(carve::math::matrix_transformation(transform));
        return result;
      }
    };




    class CSG_InvertNode : public CSG_TreeNode {
      std::vector<bool> selected_meshes;
      CSG_TreeNode *child;

    public:
      CSG_InvertNode(CSG_TreeNode *_child) : selected_meshes(), child(_child) {
      }
      CSG_InvertNode(int g_id, CSG_TreeNode *_child) : selected_meshes(), child(_child) {
        selected_meshes.resize(g_id + 1, false);
        selected_meshes[g_id] = true;
      }
      virtual ~CSG_InvertNode() {
        delete child;
      }

      template<typename T>
      CSG_InvertNode(T start, T end, CSG_TreeNode *_child) : selected_meshes(), child(_child) {
        while (start != end) {
          int g_id = (int)(*start);
          if (selected_meshes.size() < g_id + 1) selected_meshes.resize(g_id + 1, false);
          selected_meshes[g_id] = true;
          ++start;
        }
      }

      virtual carve::mesh::MeshSet<3> *eval(bool &is_temp, CSG &csg) {
        bool c_temp;
        carve::mesh::MeshSet<3> *c = child->eval(c_temp, csg);
        if (!c_temp) c = c->clone();
        if (!selected_meshes.size()) {
          c->invert();
        } else {
          for (size_t i = 0; i < c->meshes.size() && i < selected_meshes.size(); ++i) {
            if (selected_meshes[i]) {
              c->meshes[i]->invert();
            }
          }
        }
        is_temp = true;
        return c;
      }
    };




    class CSG_SelectNode : public CSG_TreeNode {
      std::vector<bool> selected_meshes;
      CSG_TreeNode *child;

    public:
      CSG_SelectNode(int m_id, CSG_TreeNode *_child) : selected_meshes(), child(_child) {
        selected_meshes.resize(m_id + 1, false);
        selected_meshes[m_id] = true;
      }

      template<typename T>
      CSG_SelectNode(T start, T end, CSG_TreeNode *_child) : selected_meshes(), child(_child) {
        while (start != end) {
          int m_id = (int)(*start);
          if ((int)selected_meshes.size() < m_id + 1) selected_meshes.resize(m_id + 1, false);
          selected_meshes[m_id] = true;
          ++start;
        }
      }

      virtual ~CSG_SelectNode() {
        delete child;
      }

      virtual carve::mesh::MeshSet<3> *eval(bool &is_temp, CSG &csg) {
        bool c_temp;
        carve::mesh::MeshSet<3> *c = child->eval(c_temp, csg);
        if (!c_temp) c = c->clone();
        size_t i = 0;
        size_t j = 0;
        for (size_t i = 0; i < c->meshes.size(); ++i) {
          if (i >= selected_meshes.size() || !selected_meshes[i]) {
            delete c->meshes[i];
            c->meshes[i] = NULL;
          } else {
            c->meshes[j++] = c->meshes[i];
          }
        }
        c->meshes.erase(c->meshes.begin() + j, c->meshes.end());
        c->collectVertices();
        is_temp = true;
        return c;
      }
    };




    class CSG_PolyNode : public CSG_TreeNode {
      carve::mesh::MeshSet<3> *poly;
      bool del;

    public:
      CSG_PolyNode(carve::mesh::MeshSet<3> *_poly, bool _del) : poly(_poly), del(_del)  {
      }
      virtual ~CSG_PolyNode() {
        static carve::TimingName FUNC_NAME("delete polyhedron");
        carve::TimingBlock block(FUNC_NAME);
    
        if (del) {
          delete poly;
        }
      }

      virtual carve::mesh::MeshSet<3> *eval(bool &is_temp, CSG &csg) {
        is_temp = false;
        return poly;
      }
    };



    class CSG_OPNode : public CSG_TreeNode {
      CSG_TreeNode *left, *right;
      CSG::OP op;
      bool rescale;
      CSG::CLASSIFY_TYPE classify_type;

    public:
      CSG_OPNode(CSG_TreeNode *_left,
                 CSG_TreeNode *_right,
                 CSG::OP _op,
                 bool _rescale,
                 CSG::CLASSIFY_TYPE _classify_type = CSG::CLASSIFY_NORMAL) : left(_left), right(_right), op(_op), rescale(_rescale), classify_type(_classify_type) {
      }

      virtual ~CSG_OPNode() {
        delete left;
        delete right;
      }

      void minmax(double &min_x, double &min_y, double &min_z,
                  double &max_x, double &max_y, double &max_z,
                  const std::vector<carve::geom3d::Vector> &points) {
        for (unsigned i = 1; i < points.size(); ++i) {
          min_x = std::min(min_x, points[i].x);
          max_x = std::max(max_x, points[i].x);
          min_y = std::min(min_y, points[i].y);
          max_y = std::max(max_y, points[i].y);
          min_z = std::min(min_z, points[i].z);
          max_z = std::max(max_z, points[i].z);
        }
      }

      virtual carve::mesh::MeshSet<3> *evalScaled(bool &is_temp, CSG &csg) {
        carve::mesh::MeshSet<3> *l, *r;
        bool l_temp, r_temp;

        l = left->eval(l_temp, csg);
        r = right->eval(r_temp, csg);

        if (!l_temp) { l = l->clone(); }
        if (!r_temp) { r = r->clone(); }

        carve::geom3d::Vector min, max;
        carve::geom3d::Vector min_l, max_l;
        carve::geom3d::Vector min_r, max_r;

        carve::geom::bounds<3>(l->vertex_storage.begin(),
                               l->vertex_storage.end(),
                               carve::mesh::Face<3>::vector_mapping(),
                               min_l,
                               max_l);
        carve::geom::bounds<3>(r->vertex_storage.begin(),
                               r->vertex_storage.end(),
                               carve::mesh::Face<3>::vector_mapping(),
                               min_r,
                               max_r);

        carve::geom::assign_op(min, min_l, min_r, carve::util::min_functor());
        carve::geom::assign_op(max, max_l, max_r, carve::util::max_functor());

        carve::rescale::rescale scaler(min.x, min.y, min.z, max.x, max.y, max.z);

        carve::rescale::fwd fwd_r(scaler);
        carve::rescale::rev rev_r(scaler);

        l->transform(fwd_r);
        r->transform(fwd_r);

        carve::mesh::MeshSet<3> *result = NULL;
        {
          static carve::TimingName FUNC_NAME("csg.compute()");
          carve::TimingBlock block(FUNC_NAME);
          result = csg.compute(l, r, op, NULL, classify_type);
        }

        {
          static carve::TimingName FUNC_NAME("delete polyhedron");
          carve::TimingBlock block(FUNC_NAME);
      
          delete l;
          delete r;
        }

        result->transform(rev_r);

        is_temp = true;
        return result;
      }
  
      virtual carve::mesh::MeshSet<3> *evalUnscaled(bool &is_temp, CSG &csg) {
        carve::mesh::MeshSet<3> *l, *r;
        bool l_temp, r_temp;

        l = left->eval(l_temp, csg);
        r = right->eval(r_temp, csg);

        carve::mesh::MeshSet<3> *result = NULL;
        {
          static carve::TimingName FUNC_NAME("csg.compute()");
          carve::TimingBlock block(FUNC_NAME);
          result = csg.compute(l, r, op, NULL, classify_type);
        }

        {
          static carve::TimingName FUNC_NAME("delete polyhedron");
          carve::TimingBlock block(FUNC_NAME);
      
          if (l_temp) delete l;
          if (r_temp) delete r;
        }

        is_temp = true;
        return result;
      }
  

      virtual carve::mesh::MeshSet<3> *eval(bool &is_temp, CSG &csg) {
        if (rescale) {
          return evalScaled(is_temp, csg);
        } else {
          return evalUnscaled(is_temp, csg);
        }
      }
    };

  }
}
