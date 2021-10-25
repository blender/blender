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

#include <carve/carve.hpp>

#include <carve/geom.hpp>
#include <carve/aabb.hpp>

#include <iostream>

#include <cmath>
#include <limits>

#if defined(HAVE_STDINT_H)
# include <stdint.h>
#endif

namespace carve {
  namespace geom {

    template<unsigned ndim,
             typename data_t,
             typename aabb_calc_t = carve::geom::get_aabb<ndim, data_t> >
    struct RTreeNode {
      typedef aabb<ndim> aabb_t;
      typedef vector<ndim> vector_t;
      typedef RTreeNode<ndim, data_t, aabb_calc_t> node_t;

      aabb_t bbox;
      node_t *child;
      node_t *sibling;
      std::vector<data_t> data;

      aabb_t getAABB() const { return bbox; }

      struct data_aabb_t {
        aabb_t bbox;
        data_t data;

        data_aabb_t() { }
        data_aabb_t(const data_t &_data) : bbox(aabb_calc_t()(_data)), data(_data) {
        }

        aabb_t getAABB() const { return bbox; }

        struct cmp {
          size_t dim;
          cmp(size_t _dim) : dim(_dim) { }
          bool operator()(const data_aabb_t &a, const data_aabb_t &b) {
            return a.bbox.pos.v[dim] < b.bbox.pos.v[dim];
          }
        };
      };

      // Fill an rtree node with a set of (data, aabb) pairs.
      template<typename iter_t>
      void _fill(iter_t begin, iter_t end, data_aabb_t) {
        data.reserve(std::distance(begin, end));
        for (iter_t i = begin; i != end; ++i) {
          data.push_back((*i).data);
        }
        bbox.fit(begin, end);
      }

      // Fill an rtree node with a set of data.
      template<typename iter_t>
      void _fill(iter_t begin, iter_t end, data_t) {
        data.reserve(std::distance(begin, end));
        std::copy(begin, end, std::back_inserter(data));
        bbox.fit(begin, end, aabb_calc_t());
      }

      // Fill an rtree node with a set of child nodes.
      template<typename iter_t>
      void _fill(iter_t begin, iter_t end, node_t *) {
        iter_t i = begin;
        node_t *curr = child = *i;
        while (++i != end) {
          curr->sibling = *i;
          curr = curr->sibling;
        }
        bbox.fit(begin, end);
      }

      // Search the rtree for objects that intersect obj (generally an aabb).
      // The aabb class must provide a method intersects(obj_t).
      template<typename obj_t, typename out_iter_t>
      void search(const obj_t &obj, out_iter_t out) const {
        if (!bbox.intersects(obj)) return;
        if (child) {
          for (node_t *node = child; node; node = node->sibling) {
            node->search(obj, out);
          }
        } else {
          std::copy(data.begin(), data.end(), out);
        }
      }

      // update the bounding box extents of nodes that intersect obj (generally an aabb).
      // The aabb class must provide a method intersects(obj_t).
      template<typename obj_t>
      void updateExtents(const obj_t &obj) {
        if (!bbox.intersects(obj)) return;

        if (child) {
          node_t *node = child;
          node->updateExtents(obj);
          bbox = node->bbox;
          for (node = node->sibling; node; node = node->sibling) {
            node->updateExtents(obj);
            bbox.unionAABB(node->bbox);
          }
        } else {
          bbox.fit(data.begin(), data.end());
        }
      }

      // update the bounding box extents of nodes that intersect obj (generally an aabb).
      // The aabb class must provide a method intersects(obj_t).
      bool remove(const data_t &val, const aabb_t &val_aabb) {
        if (!bbox.intersects(val_aabb)) return false;

        if (child) {
          node_t *node = child;
          node->remove(val, val_aabb);
          bbox = node->bbox;
          bool removed = false;
          for (node = node->sibling; node; node = node->sibling) {
            if (!removed) removed = node->remove(val, val_aabb);
            bbox.unionAABB(node->bbox);
          }
          return removed;
        } else {
          typename std::vector<data_t>::iterator i = std::remove(data.begin(), data.end(), val);
          if (i == data.end()) {
            return false;
          }
          data.erase(i, data.end());
          bbox.fit(data.begin(), data.end());
          return true;
        }
      }

      template<typename iter_t>
      RTreeNode(iter_t begin, iter_t end) : bbox(), child(NULL), sibling(NULL), data() {
        _fill(begin, end, typename std::iterator_traits<iter_t>::value_type());
      }

      ~RTreeNode() {
        if (child) {
          RTreeNode *next = child;
          while (next) {
            RTreeNode *curr = next;
            next = next->sibling;
            delete curr;
          }
        }
      }



      // functor for ordering nodes by increasing aabb midpoint, along a specified axis.
      struct aabb_cmp_mid {
        size_t dim;
        aabb_cmp_mid(size_t _dim) : dim(_dim) { }

        bool operator()(const node_t *a, const node_t *b) {
          return a->bbox.mid(dim) < b->bbox.mid(dim);
        }
        bool operator()(const data_aabb_t &a, const data_aabb_t &b) {
          return a.bbox.mid(dim) < b.bbox.mid(dim);
        }
      };

      // functor for ordering nodes by increasing aabb minimum, along a specified axis.
      struct aabb_cmp_min {
        size_t dim;
        aabb_cmp_min(size_t _dim) : dim(_dim) { }

        bool operator()(const node_t *a, const node_t *b) {
          return a->bbox.min(dim) < b->bbox.min(dim);
        }
        bool operator()(const data_aabb_t &a, const data_aabb_t &b) {
          return a.bbox.min(dim) < b.bbox.min(dim);
        }
      };

      // functor for ordering nodes by increasing aabb maximum, along a specified axis.
      struct aabb_cmp_max {
        size_t dim;
        aabb_cmp_max(size_t _dim) : dim(_dim) { }

        bool operator()(const node_t *a, const node_t *b) {
          return a->bbox.max(dim) < b->bbox.max(dim);
        }
        bool operator()(const data_aabb_t &a, const data_aabb_t &b) {
          return a.bbox.max(dim) < b.bbox.max(dim);
        }
      };

      // facade for projecting node bounding box onto an axis.
      struct aabb_extent {
        size_t dim;
        aabb_extent(size_t _dim) : dim(_dim) { }

        double min(const node_t *a) { return a->bbox.pos.v[dim] - a->bbox.extent.v[dim]; }
        double max(const node_t *a) { return a->bbox.pos.v[dim] + a->bbox.extent.v[dim]; }
        double len(const node_t *a) { return 2.0 * a->bbox.extent.v[dim]; }
        double min(const data_aabb_t &a) { return a.bbox.pos.v[dim] - a.bbox.extent.v[dim]; }
        double max(const data_aabb_t &a) { return a.bbox.pos.v[dim] + a.bbox.extent.v[dim]; }
        double len(const data_aabb_t &a) { return 2.0 * a.bbox.extent.v[dim]; }
      };

      template<typename iter_t>
      static void makeNodes(const iter_t begin,
                            const iter_t end,
                            size_t dim_num,
                            uint32_t dim_mask,
                            size_t child_size,
                            std::vector<node_t *> &out) {
        const size_t N = std::distance(begin, end);

        size_t dim = ndim;
        double r_best = N+1;

        // find the sparsest remaining dimension to partition by.
        for (size_t i = 0; i < ndim; ++i) {
          if (dim_mask & (1U << i)) continue;
          aabb_extent extent(i);
          double dmin, dmax, dsum;

          dmin = extent.min(*begin);
          dmax = extent.max(*begin);
          dsum = 0.0;
          for (iter_t j = begin; j != end; ++j) {
            dmin = std::min(dmin, extent.min(*j));
            dmax = std::max(dmax, extent.max(*j));
            dsum += extent.len(*j);
          }
          double r = dsum ? dsum / (dmax - dmin) : 0.0;
          if (r_best > r) {
            dim = i;
            r_best = r;
          }
        }

        CARVE_ASSERT(dim < ndim);

        // dim = dim_num;

        const size_t P = (N + child_size - 1) / child_size;
        const size_t n_parts = (size_t)std::ceil(std::pow((double)P, 1.0 / (ndim - dim_num)));

        std::sort(begin, end, aabb_cmp_mid(dim));

        if (dim_num == ndim - 1 || n_parts == 1) {
          for (size_t i = 0, s = 0, e = 0; i < P; ++i, s = e) {
            e = N * (i+1) / P;
            CARVE_ASSERT(e - s <= child_size);
            out.push_back(new node_t(begin + s, begin + e));
          }
        } else {
          for (size_t i = 0, s = 0, e = 0; i < n_parts; ++i, s = e) {
            e = N * (i+1) / n_parts;
            makeNodes(begin + s, begin + e, dim_num + 1, dim_mask | (1U << dim), child_size, out);
          }
        }
      }

      static node_t *construct_STR(std::vector<data_aabb_t> &data, size_t leaf_size, size_t internal_size) {
        std::vector<node_t *> out;
        makeNodes(data.begin(), data.end(), 0, 0, leaf_size, out);

        while (out.size() > 1) {
          std::vector<node_t *> next;
          makeNodes(out.begin(), out.end(), 0, 0, internal_size, next);
          std::swap(out, next);
        }

        CARVE_ASSERT(out.size() == 1);
        return out[0];
      }

      template<typename iter_t>
      static node_t *construct_STR(const iter_t &begin,
                                   const iter_t &end,
                                   size_t leaf_size,
                                   size_t internal_size) {
        std::vector<data_aabb_t> data;
        data.reserve(std::distance(begin, end));
        for (iter_t i = begin; i != end; ++i) {
          data.push_back(*i);
        }
        return construct_STR(data, leaf_size, internal_size);
      }


      template<typename iter_t>
      static node_t *construct_STR(const iter_t &begin1,
                                   const iter_t &end1,
                                   const iter_t &begin2,
                                   const iter_t &end2,
                                   size_t leaf_size,
                                   size_t internal_size) {
        std::vector<data_aabb_t> data;
        data.reserve(std::distance(begin1, end1) + std::distance(begin2, end2));
        for (iter_t i = begin1; i != end1; ++i) {
          data.push_back(*i);
        }
        for (iter_t i = begin2; i != end2; ++i) {
          data.push_back(*i);
        }
        return construct_STR(data, leaf_size, internal_size);
      }


      struct partition_info {
        double score;
        size_t partition_pos;

        partition_info() : score(std::numeric_limits<double>::max()), partition_pos(0) {
        }
        partition_info(double _score, size_t _partition_pos) :
          score(_score),
          partition_pos(_partition_pos) {
        }
      };

      static partition_info findPartition(typename std::vector<data_aabb_t>::iterator base,
                                          std::vector<size_t>::iterator begin,
                                          std::vector<size_t>::iterator end,
                                          size_t part_size) {
        CARVE_ASSERT(begin < end);

        partition_info best(std::numeric_limits<double>::max(), 0);
        const size_t N = (size_t)std::distance(begin, end);

        std::vector<double> rhs_vol(N, 0.0);

        aabb_t rhs = base[begin[N-1]].aabb;
        rhs_vol[N-1] = rhs.volume();
        for (size_t i = N - 1; i > 0; ) {
          rhs.unionAABB(base[begin[--i]].aabb);
          rhs_vol[i] = rhs.volume();
        }

        aabb_t lhs = base[begin[0]].aabb;
        for (size_t i = 1; i < N; ++i) {
          lhs.unionAABB(base[begin[i]].aabb);
          if (i % part_size == 0 || (N - i) % part_size == 0) {
            partition_info curr(lhs.volume() + rhs_vol[i], i);
            if (best.score > curr.score) best = curr;
          }
        }
        return best;
      }

      static void partition(typename std::vector<data_aabb_t>::iterator base,
                            std::vector<size_t>::iterator begin,
                            std::vector<size_t>::iterator end,
                            size_t part_size,
                            std::vector<size_t> &part_num,
                            size_t &part_next) {
        CARVE_ASSERT(begin < end);

        const size_t N = (size_t)std::distance(begin, end);

        partition_info best;
        partition_info curr;
        size_t part_curr = part_num[*begin];

        std::vector<size_t> tmp(begin, end);

        for (size_t dim = 0; dim < ndim; ++dim) {
          std::sort(tmp.begin(), tmp.end(), make_index_sort(base, aabb_cmp_min(dim)));
          curr = findPartition(base, tmp.begin(), tmp.end(), part_size);
          if (best.score > curr.score) {
            best = curr;
            std::copy(tmp.begin(), tmp.end(), begin);
          }

          std::sort(tmp.begin(), tmp.end(), make_index_sort(base, aabb_cmp_mid(dim)));
          curr = findPartition(base, tmp.begin(), tmp.end(), part_size);
          if (best.score > curr.score) {
            best = curr;
            std::copy(tmp.begin(), tmp.end(), begin);
          }

          std::sort(tmp.begin(), tmp.end(), make_index_sort(base, aabb_cmp_max(dim)));
          curr = findPartition(base, tmp.begin(), tmp.end(), part_size);
          if (best.score > curr.score) {
            best = curr;
            std::copy(tmp.begin(), tmp.end(), begin);
          }
        }

        for (size_t j = 0; j < best.partition_pos; ++j) part_num[begin[(ssize_t)j]] = part_curr;
        for (size_t j = best.partition_pos; j < N; ++j) part_num[begin[(ssize_t)j]] = part_next;
        ++part_next;

        if (best.partition_pos > part_size) {
          partition(base, begin, begin + best.partition_pos, part_size, part_num, part_next);
        }
        if (N - best.partition_pos > part_size) {
          partition(base, begin + best.partition_pos, end, part_size, part_num, part_next);
        }
      }

      static size_t makePartitions(typename std::vector<data_aabb_t>::iterator begin,
                                   typename std::vector<data_aabb_t>::iterator end,
                                   size_t part_size,
                                   std::vector<size_t> &part_num) {
        const size_t N = std::distance(begin, end);
        std::vector<size_t> idx;
        idx.reserve(N);
        for (size_t i = 0; i < N; ++i) { idx.push_back(i); }
        size_t part_next = 1;

        partition(begin, idx.begin(), idx.end(), part_size, part_num, part_next);
        return part_next;
      }

      static node_t *construct_TGS(typename std::vector<data_aabb_t>::iterator begin,
                                   typename std::vector<data_aabb_t>::iterator end,
                                   size_t leaf_size,
                                   size_t internal_size) {
        size_t N = std::distance(begin, end);

        if (N <= leaf_size) {
          return new node_t(begin, end);
        } else {
          size_t P = (N + internal_size - 1) / internal_size;
          std::vector<size_t> part_num(N, 0);
          P = makePartitions(begin, end, P, part_num);

          size_t S = 0, E = 0;
          std::vector<node_t *> children;
          for (size_t i = 0; i < P; ++i) {
            size_t j = S, k = N;
            while (true) {
              while (true) {
                if (j == k) goto done;
                else if (part_num[j] == i) ++j;
                else break;
              }
              --k;
              while (true) {
                if (j == k) goto done;
                else if (part_num[k] != i) --k;
                else break;
              }
              std::swap(*(begin+j), *(begin+k));
              std::swap(part_num[j], part_num[k]);
              ++j;
            }
          done:
            E = j;
            children.push_back(construct_TGS(begin + S, begin + E, leaf_size, internal_size));
            S = E;
          }
          return new node_t(children.begin(), children.end());
        }
      }

      template<typename iter_t>
      static node_t *construct_TGS(const iter_t &begin,
                                   const iter_t &end,
                                   size_t leaf_size,
                                   size_t internal_size) {
        std::vector<data_aabb_t> data;
        data.reserve(std::distance(begin, end));
        for (iter_t i = begin; i != end; ++i) {
          data.push_back(*i);
        }
        return construct_TGS(data.begin(), data.end(), leaf_size, internal_size);
      }

      template<typename iter_t>
      static node_t *construct_TGS(const iter_t &begin1,
                                   const iter_t &end1,
                                   const iter_t &begin2,
                                   const iter_t &end2,
                                   size_t leaf_size,
                                   size_t internal_size) {
        std::vector<data_aabb_t> data;
        data.reserve(std::distance(begin1, end1) + std::distance(begin2, end2));
        for (iter_t i = begin1; i != end1; ++i) {
          data.push_back(*i);
        }
        for (iter_t i = begin2; i != end2; ++i) {
          data.push_back(*i);
        }
        return construct_TGS(data.begin(), data.end(), leaf_size, internal_size);
      }
    };

  }
}
