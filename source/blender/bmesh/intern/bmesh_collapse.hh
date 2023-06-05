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

extern char *_last_local_obj;

#  define JVKE_CHECK_ELEMENT(elem) \
    { \
      int err = 0; \
      if ((err = bmesh_elem_check(elem, (elem)->head.htype))) { \
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

  /* Ensure correct winding. */
  do {
    if (l->radial_next != l && l->radial_next->v == l->v) {
      BM_face_normal_flip(bm, f);
      break;
    }
  } while ((l = l->next) != f->l_first);

  callbacks.on_face_create(f);
  callbacks.on_vert_kill(v);

  BM_vert_kill(bm, v);

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

      if (l2 == l2->radial_next) {
        continue;
      }

      if (BM_vert_edge_count(l2->v) == 3) {
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
                            BMVert *v_kill,
                            const bool do_del,
                            const bool combine_flags,
                            Callbacks &callbacks)
{
  BMVert *v_conn = BM_edge_other_vert(e, v_kill);

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

  /* Free any surrounding valence-3 rings disconnected from the edge. */
  bmesh_kernel_check_val3_vert<Callbacks>(bm, e, callbacks);

  Vector<BMFace *, 32> fs;
  Vector<BMEdge *, 32> deles;

  BMVert *v_del = BM_edge_other_vert(e, v_conn);
  const int tag = _FLAG_WALK_ALT; /* Using bmhead.api_flag. */
  const int dup_tag = _FLAG_OVERLAP;
  const int final_tag = _FLAG_JF;

  JVKE_CHECK_ELEMENT(v_conn);
  JVKE_CHECK_ELEMENT(v_del);

  /* Clear tags. */
  for (int i = 0; i < 2; i++) {
    BMVert *v = i ? v_del : v_conn;

    if (!v->e) {
      continue;
    }

    BMEdge *e2 = v->e;
    do {
      if (!e2->l) {
        continue;
      }

      BMLoop *l = e2->l;
      do {
        BM_ELEM_API_FLAG_DISABLE(l->f, tag);

        BMLoop *l2 = l;
        do {
          BMLoop *l3 = l2;
          do {
            BM_ELEM_API_FLAG_DISABLE(l3->f, tag);
          } while ((l3 = l3->radial_next) != l2);
        } while ((l2 = l2->next) != l);

#ifdef JVKE_DEBUG
        if (l->radial_next == l) {
          have_boundary = true;
        }
#endif

        BM_ELEM_API_FLAG_DISABLE(l->f, dup_tag);
        BM_ELEM_API_FLAG_DISABLE(l->e, dup_tag);
        BM_ELEM_API_FLAG_DISABLE(l->v, dup_tag);
      } while ((l = l->radial_next) != e2->l);
    } while ((e2 = BM_DISK_EDGE_NEXT(e2, v)) != v->e);
  }

  /* Build face list. */
  for (int i = 0; i < 2; i++) {
    BMVert *v = i ? v_del : v_conn;
    BMEdge *e2 = v->e;

    if (!e2 || !e2->l) {
      continue;
    }

    do {
      BMLoop *l = e2->l;

      if (!l) {
        continue;
      }

      do {
        if (!BM_ELEM_API_FLAG_TEST(l->f, tag)) {
          BM_ELEM_API_FLAG_ENABLE(l->f, tag);

          BMLoop *l2 = l;
          do {
            BM_ELEM_API_FLAG_DISABLE(l2->e, tag);
          } while ((l2 = l2->next) != l);

          if (fs.contains(l->f)) {
            printf("%s: error!\n", __func__);
          }
          else {
            fs.append(l->f);
          }
        }

        BMLoop *l2 = l;
        do {
          BMLoop *l3 = l2;
          do {
            if (l3->f != l->f && !BM_ELEM_API_FLAG_TEST(l3->f, tag)) {
              BM_ELEM_API_FLAG_ENABLE(l3->f, tag);
              if (fs.contains(l3->f)) {
                printf("%s: error!\n", __func__);
              }
              else {
                fs.append(l3->f);
              }
            }
          } while ((l3 = l3->radial_next) != l2);
        } while ((l2 = l2->next) != l);

      } while ((l = l->radial_next) != e2->l);
    } while ((e2 = BM_DISK_EDGE_NEXT(e2, v)) != v->e);
  }

  for (BMFace *f : fs) {
    callbacks.on_face_kill(f);
  }

  /* Unlink loops. */
  for (BMFace *f : fs) {
    BMLoop *l = f->l_first;

    do {
      BMEdge *e2 = l->e;

      l->radial_next->radial_prev = l->radial_prev;
      l->radial_prev->radial_next = l->radial_next;

      if (l == e2->l) {
        e2->l = l->radial_next;
      }

      if (l == e2->l) {
        e2->l = nullptr;
      }

      l->radial_next = l->radial_prev = l;
      if (e2->l) {
        BMLoop *l2 = e2->l;
        int count = 0;

        do {
          if (count++ > 10) {
            printf("%s: Corrupted radial list\n", __func__);
            break;
          }

          if (l2 == l) {
            printf("%s: Radial list still has deleted loop\n", __func__);
          }
        } while ((l2 = l2->next) != e2->l);
      }
    } while ((l = l->next) != f->l_first);
  }

  /* Swap verts. */
  for (BMFace *f : fs) {
    BMLoop *l = f->l_first, *lnext = nullptr;

    do {
      lnext = l->next;

      if (l->v == v_del) {
        l->v = v_conn;
      }

      BM_ELEM_API_FLAG_DISABLE(l->v, tag);

      for (int step = 0; step < 2; step++) {
        BMVert *v_edge = step ? l->e->v2 : l->e->v1;
        BMVert *v_other = BM_edge_other_vert(l->e, v_edge);

        if (v_edge != v_del) {
          continue;
        }

        if (v_other == v_conn) {
          /* Flag for later deletion. */
          if (!BM_ELEM_API_FLAG_TEST(l->e, tag)) {
            deles.append(l->e);
          }

          BM_ELEM_API_FLAG_ENABLE(l->e, tag);
        }
        else {
          BMEdge *e3;

          if ((e3 = BM_edge_exists(v_conn, v_other))) {
            if (combine_flags) {
              bool remove_smooth = !BM_elem_flag_test(l->e, BM_ELEM_SMOOTH);
              remove_smooth = remove_smooth || !BM_elem_flag_test(e3, BM_ELEM_SMOOTH);

              e3->head.hflag |= l->e->head.hflag;

              if (remove_smooth) {
                BM_elem_flag_disable(e3, BM_ELEM_SMOOTH);
              }
            }

            /* Flag for later deletion. */
            if (!BM_ELEM_API_FLAG_TEST(l->e, tag)) {
              deles.append(l->e);
            }

            BM_ELEM_API_FLAG_ENABLE(l->e, tag);

            l->e = e3;
          }
          else {
            callbacks.on_edge_kill(l->e);
            bmesh_disk_vert_replace(l->e, v_conn, v_del);
            callbacks.on_edge_create(l->e);
          }
        }
      }
    } while ((l = lnext) != f->l_first);
  }

  for (BMEdge *e2 : deles) {
    if (e2->l != nullptr) {
      printf("%s: e2->l was not null!\n", __func__);

      while (e2->l) {
        BMFace *f = e2->l->f;

        if (fs.contains(f)) {
          printf("double entry for f! f->len: %d\n", f->len);
        }

        if (!fs.contains(f)) {
          fs.append(f);
          callbacks.on_face_kill(f);

          /* Unlink face. */
          BMLoop *l = f->l_first;
          do {
            if (l->v == v_del) {
              /* Duplicate vertex; it will be filtered out later. */
              l->v = l->next->v;
            }

            if (l->e != e2) {
              bmesh_radial_loop_remove(l->e, l);
            }
          } while ((l = l->next) != f->l_first);
        }

        bmesh_radial_loop_remove(e2, e2->l);
      }
    }

    callbacks.on_edge_kill(e2);
    BM_edge_kill(bm, e2);
  }

  for (int i : fs.index_range()) {
    BMFace *f = fs[i];
    BMLoop *l, *lnext;

    /* Validate. */
    l = f->l_first;
    do {
      lnext = l == l->next ? nullptr : l->next;

      if (l->v == l->next->v) {
        l->prev->next = l->next;
        l->next->prev = l->prev;

        if (l == l->f->l_first) {
          l->f->l_first = l->next;
        }

        l->f->len--;

        if (l == l->f->l_first) {
          l->f->l_first = nullptr;
        }

        bm_kill_only_loop(bm, l);
      }
    } while (lnext && (l = lnext) != f->l_first);

    if (f->len <= 2) {
      /* Kill face. */
      while (f->l_first) {
        BMLoop *l2 = f->l_first;

        l2->prev->next = l2->next;
        l2->next->prev = l2->prev;
        f->l_first = l2->next;

        bm_kill_only_loop(bm, l2);

        if (f->l_first == l2) {
          f->l_first = nullptr;
        }
      }

      bm_kill_only_face(bm, f);
      fs[i] = nullptr;
    }
  }

  bm->elem_index_dirty |= BM_VERT | BM_EDGE | BM_FACE;
  bm->elem_table_dirty |= BM_VERT | BM_EDGE | BM_FACE;

  /* Relink. */
  for (BMFace *f : fs) {
    if (!f) {
      continue;
    }

    BM_ELEM_API_FLAG_ENABLE(f, final_tag);

    BMLoop *l = f->l_first;
    do {
      l->e = BM_edge_exists(l->v, l->next->v);

      if (!l->e) {
        printf("%s: missing edge! %p %p\n", __func__, l->v, l->next->v);

        l->e = BM_edge_create(bm, l->v, l->next->v, nullptr, BM_CREATE_NOP);
        callbacks.on_edge_create(l->e);
      }

      bmesh_radial_loop_append(l->e, l);
      JVKE_CHECK_ELEMENT(l->e);

      BM_ELEM_API_FLAG_DISABLE(l->e, dup_tag);
      BM_ELEM_API_FLAG_DISABLE(l->v, dup_tag);
      BM_ELEM_API_FLAG_DISABLE(l->f, dup_tag);
    } while ((l = l->next) != f->l_first);

    if (f->head.htype != BM_FACE) {
      printf("%s: error!! f was freed!\n", __func__);
      continue;
    }

    callbacks.on_face_create(f);
  }

  JVKE_CHECK_ELEMENT(v_conn);

#ifdef JVKE_DEBUG
  for (int step = 0; step < 2; step++) {
    BMVert *v = step ? v_conn : v_del;
    BMEdge *e1 = v->e;

    if (e1) {
      do {
        JVKE_CHECK_ELEMENT(e1);

        BMLoop *l = e1->l;

        if (!l) {
          continue;
        }

        /* boundary? */
        if (l == l->radial_next && !have_boundary) {
          trigger_jvke_error(IS_LOOP_WRONG_RADIAL_LENGTH, saved_obj);
        }

        if (!l) {
          continue;
        }

        do {
          JVKE_CHECK_ELEMENT(l);
          JVKE_CHECK_ELEMENT(l->v);
          JVKE_CHECK_ELEMENT(l->e);
          JVKE_CHECK_ELEMENT(l->f);
        } while ((l = l->radial_next) != e1->l);
      } while ((e1 = BM_DISK_EDGE_NEXT(e1, v)) != v->e);
    }
  }
#endif

  /* Use euler criteria to check for duplicate faces. */
  if (do_del && v_conn->e) {
#if 0
    int tote = 0, totv = 0, totf = 0;

    BMVert *v = v_conn;
    BMEdge *e2 = v->e;

    if (!BM_ELEM_API_FLAG_TEST(v, dup_tag)) {
      BM_ELEM_API_FLAG_ENABLE(v, dup_tag);
      totv++;
    }

    do {
      BMVert *v2 = BM_edge_other_vert(e2, v);

      if (!BM_ELEM_API_FLAG_TEST(e2, dup_tag)) {
        BM_ELEM_API_FLAG_ENABLE(e2, dup_tag);
        tote++;
      }
      if (!BM_ELEM_API_FLAG_TEST(v2, dup_tag)) {
        BM_ELEM_API_FLAG_ENABLE(v2, dup_tag);
        totv++;
      }

      if (e2->l) {
        BMLoop *l_radial = e2->l;
        do {
          if (BM_ELEM_API_FLAG_TEST(l_radial->f, dup_tag)) {
            continue;
          }

          totf++;

          BM_ELEM_API_FLAG_ENABLE(l_radial->f, dup_tag);
          BMLoop *l = l_radial;

          do {
            if (!BM_ELEM_API_FLAG_TEST(l->v, dup_tag)) {
              BM_ELEM_API_FLAG_ENABLE(l->v, dup_tag);
              totv++;
            }

            if (!BM_ELEM_API_FLAG_TEST(l->e, dup_tag)) {
              BM_ELEM_API_FLAG_ENABLE(l->e, dup_tag);
              tote++;
            }
          } while ((l = l->next) != l_radial);
        } while ((l_radial = l_radial->radial_next) != e2->l);
      }
    } while ((e2 = BM_DISK_EDGE_NEXT(e2, v)) != v->e);

    int eul = totv - tote + totf;
    if (eul != 1) {
#else
    {
      BMEdge *e2;
      BMVert *v = v_conn;
#endif
    e2 = v->e;

    do {
      BMLoop *l = e2->l;

      if (!l) {
        continue;
      }

      BMLoop *l_next = l;

      do {
        /* No guarantee each face has only one loop in radial
         * list.
         */
        l_next = l->radial_next;

        while (l_next != l && l_next->f == l->f) {
          l_next = l->radial_next;
        }

        BMFace *f;

        if ((f = BM_face_find_double(l->f))) {
          callbacks.on_face_kill(l->f);
          BM_face_kill(bm, l->f);
        }
      } while (e2->l && (l = l_next) != e2->l);
    } while ((e2 = BM_DISK_EDGE_NEXT(e2, v)) != v->e);
  }
}

if (do_del) {
  JVKE_CHECK_ELEMENT(v_del);

  if (v_del->e && v_del->e->l) {
    printf("%s: vert is not cleared\n", __func__);
  }

  if (v_del->e) {
    BMIter iter;

    BMEdge *e2;
    BM_ITER_ELEM (e2, &iter, v_del, BM_EDGES_OF_VERT) {
      callbacks.on_edge_kill(e2);
    }

    BMFace *f2;
    BM_ITER_ELEM (f2, &iter, v_del, BM_FACES_OF_VERT) {
      callbacks.on_face_kill(f2);
    }
  }

  callbacks.on_vert_kill(v_del);
  BM_vert_kill(bm, v_del);
}

#ifdef JVKE_DEBUG
bm_local_obj_free(saved_obj, buf);
#endif

return v_conn;
}
#ifdef _OTHER_TRACES
#  undef _OTHER_TRACES
#endif
}  // namespace blender::bmesh
