# mangle_tools.py (c) 2011 Phil Cote (cotejrp1)

# ###### BEGIN GPL LICENSE BLOCK ######
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ###### END GPL LICENCE BLOCK ######

# Note: properties are moved into __init__

bl_info = {
    "name": "Mangle Tools",
    "author": "Phil Cote",
    "blender": (2, 71, 0),
    "location": "3D View > Toolshelf > Create > Mangle Tools",
    "description": "Set of tools to mangle curves, meshes, and shape keys",
    "warning": "",
    "wiki_url": "",
    "category": "Object"}


import bpy
import random
from bpy.types import (
        Operator,
        Panel,
        )
import time
from math import pi
import bmesh


def move_coordinate(context, co, is_curve=False):
    advanced_objects = context.scene.advanced_objects1
    xyz_const = advanced_objects.mangle_constraint_vector
    random.seed(time.time())
    multiplier = 1

    # For curves, we base the multiplier on the circumference formula.
    # This helps make curve changes more noticable.
    if is_curve:
        multiplier = 2 * pi
    random_mag = advanced_objects.mangle_random_magnitude
    if xyz_const[0]:
        co.x += .01 * random.randrange(-random_mag, random_mag) * multiplier
    if xyz_const[1]:
        co.y += .01 * random.randrange(-random_mag, random_mag) * multiplier
    if xyz_const[2]:
        co.z += .01 * random.randrange(-random_mag, random_mag) * multiplier


class MeshManglerOperator(Operator):
    bl_idname = "ba.mesh_mangler"
    bl_label = "Mangle Mesh"
    bl_description = ("Push vertices on the selected object around in random\n"
                      "directions to create a crumpled look")
    bl_options = {"REGISTER", "UNDO"}

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return ob is not None and ob.type == 'MESH'

    def execute(self, context):
        mesh = context.active_object.data
        bm = bmesh.new()
        bm.from_mesh(mesh)
        verts = bm.verts
        advanced_objects = context.scene.advanced_objects1
        randomMag = advanced_objects.mangle_random_magnitude
        random.seed(time.time())

        if mesh.shape_keys is not None:
            self.report({'INFO'},
                        "Cannot mangle mesh: Shape keys present. Operation Cancelled")
            return {'CANCELLED'}

        for vert in verts:
            xVal = .01 * random.randrange(-randomMag, randomMag)
            yVal = .01 * random.randrange(-randomMag, randomMag)
            zVal = .01 * random.randrange(-randomMag, randomMag)

            vert.co.x = vert.co.x + xVal
            vert.co.y = vert.co.y + yVal
            vert.co.z = vert.co.z + zVal

        del verts

        bm.to_mesh(mesh)
        mesh.update()

        return {'FINISHED'}


class AnimanglerOperator(Operator):
    bl_idname = "ba.ani_mangler"
    bl_label = "Mangle Shape Key"
    bl_description = ("Make a shape key and pushes the verts around on it\n"
                      "to set up for random pulsating animation")

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return ob is not None and ob.type in ['MESH', 'CURVE']

    def execute(self, context):
        scn = context.scene.advanced_objects1
        mangleName = scn.mangle_name
        ob = context.object
        shapeKey = ob.shape_key_add(name=mangleName)
        verts = shapeKey.data

        for vert in verts:
            move_coordinate(context, vert.co, is_curve=ob.type == 'CURVE')

        return {'FINISHED'}


class CurveManglerOp(Operator):
    bl_idname = "ba.curve_mangler"
    bl_label = "Mangle Curve"
    bl_description = "Mangle a curve to the degree the user specifies"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        ob = context.active_object
        return ob is not None and ob.type == "CURVE"

    def execute(self, context):
        ob = context.active_object
        if ob.data.shape_keys is not None:
            self.report({'INFO'},
                        "Cannot mangle curve. Shape keys present. Operation Cancelled")
            return {'CANCELLED'}

        splines = context.object.data.splines

        for spline in splines:
            if spline.type == 'BEZIER':
                points = spline.bezier_points
            elif spline.type in {'POLY', 'NURBS'}:
                points = spline.points

            for point in points:
                move_coordinate(context, point.co, is_curve=True)

        return {'FINISHED'}


class MangleToolsPanel(Panel):
    bl_label = "Mangle Tools"
    bl_space_type = "VIEW_3D"
    bl_context = "objectmode"
    bl_region_type = "TOOLS"
    bl_category = "Create"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        scn = context.scene.advanced_objects1
        obj = context.object

        if obj and obj.type in ['MESH']:
            layout = self.layout

            row = layout.row(align=True)
            row.prop(scn, "mangle_constraint_vector", toggle=True)

            col = layout.column()
            col.prop(scn, "mangle_random_magnitude")
            col.operator("ba.mesh_mangler")
            col.separator()

            col.prop(scn, "mangle_name")
            col.operator("ba.ani_mangler")
        else:
            layout = self.layout
            layout.label(text="Please select a Mesh Object", icon="INFO")


def register():
    bpy.utils.register_class(AnimanglerOperator)
    bpy.utils.register_class(MeshManglerOperator)
    bpy.utils.register_class(CurveManglerOp)
    bpy.utils.register_class(MangleToolsPanel)


def unregister():
    bpy.utils.unregister_class(AnimanglerOperator)
    bpy.utils.unregister_class(MeshManglerOperator)
    bpy.utils.unregister_class(MangleToolsPanel)
    bpy.utils.unregister_class(CurveManglerOp)


if __name__ == "__main__":
    register()
