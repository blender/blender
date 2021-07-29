"""
in draw_size        s   d=2.0    n=2
in matrices         m   d=[[]]   n=0
out obj_generated   o
"""

from mathutils import Matrix

scene = bpy.context.scene
objects = bpy.data.objects

idx = 0
for i, m in enumerate(matrices):
    if not m:
        continue
    mt_name = 'empty_sv.' + str("%04d" % i)
    if mt_name not in objects:
        mt = objects.new(mt_name, None)
        mt['origin'] = 'SNLite'
        mt['idx'] = i
        # mt.location = (0, 2, 1.2*i )
        scene.objects.link(mt)
        scene.update()
    else:
        mt = objects[mt_name]
    mt.empty_draw_size = draw_size
    mt.matrix_world = Matrix(m)
    idx = i

for obj in objects:
    if obj.get('origin') == 'SNLite' and obj.get('idx') > idx:
        scene.objects.unlink(obj)
        objects.remove(obj, do_unlink=True)

obj_generated = [o for o in objects if o.get('origin') == 'SNLite']