#if 0
#  include "sculpt.hh"

typedef blender::sculpt::SculptImpl<BMVert *,
                                    BMEdge *,
                                    BMFace *,
                                    blender::sculpt::BMeshBackend,
                                    blender::sculpt::BMeshPBVH>
    BMeshSculpt;

BMeshSculpt *sculpt = new BMeshSculpt(NULL, NULL);

void test_cxsculpt()
{
  float dir[3] = {1.0f, 2.0f, 3.0f};
  float cent[3] = {0.0f, 0.0f, 0.0f};

  sculpt->moveVerts(cent, 5.0f, dir);
}
#endif
