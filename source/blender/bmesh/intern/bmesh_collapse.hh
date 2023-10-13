#include "BLI_function_ref.hh"

struct BMVert;
struct BMEdge;
struct BMFace;
struct BMesh;

namespace blender::bmesh {
struct CollapseCallbacks {
  void *customdata;
  void (*on_vert_kill)(void *customdata, BMVert *) = nullptr;
  void (*on_edge_kill)(void *customdata, BMEdge *) = nullptr;
  void (*on_face_kill)(void *customdata, BMFace *) = nullptr;
  void (*on_vert_combine)(void *customdata, BMVert * /*dest*/, BMVert * /*source*/) = nullptr;
  void (*on_edge_combine)(void *customdata, BMEdge * /*dest*/, BMEdge * /*source*/) = nullptr;
  void (*on_vert_create)(void *customdata, BMVert *) = nullptr;
  void (*on_edge_create)(void *customdata, BMEdge *) = nullptr;
  void (*on_face_create)(void *customdata, BMFace *) = nullptr;
};

BMVert *join_vert_kill_edge(
    BMesh *bm, BMEdge *e, BMVert *v_del, const bool do_del, CollapseCallbacks *callbacks);
}  // namespace blender::bmesh
