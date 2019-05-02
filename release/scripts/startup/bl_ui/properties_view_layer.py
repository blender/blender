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
from bpy.types import Panel


class ViewLayerButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "view_layer"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)


class VIEWLAYER_PT_layer(ViewLayerButtonsPanel, Panel):
    bl_label = "View Layer"
    COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE', 'BLENDER_WORKBENCH'}

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        layout.use_property_split = True

        scene = context.scene
        rd = scene.render
        layer = context.view_layer

        col = flow.column()
        col.prop(layer, "use", text="Use for Rendering")
        col = flow.column()
        col.prop(rd, "use_single_layer", text="Render Single Layer")


class VIEWLAYER_PT_eevee_layer_passes(ViewLayerButtonsPanel, Panel):
    bl_label = "Passes"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True

        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=False)

        view_layer = context.view_layer

        col = flow.column()
        col.prop(view_layer, "use_pass_combined")
        col = flow.column()
        col.prop(view_layer, "use_pass_z")
        col = flow.column()
        col.prop(view_layer, "use_pass_mist")
        col = flow.column()
        col.prop(view_layer, "use_pass_normal")
        col = flow.column()
        col.prop(view_layer, "use_pass_ambient_occlusion")
        col = flow.column()
        col.prop(view_layer, "use_pass_subsurface_direct", text="Subsurface Direct")
        col = flow.column()
        col.prop(view_layer, "use_pass_subsurface_color", text="Subsurface Color")


classes = (
    VIEWLAYER_PT_layer,
    VIEWLAYER_PT_eevee_layer_passes,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
