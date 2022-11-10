#if 0
#  include "sculpt.hh"

using BMeshSculpt = blender::sculpt::SculptImpl<BMVert *,
                                                BMEdge *,
                                                BMFace *,
                                                blender::sculpt::BMeshBackend,
                                                blender::sculpt::BMeshPBVH>;

BMeshSculpt *bmesh_sculpt = new BMeshSculpt(NULL, new blender::sculpt::BMeshPBVH());

extern "C" {
void cxx_do_draw_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  bmesh_sculpt->ss = ob->sculpt;
  bmesh_sculpt->pbvh->setSculptSession(ob->sculpt);

  bmesh_sculpt->do_draw_brush(sd, ob, nodes, totnode);
}
}

void test_cxsculpt()
{
  float dir[3] = {1.0f, 2.0f, 3.0f};
  float cent[3] = {0.0f, 0.0f, 0.0f};

  bmesh_sculpt->moveVerts(cent, 5.0f, dir);
}
#endif
