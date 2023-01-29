# SPDX-License-Identifier: GPL-2.0-or-later
from bpy.types import Menu, Panel, UIList, ViewLayer

from rna_prop_ui import PropertyPanel


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
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
        'BLENDER_WORKBENCH_NEXT'}

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
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_EEVEE_NEXT', 'BLENDER_WORKBENCH_NEXT'}

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


class VIEWLAYER_PT_eevee_next_layer_passes_data(ViewLayerButtonsPanel, Panel):
    bl_label = "Data"
    bl_parent_id = "VIEWLAYER_PT_layer_passes"

    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        scene = context.scene
        view_layer = context.view_layer

        col = layout.column()
        col.prop(view_layer, "use_pass_combined")
        col.prop(view_layer, "use_pass_z")
        col.prop(view_layer, "use_pass_mist")
        col.prop(view_layer, "use_pass_normal")
        col.prop(view_layer, "use_pass_position")
        sub = col.column()
        sub.active = not scene.eevee.use_motion_blur
        sub.prop(view_layer, "use_pass_vector")


class VIEWLAYER_PT_eevee_next_layer_passes_data(ViewLayerButtonsPanel, Panel):
    bl_label = "Data"
    bl_parent_id = "VIEWLAYER_PT_layer_passes"

    COMPAT_ENGINES = {'BLENDER_WORKBENCH_NEXT'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer

        col = layout.column()
        col.prop(view_layer, "use_pass_combined")
        col.prop(view_layer, "use_pass_z")


class VIEWLAYER_PT_eevee_layer_passes_light(ViewLayerButtonsPanel, Panel):
    bl_label = "Light"
    bl_parent_id = "VIEWLAYER_PT_layer_passes"
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_EEVEE_NEXT'}

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
                          "aovs", view_layer, "active_aov_index", rows=3)

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
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_EEVEE_NEXT'}


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

        if context.engine == 'BLENDER_EEVEE':
            col.prop(view_layer, "use_pass_cryptomatte_accurate",
                     text="Accurate Mode")


class VIEWLAYER_PT_layer_passes_cryptomatte(ViewLayerCryptomattePanel, Panel):
    bl_parent_id = "VIEWLAYER_PT_layer_passes"
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_EEVEE_NEXT'}


class VIEWLAYER_MT_lightgroup_sync(Menu):
    bl_label = "Lightgroup Sync"

    def draw(self, _context):
        layout = self.layout

        layout.operator("scene.view_layer_add_used_lightgroups", icon='ADD')
        layout.operator("scene.view_layer_remove_unused_lightgroups", icon='REMOVE')


class ViewLayerLightgroupsPanel(ViewLayerButtonsPanel, Panel):
    bl_label = "Light Groups"

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        view_layer = context.view_layer

        row = layout.row()
        col = row.column()
        col.template_list("UI_UL_list", "lightgroups", view_layer,
                          "lightgroups", view_layer, "active_lightgroup_index", rows=3)

        col = row.column()
        sub = col.column(align=True)
        sub.operator("scene.view_layer_add_lightgroup", icon='ADD', text="")
        sub.operator("scene.view_layer_remove_lightgroup", icon='REMOVE', text="")
        sub.separator()
        sub.menu("VIEWLAYER_MT_lightgroup_sync", icon='DOWNARROW_HLT', text="")


class VIEWLAYER_PT_layer_passes_lightgroups(ViewLayerLightgroupsPanel):
    bl_parent_id = "VIEWLAYER_PT_layer_passes"
    COMPAT_ENGINES = {'CYCLES'}


class VIEWLAYER_PT_layer_custom_props(PropertyPanel, Panel):
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "view_layer"
    _context_path = "view_layer"
    _property_type = ViewLayer


classes = (
    VIEWLAYER_MT_lightgroup_sync,
    VIEWLAYER_PT_layer,
    VIEWLAYER_PT_layer_passes,
    VIEWLAYER_PT_eevee_next_layer_passes_data,
    VIEWLAYER_PT_eevee_layer_passes_data,
    VIEWLAYER_PT_eevee_layer_passes_light,
    VIEWLAYER_PT_eevee_layer_passes_effects,
    VIEWLAYER_PT_layer_passes_cryptomatte,
    VIEWLAYER_PT_layer_passes_aov,
    VIEWLAYER_PT_layer_passes_lightgroups,
    VIEWLAYER_PT_layer_custom_props,
    VIEWLAYER_UL_aov,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
