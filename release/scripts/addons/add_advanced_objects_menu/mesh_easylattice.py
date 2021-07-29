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

bl_info = {
    "name": "Easy Lattice Object",
    "author": "Kursad Karatas",
    "version": (0, 6, 0),
    "blender": (2, 66, 0),
    "location": "View3D > Easy Lattice",
    "description": "Create a lattice for shape editing",
    "warning": "",
    "wiki_url": "https://wiki.blender.org/index.php/Easy_Lattice_Editing_Addon",
    "tracker_url": "https://bitbucket.org/kursad/blender_addons_easylattice/src",
    "category": "Mesh",
}


import bpy
from mathutils import (
    Matrix,
    Vector,
)
from bpy.types import Operator
from bpy.props import (
    EnumProperty,
    FloatProperty,
    IntProperty,
)


def createLattice(context, obj, props):
    # Create lattice and object
    lat = bpy.data.lattices.new('EasyLattice')
    ob = bpy.data.objects.new('EasyLattice', lat)

    # Take into consideration any selected vertices (default: all verticies)
    selectedVertices = createVertexGroup(obj)

    size, pos = findBBox(obj, selectedVertices)
    loc, rot = getTransformations(obj)

    # the position comes from the bbox
    ob.location = pos

    # the size from bbox * the incoming scale factor
    ob.scale = size * props[3]

    # the rotation comes from the combined obj world
    # matrix which was converted to euler pairs
    ob.rotation_euler = buildRot_World(obj)
    ob.show_x_ray = True

    # Link object to scene
    scn = context.scene

    # Take care of the local view
    base = scn.objects.link(ob)
    scn.objects.active = ob

    v3d = None
    if context.space_data and context.space_data.type == 'VIEW_3D':
        v3d = context.space_data

    if v3d and v3d.local_view:
        base.layers_from_view(v3d)

    scn.update()

    # Set lattice attributes
    lat.points_u = props[0]
    lat.points_v = props[1]
    lat.points_w = props[2]

    lat.interpolation_type_u = props[4]
    lat.interpolation_type_v = props[4]
    lat.interpolation_type_w = props[4]

    lat.use_outside = False

    return ob


def createVertexGroup(obj):
    vertices = obj.data.vertices
    selverts = []

    if obj.mode == "EDIT":
        bpy.ops.object.editmode_toggle()

    group = obj.vertex_groups.new("easy_lattice_group")

    for vert in vertices:
        if vert.select is True:
            selverts.append(vert)
            group.add([vert.index], 1.0, "REPLACE")

    # Default: use all vertices
    if not selverts:
        for vert in vertices:
            selverts.append(vert)
            group.add([vert.index], 1.0, "REPLACE")

    return selverts


def getTransformations(obj):
    rot = obj.rotation_euler
    loc = obj.location

    return [loc, rot]


def findBBox(obj, selvertsarray):

    mat = buildTrnScl_WorldMat(obj)
    mat_world = obj.matrix_world

    minx, miny, minz = selvertsarray[0].co
    maxx, maxy, maxz = selvertsarray[0].co

    c = 1

    for c in range(len(selvertsarray)):
        co = selvertsarray[c].co

        if co.x < minx:
            minx = co.x
        if co.y < miny:
            miny = co.y
        if co.z < minz:
            minz = co.z

        if co.x > maxx:
            maxx = co.x
        if co.y > maxy:
            maxy = co.y
        if co.z > maxz:
            maxz = co.z
        c += 1

    minpoint = Vector((minx, miny, minz))
    maxpoint = Vector((maxx, maxy, maxz))

    # The middle position has to be calculated based on the real world matrix
    pos = ((minpoint + maxpoint) / 2)

    minpoint = mat * minpoint    # Calculate only based on loc/scale
    maxpoint = mat * maxpoint    # Calculate only based on loc/scale
    pos = mat_world * pos        # the middle position has to be calculated based on the real world matrix

    size = maxpoint - minpoint
    size = Vector((max(0.1, abs(size.x)), max(0.1, abs(size.y)), max(0.1, abs(size.z)))) # Prevent zero size dimensions

    return [size, pos]


def buildTrnSclMat(obj):
    # This function builds a local matrix that encodes translation
    # and scale and it leaves out the rotation matrix
    # The rotation is applied at obejct level if there is any
    mat_trans = Matrix.Translation(obj.location)
    mat_scale = Matrix.Scale(obj.scale[0], 4, (1, 0, 0))
    mat_scale *= Matrix.Scale(obj.scale[1], 4, (0, 1, 0))
    mat_scale *= Matrix.Scale(obj.scale[2], 4, (0, 0, 1))

    mat_final = mat_trans * mat_scale

    return mat_final


def buildTrnScl_WorldMat(obj):
    # This function builds a real world matrix that encodes translation
    # and scale and it leaves out the rotation matrix
    # The rotation is applied at obejct level if there is any
    loc, rot, scl = obj.matrix_world.decompose()
    mat_trans = Matrix.Translation(loc)

    mat_scale = Matrix.Scale(scl[0], 4, (1, 0, 0))
    mat_scale *= Matrix.Scale(scl[1], 4, (0, 1, 0))
    mat_scale *= Matrix.Scale(scl[2], 4, (0, 0, 1))

    mat_final = mat_trans * mat_scale

    return mat_final


# Feature use
def buildRot_WorldMat(obj):
    # This function builds a real world matrix that encodes rotation
    # and it leaves out translation and scale matrices
    loc, rot, scl = obj.matrix_world.decompose()
    rot = rot.to_euler()

    mat_rot = Matrix.Rotation(rot[0], 4, 'X')
    mat_rot *= Matrix.Rotation(rot[1], 4, 'Z')
    mat_rot *= Matrix.Rotation(rot[2], 4, 'Y')
    return mat_rot


def buildTrn_WorldMat(obj):
    # This function builds a real world matrix that encodes translation
    # and scale and it leaves out the rotation matrix
    # The rotation is applied at obejct level if there is any
    loc, rot, scl = obj.matrix_world.decompose()
    mat_trans = Matrix.Translation(loc)

    return mat_trans


def buildScl_WorldMat(obj):
    # This function builds a real world matrix that encodes translation
    # and scale and it leaves out the rotation matrix
    # The rotation is applied at obejct level if there is any
    loc, rot, scl = obj.matrix_world.decompose()

    mat_scale = Matrix.Scale(scl[0], 4, (1, 0, 0))
    mat_scale *= Matrix.Scale(scl[1], 4, (0, 1, 0))
    mat_scale *= Matrix.Scale(scl[2], 4, (0, 0, 1))

    return mat_scale


def buildRot_World(obj):
    # This function builds a real world rotation values
    loc, rot, scl = obj.matrix_world.decompose()
    rot = rot.to_euler()

    return rot


def main(context, lat_props):
    obj = context.object

    if obj.type == "MESH":
        lat = createLattice(context, obj, lat_props)

        modif = obj.modifiers.new("EasyLattice", "LATTICE")
        modif.object = lat
        modif.vertex_group = "easy_lattice_group"

        bpy.ops.object.select_all(action='DESELECT')
        bpy.ops.object.select_pattern(pattern=lat.name, extend=False)
        context.scene.objects.active = lat

        context.scene.update()

    return


class EasyLattice(Operator):
    bl_idname = "object.easy_lattice"
    bl_label = "Easy Lattice Creator"
    bl_description = ("Create a Lattice modifier ready to edit\n"
                      "Needs an existing Active Mesh Object\n")

    lat_u = IntProperty(
            name="Lattice u",
            description="Points in u direction",
            default=3
            )
    lat_v = IntProperty(
            name="Lattice v",
            description="Points in v direction",
            default=3
            )
    lat_w = IntProperty(
            name="Lattice w",
            description="Points in w direction",
            default=3
            )
    lat_scale_factor = FloatProperty(
            name="Lattice scale factor",
            description="Adjustment to the lattice scale",
            default=1,
            min=0.1,
            step=1,
            precision=2
            )
    lat_types = (('KEY_LINEAR', "Linear", "Linear Interpolation type"),
                 ('KEY_CARDINAL', "Cardinal", "Cardinal Interpolation type"),
                 ('KEY_CATMULL_ROM', "Catmull-Rom", "Catmull-Rom Interpolation type"),
                 ('KEY_BSPLINE', "BSpline", "Key BSpline Interpolation Type")
                )
    lat_type = EnumProperty(
            name="Lattice Type",
            description="Choose Lattice Type",
            items=lat_types,
            default='KEY_BSPLINE'
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj is not None and obj.type == "MESH"

    def draw(self, context):
        layout = self.layout

        col = layout.column(align=True)
        col.prop(self, "lat_u")
        col.prop(self, "lat_v")
        col.prop(self, "lat_w")

        layout.prop(self, "lat_scale_factor")

        layout.prop(self, "lat_type")

    def execute(self, context):
        lat_u = self.lat_u
        lat_v = self.lat_v
        lat_w = self.lat_w

        lat_scale_factor = self.lat_scale_factor

        # enum property no need to complicate things
        lat_type = self.lat_type
        # XXX, should use keyword args
        lat_props = [lat_u, lat_v, lat_w, lat_scale_factor, lat_type]
        try:
            main(context, lat_props)

        except Exception as ex:
            print("\n[Add Advanced Objects]\nOperator:object.easy_lattice\n{}\n".format(ex))
            self.report(
                {'WARNING'},
                "Easy Lattice Creator could not be completed (See Console for more info)"
            )
            return {"CANCELLED"}

        return {"FINISHED"}

    def invoke(self, context, event):
        wm = context.window_manager
        return wm.invoke_props_dialog(self)


def register():
    bpy.utils.register_class(EasyLattice)


def unregister():
    bpy.utils.unregister_class(EasyLattice)


if __name__ == "__main__":
    register()
