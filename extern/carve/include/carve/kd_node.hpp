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
#include <carve/geom.hpp>
#include <carve/aabb.hpp>

#include <queue>
#include <list>
#include <limits>

namespace carve {
  namespace geom {
    template<unsigned ndim,
             typename data_t,
             typename inserter_t,
             typename aabb_calc_t>
    class kd_node {
      kd_node(const kd_node &);
      kd_node &operator=(const kd_node &);

    public:
      kd_node *c_neg;
      kd_node *c_pos;
      kd_node *parent;
      axis_pos splitpos;

      typedef vector<ndim> vector_t;
      typedef std::list<data_t> container_t;

      container_t data;

      kd_node(kd_node *_parent = NULL) : c_neg(NULL), c_pos(NULL), parent(_parent), splitpos(0, 0.0) {
      }

      ~kd_node() {
        if (c_neg) delete c_neg;
        if (c_pos) delete c_pos;
      }

      template<typename visitor_t>
      void closeNodes(const vector_t &p, double d, visitor_t &visit) const {
        if (c_neg) {
          double delta = splitpos.pos - p[splitpos.axis];
          if (delta <= d) c_neg->closeNodes(p, d, visit);
          if (delta >= -d) c_pos->closeNodes(p, d, visit);
        } else {
          visit(this);
        }
      }

      void removeData(const data_t &d) {
        typename container_t::iterator i = std::find(data.begin(), data.end(), d);

        if (i != data.end()) {
          data.erase(i);
        }
      }

      void addData(const data_t &d) {
        data.push_back(d);
      }

      void insert(const data_t &data, inserter_t &inserter) {
        inserter.insert(this, data);
      }

      void insert(const data_t &data) {
        inserter_t inserter;
        insert(data, inserter);
      }

      void remove(const data_t &data, inserter_t &inserter) {
        inserter.remove(this, data);
      }

      void remove(const data_t &data) {
        inserter_t inserter;
        remove(data, inserter);
      }

      carve::geom::aabb<ndim> nodeAABB() const {
        carve::geom::aabb<ndim> aabb;
        if (c_neg) {
          aabb = c_neg->nodeAABB();
          aabb.unionAABB(c_pos->nodeAABB());
        } else {
          if (data.size()) {
            typename container_t::const_iterator i = data.begin();
            aabb = aabb_calc_t()(*i);
            while (i != data.end()) {
              aabb.unionAABB(aabb_calc_t()(*i));
              ++i;
            }
          }
        }
        return aabb;
      }

      bool split(axis_pos split_at, inserter_t &inserter) {
        if (c_neg) {
          // already split
          return false;
        }

        c_neg = new kd_node(this);
        c_pos = new kd_node(this);

        // choose an axis and split point.
        splitpos = split_at;

        carve::geom::aabb<ndim> aabb;

        if (splitpos.axis < 0 ||
            splitpos.axis >= ndim ||
            splitpos.pos == std::numeric_limits<double>::max()) {
          // need an aabb
          if (data.size()) {
            typename container_t::const_iterator i = data.begin();
            aabb = aabb_calc_t()(*i);
            while (i != data.end()) {
              aabb.unionAABB(aabb_calc_t()(*i));
              ++i;
            }
          }
        }

        if (splitpos.axis < 0 || splitpos.axis >= ndim) {

          // choose an axis;

          // if no axis was specified, force calculation of the split position.
          splitpos.pos = std::numeric_limits<double>::max();

          // choose the axis of the AABB with the biggest extent.
          splitpos.axis = largestAxis(aabb.extent);

          if (parent && splitpos.axis == parent->splitpos.axis) {
            // but don't choose the same axis as the parent node;
            // choose the axis with the second greatest AABB extent.
            double e = -1.0;
            int a = -1;
            for (unsigned i = 0; i < ndim; ++i) {
              if (i == splitpos.axis) continue;
              if (e < aabb.extent[i]) { a = i; e = aabb.extent[i]; }
            }
            if (a != -1) {
              splitpos.axis = a;
            }
          }
        }

        if (splitpos.pos == std::numeric_limits<double>::max()) {
          carve::geom::vector<ndim> min = aabb.min();
          carve::geom::vector<ndim> max = aabb.max();
          splitpos.pos = aabb.pos.v[splitpos.axis];
        }

        inserter.propagate(this);

        return true;
      }

      bool split(axis_pos split_at = axis_pos(-1, std::numeric_limits<double>::max())) {
        inserter_t inserter;
        return split(split_at, inserter);
      }

      void splitn(int num, inserter_t &inserter) {
        if (num <= 0) return;
        if (!c_neg) {
          split(inserter);
        }
        if (c_pos) c_pos->splitn(num-1, inserter);
        if (c_neg) c_neg->splitn(num-1, inserter);
      }

      void splitn(int num) {
        inserter_t inserter;
        splitn(num, inserter);
      }

      template<typename split_t>
      void splitn(int num, split_t splitter, inserter_t &inserter) {
        if (num <= 0) return;
        if (!c_neg) {
          split(inserter, splitter(this));
        }
        if (c_pos) c_pos->splitn(num-1, inserter, splitter);
        if (c_neg) c_neg->splitn(num-1, inserter, splitter);
      }

      template<typename split_t>
      void splitn(int num, split_t splitter) {
        inserter_t inserter;
        splitn(num, splitter, inserter);
      }

      template<typename pred_t>
      void splitpred(pred_t pred, inserter_t &inserter, int depth = 0) {
        if (!c_neg) {
          axis_pos splitpos(-1, std::numeric_limits<double>::max());
          if (!pred(this, depth, splitpos)) return;
          split(splitpos, inserter);
        }
        if (c_pos) c_pos->splitpred(pred, inserter, depth + 1);
        if (c_neg) c_neg->splitpred(pred, inserter, depth + 1);
      }

      template<typename pred_t>
      void splitpred(pred_t pred, int depth = 0) {
        inserter_t inserter;
        splitpred(pred, inserter, depth);
      }

      // distance_t must provide:
      // double operator()(data_t, vector<ndim>);
      // double operator()(axis_pos, vector<ndim>);
      template<typename distance_t>
      struct near_point_query {

        // q_t - the priority queue value type.
        // q_t.first:  distance from object to query point.
        // q_t.second: pointer to object
        typedef std::pair<double, const data_t *> q_t;

        // the queue priority should sort from smallest distance to largest, and on equal distance, by object pointer.
        struct pcmp {
          bool operator()(const q_t &a, const q_t &b) {
            return (a.first > b.first) || ((a.first == b.first) && (a.second < b.second));
          }
        };

        vector<ndim> point;
        const kd_node *node;
        std::priority_queue<q_t, std::vector<q_t>, pcmp> pq;

        distance_t dist;
        double dist_to_parent_split;

        void addToPQ(kd_node *node) {
          if (node->c_neg) {
            addToPQ(node->c_neg);
            addToPQ(node->c_pos);
          } else {
            for (typename kd_node::container_t::const_iterator i = node->data.begin(); i != node->data.end(); ++i) {
              double d = dist((*i), point);
              pq.push(std::make_pair(d, &(*i)));
            }
          }
        }

        const data_t *next() {
          while (1) {
            if (pq.size()) {
              q_t t = pq.top();
              if (!node->parent || t.first < dist_to_parent_split) {
                pq.pop();
                return t.second;
              }
            }

            if (!node->parent) return NULL;

            if (node->parent->c_neg == node) {
              addToPQ(node->parent->c_pos);
            } else {
              addToPQ(node->parent->c_neg);
            }

            node = node->parent;
            dist_to_parent_split = dist(node->splitpos, point);
          }
        }

        near_point_query(const vector<ndim> _point, const kd_node *_node) : point(_point), node(_node), pq(), dist() {
          while (node->c_neg) {
            node = (point[node->axis] < node->pos) ? node->c_neg : node->c_pos;
          }
          if (node->parent) {
            dist_to_parent_split = dist(node->parent->splitpos, point);
          } else {
            dist_to_parent_split = HUGE_VAL;
          }
          addToPQ(node);
        }
      };

    };

  }
}
