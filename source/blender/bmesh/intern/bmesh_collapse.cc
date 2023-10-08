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

  return 0;
}

int bmesh_elem_check_all(void *elem, char htype)
{
  return bmesh_elem_check_all_intern(elem, htype);
}
#endif
}  // namespace blender::bmesh

extern "C" BMVert *bmesh_kernel_join_vert_kill_edge(BMesh *bm,
                                                    BMEdge *e,
                                                    BMVert *v_kill,
                                                    const bool do_del)
{
  NullCollapseCallbacks callbacks;
  return blender::bmesh::join_vert_kill_edge<NullCollapseCallbacks>(
      bm, e, v_kill, do_del, callbacks);
}
