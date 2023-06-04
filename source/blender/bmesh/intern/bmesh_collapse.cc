#include "bmesh_collapse.hh"

using blender::bmesh::NullCollapseCallbacks;

namespace blender::bmesh {
char *_last_local_obj = nullptr;
}

extern "C" BMVert *bmesh_kernel_join_vert_kill_edge(
    BMesh *bm, BMEdge *e, BMVert *v_kill, const bool do_del, const bool combine_flags)
{
  NullCollapseCallbacks callbacks;
  return blender::bmesh::join_vert_kill_edge<NullCollapseCallbacks>(
      bm, e, v_kill, do_del, combine_flags, callbacks);
}
