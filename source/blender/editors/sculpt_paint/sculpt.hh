#if 0
#  pragma once

/*

This is a proof of concept of how a C++ sculpt system could work.
It's a design study, not even a proposal.

We can't really use virtual-based polymorphism for performance reasons,
so the idea is to use templates and C++20's concepts instead.

*/

#  include "BKE_pbvh.h"
#  include "BLI_bitmap.h"
#  include "BLI_map.hh"
#  include "BLI_math.h"
#  include "BLI_mempool.h"
#  include "BLI_task.hh"
#  include "MEM_guardedalloc.h"

#  include "DNA_brush_enums.h"
#  include "DNA_brush_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_meshdata_types.h"
#  include "DNA_scene_types.h"

#  include "BKE_brush.h"
#  include "BKE_mesh.h"
#  include "BKE_object.h"
#  include "BKE_paint.h"
#  include "BKE_pbvh.h"

#  include "sculpt_intern.h"

#  include "bmesh.h"
#  include <concepts>
#  include <functional>
#  include <iterator>

/* clang-format off */
#include "../../blenkernel/intern/pbvh_intern.h"
/* clang-format on */

extern PBVHNode *the_fake_node;

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
      i = _i = 0;

      while (_i < _node->bm_unique_verts->length && _node->bm_unique_verts->elems[_i] == nullptr) {
        _i++;
      }
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
      color = a.color;
    }

    unique_verts_iter(bool is_end_iter)
    {
      i = _i = index = 0;
      vertex = nullptr;
      co = nullptr;
      no = nullptr;
      mask = nullptr;
      color = nullptr;
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

    inline unique_verts_iter &operator++()
    {
      i++;
      _i++;

      while (i < _node->bm_unique_verts->length && _node->bm_unique_verts->elems[_i] == nullptr) {
        _i++;
      }

      if (_i >= _node->bm_unique_verts->length) {
        vertex = NULL;
      }
      else {
        vertex = reinterpret_cast<BMVert *>(_node->bm_unique_verts->elems + _i);
      }

      if (vertex) {
        deprecated_vertref.i = reinterpret_cast<intptr_t>(vertex);
        co = vertex->co;
        no = vertex->no;
      }

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
    }

    inline unique_verts_iter begin()
    {
      unique_verts_iter ret = *this;

      ret.reset();

      return ret;
    }

    inline unique_verts_iter end()
    {
      return unique_verts_iter(true);
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

    inline unique_verts_iter operator*()
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
          radius_scale(radius_scale),
          test(*test),
          test_func(test_func),
          _ss(ss),
          _thread_id(thread_id)
    {
      copy_v3_v3(this->center, center);
      this->radius = radius;
      this->radius_squared = radius * radius;
    }

    brush_verts_iter(const brush_verts_iter &a)
    {
      copy_v3_v3(center, a.center);
      radius = a.radius;
      radius_squared = a.radius_squared;
      _node = a._node;
      brush = a.brush;

      _ss = a._ss;
    }

    brush_verts_iter(bool is_end)
    {
      brush = nullptr;
    }

    brush_verts_iter begin()
    {
      brush_verts_iter ret = *this;
      unique_verts_iter &viter = static_cast<unique_verts_iter &>(ret);

      viter.reset();

      return ret;
    }

    brush_verts_iter end()
    {
      return brush_verts_iter(true);
    }

    inline brush_verts_iter &operator++()
    {
      unique_verts_iter::operator++();

      skip_outside();

      if (!vertex) {
        brush = nullptr;
      }

      fade = SCULPT_brush_strength_factor(
          _ss, brush, co, 3, NULL, no, mask ? *mask : 0.0f, deprecated_vertref, _thread_id);

      return *this;
    }

    inline bool operator==(const brush_verts_iter &b)
    {
      // detect comparison with end iterator
      if (!brush && !b.brush) {
        return true;
      }

      return unique_verts_iter::operator==(static_cast<const unique_verts_iter &>(b));
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

    unique_verts_iter viter;

   private:
    inline void skip_outside()
    {
      while (vertex && !test_func(&test, co)) {
        unique_verts_iter::operator*();
      }
    }

    SculptSession *_ss;
    PBVHNode *_node;
    int _thread_id;
  };

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
  void forVertsInRange(
      BrushSearchArgs args,
      std::function<void(brush_verts_iter biter, int node_i, void *userdata)> callback,
      std::function<void(PBVHNode *node, int node_i, void *userdata)> node_callback)
  {
    PBVHNode **nodes = NULL;
    int totnode = 0;

    SculptBrushTest default_test;

    if (!args.test) {
      args.test = &default_test;
      args.test_func = SCULPT_brush_test_init_with_falloff_shape(
          _ss, &default_test, args.brush->falloff_shape);
    }

    SculptSearchSphereData data = {.ss = _ss,
                                   .sd = _sd,
                                   .radius_squared = args.radius * args.radius,
                                   .original = args.use_original,
                                   .center = args.center};

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
                                   args.test,
                                   args.test_func,
                                   args.brush,
                                   args.center,
                                   args.radius,
                                   (int)i);

            for (auto viter : biter) {
              callback(viter, (int)i, args.userdata);
            }

            if (node_callback) {
              node_callback(nodes[i], (int)i, args.userdata);
            }
          }
        });
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

template<class PBVHClass, class V, class E, class F>
concept PBVHBackend = requires(PBVHClass b, V v){
  {(*(b.forAllUniqueVerts(nullptr))).vertex} -> std::same_as<V>;
  {b.getVertexCo(v)} -> std::same_as<float*>;
  {b.getVertexNormal(v)} -> std::same_as<const float*>;
};
/* clang-format on */

template<class V, class E, class F, class Backend, PBVHBackend<V, E, F> PBVHClass>
class SculptImpl {
 public:
  PBVHClass *pbvh;
  SculptSession *ss;

  SculptImpl(SculptSession *ss, PBVHClass *pbvh) : pbvh(pbvh), ss(ss)
  {
  }

  /*
  &((BrushSearchArgs *){0})
  */
  inline void moveVerts(float cent[3], float radius, float offset[3])
  {
    /* clang-format off */
    pbvh->forVertsInRange(
        {
          .brush = ss->cache->brush,
          .radius = radius,
          .use_threads = true,
          .use_original = false
        },

        [&offset](auto viter, int node_i, void *userdata) {
          //add offset to vertex coordinates

          madd_v3_v3fl(viter.co, offset, viter.fade);
          printf("yay");
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
#endif
