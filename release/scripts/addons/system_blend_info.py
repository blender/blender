# scene_blend_info.py Copyright (C) 2010, Mariano Hidalgo
#
# Show Information About the Blend.
# ***** BEGIN GPL LICENSE BLOCK *****
#
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENCE BLOCK *****

bl_info = {
    "name": "Scene Information",
    "author": "uselessdreamer",
    "version": (0,3),
    "blender": (2, 59, 0),
    "location": "Properties > Scene > Blend Info Panel",
    "description": "Show information about the .blend",
    "warning": "",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/System/Blend Info",
    "category": "System",
}

import bpy


def quantity_string(quantity, text_single, text_plural, text_none=None):
    sep = " "

    if not text_none:
        text_none = text_plural

    if quantity == 0:
        string = str(quantity) + sep + text_none

    if quantity == 1:
        string = str(quantity) + sep + text_single

    if quantity >= 2:
        string = str(quantity) + sep + text_plural

    if quantity < 0:
        return None

    return string


class OBJECT_PT_blendinfo(bpy.types.Panel):
    bl_label = "Blend Info"
    bl_space_type = "PROPERTIES"
    bl_region_type = "WINDOW"
    bl_context = "scene"

    def draw(self, context):
        ob_cols = []
        db_cols = []

        objects = bpy.data.objects

        layout = self.layout

        # OBJECTS

        l_row = layout.row()
        num = len(bpy.data.objects)
        l_row.label(text=quantity_string(num, "Object", "Objects")
            + " in the scene:",
            icon='OBJECT_DATA')

        l_row = layout.row()
        ob_cols.append(l_row.column())
        ob_cols.append(l_row.column())

        row = ob_cols[0].row()
        meshes = [o for o in objects.values() if o.type == 'MESH']
        num = len(meshes)
        row.label(text=quantity_string(num, "Mesh", "Meshes"),
            icon='MESH_DATA')

        row = ob_cols[1].row()
        curves = [o for o in objects.values() if o.type == 'CURVE']
        num = len(curves)
        row.label(text=quantity_string(num, "Curve", "Curves"),
            icon='CURVE_DATA')

        row = ob_cols[0].row()
        cameras = [o for o in objects.values() if o.type == 'CAMERA']
        num = len(cameras)
        row.label(text=quantity_string(num, "Camera", "Cameras"),
            icon='CAMERA_DATA')

        row = ob_cols[1].row()
        lamps = [o for o in objects.values() if o.type == 'LAMP']
        num = len(lamps)
        row.label(text=quantity_string(num, "Lamp", "Lamps"),
            icon='LAMP_DATA')

        row = ob_cols[0].row()
        armatures = [o for o in objects.values() if o.type == 'ARMATURE']
        num = len(armatures)
        row.label(text=quantity_string(num, "Armature", "Armatures"),
            icon='ARMATURE_DATA')

        row = ob_cols[1].row()
        lattices = [o for o in objects.values() if o.type == 'LATTICE']
        num = len(lattices)
        row.label(text=quantity_string(num, "Lattice", "Lattices"),
            icon='LATTICE_DATA')

        row = ob_cols[0].row()
        empties = [o for o in objects.values() if o.type == 'EMPTY']
        num = len(empties)
        row.label(text=quantity_string(num, "Empty", "Empties"),
            icon='EMPTY_DATA')

        row = ob_cols[1].row()
        empties = [o for o in objects.values() if o.type == 'SPEAKER']
        num = len(empties)
        row.label(text=quantity_string(num, "Speaker", "Speakers"),
            icon='OUTLINER_OB_SPEAKER')

        layout.separator()

        # DATABLOCKS

        l_row = layout.row()
        num = len(bpy.data.objects)
        l_row.label(text="Datablocks in the scene:")

        l_row = layout.row()
        db_cols.append(l_row.column())
        db_cols.append(l_row.column())

        row = db_cols[0].row()
        num = len(bpy.data.meshes)
        row.label(text=quantity_string(num, "Mesh", "Meshes"),
            icon='MESH_DATA')

        row = db_cols[1].row()
        num = len(bpy.data.curves)
        row.label(text=quantity_string(num, "Curve", "Curves"),
            icon='CURVE_DATA')

        row = db_cols[0].row()
        num = len(bpy.data.cameras)
        row.label(text=quantity_string(num, "Camera", "Cameras"),
            icon='CAMERA_DATA')

        row = db_cols[1].row()
        num = len(bpy.data.lamps)
        row.label(text=quantity_string(num, "Lamp", "Lamps"),
            icon='LAMP_DATA')

        row = db_cols[0].row()
        num = len(bpy.data.armatures)
        row.label(text=quantity_string(num, "Armature", "Armatures"),
            icon='ARMATURE_DATA')

        row = db_cols[1].row()
        num = len(bpy.data.lattices)
        row.label(text=quantity_string(num, "Lattice", "Lattices"),
            icon='LATTICE_DATA')

        row = db_cols[0].row()
        num = len(bpy.data.materials)
        row.label(text=quantity_string(num, "Material", "Materials"),
            icon='MATERIAL_DATA')

        row = db_cols[1].row()
        num = len(bpy.data.worlds)
        row.label(text=quantity_string(num, "World", "Worlds"),
            icon='WORLD_DATA')

        row = db_cols[0].row()
        num = len(bpy.data.textures)
        row.label(text=quantity_string(num, "Texture", "Textures"),
            icon='TEXTURE_DATA')

        row = db_cols[1].row()
        num = len(bpy.data.images)
        row.label(text=quantity_string(num, "Image", "Images"),
            icon='IMAGE_DATA')

        row = db_cols[0].row()
        num = len(bpy.data.texts)
        row.label(text=quantity_string(num, "Text", "Texts"),
            icon='TEXT')


def register():
    bpy.utils.register_module(__name__)

    pass

def unregister():
    bpy.utils.unregister_module(__name__)

    pass

if __name__ == "__main__":
    register()
