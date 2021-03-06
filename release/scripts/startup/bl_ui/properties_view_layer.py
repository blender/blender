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
from bpy.types import Panel, UIList


class VIEWLAYER_UL_aov(UIList):
    def draw_item(self, _context, layout, _data, item, icon, _active_data, _active_propname):
        row = layout.row()
        split = row.split(factor=0.65)
        icon = 'NONE' if item.is_valid else 'ERROR'
        split.row().prop(item, "name", text="", icon=icon, emboss=False)
        split.row().prop(item, "type", text="", emboss=False)


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

        scene = context.scene
        rd = scene.render
        layer = context.view_layer

        col = layout.column()
        col.prop(layer, "use", text="Use for Rendering")
        col.prop(rd, "use_single_layer", text="Render Single Layer")


class VIEWLAYER_PT_layer_passes(ViewLayerButtonsPanel, Panel):
    bl_label = "Passes"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        pass


class VIEWLAYER_PT_eevee_layer_passes_data(ViewLayerButtonsPanel, Panel):
    bl_label = "Data"
    bl_parent_id = "VIEWLAYER_PT_layer_passes"

    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer

        col = layout.column()
        col.prop(view_layer, "use_pass_combined")
        col.prop(view_layer, "use_pass_z")
        col.prop(view_layer, "use_pass_mist")
        col.prop(view_layer, "use_pass_normal")


class VIEWLAYER_PT_eevee_layer_passes_light(ViewLayerButtonsPanel, Panel):
    bl_label = "Light"
    bl_parent_id = "VIEWLAYER_PT_layer_passes"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        view_layer_eevee = view_layer.eevee

        col = layout.column(heading="Diffuse", align=True)
        col.prop(view_layer, "use_pass_diffuse_direct", text="Light")
        col.prop(view_layer, "use_pass_diffuse_color", text="Color")

        col = layout.column(heading="Specular", align=True)
        col.prop(view_layer, "use_pass_glossy_direct", text="Light")
        col.prop(view_layer, "use_pass_glossy_color", text="Color")

        col = layout.column(heading="Volume", align=True)
        col.prop(view_layer_eevee, "use_pass_volume_direct", text="Light")

        col = layout.column(heading="Other", align=True)
        col.prop(view_layer, "use_pass_emit", text="Emission")
        col.prop(view_layer, "use_pass_environment")
        col.prop(view_layer, "use_pass_shadow")
        col.prop(view_layer, "use_pass_ambient_occlusion",
                 text="Ambient Occlusion")


class VIEWLAYER_PT_eevee_layer_passes_effects(ViewLayerButtonsPanel, Panel):
    bl_label = "Effects"
    bl_parent_id = "VIEWLAYER_PT_layer_passes"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer
        view_layer_eevee = view_layer.eevee
        scene = context.scene
        scene_eevee = scene.eevee

        col = layout.column()
        col.prop(view_layer_eevee, "use_pass_bloom", text="Bloom")
        col.active = scene_eevee.use_bloom


class ViewLayerAOVPanel(ViewLayerButtonsPanel, Panel):
    bl_label = "Shader AOV"

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer

        row = layout.row()
        col = row.column()
        col.template_list("VIEWLAYER_UL_aov", "aovs", view_layer,
                          "aovs", view_layer, "active_aov_index", rows=2)

        col = row.column()
        sub = col.column(align=True)
        sub.operator("scene.view_layer_add_aov", icon='ADD', text="")
        sub.operator("scene.view_layer_remove_aov", icon='REMOVE', text="")

        aov = view_layer.active_aov
        if aov and not aov.is_valid:
            layout.label(
                text="Conflicts with another render pass with the same name", icon='ERROR')


class VIEWLAYER_PT_layer_passes_aov(ViewLayerAOVPanel):
    bl_parent_id = "VIEWLAYER_PT_layer_passes"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}


class ViewLayerCryptomattePanel(ViewLayerButtonsPanel, Panel):
    bl_label = "Cryptomatte"

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer

        col = layout.column()
        col.prop(view_layer, "use_pass_cryptomatte_object", text="Object")
        col.prop(view_layer, "use_pass_cryptomatte_material", text="Material")
        col.prop(view_layer, "use_pass_cryptomatte_asset", text="Asset")
        col = layout.column()
        col.active = any((view_layer.use_pass_cryptomatte_object,
                          view_layer.use_pass_cryptomatte_material,
                          view_layer.use_pass_cryptomatte_asset))
        col.prop(view_layer, "pass_cryptomatte_depth", text="Levels")
        col.prop(view_layer, "use_pass_cryptomatte_accurate",
                 text="Accurate Mode")


class VIEWLAYER_PT_layer_passes_cryptomatte(ViewLayerCryptomattePanel, Panel):
    bl_parent_id = "VIEWLAYER_PT_layer_passes"
    COMPAT_ENGINES = {'BLENDER_EEVEE'}


classes = (
    VIEWLAYER_PT_layer,
    VIEWLAYER_PT_layer_passes,
    VIEWLAYER_PT_eevee_layer_passes_data,
    VIEWLAYER_PT_eevee_layer_passes_light,
    VIEWLAYER_PT_eevee_layer_passes_effects,
    VIEWLAYER_PT_layer_passes_cryptomatte,
    VIEWLAYER_PT_layer_passes_aov,
    VIEWLAYER_UL_aov,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
