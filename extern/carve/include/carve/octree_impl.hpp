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
    template<typename filter_t>
    void Octree::doFindEdges(const carve::poly::Geometry<3>::face_t &f,
                             Node *node,
                             std::vector<const carve::poly::Geometry<3>::edge_t *> &out,
                             unsigned depth,
                             filter_t filter) const {
      if (node == NULL) {
        return;
      }

      if (node->aabb.intersects(f.aabb) && node->aabb.intersects(f.plane_eqn)) {
        if (node->hasChildren()) {
          for (int i = 0; i < 8; ++i) {
            doFindEdges(f, node->children[i], out, depth + 1, filter);
          }
        } else {
          if (depth < MAX_SPLIT_DEPTH && node->edges.size() > EDGE_SPLIT_THRESHOLD) {
            if (!node->split()) {
              for (int i = 0; i < 8; ++i) {
                doFindEdges(f, node->children[i], out, depth + 1, filter);
              }
              return;
            }
          }
          for (std::vector<const carve::poly::Geometry<3>::edge_t*>::const_iterator it = node->edges.begin(), e = node->edges.end(); it != e; ++it) {
            if ((*it)->tag_once()) {
              if (filter(*it)) {
                out.push_back(*it);
              }
            }
          }
        }
      }
    }

    template<typename filter_t>
    void Octree::findEdgesNear(const carve::poly::Geometry<3>::face_t &f, std::vector<const carve::poly::Geometry<3>::edge_t *> &out, filter_t filter) const {
      tagable::tag_begin();
      doFindEdges(f, root, out, 0, filter);
    }

    template <typename func_t>
    void Octree::doIterate(int level, Node *node, const func_t &f) const{
      f(level, node);
      if (node->hasChildren()) {
        for (int i = 0; i < 8; ++i) {
          doIterate(level + 1, node->children[i], f);
        }
      }
    }

    template <typename func_t>
    void Octree::iterateNodes(const func_t &f) const {
      doIterate(0, root, f);
    }

  }
}
