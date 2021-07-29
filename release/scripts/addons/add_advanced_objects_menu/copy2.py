# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see http://www.gnu.org/licenses/
#  or write to the Free Software Foundation, Inc., 51 Franklin Street,
#  Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

bl_info = {
    "name": "Copy2 Vertices, Edges or Faces",
    "author": "Eleanor Howick (elfnor.com)",
    "version": (0, 1, 1),
    "blender": (2, 71, 0),
    "location": "3D View > Object > Copy 2",
    "description": "Copy one object to the selected vertices, edges or faces of another object",
    "warning": "",
    "category": "Object"
}

import bpy
from bpy.types import Operator
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatProperty,
        )
from mathutils import (
        Vector,
        Matrix,
        )


class Copy2(Operator):
    bl_idname = "mesh.copy2"
    bl_label = "Copy 2"
    bl_description = ("Copy Vertices, Edges or Faces to the Selected object\n"
                      "Needs an existing Active Mesh Object")
    bl_options = {"REGISTER", "UNDO"}

    obj_list = None

    def obj_list_cb(self, context):
        return Copy2.obj_list

    def sec_axes_list_cb(self, context):
        if self.priaxes == 'X':
            sec_list = [('Y', "Y", "Secondary axis Y"),
                        ('Z', "Z", "Secondary axis Z")]

        if self.priaxes == 'Y':
            sec_list = [('X', "X", "Secondary axis X"),
                        ('Z', "Z", "Secondary axis Z")]

        if self.priaxes == 'Z':
            sec_list = [('X', "X", "Secondary axis X"),
                        ('Y', "Y", "Secondary axis Y")]
        return sec_list

    copytype = EnumProperty(
            items=(('V', "Vertex",
                    "Paste the Copied Geometry to Vertices of the Active Object", 'VERTEXSEL', 0),
                   ('E', "Edge",
                    "Paste the Copied Geometry to Edges of the Active Object", 'EDGESEL', 1),
                   ('F', "Face",
                    "Paste the Copied Geometry to Faces of the Active Object", 'FACESEL', 2)),
            )
    copyfromobject = EnumProperty(
            name="Copy from",
            description="Copy an Object from the list",
            items=obj_list_cb
            )
    priaxes = EnumProperty(
            description="Primary axes used for Copied Object orientation",
            items=(('X', "X", "Along X"),
                   ('Y', "Y", "Along Y"),
                   ('Z', "Z", "Along Z")),
            )
    edgescale = BoolProperty(
            name="Scale to fill edge",
            default=False
            )
    secaxes = EnumProperty(
            name="Secondary Axis",
            description="Secondary axis used for Copied Object orientation",
            items=sec_axes_list_cb
            )
    scale = FloatProperty(
            name="Scale",
            default=1.0,
            min=0.0,
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj and obj.type == "MESH"

    def draw(self, context):
        layout = self.layout

        layout.prop(self, "copyfromobject")
        layout.label("to:")
        layout.prop(self, "copytype", expand=True)
        layout.label("Primary axis:")
        layout.prop(self, "priaxes", expand=True)
        layout.label("Secondary axis:")
        layout.prop(self, "secaxes", expand=True)
        if self.copytype == "E":
            layout.prop(self, "edgescale")
            if self.edgescale:
                layout.prop(self, "scale")
        return

    def execute(self, context):
        copytoobject = context.active_object.name
        axes = self.priaxes + self.secaxes

        # check if there is a problem with the strings related to some chars
        copy_to_object = bpy.data.objects[copytoobject] if \
                         copytoobject in bpy.data.objects else None

        copy_from_object = bpy.data.objects[self.copyfromobject] if \
                           self.copyfromobject in bpy.data.objects else None

        if copy_to_object is None or copy_from_object is None:
            self.report({"WARNING"},
                        "There was a problem with retrieving Object data. Operation Cancelled")
            return {"CANCELLED"}
        try:
            copy_to_from(
                    context.scene,
                    copy_to_object,
                    copy_from_object,
                    self.copytype,
                    axes,
                    self.edgescale,
                    self.scale
                    )
        except Exception as e:
            self.report({"WARNING"},
                        "Copy2 could not be completed (Check the Console for more info)")
            print("\n[Add Advanced Objects]\nOperator: mesh.copy2\n{}\n".format(e))

            return {"CANCELLED"}

        return {"FINISHED"}

    def invoke(self, context, event):
        Copy2.obj_list = [(obj.name, obj.name, obj.name) for obj in bpy.data.objects]

        return {"FINISHED"}


def copy_to_from(scene, to_obj, from_obj, copymode, axes, edgescale, scale):
    if copymode == 'V':
        vertex_copy(scene, to_obj, from_obj, axes)

    if copymode == 'E':
        # don't pass edgescalling to object types that cannot be scaled
        if from_obj.type in ["CAMERA", "LAMP", "EMPTY", "ARMATURE", "SPEAKER", "META"]:
            edgescale = False
        edge_copy(scene, to_obj, from_obj, axes, edgescale, scale)

    if copymode == 'F':
        face_copy(scene, to_obj, from_obj, axes)


axes_dict = {'XY': (1, 2, 0),
             'XZ': (2, 1, 0),
             'YX': (0, 2, 1),
             'YZ': (2, 0, 1),
             'ZX': (0, 1, 2),
             'ZY': (1, 0, 2)}


def copyto(scene, source_obj, pos, xdir, zdir, axes, scale=None):
    """
    copy the source_obj to pos, so its primary axis points in zdir and its
    secondary axis points in xdir
    """
    copy_obj = source_obj.copy()
    scene.objects.link(copy_obj)

    xdir = xdir.normalized()
    zdir = zdir.normalized()
    # rotation first
    z_axis = zdir
    x_axis = xdir
    y_axis = z_axis.cross(x_axis)
    # use axes_dict to assign the axis as chosen in panel
    A, B, C = axes_dict[axes]
    rot_mat = Matrix()
    rot_mat[A].xyz = x_axis
    rot_mat[B].xyz = y_axis
    rot_mat[C].xyz = z_axis
    rot_mat.transpose()

    # rotate object
    copy_obj.matrix_world = rot_mat

    # move object into position
    copy_obj.location = pos

    # scale object
    if scale is not None:
        copy_obj.scale = scale

    return copy_obj


def vertex_copy(scene, obj, source_obj, axes):
    # vertex select mode
    sel_verts = []
    copy_list = []

    for v in obj.data.vertices:
        if v.select is True:
            sel_verts.append(v)

    # make a set for each vertex. The set contains all the connected vertices
    # use sets so the list is unique
    vert_con = [set() for i in range(len(obj.data.vertices))]
    for e in obj.data.edges:
        vert_con[e.vertices[0]].add(e.vertices[1])
        vert_con[e.vertices[1]].add(e.vertices[0])

    for v in sel_verts:
        pos = v.co * obj.matrix_world.transposed()
        xco = obj.data.vertices[list(vert_con[v.index])[0]].co * obj.matrix_world.transposed()

        zdir = (v.co + v.normal) * obj.matrix_world.transposed() - pos
        zdir = zdir.normalized()

        edir = pos - xco

        # edir is nor perpendicular to z dir
        # want xdir to be projection of edir onto plane through pos with direction zdir
        xdir = edir - edir.dot(zdir) * zdir
        xdir = -xdir.normalized()

        copy = copyto(scene, source_obj, pos, xdir, zdir, axes)
        copy_list.append(copy)

    # select all copied objects
    for copy in copy_list:
        copy.select = True
    obj.select = False


def edge_copy(scene, obj, source_obj, axes, es, scale):
    # edge select mode
    sel_edges = []
    copy_list = []

    for e in obj.data.edges:
        if e.select is True:
            sel_edges.append(e)

    for e in sel_edges:
        # pos is average of two edge vertexs
        v0 = obj.data.vertices[e.vertices[0]].co * obj.matrix_world.transposed()
        v1 = obj.data.vertices[e.vertices[1]].co * obj.matrix_world.transposed()
        pos = (v0 + v1) / 2
        # xdir is along edge
        xdir = v0 - v1
        xlen = xdir.magnitude
        xdir = xdir.normalized()
        # project each edge vertex normal onto plane normal to xdir
        vn0 = (obj.data.vertices[e.vertices[0]].co * obj.matrix_world.transposed() +
               obj.data.vertices[e.vertices[0]].normal) - v0
        vn1 = (obj.data.vertices[e.vertices[1]].co * obj.matrix_world.transposed() +
               obj.data.vertices[e.vertices[1]].normal) - v1
        vn0p = vn0 - vn0.dot(xdir) * xdir
        vn1p = vn1 - vn1.dot(xdir) * xdir
        # the mean of the two projected normals is the zdir
        zdir = vn0p + vn1p
        zdir = zdir.normalized()
        escale = None
        if es:
            escale = Vector([1.0, 1.0, 1.0])
            i = list('XYZ').index(axes[1])
            escale[i] = scale * xlen / source_obj.dimensions[i]

        copy = copyto(scene, source_obj, pos, xdir, zdir, axes, scale=escale)
        copy_list.append(copy)

    # select all copied objects
    for copy in copy_list:
        copy.select = True
    obj.select = False


def face_copy(scene, obj, source_obj, axes):
    # face select mode
    sel_faces = []
    copy_list = []

    for f in obj.data.polygons:
        if f.select is True:
            sel_faces.append(f)

    for f in sel_faces:
        fco = f.center * obj.matrix_world.transposed()
        # get first vertex corner of transformed object
        vco = obj.data.vertices[f.vertices[0]].co * obj.matrix_world.transposed()
        # get face normal of transformed object
        fn = (f.center + f.normal) * obj.matrix_world.transposed() - fco
        fn = fn.normalized()

        copy = copyto(scene, source_obj, fco, vco - fco, fn, axes)
        copy_list.append(copy)

    # select all copied objects
    for copy in copy_list:
        copy.select = True
    obj.select = False


def register():
    bpy.utils.register_class(Copy2)


def unregister():
    bpy.utils.unregister_class(Copy2)


if __name__ == "__main__":
    register()
