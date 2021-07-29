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
    "name": "Constellation",
    "author": "Oscurart",
    "blender": (2, 67, 0),
    "location": "3D View > Toolshelf > Create > Constellation",
    "description": "Create a new Mesh From Selected",
    "warning": "",
    "wiki_url": "",
    "category": "Add Mesh"}

# Note the setting is moved to __init__ search for
# the adv_obj and advanced_objects patterns

import bpy
from bpy.props import FloatProperty
from math import sqrt
from bpy.types import (
        Operator,
        Panel,
        )


def VertDis(a, b):
    dst = sqrt(pow(a.co.x - b.co.x, 2) +
               pow(a.co.y - b.co.y, 2) +
               pow(a.co.z - b.co.z, 2))
    return(dst)


def OscConstellation(limit):
    actobj = bpy.context.object
    vertlist = []
    edgelist = []
    edgei = 0

    for ind, verta in enumerate(actobj.data.vertices[:]):
        for vertb in actobj.data.vertices[ind:]:
            if VertDis(verta, vertb) <= limit:
                vertlist.append(verta.co[:])
                vertlist.append(vertb.co[:])
                edgelist.append((edgei, edgei + 1))
                edgei += 2

    mesh = bpy.data.meshes.new("rsdata")
    obj = bpy.data.objects.new("rsObject", mesh)
    bpy.context.scene.objects.link(obj)
    mesh.from_pydata(vertlist, edgelist, [])


class Oscurart_Constellation(Operator):
    bl_idname = "mesh.constellation"
    bl_label = "Constellation"
    bl_description = ("Create a Constellation Mesh - Cloud of Vertices\n"
                      "Note: can produce a lot of geometry\n"
                      "Needs an existing Active Mesh Object")
    bl_options = {'REGISTER', 'UNDO'}

    limit = FloatProperty(
            name="Threshold",
            description="Edges will be created only if the distance\n"
                        "between vertices is smaller than this value",
            default=2,
            min=0
            )

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == "MESH")

    def invoke(self, context, event):
        adv_obj = context.scene.advanced_objects1
        self.limit = adv_obj.constellation_limit

        return self.execute(context)

    def draw(self, context):
        layout = self.layout

        layout.prop(self, "limit")

    def execute(self, context):
        try:
            OscConstellation(self.limit)
        except Exception as e:
            print("\n[Add Advanced Objects]\nOperator: mesh.constellation\n{}".format(e))

            self.report({"WARNING"},
                        "Constellation Operation could not be Completed (See Console for more Info)")

            return {"CANCELLED"}

        return {'FINISHED'}


class Constellation_Operator_Panel(Panel):
    bl_label = "Constellation"
    bl_region_type = "TOOLS"
    bl_space_type = "VIEW_3D"
    bl_options = {'DEFAULT_CLOSED'}
    bl_context = "objectmode"
    bl_category = "Create"

    def draw(self, context):
        layout = self.layout
        adv_obj = context.scene.advanced_objects1

        box = layout.box()
        col = box.column(align=True)
        col.label("Constellation:")
        col.operator("mesh.constellation", text="Cross Section")
        col.prop(adv_obj, "constellation_limit")


# Register
def register():
    bpy.utils.register_class(Oscurart_Constellation)
    bpy.utils.register_class(Constellation_Operator_Panel)


def unregister():
    bpy.utils.unregister_class(Oscurart_Constellation)
    bpy.utils.unregister_class(Constellation_Operator_Panel)


if __name__ == "__main__":
    register()
