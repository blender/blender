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
#include <carve/classification.hpp>
#include <carve/collection_types.hpp>

namespace carve {
  namespace csg {

    struct FaceLoopGroup;

    struct FaceLoop {
      FaceLoop *next, *prev;
      const carve::mesh::MeshSet<3>::face_t *orig_face;
      std::vector<carve::mesh::MeshSet<3>::vertex_t *> vertices;
      FaceLoopGroup *group;

      FaceLoop(const carve::mesh::MeshSet<3>::face_t *f, const std::vector<carve::mesh::MeshSet<3>::vertex_t *> &v) : next(NULL), prev(NULL), orig_face(f), vertices(v), group(NULL) {}
    };


    struct FaceLoopList {
      FaceLoop *head, *tail;
      unsigned count;

      FaceLoopList() : head(NULL), tail(NULL), count(0) { }

      void append(FaceLoop *f) {
        f->prev = tail;
        f->next = NULL;
        if (tail) tail->next = f;
        tail = f;
        if (!head) head = f;
        count++;
      }

      void prepend(FaceLoop *f) {
        f->next = head;
        f->prev = NULL;
        if (head) head->prev = f;
        head = f;
        if (!tail) tail = f;
        count++;
      }

      unsigned size() const {
        return count;
      }

      FaceLoop *remove(FaceLoop *f) {
        FaceLoop *r = f->next;
        if (f->prev) { f->prev->next = f->next; } else { head = f->next; }
        if (f->next) { f->next->prev = f->prev; } else { tail = f->prev; }
        f->next = f->prev = NULL;
        count--;
        return r;
      }

      ~FaceLoopList() {
        FaceLoop *a = head, *b;
        while (a) {
          b = a;
          a = a->next;
          delete b;
        }
      }
    };

    struct FaceLoopGroup {
      const carve::mesh::MeshSet<3> *src;
      FaceLoopList face_loops;
      V2Set perimeter;
      std::list<ClassificationInfo> classification;

      FaceLoopGroup(const carve::mesh::MeshSet<3> *_src) : src(_src) {
      }

      FaceClass classificationAgainst(const carve::mesh::MeshSet<3>::mesh_t *mesh) const;
    };



    typedef std::list<FaceLoopGroup> FLGroupList;

  }
}
