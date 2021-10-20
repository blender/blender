#if 0
#  pragma optimize("", off)
#  pragma once

/*

This is a proof of concept of how a C++ sculpt system could work.
It's a design study, not even a proposal.

We can't really use virtual-based polymorphism for performance reasons,
so the idea is to use templates and C++20's concepts instead.

*/

#  if __cplusplus >= 202000
#    include <concepts>
#  endif

#  include <functional>
#  include <iterator>

#  include <cstdint>

//#  define COMPILER_EXPLORER

#  ifndef COMPILER_EXPLORER
#    include "BKE_pbvh.h"
#    include "BLI_bitmap.h"
#    include "BLI_map.hh"
#    include "BLI_math.h"
#    include "BLI_mempool.h"
#    include "BLI_task.h"
#    include "BLI_task.hh"
#    include "MEM_guardedalloc.h"

#    include "DNA_brush_enums.h"
#    include "DNA_brush_types.h"
#    include "DNA_mesh_types.h"
#    include "DNA_meshdata_types.h"
#    include "DNA_scene_types.h"

#    include "BKE_brush.h"
#    include "BKE_colortools.h"
//#include "BKE_brush_engine.h"
#    include "BKE_mesh.h"
#    include "BKE_object.h"
#    include "BKE_paint.h"
#    include "BKE_pbvh.h"

extern "C" {
#    include "sculpt_intern.h"
}

#    include "bmesh.h"

/* clang-format off */
#include "../../blenkernel/intern/pbvh_intern.h"
/* clang-format on */

#  else
void copy_v3_v3(float *a, float *b)
{
  a[0] = b[0];
  a[1] = b[1];
  a[2] = b[2];
}

void madd_v3_v3fl(float *r, float *a, float m)
{
  r[0] += a[0] * m;
  r[1] += a[1] * m;
  r[2] += a[2] * m;
}

namespace blender {
class IndexRange {
 public:
  IndexRange(int start, int end) : _start(start), _end(end), _i(start)
  {
  }

  IndexRange(const IndexRange &r) : _start(r._start), _end(r._end), _i(r._i)
  {
  }

  inline IndexRange &operator++()
  {
    _i++;

    return *this;
  }

  inline IndexRange operator++(int)
  {
    IndexRange tmp(*this);
    operator++();
    return tmp;
  }

  inline IndexRange begin()
  {
    IndexRange ret(_start, _end);
    ret._i = _start;

    return ret;
  }

  inline IndexRange end()
  {
    IndexRange ret(_start, _end);
    ret._i = _end;

    return ret;
  }

  inline bool operator==(const IndexRange &b)
  {
    // detect comparison with end iterator
    return _i == b._i;
  }

  inline bool operator!=(const IndexRange &b)
  {
    return !(*this == b);
  }

  inline int operator*()
  {
    return _i;
  }

 private:
  int _start, _end, _i;
};

namespace threading {
void parallel_for(IndexRange range, int n, std::function<void(IndexRange &)> &&func)
{
  for (int i : range) {
    // std::invoke<void(IndexRange*)>(func, IndexRange(i, i + 1));
    IndexRange subrange(i, i + 1);
    func(subrange);
  }
}
}  // namespace threading
}  // namespace blender

typedef struct PBVHNode {
  struct {
    void **elems;
    int length;
  } * bm_unique_verts, *bm_other_verts, *bm_faces;
} PBVHNode;
typedef struct PBVH PBVH;

typedef struct BMVert {
  struct {
    char htype, hflag;
    short api_flag;
    int index;
    void *data;
  } head;

  struct BMEdge *e;
  float co[3];
  float no[3];
} BMVert;

struct BMLoop;

typedef struct BMEdge {
  struct {
    char htype, hflag;
    short api_flag;
    int index;
    void *data;
  } head;

  BMVert *v1, *v2;
  struct BMLoop *l;
  struct {
    BMEdge *next, *prev;
  } v1_disk_link, v2_disk_link;
} BMEdge;

struct BMFace;

typedef struct BMLoop {
  struct {
    char htype, hflag;
    short api_flag;
    int index;
    void *data;
  } head;

  BMVert *v;
  BMEdge *e;
  struct BMFace *f;
  BMLoop *radial_next, *radial_prev;
  BMLoop *next, *prev;
} BMLoop;

typedef struct BMFace {
  struct {
    char htype, hflag;
    short api_flag;
    int index;
    void *data;
  } head;

  BMLoop *l_first;
  int len;
} BMFace;

typedef struct BMesh {
  int totvert, totedge, totloop, totface;
} BMesh;

typedef struct SculptVertRef {
  intptr_t i;
} SculptVertRef;

typedef struct SculptBrushTest {
  int value;
} SculptBrushTest;

struct SculptSession;
typedef float (*SculptBrushTestFn)(SculptBrushTest *test, float *co);
typedef bool (*BKE_pbvh_SearchCallback)(PBVHNode *node, void *data);
void BKE_pbvh_search_gather(
    PBVH *pbvh, BKE_pbvh_SearchCallback scb, void *search_data, PBVHNode ***array, int *tot);
SculptBrushTestFn SCULPT_brush_test_init_with_falloff_shape(struct SculptSession *ss,
                                                            SculptBrushTest *test,
                                                            char falloff_shape);
bool SCULPT_search_sphere_cb(PBVHNode *node, void *data_v);
void BKE_pbvh_node_mark_update(PBVHNode *node);

typedef struct {
  struct Sculpt *sd;
  struct SculptSession *ss;
  float radius_squared;
  const float *center;
  bool original;
  /* This ignores fully masked and fully hidden nodes. */
  bool ignore_fully_ineffective;
  struct Object *ob;
  struct Brush *brush;
} SculptSearchSphereData;

float SCULPT_brush_strength_factor(struct SculptSession *ss,
                                   const struct Brush *br,
                                   const float point[3],
                                   const float len,
                                   const short vno[3],
                                   const float fno[3],
                                   const float mask,
                                   const SculptVertRef vertex_index,
                                   const int thread_id);
extern PBVHNode *the_fake_node;
#  endif

namespace blender {
namespace sculpt {

typedef struct BrushSearchArgs {
  float *center;
  float radius;
  bool use_threads;
  bool use_original;
  void *userdata;
  const Brush *brush;
  SculptBrushTest *test;  // may be NULL, will be pulled from brush
  SculptBrushTestFn test_func;
} BrushSearchArgs;

class BMeshPBVH {
 public:
  BMeshPBVH()
  {
  }

  struct unique_verts_iter : public std::iterator<std::forward_iterator_tag, BMVert> {
   public:
    ~unique_verts_iter(){};

    unique_verts_iter(PBVHNode *node) : _node(node)
    {
      reset();
      loadPointers();
    }

    unique_verts_iter(const unique_verts_iter &a)
    {
      i = a.i;
      _i = a._i;
      index = a.index;
      vertex = a.vertex;
      co = a.co;
      no = a.no;
      mask = a.mask;
      _node = a._node;
      color = a.color;
      deprecated_vertref = a.deprecated_vertref;
    }

    unique_verts_iter()
    {
      i = _i = index = 0;
      vertex = nullptr;
      co = nullptr;
      no = nullptr;
      mask = nullptr;
      color = nullptr;
    }

    void loadPointers()
    {
      if (_i >= _node->bm_unique_verts->length) {
        vertex = nullptr;
        co = nullptr;
        no = nullptr;
      }
      else {
        vertex = reinterpret_cast<BMVert *>(_node->bm_unique_verts->elems[_i]);
      }

      mask = nullptr;

      if (vertex) {
        deprecated_vertref.i = reinterpret_cast<intptr_t>(vertex);
        co = vertex->co;
        no = vertex->no;
      }
    }

    inline unique_verts_iter &operator++()
    {
      i++;
      _i++;

      while (_i < _node->bm_unique_verts->length && _node->bm_unique_verts->elems[_i] == nullptr) {
        _i++;
      }

      loadPointers();

      return *this;
    }

    inline unique_verts_iter operator++(int)
    {
      unique_verts_iter tmp(*this);
      operator++();
      return tmp;
    }

    inline void reset()
    {
      _i = i = 0;
      while (_i < _node->bm_unique_verts->length && _node->bm_unique_verts->elems[_i] == nullptr) {
        _i++;
      }

      loadPointers();
    }

    inline unique_verts_iter begin()
    {
      unique_verts_iter ret = *this;

      ret.reset();

      return ret;
    }

    inline unique_verts_iter end()
    {
      unique_verts_iter ret;

      ret.vertex = nullptr;

      return ret;
    }

    inline bool operator==(const unique_verts_iter &b)
    {
      // detect comparison with end iterator
      return (!vertex && !b.vertex) || (_node == b._node && i == b.i);
    }

    inline bool operator!=(const unique_verts_iter &b)
    {
      return !(*this == b);
    }

    inline unique_verts_iter &operator*()
    {
      return *this;
    }

    SculptVertRef deprecated_vertref;

    int i;
    int index;
    BMVert *vertex;
    float *co;
    float *no;
    float *mask;
    float *color;

   private:
    PBVHNode *_node;
    int _i;
  };

  struct edges_round_vert_iter : public std::iterator<std::forward_iterator_tag, BMVert> {
   public:
    ~edges_round_vert_iter(){};

    edges_round_vert_iter(BMVert *v) : v(v)
    {
      e = v->e;
      _start_e = v->e;
      operator++();
    }

    edges_round_vert_iter(const edges_round_vert_iter &a)
    {
      this->v = a.v;
      this->e = a.e;
      this->_start_e = a._start_e;
    }

    inline edges_round_vert_iter &operator++()
    {
      e = e->v1 == v ? e->v1_disk_link.next : e->v2_disk_link.next;
      return *this;
    }

    inline edges_round_vert_iter operator++(int)
    {
      edges_round_vert_iter tmp(*this);

      operator++();

      return tmp;
    }

    inline void reset()
    {
      e = _start_e;
      operator++();
    }

    inline edges_round_vert_iter begin()
    {
      return edges_round_vert_iter(v);
    }

    inline edges_round_vert_iter end()
    {
      edges_round_vert_iter ret(v);
      ret.e = ret._start_e;

      return ret;
    }

    inline bool operator==(const edges_round_vert_iter &b)
    {
      return e == b.e;
    }

    inline bool operator!=(const edges_round_vert_iter &b)
    {
      return this->e != b.e;
    }

    inline BMEdge *operator*()
    {
      return e;
    }

    BMEdge *e;
    BMVert *v;

   private:
    BMEdge *_start_e;
  };

  struct brush_verts_iter : unique_verts_iter {
   public:
    ~brush_verts_iter(){};

    brush_verts_iter(SculptSession *ss,
                     PBVHNode *node,
                     SculptBrushTest *test,
                     SculptBrushTestFn test_func,
                     const Brush *brush,
                     float *center,
                     float radius,
                     int thread_id)
        : _node(node),
          brush(brush),
          radius_scale(1.0f),
          test(*test),
          test_func(test_func),
          _ss(ss),
          _thread_id(thread_id),
          unique_verts_iter(node)
    {
      copy_v3_v3((float *)this->center, (float *)center);
      this->radius = radius;
      this->radius_squared = radius * radius;
    }

    brush_verts_iter(const brush_verts_iter &a) : unique_verts_iter(a)
    {
      copy_v3_v3((float *)center, (float *)a.center);
      radius = a.radius;
      radius_squared = a.radius_squared;
      radius_scale = a.radius_scale;
      _thread_id = a._thread_id;

      fade = a.fade;
      _node = a._node;
      brush = a.brush;
      test = a.test;
      test_func = a.test_func;

      _ss = a._ss;
    }

    inline brush_verts_iter begin()
    {
      brush_verts_iter ret = *this;

      static_cast<unique_verts_iter *>(&ret)->reset();
      ret.skip_outside();

      ret.loadPointers();
      if (!ret.vertex) {
        ret.brush = nullptr;
      }
      // ret = ++ret;

      return ret;
    }

    void loadPointers()
    {
      if (!vertex) {
        brush = nullptr;
        fade = 0.0f;

        return;
      }

      fade = SCULPT_brush_strength_factor(_ss,
                                          brush,
                                          co,
                                          sqrtf(test.dist),
                                          NULL,
                                          no,
                                          mask ? *mask : 0.0f,
                                          deprecated_vertref,
                                          _thread_id);
    }

    brush_verts_iter(bool is_end)
    {
      brush = nullptr;
      vertex = nullptr;
    }

    inline brush_verts_iter end()
    {
      return brush_verts_iter(true);
    }

    inline brush_verts_iter operator++(int)
    {
      brush_verts_iter tmp = *this;
      operator++();
      return tmp;
    }

    inline brush_verts_iter &operator++()
    {
      unique_verts_iter::operator++();

      if (!brush || !vertex) {
        fade = 0.0f;
        vertex = nullptr;
        brush = nullptr;
        co = nullptr;
        no = nullptr;

        return *this;
      }

      skip_outside();

      if (!vertex) {
        brush = nullptr;
        return *this;
      }

      loadPointers();

      return *this;
    }

    inline bool operator==(const brush_verts_iter &b)
    {
      // detect comparison with end iterator
      if (!brush && !b.brush) {
        return true;
      }

      return false;
      // return unique_verts_iter::operator==(static_cast<const unique_verts_iter &>(b));
    }

    inline bool operator!=(const brush_verts_iter &b)
    {
      return !(*this == b);
    }

    inline brush_verts_iter &operator*()
    {
      return *this;
    }

    const Brush *brush;
    float center[3];
    float radius;
    float radius_scale;
    float radius_squared;

    float fade;

    SculptBrushTest test;
    SculptBrushTestFn test_func;

   private:
    inline void skip_outside()
    {
      while (vertex && !test_func(&test, co)) {
        unique_verts_iter::operator++();
      }
    }

    SculptSession *_ss;
    PBVHNode *_node;
    int _thread_id;
  };

  inline edges_round_vert_iter edgesOfVert(BMVert *v)
  {
    return edges_round_vert_iter(v);
  }

  inline unique_verts_iter forAllUniqueVerts(PBVHNode *node)
  {
    unique_verts_iter ret(node);

    return ret;
  }

  inline float *getVertexCo(BMVert *v)
  {
    return v->co;
  }

  const inline float *getVertexNormal(BMVert *v)
  {
    return v->no;
  }

  inline void setVertexNormal(BMVert *v, float *no)
  {
  }

  /*
  * SculptSession *ss,
                   PBVHNode *node,
                   SculptBrushTest test,
                   SculptBrushTestFn test_func,
                   Brush *brush,
                   float center[3],
                   float radius,
                   int thread_id)
  */
  inline void forVertsInRangeOfNodes(
      BrushSearchArgs *args,
      PBVHNode **nodes,
      int totnode,
      std::function<void(PBVHNode *node, brush_verts_iter biter, int node_i, void *userdata)>
          callback,
      std::function<void(PBVHNode *node, int node_i, void *userdata)> post_node_callback)
  {
    SculptSession *ss = _ss;

    /*SculptSession *ss,
                     PBVHNode *node,
                     SculptBrushTest test,
                     SculptBrushTestFn test_func,
                     Brush *brush,
                     float *center,
                     float radius,
                     int thread_id)
                     */

    blender::IndexRange range(0, totnode);
    blender::threading::parallel_for(
        range,
        5,
        [&args, &nodes, &ss, &callback, &post_node_callback](blender::IndexRange &subrange) {
          for (auto i : subrange) {
            brush_verts_iter biter(ss,
                                   nodes[i],
                                   args->test,
                                   args->test_func,
                                   args->brush,
                                   args->center,
                                   args->radius,
                                   (int)BLI_task_parallel_thread_id(NULL));

            callback(nodes[i], biter, (int)i, args->userdata);

            if (post_node_callback) {
              post_node_callback(nodes[i], (int)i, args->userdata);
            }
          }
        });
  }

  inline void forVertsInRange(
      BrushSearchArgs *args,
      std::function<void(brush_verts_iter biter, int node_i, void *userdata)> callback,
      std::function<void(PBVHNode *node, int node_i, void *userdata)> node_callback)
  {
    PBVHNode **nodes = NULL;
    int totnode = 0;

    SculptBrushTest default_test;

    if (!args->test) {
      args->test = &default_test;
      args->test_func = SCULPT_brush_test_init_with_falloff_shape(
          _ss, &default_test, /*args->brush->falloff_shape*/ 0);
    }

    SculptSearchSphereData data = {_sd,
                                   _ss,
                                   args->radius * args->radius,
                                   args->center,
                                   args->use_original,
                                   false,
                                   nullptr,
                                   nullptr};

    BKE_pbvh_search_gather(_pbvh, SCULPT_search_sphere_cb, &data, &nodes, &totnode);

    SculptSession *ss = _ss;

    /*SculptSession *ss,
                     PBVHNode *node,
                     SculptBrushTest test,
                     SculptBrushTestFn test_func,
                     Brush *brush,
                     float *center,
                     float radius,
                     int thread_id)
                     */

    blender::IndexRange range(0, totnode);
    blender::threading::parallel_for(
        range, 1, [&args, &nodes, &ss, &callback, &node_callback](blender::IndexRange &subrange) {
          for (auto i : subrange) {
            brush_verts_iter biter(ss,
                                   nodes[i],
                                   args->test,
                                   args->test_func,
                                   args->brush,
                                   args->center,
                                   args->radius,
                                   (int)i);

            callback(biter, (int)i, args->userdata);

            if (node_callback) {
              node_callback(nodes[i], (int)i, args->userdata);
            }
          }
        });
  }

  void setSculptSession(SculptSession *ss)
  {
    _ss = ss;
  }

 private:
  BMesh *_bm;
  PBVH *_pbvh;
  SculptSession *_ss;
  Sculpt *_sd;
};

class BMeshBackend {
 public:
  BMeshBackend()
  {
  }

 private:
};

/* clang-format off */

#if __cplusplus >= 202000
template<class PBVHClass, class V, class E, class F>
concept PBVHBackend = requires(PBVHClass b, V v){
  {(*(b.forAllUniqueVerts(nullptr))).vertex} -> std::same_as<V>;
  {b.getVertexCo(v)} -> std::same_as<float*>;
  {b.getVertexNormal(v)} -> std::same_as<const float*>;
};
/* clang-format on */

template<class V, class E, class F, class Backend, PBVHBackend<V, E, F> PBVHClass>
#  else
template<class V, class E, class F, class Backend, class PBVHClass>
#  endif
class SculptImpl {
 public:
  PBVHClass *pbvh;
  SculptSession *ss;

  SculptImpl(SculptSession *ss, PBVHClass *pbvh) : pbvh(pbvh), ss(ss)
  {
  }

  inline void do_draw_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
  {
    Brush *brush = BKE_paint_brush(&sd->paint);
    float offset[3];
    const float bstrength = ss->cache->bstrength;

    /* Offset with as much as possible factored in already. */
    float effective_normal[3];
    SCULPT_tilt_effective_normal_get(ss, brush, effective_normal);
    mul_v3_v3fl(offset, effective_normal, ss->cache->radius);
    mul_v3_v3(offset, ss->cache->scale);
    mul_v3_fl(offset, bstrength);

    /* XXX: this shouldn't be necessary, but sculpting crashes in blender2.8 otherwise
     * initialize before threads so they can do curve mapping. */
    BKE_curvemapping_init(brush->curve);

    SculptBrushTest test;
    SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
        ss, &test, ss->cache->brush->falloff_shape);

    /*
    * float *center;
  float radius;
  bool use_threads;
  bool use_original;
  void *userdata;
  const Brush *brush;
  SculptBrushTest *test;  // may be NULL, will be pulled from brush
  SculptBrushTestFn test_func;
    */
    BrushSearchArgs args = {ss->cache->location,
                            ss->cache->radius,
                            true,
                            false,
                            NULL,
                            ss->cache->brush,
                            &test,
                            sculpt_brush_test_sq_fn};

    pbvh->forVertsInRangeOfNodes(
        &args,
        nodes,
        totnode,
        [&](PBVHNode *node, auto verts, int node_i, void *userdata) {
          float(*proxy)[3];

          proxy = BKE_pbvh_node_add_proxy(ss->pbvh, node)->co;
          bool modified = false;

          for (auto viter : verts) {
            float fade = viter.fade;

            mul_v3_v3fl(proxy[viter.i], offset, fade);
            modified = true;
          }
        },
        [](PBVHNode *node, int node_i, void *userdata) {});
  }
  /*
  &((BrushSearchArgs *){0})
  */
  inline void moveVerts(float cent[3], float radius, float offset[3])
  {
    BrushSearchArgs args = {cent, radius, true, false, nullptr /*ss->cache->brush*/};

    /* clang-format off */
    pbvh->forVertsInRange(
        &args,

        [&](auto verts, int node_i, void *userdata) {
          for (auto viter : verts) {
            //add offset to vertex coordinates
            for (auto e : pbvh->edgesOfVert(viter.vertex)) {
              BMVert *v2 = viter.vertex == e->v1 ? e->v2 : e->v1;

              madd_v3_v3fl(v2->co, offset, viter.fade);
            }
          
            madd_v3_v3fl(viter.co, offset, viter.fade);
            printf("test");
          }
        },

        [](PBVHNode *node, int node_i, void *userdata) {
          BKE_pbvh_node_mark_update(node);
        });

    /* clang-format on */
  }

 private:
};

}  // namespace sculpt
}  // namespace blender

#  ifdef COMPILER_EXPLORER

using BMeshSculpt = blender::sculpt::SculptImpl<BMVert *,
                                                BMEdge *,
                                                BMFace *,
                                                blender::sculpt::BMeshBackend,
                                                blender::sculpt::BMeshPBVH>;

BMeshSculpt *sculpt = new BMeshSculpt(NULL, NULL);

void test_cxsculpt()
{
  float dir[3] = {1.0f, 2.0f, 3.0f};
  float cent[3] = {0.0f, 0.0f, 0.0f};

  sculpt->moveVerts(cent, 5.0f, dir);
}
#  endif

#endif
