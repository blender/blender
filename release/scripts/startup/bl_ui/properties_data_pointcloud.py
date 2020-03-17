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
import bpy
from bpy.types import Panel, UIList
from rna_prop_ui import PropertyPanel


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        engine = context.scene.render.engine
        return hasattr(context, 'pointcloud') and context.pointcloud and (engine in cls.COMPAT_ENGINES)


class DATA_PT_context_pointcloud(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        pointcloud = context.pointcloud
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif pointcloud:
            layout.template_ID(space, "pin_id")


class DATA_PT_pointcloud(DataButtonsPanel, Panel):
    bl_label = "Point Cloud"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout
        pointcloud = context.pointcloud
        pass

class DATA_PT_custom_props_pointcloud(DataButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}
    _context_path = "object.data"
    _property_type = bpy.types.PointCloud if hasattr(bpy.types, "PointCloud") else None


classes = (
    DATA_PT_context_pointcloud,
    DATA_PT_pointcloud,
    DATA_PT_custom_props_pointcloud,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
