/* NotForPR: This is a proposed new vertex iterator for brushes.
 * It exists in this branch to debug threading problems
 * (it builds a list of verts under the brush first and calls
 *  parallel_for on it, so it's much more efficient at allocating
 *  to threads--basically it lets us test if worker malallocation
 *  is the root cause of a performance problem).
 *
 * Delete this file before committing to master; a seperate PR
 * will be created for this.
 */
#pragma once

#include "BLI_array.hh"
#include "BLI_index_range.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_span.hh"
#include "BLI_task.hh"
#include "BLI_threads.h"
#include "BLI_vector.hh"
#include "BLI_timeit.hh"

#include "BKE_pbvh.h"
#include "intern/pbvh_intern.hh"

#include <functional>
#include <utility>

namespace blender::bke::pbvh {
template<typename NodeData> struct VertexRange {
  struct iterator {
    PBVHVertRef vertex;
    float *co, *no, *mask, *color;
    const float *origco, *origno;
    const float *origcolor, *origmask;
    PBVHNode *node;
    NodeData *userdata;
    bool is_mesh;

    iterator(IndexRange _range, VertexRange &_owner, int _i) : range(_range), owner(_owner), i(_i)
    {
      is_mesh = owner.pbvh->header.type == PBVH_FACES;
      if (_i >= owner.range.start() && _i <= owner.range.last()) {
        load_data();
      }
    }

    iterator(const iterator &b) : range(b.range), owner(b.owner), i(b.i) {}

    iterator &operator*()
    {
      return *this;
    }

    bool operator==(const iterator &b)
    {
      return b.i == i;
    }

    bool operator!=(const iterator &b)
    {
      return b.i != i;
    }

    iterator &operator++()
    {
      i++;
      load_data();
      return *this;
    }

    void load_data()
    {
      if (i == range.start() + range.size()) {
        return;
      }

      node = owner.nodes[owner.verts[i].second];
      vertex = owner.verts[i].first;
      userdata = &owner.node_data[owner.verts[i].second];

      switch (BKE_pbvh_type(owner.pbvh)) {
        case PBVH_BMESH: {
          BMVert *v = reinterpret_cast<BMVert *>(vertex.i);

          co = v->co;
          no = v->no;

          mask = owner.pbvh->cd_vert_mask_offset ?
                     BM_ELEM_CD_PTR<float *>(v, owner.pbvh->cd_vert_mask_offset) :
                     nullptr;

          break;
        }
        case PBVH_FACES:
          mask = owner.vert_mask ? owner.vert_mask + vertex.i : nullptr;
          co = owner.pbvh->vert_positions[vertex.i];
          no = owner.pbvh->vert_normals[vertex.i];

          break;
        case PBVH_GRIDS:
          break;
      }
    }

   private:
    int i;
    VertexRange &owner;
    IndexRange range;
  };

  PBVH *pbvh;
  Span<std::pair<PBVHVertRef, int>> verts;
  IndexRange range;
  float *vert_mask;
  MutableSpan<NodeData> node_data;
  Span<PBVHNode *> nodes;

  VertexRange(PBVH *_pbvh,
              Span<std::pair<PBVHVertRef, int>> _verts,
              Span<PBVHNode *> _nodes,
              MutableSpan<NodeData> _node_data,
              IndexRange _range)
      : pbvh(_pbvh), verts(_verts), range(_range), nodes(_nodes), node_data(_node_data)
  {
    if (BKE_pbvh_type(pbvh) == PBVH_FACES) {
      vert_mask = static_cast<float *>(
          CustomData_get_layer_for_write(pbvh->vdata, CD_PAINT_MASK, pbvh->totvert));
    }
  }

  iterator begin()
  {
    return iterator(range, *this, range.start());
  }

  iterator end()
  {
    return iterator(range, *this, range.start() + range.size());
  }
};

struct ForEachThreadData {
  Vector<blender::IndexRange> *ranges;
  std::function<void(blender::IndexRange range)> visit;
};

static void *foreach_thread_task(void *userdata)
{
  ForEachThreadData *data = static_cast<ForEachThreadData *>(userdata);
  for (int i : data->ranges->index_range()) {
    data->visit((*data->ranges)[i]);
  }

  return nullptr;
}

template<typename NodeData>
void brush_vertex_iter(
    PBVH *pbvh,
    Span<PBVHNode *> nodes,
    bool threaded,
    std::function<bool(PBVHVertRef vertex, const float *co, const float *no, const float mask)>
        filter_verts,
    std::function<NodeData(PBVHNode *node)> node_visit_pre,
    std::function<void(VertexRange<NodeData> range)> visit,
    std::function<void(PBVHNode *node)> node_visit_post)
{
  //SCOPED_TIMER(__func__);

  int vert_count = 0;

  Array<NodeData> node_data(nodes.size());
  Array<bool> used_nodes(nodes.size());

  Vector<std::pair<PBVHVertRef, int>> verts;

  int vertex_i = 0;
  for (int i : nodes.index_range()) {
    PBVHNode *node = nodes[i];

    bool used = false;

    PBVHVertexIter vd;
    BKE_pbvh_vertex_iter_begin (pbvh, node, vd, PBVH_ITER_UNIQUE) {
      if (filter_verts(vd.vertex, vd.co, vd.fno, vd.mask ? *vd.mask : 0.0f)) {
        verts.append(std::make_pair(vd.vertex, i));
        used = true;
      }
    }
    BKE_pbvh_vertex_iter_end;

    used_nodes[i] = used;
  }

  //printf("threaded: %s\n", threaded ? "true" : "false");

  if (threaded) {
    threading::parallel_for_each(nodes.index_range(), [&](int i) {
      //
      if (used_nodes[i]) {
        node_data[i] = node_visit_pre(nodes[i]);
      }
    });
  }
  else {
    for (int i : nodes.index_range()) {
      if (used_nodes[i]) {
        node_data[i] = node_visit_pre(nodes[i]);
      }
    }
  }

  int thread_count = BLI_system_thread_count();
  const int grain_size = 32; //max_ii(verts.size() / thread_count / 4, 4);
  //printf("verts size:%d (%d)\n", verts.size(), verts.size() / grain_size);

  if (!threaded) {
    visit(VertexRange<NodeData>(pbvh, verts, nodes, node_data, verts.index_range()));
  }
  else {
#if 1
    threading::parallel_for(verts.index_range(), grain_size, [&](IndexRange range) {
      visit(VertexRange<NodeData>(pbvh, verts, nodes, node_data, range));
    });
#else
    int threads_count = BLI_system_thread_count();
    Array<Vector<IndexRange>> ranges(threads_count);

    int i = 0;
    int j = 0;
    IndexRange vrange = verts.index_range();
    printf("\n");
    while (i < verts.size()) {
      IndexRange range = vrange.slice(i, min_ii(grain_size, verts.size() - i));

      // printf("%d %d: %d\n", range.start(), range.one_after_last(), range.size());

      ranges[j].append(range);

      // printf("%d: %d\n", i, min_ii(i + grain_size, verts.size()));
      j = (j + 1) % threads_count;
      i += grain_size;
    }

    ListBase threads;
    BLI_threadpool_init(&threads, foreach_thread_task, threads_count);
    Array<ForEachThreadData> tdata(threads_count);

    auto visit2 = [&](IndexRange range) {
      visit(VertexRange<NodeData>(pbvh, verts, nodes, node_data, range));
    };

    for (int i = 0; i < threads_count; i++) {
      if (ranges[i].size() == 0) {
        continue;
      }

      tdata[i].ranges = &ranges[i];
      tdata[i].visit = visit2;

      BLI_threadpool_insert(&threads, &tdata[i]);
    }

    BLI_threadpool_end(&threads);
#endif
  }

  for (PBVHNode *node : nodes) {
    node_visit_post(node);
  }
}
}  // namespace blender::bke::pbvh
