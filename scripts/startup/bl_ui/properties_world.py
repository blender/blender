# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Panel
from bpy.app.translations import contexts as i18n_contexts
from rna_prop_ui import PropertyPanel
from bpy_extras.node_utils import find_node_input


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
        'BLENDER_EEVEE',
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
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_EEVEE_NEXT'}

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


class WORLD_PT_custom_props(WorldButtonsPanel, PropertyPanel, Panel):
    COMPAT_ENGINES = {
        'BLENDER_RENDER',
        'BLENDER_EEVEE',
        'BLENDER_EEVEE_NEXT',
        'BLENDER_WORKBENCH',
    }
    _context_path = "world"
    _property_type = bpy.types.World


class EEVEE_WORLD_PT_surface(WorldButtonsPanel, Panel):
    bl_label = "Surface"
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_EEVEE_NEXT'}

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
    COMPAT_ENGINES = {'BLENDER_EEVEE', 'BLENDER_EEVEE_NEXT'}

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

        if node:
            input = find_node_input(node, "Volume")
            if input:
                layout.template_node_view(ntree, node, input)
            else:
                layout.label(text="Incompatible output node")
        else:
            layout.label(text="No output node")


class EEVEE_WORLD_PT_probe(WorldButtonsPanel, Panel):
    bl_label = "Light Probe"
    bl_translation_context = i18n_contexts.id_id
    bl_options = {'DEFAULT_CLOSED'}
    COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}

    @classmethod
    def poll(cls, context):
        engine = context.engine
        world = context.world
        return world and (engine in cls.COMPAT_ENGINES)

    def draw(self, context):
        layout = self.layout

        world = context.world

        layout.use_property_split = True
        layout.prop(world, "probe_resolution")


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
    EEVEE_WORLD_PT_probe,
    WORLD_PT_viewport_display,
    WORLD_PT_custom_props,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
