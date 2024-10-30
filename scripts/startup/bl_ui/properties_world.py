# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Panel
from bpy.app.translations import contexts as i18n_contexts
from rna_prop_ui import PropertyPanel
from bpy_extras.node_utils import find_node_input
from .space_properties import PropertiesAnimationMixin


class WorldButtonsPanel:
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "world"
    # COMPAT_ENGINES must be defined in each subclass, external engines can add themselves here

    @classmethod
    def poll(cls, context):
        return (context.world and context.engine in cls.COMPAT_ENGINES)


class WORLD_PT_context_world(WorldButtonsPanel, Panel):
    bl_label = ""
    bl_options = {'HIDE_HEADER'}
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

    @classmethod
    def poll(cls, context):
        return (context.engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        world = context.world
        space = context.space_data

        if scene:
            layout.template_ID(scene, "world", new="world.new")
        elif world:
            layout.template_ID(space, "pin_id")


class EEVEE_WORLD_PT_mist(WorldButtonsPanel, Panel):
    bl_label = "Mist Pass"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.world and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        world = context.world

        col = layout.column(align=True)
        col.prop(world.mist_settings, "start")
        col.prop(world.mist_settings, "depth")

        col = layout.column()
        col.prop(world.mist_settings, "falloff")


class WORLD_PT_animation(WorldButtonsPanel, PropertiesAnimationMixin, PropertyPanel, Panel):
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False

        # WorldButtonsPanel.poll ensures this is not None.
        world = context.world

        col = layout.column(align=True)
        col.label(text="World")
        self.draw_action_and_slot_selector(context, col, world)

        if node_tree := world.node_tree:
            col = layout.column(align=True)
            col.label(text="Shader Node Tree")
            self.draw_action_and_slot_selector(context, col, node_tree)


class WORLD_PT_custom_props(WorldButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }
    _context_path = "world"
    _property_type = bpy.types.World


class EEVEE_WORLD_PT_surface(WorldButtonsPanel, Panel):
    bl_label = "Surface"
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        return context.world and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        world = context.world

        layout.prop(world, "use_nodes", icon='NODETREE')
        layout.separator()

        layout.use_property_split = True

        if world.use_nodes:
            ntree = world.node_tree
            node = ntree.get_output_node('EEVEE')

            if node:
                input = find_node_input(node, "Surface")
                if input:
                    layout.template_node_view(ntree, node, input)
                else:
                    layout.label(text="Incompatible output node")
            else:
                layout.label(text="No output node")
        else:
            layout.prop(world, "color")


class EEVEE_WORLD_PT_volume(WorldButtonsPanel, Panel):
    bl_label = "Volume"
    bl_translation_context = i18n_contexts.id_id
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        world = context.world
        return world and world.use_nodes and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        world = context.world
        ntree = world.node_tree
        node = ntree.get_output_node('EEVEE')

        layout.use_property_split = True

        if world.use_eevee_finite_volume:
            layout.operator("world.convert_volume_to_mesh", icon='WORLD', text="Convert Volume")

        if node:
            input = find_node_input(node, "Volume")
            if input:
                layout.template_node_view(ntree, node, input)
            else:
                layout.label(text="Incompatible output node")
        else:
            layout.label(text="No output node")


class EEVEE_WORLD_PT_settings(WorldButtonsPanel, Panel):
    bl_label = "Settings"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        world = context.world
        return world and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        pass


class EEVEE_WORLD_PT_lightprobe(WorldButtonsPanel, Panel):
    bl_label = "Light Probe"
    bl_parent_id = "EEVEE_WORLD_PT_settings"
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw(self, context):
        layout = self.layout

        world = context.world

        layout.use_property_split = True
        layout.prop(world, "probe_resolution", text="Resolution")


class EEVEE_WORLD_PT_sun(WorldButtonsPanel, Panel):
    bl_label = "Sun"
    bl_parent_id = "EEVEE_WORLD_PT_settings"
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw(self, context):
        layout = self.layout

        world = context.world

        layout.use_property_split = True
        layout.prop(world, "sun_threshold", text="Threshold")
        layout.prop(world, "sun_angle", text="Angle")


class EEVEE_WORLD_PT_sun_shadow(WorldButtonsPanel, Panel):
    bl_label = "Shadow"
    bl_parent_id = "EEVEE_WORLD_PT_sun"
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    def draw_header(self, context):
        world = context.world
        self.layout.prop(world, "use_sun_shadow", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        world = context.world

        col = layout.column(align=False, heading="Jitter")
        row = col.row(align=True)
        sub = row.row(align=True)
        sub.prop(world, "use_sun_shadow_jitter", text="")
        sub = sub.row(align=True)
        sub.active = world.use_sun_shadow_jitter
        sub.prop(world, "sun_shadow_jitter_overblur", text="Overblur")

        col.separator()

        col = layout.column()
        col.prop(world, "sun_shadow_filter_radius", text="Filter")
        col.prop(world, "sun_shadow_maximum_resolution", text="Resolution Limit")


class WORLD_PT_viewport_display(WorldButtonsPanel, Panel):
    bl_label = "Viewport Display"
    bl_options = {'DEFAULT_CLOSED'}
    bl_order = 10

    @classmethod
    def poll(cls, context):
        return context.world

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        world = context.world
        layout.prop(world, "color")


classes = (
    WORLD_PT_context_world,
    EEVEE_WORLD_PT_surface,
    EEVEE_WORLD_PT_volume,
    EEVEE_WORLD_PT_mist,
    EEVEE_WORLD_PT_settings,
    EEVEE_WORLD_PT_lightprobe,
    EEVEE_WORLD_PT_sun,
    EEVEE_WORLD_PT_sun_shadow,
    WORLD_PT_viewport_display,
    WORLD_PT_animation,
    WORLD_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
