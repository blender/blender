#include "bmesh_collapse.hh"
#include "bmesh_private.h"

using blender::bmesh::NullCollapseCallbacks;

namespace blender::bmesh {

#ifdef JVKE_DEBUG
char *_last_local_obj = nullptr;
static ATTR_NO_OPT int bmesh_elem_check_all_intern(void *elem, char htype, int depth = 0)
{
  int ret = bmesh_elem_check(elem, htype);

  if (ret || depth > 2) {
    return ret;
  }

  switch (htype) {
    case BM_VERT: {
      BMVert *v = static_cast<BMVert *>(elem);

      if (!v->e) {
        return 0;
      }

      int count1 = 0;
      BMEdge *e = v->e;

      do {
        BMVert *v2 = BM_edge_other_vert(e, v);
        int ret2 = bmesh_elem_check_all_intern(static_cast<void *>(v2), BM_VERT, depth + 1);
        if (ret2) {
          return ret2;
        }

        if (count1++ > 1000) {
          return IS_EDGE_NULL_DISK_LINK;
        }

        if (!e->l) {
          continue;
        }

        int count2 = 0;
        BMLoop *l = e->l;
        do {
          int ret2 = bmesh_elem_check_all_intern(static_cast<void *>(l->f), BM_FACE, depth + 1);
          if (ret2) {
            return ret2;
          }

          if (count2++ > 100) {
            return IS_LOOP_WRONG_RADIAL_LENGTH;
          }
        } while ((l = l->radial_next) != e->l);
      } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
      break;
    }
    case BM_EDGE: {
      BMEdge *e = static_cast<BMEdge *>(elem);
      BMLoop *l = e->l;

      if (!l) {
        return 0;
      }

      int count = 0;

      do {
        if (count++ > 100) {
          return IS_LOOP_WRONG_RADIAL_LENGTH;
        }
      } while ((l = l->radial_next) != e->l);
      break;
    }
    case BM_LOOP: {
      BMLoop *l = static_cast<BMLoop *>(elem);
      BMLoop *l2 = l;
      int count = 0;

      if (BM_elem_is_free((BMElem *)l->f, BM_FACE)) {
        return IS_LOOP_WRONG_FACE_TYPE;
      }
      if (BM_elem_is_free((BMElem *)l->e, BM_EDGE)) {
        return IS_LOOP_WRONG_EDGE_TYPE;
      }
      if (BM_elem_is_free((BMElem *)l->v, BM_VERT)) {
        return IS_LOOP_WRONG_VERT_TYPE;
      }

      do {
        if (count++ > 100) {
          return IS_LOOP_WRONG_RADIAL_LENGTH;
        }
      } while ((l2 = l2->radial_next) != l);
      break;
    }
    case BM_FACE: {
      BMFace *f = static_cast<BMFace *>(elem);
      BMLoop *l = f->l_first;
      int count = 0;

      do {
        if (count++ > 100000) {
          return IS_FACE_WRONG_LENGTH;
        }

        int ret2 = bmesh_elem_check_all_intern(static_cast<void *>(l), BM_LOOP, depth + 1);
        if (ret2) {
          return ret2;
        }
      } while ((l = l->next) != f->l_first);

      break;
    }
  }

  return 0;
}

int bmesh_elem_check_all(void *elem, char htype)
{
  return bmesh_elem_check_all_intern(elem, htype);
}
#endif
}  // namespace blender::bmesh

extern "C" BMVert *bmesh_kernel_join_vert_kill_edge(
    BMesh *bm, BMEdge *e, BMVert *v_kill, const bool do_del, const bool combine_flags)
{
  NullCollapseCallbacks callbacks;
  return blender::bmesh::join_vert_kill_edge<NullCollapseCallbacks>(
      bm, e, v_kill, do_del, combine_flags, callbacks);
}
