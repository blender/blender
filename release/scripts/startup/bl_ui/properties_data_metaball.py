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
from bpy.types import Panel
from rna_prop_ui import PropertyPanel


class DataButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "data"

    @classmethod
    def poll(cls, context):
        return context.meta_ball


class DATA_PT_context_metaball(DataButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}

    def draw(self, context):
        layout = self.layout

        ob = context.object
        mball = context.meta_ball
        space = context.space_data

        if ob:
            layout.template_ID(ob, "data")
        elif mball:
            layout.template_ID(space, "pin_id")


class DATA_PT_metaball(DataButtonsPanel, Panel):
    bl_label = "Metaball"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        mball = context.meta_ball

        col = layout.column()
        col.label(text="Resolution:")
        sub = col.column(align=True)
        sub.prop(mball, "resolution", text="Resolution View")
        sub.prop(mball, "render_resolution", text="Render")

        col.prop(mball, "threshold", text="Threshold")
        col.prop(mball, "update_method")


class DATA_PT_mball_texture_space(DataButtonsPanel, Panel):
    bl_label = "Texture Space"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        mball = context.meta_ball

        layout.prop(mball, "use_auto_texspace")

        col = layout.column()
        col.prop(mball, "texspace_location")
        col.prop(mball, "texspace_size")


class DATA_PT_metaball_element(DataButtonsPanel, Panel):
    bl_label = "Active Element"

    @classmethod
    def poll(cls, context):
        return (context.meta_ball and context.meta_ball.elements.active)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        metaelem = context.meta_ball.elements.active

        col = layout.column()

        col.prop(metaelem, "type")

        col.separator()

        col.prop(metaelem, "stiffness", text="Stiffness")
        col.prop(metaelem, "use_negative", text="Negative")
        col.prop(metaelem, "hide", text="Hide")

        sub = col.column(align=True)

        if metaelem.type in {'CUBE', 'ELLIPSOID'}:
            sub.prop(metaelem, "size_x", text="Size X")
            sub.prop(metaelem, "size_y", text="Y")
            sub.prop(metaelem, "size_z", text="Z")

        elif metaelem.type == 'TUBE':
            sub.prop(metaelem, "size_x", text="Size X")

        elif metaelem.type == 'PLANE':
            sub.prop(metaelem, "size_x", text="Size X")
            sub.prop(metaelem, "size_y", text="Y")


class DATA_PT_custom_props_metaball(DataButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_OPENGL'}
    _context_path = "object.data"
    _property_type = bpy.types.MetaBall


classes = (
    DATA_PT_context_metaball,
    DATA_PT_metaball,
    DATA_PT_mball_texture_space,
    DATA_PT_metaball_element,
    DATA_PT_custom_props_metaball,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
