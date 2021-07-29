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

# <pep8 compliant>

bl_info = {
    "name": "Inset Polygon",
    "author": "Howard Trickey",
    "version": (1, 0, 1),
    "blender": (2, 73, 0),
    "location": "View3D > Tools",
    "description": "Make an inset polygon inside selection.",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Modeling/Inset-Polygon",
    "category": "Mesh"}


if "bpy" in locals():
    import importlib
else:
    from . import (
            geom,
            model,
            offset,
            triquad,
            )

import math
import bpy
import bmesh
import mathutils
from bpy.props import (
        BoolProperty,
        EnumProperty,
        FloatProperty,
        )


class Inset(bpy.types.Operator):
    bl_idname = "mesh.insetpoly"
    bl_label = "Inset Polygon"
    bl_description = "Make an inset polygon inside selection"
    bl_options = {'REGISTER', 'UNDO'}

    inset_amount = FloatProperty(name="Amount",
        description="Amount to move inset edges",
        default=5.0,
        min=0.0,
        max=1000.0,
        soft_min=0.0,
        soft_max=100.0,
        unit='LENGTH')
    inset_height = FloatProperty(name="Height",
        description="Amount to raise inset faces",
        default=0.0,
        min=-10000.0,
        max=10000.0,
        soft_min=-500.0,
        soft_max=500.0,
        unit='LENGTH')
    region = BoolProperty(name="Region",
        description="Inset selection as one region?",
        default=True)
    scale = EnumProperty(name="Scale",
        description="Scale for amount",
        items=[
            ('PERCENT', "Percent",
                "Percentage of maximum inset amount"),
            ('ABSOLUTE', "Absolute",
                "Length in blender units")
            ],
        default='PERCENT')

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH' and context.mode == 'EDIT_MESH')

    def draw(self, context):
        layout = self.layout
        box = layout.box()
        box.label("Inset Options:")
        box.prop(self, "scale")
        box.prop(self, "inset_amount")
        box.prop(self, "inset_height")
        box.prop(self, "region")

    def invoke(self, context, event):
        self.action(context)
        return {'FINISHED'}

    def execute(self, context):
        self.action(context)
        return {'FINISHED'}

    def action(self, context):
        save_global_undo = bpy.context.user_preferences.edit.use_global_undo
        bpy.context.user_preferences.edit.use_global_undo = False
        obj = bpy.context.active_object
        mesh = obj.data
        do_inset(mesh, self.inset_amount, self.inset_height, self.region,
            self.scale == 'PERCENT')
        bpy.context.user_preferences.edit.use_global_undo = save_global_undo
        bpy.ops.object.editmode_toggle()
        bpy.ops.object.editmode_toggle()


def do_inset(mesh, amount, height, region, as_percent):
    if amount <= 0.0:
        return
    pitch = math.atan(height / amount)
    selfaces = []
    selface_indices = []
    bm = bmesh.from_edit_mesh(mesh)
    for face in bm.faces:
        if face.select:
            selfaces.append(face)
            selface_indices.append(face.index)
    m = geom.Model()
    # if add all mesh.vertices, coord indices will line up
    # Note: not using Points.AddPoint which does dup elim
    # because then would have to map vertices in and out
    m.points.pos = [v.co.to_tuple() for v in bm.verts]
    for f in selfaces:
        m.faces.append([loop.vert.index for loop in f.loops])
        m.face_data.append(f.index)
    orig_numv = len(m.points.pos)
    orig_numf = len(m.faces)
    model.BevelSelectionInModel(m, amount, pitch, True, region, as_percent)
    if len(m.faces) == orig_numf:
        # something went wrong with Bevel - just treat as no-op
        return
    blender_faces = m.faces[orig_numf:len(m.faces)]
    blender_old_face_index = m.face_data[orig_numf:len(m.faces)]
    for i in range(orig_numv, len(m.points.pos)):
        bvertnew = bm.verts.new(m.points.pos[i])
    bm.verts.index_update()
    bm.verts.ensure_lookup_table()
    new_faces = []
    start_faces = len(bm.faces)
    for i, newf in enumerate(blender_faces):
        vs = remove_dups([bm.verts[j] for j in newf])
        if len(vs) < 3:
            continue
        # copy face attributes from old face that it was derived from
        bfi = blender_old_face_index[i]
        if bfi and 0 <= bfi < start_faces:
            bm.faces.ensure_lookup_table()
            oldface = bm.faces[bfi]
            bfacenew = bm.faces.new(vs, oldface)
            # bfacenew.copy_from_face_interp(oldface)
        else:
            bfacenew = bm.faces.new(vs)
        new_faces.append(bfacenew)
    # deselect original faces
    for face in selfaces:
        face.select_set(False)
    # remove original faces
    bmesh.ops.delete(bm, geom=selfaces, context=5)  # 5 = DEL_FACES
    # select all new faces (should only select inner faces, but that needs more surgery on rest of code)
    for face in new_faces:
        face.select_set(True)

def remove_dups(vs):
    seen = set()
    return [x for x in vs if not (x in seen or seen.add(x))]

def panel_func(self, context):
    self.layout.label(text="Inset Polygon:")
    self.layout.operator("mesh.insetpoly", text="Inset Polygon")


def register():
    bpy.utils.register_class(Inset)
    bpy.types.VIEW3D_PT_tools_meshedit.append(panel_func)


def unregister():
    bpy.utils.unregister_class(Inset)
    bpy.types.VIEW3D_PT_tools_meshedit.remove(panel_func)


if __name__ == "__main__":
    register()
