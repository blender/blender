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

# --------------------------------- UV to MESH ------------------------------- #
# -------------------------------- version 0.1.1 ----------------------------- #
#                                                                              #
# Create a new Mesh based on active UV                                         #
#                                                                              #
#                        (c)   Alessandro Zomparelli                           #
#                                    (2017)                                    #
#                                                                              #
# http://www.co-de-it.com/                                                     #
#                                                                              #
# ############################################################################ #

bl_info = {
    "name": "UV to Mesh",
    "author": "Alessandro Zomparelli (Co-de-iT)",
    "version": (0, 1, 1),
    "blender": (2, 7, 9),
    "location": "",
    "description": "Create a new Mesh based on active UV",
    "warning": "",
    "wiki_url": "",
    "category": "Mesh"}


import bpy
import math
from bpy.types import Operator
from bpy.props import BoolProperty
from mathutils import Vector


class uv_to_mesh(Operator):
    bl_idname = "object.uv_to_mesh"
    bl_label = "UV to Mesh"
    bl_description = ("Create a new Mesh based on active UV")
    bl_options = {'REGISTER', 'UNDO'}

    apply_modifiers = BoolProperty(
            name="Apply Modifiers",
            default=False,
            description="Apply object's modifiers"
            )
    vertex_groups = BoolProperty(
            name="Keep Vertex Groups",
            default=False,
            description="Transfer all the Vertex Groups"
            )
    materials = BoolProperty(
            name="Keep Materials",
            default=True,
            description="Transfer all the Materials"
            )
    auto_scale = BoolProperty(
            name="Resize",
            default=True,
            description="Scale the new object in order to preserve the average surface area"
            )

    def execute(self, context):
        bpy.ops.object.mode_set(mode='OBJECT')
        for o in bpy.data.objects:
            o.select = False
        bpy.context.object.select = True

        if self.apply_modifiers:
            bpy.ops.object.duplicate_move()
            bpy.ops.object.convert(target='MESH')
        ob0 = bpy.context.object

        me0 = ob0.to_mesh(bpy.context.scene,
                    apply_modifiers=self.apply_modifiers, settings='PREVIEW')
        area = 0

        verts = []
        faces = []
        face_materials = []
        for face in me0.polygons:
            area += face.area
            uv_face = []
            store = False
            try:
                for loop in face.loop_indices:
                    uv = me0.uv_layers.active.data[loop].uv
                    if uv.x != 0 and uv.y != 0:
                        store = True
                    new_vert = Vector((uv.x, uv.y, 0))
                    verts.append(new_vert)
                    uv_face.append(loop)
                if store:
                    faces.append(uv_face)
                    face_materials.append(face.material_index)
            except:
                self.report({'ERROR'}, "Missing UV Map")

                return {'CANCELLED'}

        name = ob0.name + 'UV'
        # Create mesh and object
        me = bpy.data.meshes.new(name + 'Mesh')
        ob = bpy.data.objects.new(name, me)

        # Link object to scene and make active
        scn = bpy.context.scene
        scn.objects.link(ob)
        scn.objects.active = ob
        ob.select = True

        # Create mesh from given verts, faces.
        me.from_pydata(verts, [], faces)
        # Update mesh with new data
        me.update()
        if self.auto_scale:
            new_area = 0
            for p in me.polygons:
                new_area += p.area
            if new_area == 0:
                self.report({'ERROR'}, "Impossible to generate mesh from UV")

                return {'CANCELLED'}

        # VERTEX GROUPS
        if self.vertex_groups:
            try:
                for group in ob0.vertex_groups:
                    index = group.index
                    ob.vertex_groups.new(group.name)
                    for p in me0.polygons:
                        for vert, loop in zip(p.vertices, p.loop_indices):
                            ob.vertex_groups[index].add([loop], group.weight(vert), "ADD")
            except:
                pass

        ob0.select = False
        if self.auto_scale:
            scaleFactor = math.pow(area / new_area, 1 / 2)
            ob.scale = Vector((scaleFactor, scaleFactor, scaleFactor))

        bpy.ops.object.mode_set(mode='EDIT', toggle=False)
        bpy.ops.mesh.remove_doubles(threshold=1e-06)
        bpy.ops.object.mode_set(mode='OBJECT', toggle=False)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)

        # MATERIALS
        if self.materials:
            try:
                # assign old material
                uv_materials = [slot.material for slot in ob0.material_slots]
                for i in range(len(uv_materials)):
                    bpy.ops.object.material_slot_add()
                    bpy.context.object.material_slots[i].material = uv_materials[i]
                for i in range(len(ob.data.polygons)):
                    ob.data.polygons[i].material_index = face_materials[i]
            except:
                pass

        if self.apply_modifiers:
            bpy.ops.object.mode_set(mode='OBJECT')
            ob.select = False
            ob0.select = True
            bpy.ops.object.delete(use_global=False)
            ob.select = True
            bpy.context.scene.objects.active = ob

        return {'FINISHED'}


def register():
    bpy.utils.register_class(uv_to_mesh)


def unregister():
    bpy.utils.unregister_class(uv_to_mesh)


if __name__ == "__main__":
    register()
