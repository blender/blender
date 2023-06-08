#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_map.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_set.hh"
#include "BLI_span.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

using blender::float2;
using blender::float3;
using blender::IndexRange;
using blender::Map;
using blender::MutableSpan;
using blender::Set;
using blender::Span;
using blender::Vector;

#include <functional>
#include <string>
#include <type_traits>
#include <utility>

#include "bmesh.h"
#include "bmesh_private.h"
extern "C" {
#include "bmesh_structure.h"
}

//#define JVKE_DEBUG

namespace blender::bmesh {

struct NullCollapseCallbacks {
  inline void on_vert_kill(BMVert *) {}
  inline void on_edge_kill(BMEdge *) {}
  /* Called when verts are combined. */
  inline void on_vert_combine(BMVert * /*dest*/, BMVert * /*source*/) {}
  /* Called when edges are combined. */
  inline void on_edge_combine(BMEdge * /*dest*/, BMEdge * /*source*/) {}
  inline void on_face_kill(BMFace *) {}
  inline void on_vert_create(BMVert *) {}
  inline void on_edge_create(BMEdge *) {}
  inline void on_face_create(BMFace *) {}
};

#ifdef JVKE_DEBUG

static void bm_local_obj_free(char *str, char *fixed)
{
  if (str != fixed) {
    MEM_freeN(str);
  }
}

#  define LOCAL_OBJ_SIZE 512

static char *obj_append_line(const char *line, char *str, char *fixed, int *size, int *i)
{
  int len = (int)strlen(line);

  if (*i + len + 1 >= *size) {
    *size += len + ((*size) >> 1);

    if (str == fixed) {
      str = static_cast<char *>(MEM_mallocN(*size, "buf"));
      memcpy(static_cast<void *>(str), fixed, LOCAL_OBJ_SIZE);
    }
    else {
      str = static_cast<char *>(MEM_reallocN(str, *size));
    }
  }

  memcpy(str + *i, line, len);
  str[*i + len] = 0;

  *i += len;

  return str;
}

/* NotForPr: saves an obj of the neighborhood around an edge prior to collapse
 *           into a buffer that can be read from a debugger.
 */
static char *bm_save_local_obj_text(
    BMesh *, int depth, char buf[LOCAL_OBJ_SIZE], const char *fmt, ...)
{
  va_list vl;
  va_start(vl, fmt);

  buf[0] = 0;

  Vector<BMVert *, 64> vs;
  Vector<BMVert *, 8> initial_vs;
  Vector<BMEdge *, 64> es;
  Vector<BMEdge *, 8> initial_es;
  Vector<BMFace *, 64> fs;
  Vector<BMFace *, 8> initial_fs;

  Set<void *, 300> visit;

  const char *c = fmt;
  while (*c) {
    if (*c == ' ' || *c == '\t') {
      c++;
      continue;
    }

    void *ptr = va_arg(vl, void *);

    switch (*c) {
      case 'v':
        vs.append(static_cast<BMVert *>(ptr));
        initial_vs.append(static_cast<BMVert *>(ptr));
        break;
      case 'e':
        es.append(static_cast<BMEdge *>(ptr));
        initial_es.append(static_cast<BMEdge *>(ptr));
        break;
      case 'f':
        fs.append(static_cast<BMFace *>(ptr));
        initial_fs.append(static_cast<BMFace *>(ptr));
        break;
    }

    c++;
  }

  va_end(vl);

  int tag = 4;
  for (BMFace *f : fs) {
    BMLoop *l = f->l_first;

    do {
      l->v->head.api_flag &= ~tag;
      l->e->head.api_flag &= ~tag;
    } while ((l = l->next) != f->l_first);
  }

  for (BMEdge *e : es) {
    e->v1->head.api_flag &= ~tag;
    e->v2->head.api_flag &= ~tag;
  }

  for (BMVert *v : vs) {
    v->head.api_flag |= tag;
  }

  for (BMEdge *e : es) {
    if (!(e->v1->head.api_flag & tag)) {
      vs.append(e->v1);
      e->v1->head.api_flag |= tag;
    }

    if (!(e->v2->head.api_flag & tag)) {
      vs.append(e->v2);
      e->v2->head.api_flag |= tag;
    }

    e->head.api_flag |= tag;
  }

  for (BMFace *f : fs) {
    BMLoop *l = f->l_first;

    do {
      if (!(l->v->head.api_flag & tag)) {
        vs.append(l->v);
        l->v->head.api_flag |= tag;
      }

      if (!(l->e->head.api_flag & tag)) {
        es.append(l->e);
        l->e->head.api_flag |= tag;
      }
    } while ((l = l->next) != f->l_first);
  }

  struct StackItem {
    BMVert *v;
    int depth;
  };

  Vector<StackItem, 32> stack;
  Set<void *, 300> elemset;

  for (BMVert *v : vs) {
    elemset.add(static_cast<void *>(v));
  }
  for (BMEdge *e : es) {
    elemset.add(static_cast<void *>(e));
  }
  for (BMFace *f : fs) {
    elemset.add(static_cast<void *>(f));
  }

  stack.clear();
  stack.append({vs[0], 0});
  while (stack.size() > 0) {
    StackItem item = stack.pop_last();
    BMVert *v = item.v;
    int startdepth = item.depth;

    if (elemset.add(static_cast<void *>(v))) {
      vs.append(v);
    }

    if (!v->e || item.depth > depth) {
      continue;
    }

    BMEdge *e = v->e;
    do {
      if (visit.add(static_cast<void *>(e))) {
        stack.append({e->v1, startdepth + 1});
        stack.append({e->v2, startdepth + 1});
      }

      if (!e->l) {
        continue;
      }

      BMLoop *l = e->l;
      do {
        if (visit.add(static_cast<void *>(l->f))) {
          if (elemset.add(static_cast<void *>(l->f))) {
            fs.append(l->f);
          }

          BMLoop *l2 = l;
          do {
            if (visit.add(static_cast<void *>(l->v))) {
              stack.append({l->v, startdepth + 1});
            }
          } while ((l2 = l2->next) != l);
        }
      } while ((l = l->radial_next) != e->l);
    } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
  }

  char *str = buf;
  int size = LOCAL_OBJ_SIZE - 1;
  int stri = 0;

  char line[128];
  line[0] = 0;

  for (BMVert *v : vs) {
    v->head.api_flag &= ~tag;
  }
  for (BMEdge *e : es) {
    e->head.api_flag &= ~tag;
  }

  for (BMFace *f : fs) {
    f->head.api_flag &= ~tag;
  }
  for (BMVert *v : initial_vs) {
    v->head.api_flag |= tag;
  }

  for (BMEdge *e : initial_es) {
    e->head.api_flag |= tag;
    e->v1->head.api_flag |= tag;
    e->v2->head.api_flag |= tag;
  }

  for (BMFace *f : initial_fs) {
    f->head.api_flag |= tag;
    BMLoop *l = f->l_first;

    do {
      l->v->head.api_flag |= tag;
    } while ((l = l->next) != f->l_first);
  }

  for (int i : vs.index_range()) {
    BMVert *v = vs[i];

    if (v->head.api_flag & tag) {
      sprintf(line, "#select\n");
      str = obj_append_line(line, str, buf, &size, &stri);
    }

    v->head.index = i + 1;
    sprintf(line, "v %.4f %.4f %.4f\n", v->co[0], v->co[1], v->co[2]);

    str = obj_append_line(line, str, buf, &size, &stri);
  }

  /* save wire edges */
  for (BMEdge *e : es) {
    if (e->l) {
      continue;
    }

    sprintf(line, "l %d %d\n", e->v1->head.index, e->v2->head.index);
    str = obj_append_line(line, str, buf, &size, &stri);
  }

  for (BMFace *f : fs) {
    BMLoop *l = f->l_first;

    sprintf(line, "f");
    str = obj_append_line(line, str, buf, &size, &stri);

    do {
      sprintf(line, " %d", l->v->head.index);

      str = obj_append_line(line, str, buf, &size, &stri);
    } while ((l = l->next) != f->l_first);

    str = obj_append_line("\n", str, buf, &size, &stri);
  }

  return str;
}

static void check_mesh_radial(BMesh *bm)
{
  return;

  BMIter iter;
  BMEdge *e;
  BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
    int count = 0;
    BMLoop *l = e->l;

    if (!l) {
      continue;
    }

    do {
      if (BM_elem_is_free((BMElem *)l, BM_LOOP)) {
        printf("Freed loop in edge %p radial cycle\n", e);
      }

      if (count++ > 100) {
        printf("Corrupted radial cycle for %p\n", e);
        break;
      }
    } while ((l = l->radial_next) != e->l);
  }
}

static void trigger_jvke_error(int err, char *obj_text)
{
  printf("========= ERROR %s============\n\n%s\n\n", bm_get_error_str(err), obj_text);
}

int bmesh_elem_check_all(void *elem, char htype);

extern char *_last_local_obj;

#  define JVKE_CHECK_ELEMENT(elem) \
    { \
      int err = 0; \
      if ((err = bmesh_elem_check_all(elem, (elem)->head.htype))) { \
        trigger_jvke_error(err, saved_obj); \
      } \
    }
#else
#  define JVKE_CHECK_ELEMENT(elem)
#endif

template<typename Callbacks = NullCollapseCallbacks>
static bool cleanup_vert(BMesh *bm, BMVert *v, Callbacks &callbacks)
{
  BMEdge *e = v->e;

  if (!e->l || e->l->f == e->l->radial_next->f) {
    return false;
  }

  BMFace *f_example = nullptr;

  BMIter iter;
  BMFace *f;
  BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
    callbacks.on_face_kill(f);
  }

  do {
    BMLoop *l = e->l;
    if (!l) {
      continue;
    }

    callbacks.on_edge_kill(e);

    if (!f_example) {
      f_example = l->f;
    }
  } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

  BMVert *v1 = BM_edge_other_vert(v->e, v);
  BMVert *v2 = BM_edge_other_vert(BM_DISK_EDGE_NEXT(v->e, v), v);
  BMVert *v3 = BM_edge_other_vert(BM_DISK_EDGE_NEXT(BM_DISK_EDGE_NEXT(v->e, v), v), v);

  f = BM_face_create_quad_tri(bm, v1, v2, v3, nullptr, f_example, BM_CREATE_NOP);
  BMLoop *l = f->l_first;

  callbacks.on_vert_kill(v);
  BM_vert_kill(bm, v);

  /* Ensure correct winding. */
  do {
    if (l->radial_next != l && l->radial_next->v == l->v) {
      BM_face_normal_flip(bm, f);
      break;
    }
  } while ((l = l->next) != f->l_first);

  l = f->l_first;
  do {
    if (l != l->radial_next) {
      BMLoop *l2 = l->radial_next;
      if (l2->v != l->v) {
        l2 = l2->next;
      }

      CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata, l2->head.data, &l->head.data);
    }
  } while ((l = l->next) != f->l_first);

  callbacks.on_face_create(f);

  return true;
}

template<typename Callbacks = NullCollapseCallbacks>
static void bmesh_kernel_check_val3_vert(BMesh *bm, BMEdge *e, Callbacks &callbacks)
{
  if (!e->l) {
    return;
  }

  bool stop;

  do {
    stop = true;

    BMLoop *l = e->l;

    if (!l) {
      break;
    }

    do {
      BMLoop *l2 = l->prev;

      if (l2 == l2->radial_next || !l2->v->e) {
        continue;
      }

      bool bad = false;
      int count = 0;

      BMEdge *e2 = l2->v->e;
      do {
        if (!e2->l || e2->l == e2->l->radial_next || e2->l->radial_next->radial_next != e2->l) {
          bad = true;
          break;
        }

        bad = bad || e2->l->f->len != 3 || e2->l->radial_next->f->len != 3;
        count++;
      } while ((e2 = BM_DISK_EDGE_NEXT(e2, l2->v)) != l2->v->e);

      bad = bad || count != 3;

      if (!bad) {
        if (cleanup_vert<Callbacks>(bm, l2->v, callbacks)) {
          stop = false;
          break;
        }
      }
    } while ((l = l->radial_next) != e->l);
  } while (!stop);
}

/**
 * \brief Join Vert Kill Edge (JVKE)
 *
 * Collapse an edge, merging surrounding data.
 *
 * Unlike #BM_vert_collapse_edge & #bmesh_kernel_join_edge_kill_vert
 * which only handle 2 valence verts,
 * this can handle any number of connected edges/faces.
 *
 * <pre>
 * Before: -> After:
 * +-+-+-+    +-+-+-+
 * | | | |    | \ / |
 * +-+-+-+    +--+--+
 * | | | |    | / \ |
 * +-+-+-+    +-+-+-+
 * </pre>
 */

template<typename Callbacks = NullCollapseCallbacks>
BMVert *join_vert_kill_edge(BMesh *bm,
                            BMEdge *e,
                            BMVert *v_del,
                            const bool do_del,
                            const bool combine_flags,
                            Callbacks &callbacks)
{
  BMVert *v_conn = BM_edge_other_vert(e, v_del);

#ifdef JVKE_DEBUG
  char buf[LOCAL_OBJ_SIZE];

  bool have_boundary = false;

  if (_last_local_obj) {
    MEM_freeN(static_cast<void *>(_last_local_obj));
  }

  char *saved_obj = bm_save_local_obj_text(bm, 2, buf, "e", e);

  _last_local_obj = static_cast<char *>(MEM_mallocN(strlen(saved_obj) + 1, "_last_local_obj"));
  BLI_strncpy(_last_local_obj, saved_obj, strlen(saved_obj) + 1);
#endif

  /* Destroy any valence-3 verts that might turn into non-manifold "fins." */
  bmesh_kernel_check_val3_vert<Callbacks>(bm, e, callbacks);

  Set<BMEdge *, 32> es;
  Set<BMFace *, 32> fs;

  const int dup_tag = _FLAG_OVERLAP;

  callbacks.on_vert_combine(v_conn, v_del);

  for (int i = 0; i < 2; i++) {
    BMVert *v = i ? v_del : v_conn;

    BMEdge *e = v->e;
    do {
      es.add(e);

      BMLoop *l = e->l;
      if (!l) {
        continue;
      }

      do {
        fs.add(l->f);
        BMLoop *l2 = l;
        do {
          es.add(l2->e);
          BMLoop *l3 = l2;
          do {
            fs.add(l3->f);
          } while ((l3 = l3->radial_next) != l2);
        } while ((l2 = l2->next) != l);
      } while ((l = l->radial_next) != e->l);
    } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
  }

  /* Inform callbacks we've "killed" all the faces. */
  for (BMFace *f : fs) {
    callbacks.on_face_kill(f);
  }

  /* Unlink loops. */
  for (BMFace *f : fs) {
    BMLoop *l = f->l_first;
    do {
      BMEdge *e = l->e;
      bmesh_radial_loop_remove(l->e, l);
      l->e = e;
    } while ((l = l->next) != f->l_first);
  }

  /* Swap edges. */
  for (BMEdge *e : es) {
    if (e->v1 != v_del && e->v2 != v_del) {
      continue;
    }

    if (e->v1 == v_conn || e->v2 == v_conn) {
      if (e->l) {
        printf("%s: ErROR!\n", __func__);
      }

      callbacks.on_edge_kill(e);
      BM_edge_kill(bm, e);
      continue;
    }

    BMVert *otherv = e->v1 == v_del ? e->v2 : e->v1;

    BMEdge *exist = BM_edge_exists(otherv, v_conn);

    if (exist) {
      if (e->l) {
        printf("%s: ERROR!\n", __func__);
      }

      callbacks.on_edge_combine(exist, e);

      if (combine_flags) {
        exist->head.hflag |= e->head.hflag;
      }

      callbacks.on_edge_kill(e);
      BM_edge_kill(bm, e);
    }
    else {
      callbacks.on_edge_kill(e);
      bmesh_disk_vert_replace(e, v_conn, v_del);
      callbacks.on_edge_create(e);
    }
  }

  auto remove_loop = [&bm](BMFace *f, BMLoop *l) {
    l->next->prev = l->prev;
    l->prev->next = l->next;
    if (l == f->l_first) {
      f->l_first = l->next;
    }

    f->len--;
    bm_kill_only_loop(bm, l);
  };

  /* Swap loops */
  for (BMFace *f : fs) {
    BMLoop *l = f->l_first;
    BMLoop *lnext;
    bool found = false;

    /* Swap v_del and remove duplicate v_conn's. */
    do {
      lnext = l->next;

      if (l->v == v_del) {
        l->v = v_conn;
      }
      if (l->v == v_conn) {
        if (found) {
          remove_loop(f, l);
        }
        else {
          found = true;
        }
      }
    } while ((l = lnext) != f->l_first);

    /* Remove any remaining duplicate verts. */
    do {
      lnext = l->next;
      if (l->v == l->next->v) {
        remove_loop(f, l);
      }
    } while ((l = lnext) != f->l_first);
  }

  Vector<BMFace *, 32> finalfs;

  /* Relink faces. */
  for (BMFace *f : fs) {
    if (f->len < 3) {
      BMLoop *l = f->l_first;
      BMLoop *lnext;
      do {
        lnext = l->next;
        bm_kill_only_loop(bm, l);
      } while ((l = lnext) != f->l_first);

      bm_kill_only_face(bm, f);
      continue;
    }

    BMLoop *l = f->l_first;
    do {
      BMEdge *exist_e = BM_edge_exists(l->v, l->next->v);
      if (!exist_e) {
        exist_e = BM_edge_create(bm, l->v, l->next->v, nullptr, BM_CREATE_NOP);
        callbacks.on_edge_create(exist_e);
      }

      l->e = exist_e;
      bmesh_radial_loop_append(l->e, l);

      BM_ELEM_API_FLAG_DISABLE(l->v, dup_tag);
      BM_ELEM_API_FLAG_DISABLE(l->e, dup_tag);
      BM_ELEM_API_FLAG_DISABLE(l->f, dup_tag);
    } while ((l = l->next) != f->l_first);

    callbacks.on_face_create(f);
    finalfs.append(f);
  }

  for (BMFace *f : finalfs) {
    if (BM_elem_is_free((BMElem *)f, BM_FACE)) {
      continue;
    }

    BMFace *f2;
    while ((f2 = BM_face_find_double(f))) {
      printf("%s: removing duplicate face.\n", __func__);
      callbacks.on_face_kill(f2);
      BM_face_kill(bm, f2);
    }
  }

  JVKE_CHECK_ELEMENT(v_conn);

  if (do_del && !v_del->e) {
    callbacks.on_vert_kill(v_del);
    BM_vert_kill(bm, v_del);
  }

  return v_conn;
}
}  // namespace blender::bmesh
