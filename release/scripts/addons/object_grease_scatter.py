# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8-80 compliant>

# Script copyright (C) Campbell Barton

bl_info = {
    "name": "Grease Scatter Objects",
    "author": "Campbell Barton",
    "version": (0, 1),
    "blender": (2, 58, 0),
    "location": "3D View, Add Mesh",
    "description": "Scatter a group of objects onto the active mesh using "
                   "the grease pencil lines",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Object/Grease_Scatter",
    "support": 'OFFICIAL',
    "category": "Object",
}

from mathutils import Vector, Matrix, Quaternion
from random import uniform, shuffle
import bpy


def _main(self,
          obj,
          group,
          DENSITY=1.0,
          SCALE=0.6,
          RAND_LOC=0.8,
          RAND_ALIGN=0.75,
          ):

    from math import radians, pi

    # OFS = 0.2
    SEEK = 2.0  # distance for ray to seek
    BAD_NORMAL = Vector((0.0, 0.0, -1.0))
    WALL_LIMIT = radians(45.0)

    mats = (Matrix.Rotation(radians(-45), 3, 'X'),
            Matrix.Rotation(radians(+45), 3, 'X'),
            Matrix.Rotation(radians(-45), 3, 'Y'),
            Matrix.Rotation(radians(+45), 3, 'Y'),
            Matrix.Rotation(radians(-45), 3, 'Z'),
            Matrix.Rotation(radians(+45), 3, 'Z'),
            )

    Z_UP = Vector((0.0, 0.0, 1.0))
    Y_UP = Vector((0.0, 1.0, 0.0))

    if not group:
        self.report({'WARNING'}, "Group '%s' not found" % obj.name)
        return

    def debug_edge(v1, v2):
        mesh = bpy.data.meshes.new("Retopo")
        mesh.from_pydata([v1, v2], [(0.0, 1.0)], [])

        scene = bpy.context.scene
        mesh.update()
        obj_new = bpy.data.objects.new("Torus", mesh)
        scene.objects.link(obj_new)

    ray = obj.ray_cast
    closest_point_on_mesh = obj.closest_point_on_mesh

    obj_mat = obj.matrix_world.copy()
    obj_mat_inv = obj_mat.inverted()
    # obj_quat = obj_mat.to_quaternion()
    # obj_quat_inv = obj_mat_inv.to_quaternion()

    DEBUG = False

    def fix_point(p):
        ok, hit, no, ind = closest_point_on_mesh(obj_mat_inv * p)
        if ok:
            if DEBUG:
                return [p, no, None]
            else:
                # print("good", hit, no)
                return [hit, no, None]

        # worry!
        print("bad!", p, BAD_NORMAL)

        return [p, BAD_NORMAL, None]

    def get_points(stroke):
        return [fix_point(point.co) for point in stroke.points]

    def get_splines(gp):
        if gp.layers.active:
            frame = gp.layers.active.active_frame
            return [get_points(stroke) for stroke in frame.strokes]
        else:
            return []

    def main():
        scene = bpy.context.scene
        obj = bpy.context.object

        gp = None

        if obj:
            gp = obj.grease_pencil

        if not gp:
            gp = scene.grease_pencil

        if not gp:
            self.report({'WARNING'}, "No grease pencil layer found")
            return

        splines = get_splines(gp)

        for s in splines:
            for pt in s:
                p = pt[0]
                n = pt[1]
                # print(p, n)
                if n is BAD_NORMAL:
                    continue

                # # dont self intersect
                best_nor = None
                #best_hit = None
                best_dist = 10000000.0
                pofs = p + n * 0.01

                n_seek = n * SEEK
                m_alt_1 = Matrix.Rotation(radians(22.5), 3, n)
                m_alt_2 = Matrix.Rotation(radians(-22.5), 3, n)
                for _m in mats:
                    for m in (_m, m_alt_1 * _m, m_alt_2 * _m):
                        pdir = m * n_seek
                        ok, hit, nor, ind = ray(pofs, pdir, best_dist)
                        if ok:
                            best_dist = (pofs - hit).length
                            best_nor = nor
                            # best_hit = hit

                if best_nor:
                    pt[1].length = best_dist
                    best_nor.negate()
                    pt[2] = best_nor

                    #scene.cursor_location[:] = best_hitnyway
                    # bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP',
                    #                         iterations=1)
                    # debug_edge(p, best_hit)
                    # p[:] = best_hit

        # Now we need to do scattering.
        # first corners
        hits = []
        nors = []
        oris = []
        for s in splines:
            # point, normal, n_other the closest hit normal
            for p, n, n_other in s:
                if n is BAD_NORMAL:
                    continue
                if n_other:
                    # cast vectors twice as long as the distance
                    # needed just in case.
                    n_down = (n * -SEEK)
                    l = n_down.length
                    n_other.length = l

                    vantage = p + n
                    if DEBUG:
                        p[:] = vantage

                    # We should cast rays between n_down and n_other
                    #for f in (0.0, 0.2, 0.4, 0.6, 0.8, 1.0):
                    TOT = int(10 * DENSITY)
                    #for i in list(range(TOT)):
                    for i in list(range(TOT))[int(TOT / 1.5):]:  # second half
                        f = i / (TOT - 1)

                        # focus on the center
                        '''
                        f -= 0.5
                        f = f*f
                        f += 0.5
                        '''

                        ntmp = f * n_down + (1.0 - f) * n_other
                        # randomize
                        ntmp.x += uniform(-l, l) * RAND_LOC
                        ntmp.y += uniform(-l, l) * RAND_LOC
                        ntmp.z += uniform(-l, l) * RAND_LOC

                        ok, hit, hit_no, ind = ray(vantage, ntmp, ntmp.length)
                        # print(hit, hit_no)
                        if ok:
                            if hit_no.angle(Z_UP) < WALL_LIMIT:
                                hits.append(hit)
                                nors.append(hit_no)
                                oris.append(n_other.cross(hit_no))
                                #oris.append(n_other)

        if 0:
            mesh = bpy.data.meshes.new("ScatterDupliFace")
            mesh.from_pydata(hits, [], [])

            scene = bpy.context.scene
            mesh.update()
            obj_new = bpy.data.objects.new("ScatterPar", mesh)
            scene.objects.link(obj_new)
            obj_new.layers[:] = obj.layers

            # Now setup dupli-faces
            obj_new.dupli_type = 'VERTS'
            ob_child = bpy.data.objects["trash"]
            ob_child.location = obj_new.location
            ob_child.parent = obj_new
        else:

            def apply_faces(triples):
                # first randomize the faces
                shuffle(triples)

                obs = group.objects[:]
                tot = len(obs)
                tot_div = int(len(triples) / tot)

                for inst_ob in obs:
                    triple_sub = triples[0:tot_div]
                    triples[0:tot_div] = []

                    vv = [tuple(v) for f in triple_sub for v in f]

                    mesh = bpy.data.meshes.new("ScatterDupliFace")
                    mesh.from_pydata(vv, [], [(i * 3, i * 3 + 1, i * 3 + 2)
                                              for i in range(len(triple_sub))])

                    scene = bpy.context.scene
                    mesh.update()
                    obj_new = bpy.data.objects.new("ScatterPar", mesh)

                    scene.objects.link(obj_new)
                    obj_new.layers[:] = obj.layers

                    # Now setup dupli-faces
                    obj_new.dupli_type = 'FACES'
                    obj_new.use_dupli_faces_scale = True
                    obj_new.dupli_faces_scale = 100.0

                    inst_ob.location = 0.0, 0.0, 0.0
                    inst_ob.parent = obj_new

                    # align the object with worldspace
                    obj_new.matrix_world = obj_mat

                    # BGE settings for testing
                    '''
                    inst_ob.game.physics_type = 'RIGID_BODY'
                    inst_ob.game.use_collision_bounds = True
                    inst_ob.game.collision_bounds = 'TRIANGLE_MESH'
                    inst_ob.game.collision_margin = 0.1
                    obj_new.select = True
                    '''

            # build faces from vert/normals
            tri = (Vector((0.0, 0.0, 0.01)),
                   Vector((0.0, 0.0, 0.0)),
                   Vector((0.0, 0.01, 0.01)))

            coords = []
            # face_ind = []
            for i in range(len(hits)):
                co = hits[i]
                no = nors[i]
                ori = oris[i]
                quat = no.to_track_quat('X', 'Z')

                # make 2 angles and blend
                angle = uniform(-pi, pi)
                angle_aligned = -(ori.angle(quat * Y_UP, pi))

                quat = Quaternion(no,
                                  (angle * (1.0 - RAND_ALIGN)) +
                                  (angle_aligned * RAND_ALIGN)
                                  ).cross(quat)

                f = uniform(0.1, 1.2) * SCALE

                coords.append([co + (quat * (tri[0] * f)),
                               co + (quat * (tri[1] * f)),
                               co + (quat * (tri[2] * f)),
                               ])

            apply_faces(coords)

    main()


from bpy.props import FloatProperty,  StringProperty


class Scatter(bpy.types.Operator):
    """"""
    bl_idname = "object.scatter"
    bl_label = "Grease Pencil Scatter"

    density = FloatProperty(
            name="Density",
            description="Multiplier for the density of items",
            default=1.0, min=0.01, max=10.0,
            )
    scale = FloatProperty(
            name="Scale",
            description="Size multiplier for duplifaces",
            default=1.0, min=0.01, max=10.0,
            )
    rand_align = FloatProperty(
            name="Random Align",
            description="Randomize alignment with the walls",
            default=0.75, min=0.0, max=1.0,
            )
    rand_loc = FloatProperty(
            name="Random Loc",
            description="Randomize placement",
            default=0.75, min=0.0, max=1.0,
            )
    # XXX, should not be a string - TODO, add a way for scritps to select ID's
    group = StringProperty(
            name="Group",
            description=("Group name to use for object placement, "
                         "defaults to object name when that matches a group"))

    def execute(self, context):
        obj = bpy.context.object
        group = bpy.data.groups.get(self.group)

        if not group:
            self.report({'ERROR'}, "Group %r not found" % self.group)
            return {'CANCELLED'}

        _main(self,
              obj,
              group,
              DENSITY=self.density,
              SCALE=self.scale,
              RAND_LOC=self.rand_loc,
              RAND_ALIGN=self.rand_align,
              )
        return {'FINISHED'}

    def check(self, context):
        if self.group not in bpy.data.groups:
            self.group = ""
            return True
        return False

    def invoke(self, context, event):

        # useful to initialize, take a guess
        if not self.group and context.object.name in bpy.data.groups:
            self.group = context.object.name

        wm = context.window_manager
        wm.invoke_props_dialog(self, width=180)
        return {'RUNNING_MODAL'}


def menu_func(self, context):
    self.layout.operator(Scatter.bl_idname, icon='AUTO')


def register():
    bpy.utils.register_class(Scatter)
    bpy.types.INFO_MT_mesh_add.append(menu_func)


def unregister():
    bpy.utils.unregister_class(Scatter)
    bpy.types.INFO_MT_mesh_add.remove(menu_func)

#if __name__ == "__main__":
#    _main()
