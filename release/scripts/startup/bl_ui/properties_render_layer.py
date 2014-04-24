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


class RenderLayerButtonsPanel():
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "render_layer"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        scene = context.scene
        return scene and (scene.render.engine in cls.COMPAT_ENGINES)


class RENDERLAYER_UL_renderlayers(UIList):
    def draw_item(self, context, layout, data, item, icon, active_data, active_propname, index):
        # assert(isinstance(item, bpy.types.SceneRenderLayer)
        layer = item
        if self.layout_type in {'DEFAULT', 'COMPACT'}:
            layout.prop(layer, "name", text="", icon_value=icon, emboss=False)
            layout.prop(layer, "use", text="", index=index)
        elif self.layout_type in {'GRID'}:
            layout.alignment = 'CENTER'
            layout.label("", icon_value=icon)


class RENDERLAYER_PT_layers(RenderLayerButtonsPanel, Panel):
    bl_label = "Layer List"
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render

        row = layout.row()
        col = row.column()
        col.template_list("RENDERLAYER_UL_renderlayers", "", rd, "layers", rd.layers, "active_index", rows=2)

        col = row.column()
        sub = col.column(align=True)
        sub.operator("scene.render_layer_add", icon='ZOOMIN', text="")
        sub.operator("scene.render_layer_remove", icon='ZOOMOUT', text="")
        col.prop(rd, "use_single_layer", icon_only=True)


class RENDERLAYER_PT_layer_options(RenderLayerButtonsPanel, Panel):
    bl_label = "Layer"
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        rl = rd.layers.active

        split = layout.split()

        col = split.column()
        col.prop(scene, "layers", text="Scene")
        col.label(text="")
        col.prop(rl, "light_override", text="Light")
        col.prop(rl, "material_override", text="Material")

        col = split.column()
        col.prop(rl, "layers", text="Layer")
        col.prop(rl, "layers_zmask", text="Mask Layer")

        layout.separator()
        layout.label(text="Include:")

        split = layout.split()

        col = split.column()
        col.prop(rl, "use_zmask")
        row = col.row()
        row.prop(rl, "invert_zmask", text="Negate")
        row.active = rl.use_zmask
        col.prop(rl, "use_all_z")

        col = split.column()
        col.prop(rl, "use_solid")
        col.prop(rl, "use_halo")
        col.prop(rl, "use_ztransp")

        col = split.column()
        col.prop(rl, "use_sky")
        col.prop(rl, "use_edge_enhance")
        col.prop(rl, "use_strand")
        if bpy.app.build_options.freestyle:
            row = col.row()
            row.prop(rl, "use_freestyle")
            row.active = rd.use_freestyle


class RENDERLAYER_PT_layer_passes(RenderLayerButtonsPanel, Panel):
    bl_label = "Passes"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_RENDER'}

    def draw_pass_type_buttons(self, box, rl, pass_type):
        # property names
        use_pass_type = "use_pass_" + pass_type
        exclude_pass_type = "exclude_" + pass_type
        # draw pass type buttons
        row = box.row()
        row.prop(rl, use_pass_type)
        row.prop(rl, exclude_pass_type, text="")

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        rd = scene.render
        rl = rd.layers.active

        split = layout.split()

        col = split.column()
        col.prop(rl, "use_pass_combined")
        col.prop(rl, "use_pass_z")
        col.prop(rl, "use_pass_vector")
        col.prop(rl, "use_pass_normal")
        col.prop(rl, "use_pass_uv")
        col.prop(rl, "use_pass_mist")
        col.prop(rl, "use_pass_object_index")
        col.prop(rl, "use_pass_material_index")
        col.prop(rl, "use_pass_color")

        col = split.column()
        col.prop(rl, "use_pass_diffuse")
        self.draw_pass_type_buttons(col, rl, "specular")
        self.draw_pass_type_buttons(col, rl, "shadow")
        self.draw_pass_type_buttons(col, rl, "emit")
        self.draw_pass_type_buttons(col, rl, "ambient_occlusion")
        self.draw_pass_type_buttons(col, rl, "environment")
        self.draw_pass_type_buttons(col, rl, "indirect")
        self.draw_pass_type_buttons(col, rl, "reflection")
        self.draw_pass_type_buttons(col, rl, "refraction")


if __name__ == "__main__":  # only for live edit.
    bpy.utils.register_module(__name__)
